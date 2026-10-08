#pragma once

// The wire, behind an interface. Game code (GameServer / GameClient) only
// ever talks to a Transport: connect or listen, send a message reliably or
// not, poll for what arrived. ENet is the shipped backend
// (make_enet_transport); SimTransport wraps any backend to add latency,
// jitter and loss for testing. Swap the backend (Steam sockets, WebRTC,
// QUIC...) without touching the game.
//
// Not thread-safe: one thread owns a Transport.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ts {
namespace tombstone {
namespace net {

// Peers are small numbers, never reused within one Transport. 0 = nobody.
using PeerId = std::uint32_t;
inline constexpr PeerId kNoPeer = 0;

// Reliable: arrives once, in order (resent until acknowledged). Unreliable:
// may drop or arrive late; newer ones win. Separate channels, so a lost
// snapshot never holds up a toast and the other way round.
enum class Delivery : std::uint8_t { Reliable, Unreliable };

struct TransportEvent {
  enum class Type : std::uint8_t { Connected, Disconnected, Received };
  Type type = Type::Received;
  PeerId peer = kNoPeer;
  // Disconnected: the code the far side gave (0 = none / timed out).
  std::uint32_t reason = 0;
  Delivery delivery = Delivery::Reliable;
  std::vector<std::uint8_t> data;
};

// Traffic counters for one peer (application payload bytes, so a sim layer
// above the socket counts the same way) plus the transport's own RTT.
struct LinkStats {
  std::uint64_t bytes_sent = 0;
  std::uint64_t bytes_received = 0;
  std::uint64_t packets_sent = 0;
  std::uint64_t packets_received = 0;
  std::uint32_t rtt_ms = 0;
};

class Transport {
 public:
  virtual ~Transport() = default;

  // Server side: bind UDP `port` on every interface (0 = any free port; see
  // port()) for up to max_peers connections.
  virtual bool listen(std::uint16_t port, std::size_t max_peers,
                      std::string* error_out) = 0;
  // Client side: start connecting (host name or address). A Connected or
  // Disconnected event reports how it went.
  virtual PeerId connect(const std::string& host, std::uint16_t port,
                         std::string* error_out) = 0;
  // Queue a message. False when the peer is gone.
  virtual bool send(PeerId peer, Delivery delivery, const std::uint8_t* data,
                    std::size_t size) = 0;
  bool send(PeerId peer, Delivery delivery,
            const std::vector<std::uint8_t>& data) {
    return send(peer, delivery, data.data(), data.size());
  }
  // Graceful: queued reliable messages go first, then the far side hears
  // Disconnected with `reason`; we hear it too once it acknowledges.
  virtual void disconnect(PeerId peer, std::uint32_t reason) = 0;
  // Forget a peer now (no goodbye, no event).
  virtual void drop(PeerId peer) = 0;
  // Service the socket and hand out one event. False when nothing waits.
  virtual bool poll(TransportEvent* out) = 0;
  // Push queued messages onto the wire now.
  virtual void flush() = 0;
  // Bound port (listen) or 0.
  virtual std::uint16_t port() const = 0;
  virtual LinkStats stats(PeerId peer) const = 0;
  // Tear everything down immediately (peers time out on their side).
  virtual void close() = 0;
  virtual const char* name() const = 0;
};

// UDP via ENet: 2 channels (0 reliable, 1 unreliable), 3-10 s timeouts.
std::unique_ptr<Transport> make_enet_transport();

// Connect-time check word ENet carries in its own handshake ("TSMP").
inline constexpr std::uint32_t kConnectMagic = 0x504D5354u;

// Parse "host:port" / "[v6]:port" / "host" (default_port). False when the
// port is not 1..65535.
bool parse_endpoint(const std::string& text, std::uint16_t default_port,
                    std::string* host, std::uint16_t* port);

}  // namespace net
}  // namespace tombstone
}  // namespace ts
