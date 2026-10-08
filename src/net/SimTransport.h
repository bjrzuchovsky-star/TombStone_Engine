#pragma once

// A Transport that wraps another and makes the wire worse on purpose:
// added one-way latency, jitter and packet loss, applied to everything this
// side sends and receives. Used by the editor's local posse and the network
// smoke to test prediction and interpolation on a bad line without leaving
// the machine.
//
// Loss only hits unreliable traffic (snapshots, input, pings): the reliable
// channel is resent by the transport underneath until it lands, so dropping
// it up here would lose it for good, which no real network does. Reliable
// traffic is delayed and stays in order.

#include "net/Transport.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace ts {
namespace tombstone {
namespace net {

struct LinkSim {
  int latency_ms = 0;   // added each way
  int jitter_ms = 0;    // +- uniformly on top of latency
  float loss = 0.0f;    // 0..1, unreliable messages only
  bool active() const { return latency_ms > 0 || jitter_ms > 0 || loss > 0.0f; }
};

class SimTransport final : public Transport {
 public:
  SimTransport(std::unique_ptr<Transport> inner, LinkSim sim,
               std::uint64_t seed = 0x5EED5EEDu);
  ~SimTransport() override;

  // Thread-safe: the editor's UI thread tunes a server running elsewhere.
  void set_sim(const LinkSim& sim);
  LinkSim sim() const;
  // Messages the sim threw away (both directions).
  std::uint64_t dropped() const;

  bool listen(std::uint16_t port, std::size_t max_peers,
              std::string* error_out) override;
  PeerId connect(const std::string& host, std::uint16_t port,
                 std::string* error_out) override;
  bool send(PeerId peer, Delivery delivery, const std::uint8_t* data,
            std::size_t size) override;
  using Transport::send;
  void disconnect(PeerId peer, std::uint32_t reason) override;
  void drop(PeerId peer) override;
  bool poll(TransportEvent* out) override;
  void flush() override;
  std::uint16_t port() const override;
  LinkStats stats(PeerId peer) const override;
  void close() override;
  const char* name() const override { return inner_->name(); }

 private:
  struct Outgoing {
    double due = 0.0;
    PeerId peer = kNoPeer;
    bool is_disconnect = false;
    std::uint32_t reason = 0;
    Delivery delivery = Delivery::Reliable;
    std::vector<std::uint8_t> data;
  };
  struct Incoming {
    double due = 0.0;
    std::uint64_t order = 0;
    TransportEvent event;
  };

  double now_ms() const;
  // Delivery time for something leaving now; ordered traffic never
  // overtakes what went before it on the same peer.
  double schedule(PeerId peer, bool ordered, std::unordered_map<PeerId, double>* last);
  bool lose();
  void pump_outgoing(bool all);
  void pump_incoming();

  std::unique_ptr<Transport> inner_;
  mutable std::mutex sim_mutex_;
  LinkSim sim_;
  std::uint64_t rng_ = 0;
  std::uint64_t dropped_ = 0;
  std::uint64_t order_ = 0;
  std::deque<Outgoing> outgoing_;
  std::vector<Incoming> incoming_;
  std::unordered_map<PeerId, double> last_out_;
  std::unordered_map<PeerId, double> last_in_;
};

}  // namespace net
}  // namespace tombstone
}  // namespace ts
