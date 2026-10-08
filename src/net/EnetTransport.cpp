// ENet backend for net::Transport.

#include "net/Transport.h"

#include "net/Net.h"

#include <enet/enet.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <unordered_map>

namespace ts {
namespace tombstone {
namespace net {

namespace {

constexpr std::size_t kChannels = 2;
constexpr enet_uint8 kReliableChannel = 0;
constexpr enet_uint8 kUnreliableChannel = 1;
// Peer timeouts (ms): give up after 3 s of silence at best, 10 s at most.
constexpr enet_uint32 kTimeoutMin = 3000;
constexpr enet_uint32 kTimeoutMax = 10000;

PeerId peer_id(const ENetPeer* p) {
  return static_cast<PeerId>(reinterpret_cast<std::uintptr_t>(p->data));
}

class EnetTransport final : public Transport {
 public:
  ~EnetTransport() override { close(); }

  bool listen(std::uint16_t port, std::size_t max_peers,
              std::string* error_out) override {
    if (!open_library(error_out)) return false;
    close_host();
    ENetAddress addr;
    std::memset(&addr, 0, sizeof addr);
    addr.host = ENET_HOST_ANY;
    addr.port = port;
    host_ = enet_host_create(&addr, max_peers, kChannels, 0, 0);
    if (!host_) {
      if (error_out) {
        *error_out = "Could not bind UDP port " + std::to_string(port) +
                     " (already in use, or not allowed).";
      }
      return false;
    }
    return true;
  }

  PeerId connect(const std::string& host, std::uint16_t port,
                 std::string* error_out) override {
    if (!open_library(error_out)) return kNoPeer;
    close_host();
    host_ = enet_host_create(nullptr, 1, kChannels, 0, 0);
    if (!host_) {
      if (error_out) *error_out = "Could not open a UDP socket.";
      return kNoPeer;
    }
    ENetAddress addr;
    std::memset(&addr, 0, sizeof addr);
    if (enet_address_set_host(&addr, host.c_str()) != 0) {
      if (error_out) *error_out = "Could not find host \"" + host + "\".";
      close_host();
      return kNoPeer;
    }
    addr.port = port;
    ENetPeer* p = enet_host_connect(host_, &addr, kChannels, kConnectMagic);
    if (!p) {
      if (error_out) *error_out = "Could not start a connection.";
      close_host();
      return kNoPeer;
    }
    enet_peer_timeout(p, 0, kTimeoutMin, kTimeoutMax);
    return adopt(p);
  }

  bool send(PeerId peer, Delivery delivery, const std::uint8_t* data,
            std::size_t size) override {
    ENetPeer* p = find(peer);
    if (!p || p->state != ENET_PEER_STATE_CONNECTED) return false;
    const bool reliable = delivery == Delivery::Reliable;
    // Unreliable messages bigger than the MTU fragment unreliably too (all
    // pieces or nothing), instead of ENet's default of sending them
    // reliably.
    ENetPacket* pkt = enet_packet_create(
        data, size,
        reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT);
    if (!pkt) return false;
    if (enet_peer_send(p, reliable ? kReliableChannel : kUnreliableChannel, pkt) != 0) {
      enet_packet_destroy(pkt);
      return false;
    }
    LinkStats& s = stats_[peer];
    s.bytes_sent += size;
    ++s.packets_sent;
    return true;
  }

  void disconnect(PeerId peer, std::uint32_t reason) override {
    if (ENetPeer* p = find(peer)) {
      enet_peer_disconnect_later(p, reason);
    }
  }

  void drop(PeerId peer) override {
    if (ENetPeer* p = find(peer)) {
      p->data = nullptr;
      enet_peer_reset(p);
    }
    peers_.erase(peer);
    stats_.erase(peer);
  }

  bool poll(TransportEvent* out) override {
    if (!host_) return false;
    ENetEvent ev;
    while (enet_host_service(host_, &ev, 0) > 0) {
      switch (ev.type) {
        case ENET_EVENT_TYPE_CONNECT: {
          PeerId id = peer_id(ev.peer);
          if (id == kNoPeer) {
            // Somebody new knocking (server side).
            if (ev.data != kConnectMagic) {
              enet_peer_reset(ev.peer);  // not one of ours
              continue;
            }
            enet_peer_timeout(ev.peer, 0, kTimeoutMin, kTimeoutMax);
            id = adopt(ev.peer);
          }
          *out = TransportEvent{};
          out->type = TransportEvent::Type::Connected;
          out->peer = id;
          return true;
        }
        case ENET_EVENT_TYPE_RECEIVE: {
          const PeerId id = peer_id(ev.peer);
          if (id == kNoPeer) {
            enet_packet_destroy(ev.packet);
            continue;
          }
          *out = TransportEvent{};
          out->type = TransportEvent::Type::Received;
          out->peer = id;
          out->delivery = ev.channelID == kReliableChannel ? Delivery::Reliable
                                                           : Delivery::Unreliable;
          out->data.assign(ev.packet->data, ev.packet->data + ev.packet->dataLength);
          LinkStats& s = stats_[id];
          s.bytes_received += ev.packet->dataLength;
          ++s.packets_received;
          enet_packet_destroy(ev.packet);
          return true;
        }
        case ENET_EVENT_TYPE_DISCONNECT: {
          const PeerId id = peer_id(ev.peer);
          ev.peer->data = nullptr;
          if (id == kNoPeer) continue;
          peers_.erase(id);
          stats_.erase(id);
          *out = TransportEvent{};
          out->type = TransportEvent::Type::Disconnected;
          out->peer = id;
          out->reason = ev.data;
          return true;
        }
        case ENET_EVENT_TYPE_NONE:
        default:
          break;
      }
    }
    return false;
  }

  void flush() override {
    if (host_) enet_host_flush(host_);
  }

  std::uint16_t port() const override { return host_ ? host_->address.port : 0; }

  LinkStats stats(PeerId peer) const override {
    LinkStats s;
    auto it = stats_.find(peer);
    if (it != stats_.end()) s = it->second;
    auto pit = peers_.find(peer);
    if (pit != peers_.end()) s.rtt_ms = pit->second->roundTripTime;
    return s;
  }

  void close() override {
    close_host();
    if (library_) {
      net_shutdown();
      library_ = false;
    }
  }

  const char* name() const override { return "ENet 1.3.18 (UDP)"; }

 private:
  bool open_library(std::string* error_out) {
    if (library_) return true;
    if (!net_startup(error_out)) return false;
    library_ = true;
    return true;
  }

  void close_host() {
    if (host_) {
      for (auto& kv : peers_) {
        kv.second->data = nullptr;
      }
      enet_host_destroy(host_);
      host_ = nullptr;
    }
    peers_.clear();
    stats_.clear();
  }

  PeerId adopt(ENetPeer* p) {
    const PeerId id = next_id_++;
    p->data = reinterpret_cast<void*>(static_cast<std::uintptr_t>(id));
    peers_[id] = p;
    stats_[id] = LinkStats{};
    return id;
  }

  ENetPeer* find(PeerId peer) const {
    auto it = peers_.find(peer);
    return it == peers_.end() ? nullptr : it->second;
  }

  ENetHost* host_ = nullptr;
  bool library_ = false;
  PeerId next_id_ = 1;
  std::unordered_map<PeerId, ENetPeer*> peers_;
  std::unordered_map<PeerId, LinkStats> stats_;
};

}  // namespace

std::unique_ptr<Transport> make_enet_transport() {
  return std::make_unique<EnetTransport>();
}

bool parse_endpoint(const std::string& text, std::uint16_t default_port,
                    std::string* host, std::uint16_t* port) {
  std::string h = text;
  std::string p;
  if (!h.empty() && h.front() == '[') {
    const std::size_t close = h.find(']');
    if (close == std::string::npos) return false;
    if (close + 1 < h.size()) {
      if (h[close + 1] != ':') return false;
      p = h.substr(close + 2);
    }
    h = h.substr(1, close - 1);
  } else {
    const std::size_t colon = h.rfind(':');
    if (colon != std::string::npos && h.find(':') == colon) {
      p = h.substr(colon + 1);
      h = h.substr(0, colon);
    }
  }
  if (h.empty()) return false;
  unsigned long value = default_port;
  if (!p.empty()) {
    if (p.size() > 5 ||
        std::any_of(p.begin(), p.end(), [](char c) { return c < '0' || c > '9'; })) {
      return false;
    }
    value = std::stoul(p);
  }
  if (value == 0 || value > 65535) return false;
  *host = h;
  *port = static_cast<std::uint16_t>(value);
  return true;
}

}  // namespace net
}  // namespace tombstone
}  // namespace ts
