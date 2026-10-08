#include "net/SimTransport.h"

#include <algorithm>
#include <chrono>

namespace ts {
namespace tombstone {
namespace net {

namespace {

std::uint64_t splitmix(std::uint64_t* s) {
  std::uint64_t z = (*s += 0x9E3779B97F4A7C15ull);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

double unit(std::uint64_t* s) {
  return static_cast<double>(splitmix(s) >> 11) * (1.0 / static_cast<double>(std::uint64_t{1} << 53));
}

}  // namespace

SimTransport::SimTransport(std::unique_ptr<Transport> inner, LinkSim sim,
                           std::uint64_t seed)
    : inner_(std::move(inner)), sim_(sim), rng_(seed) {}

SimTransport::~SimTransport() = default;

void SimTransport::set_sim(const LinkSim& sim) {
  std::lock_guard<std::mutex> lock(sim_mutex_);
  sim_ = sim;
  sim_.latency_ms = std::clamp(sim_.latency_ms, 0, 2000);
  sim_.jitter_ms = std::clamp(sim_.jitter_ms, 0, 1000);
  sim_.loss = std::clamp(sim_.loss, 0.0f, 0.9f);
}

LinkSim SimTransport::sim() const {
  std::lock_guard<std::mutex> lock(sim_mutex_);
  return sim_;
}

std::uint64_t SimTransport::dropped() const {
  std::lock_guard<std::mutex> lock(sim_mutex_);
  return dropped_;
}

double SimTransport::now_ms() const {
  using namespace std::chrono;
  return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

double SimTransport::schedule(PeerId peer, bool ordered,
                              std::unordered_map<PeerId, double>* last) {
  const LinkSim s = sim();
  double delay = s.latency_ms;
  if (s.jitter_ms > 0) {
    delay += (unit(&rng_) * 2.0 - 1.0) * s.jitter_ms;
  }
  double due = now_ms() + std::max(0.0, delay);
  if (ordered) {
    double& prev = (*last)[peer];
    due = std::max(due, prev);
    prev = due;
  }
  return due;
}

bool SimTransport::lose() {
  const LinkSim s = sim();
  if (s.loss <= 0.0f || unit(&rng_) >= s.loss) return false;
  std::lock_guard<std::mutex> lock(sim_mutex_);
  ++dropped_;
  return true;
}

bool SimTransport::listen(std::uint16_t port, std::size_t max_peers,
                          std::string* error_out) {
  return inner_->listen(port, max_peers, error_out);
}

PeerId SimTransport::connect(const std::string& host, std::uint16_t port,
                             std::string* error_out) {
  return inner_->connect(host, port, error_out);
}

bool SimTransport::send(PeerId peer, Delivery delivery, const std::uint8_t* data,
                        std::size_t size) {
  if (delivery == Delivery::Unreliable && lose()) {
    return true;  // gone on the wire; the sender never knows
  }
  Outgoing o;
  o.peer = peer;
  o.delivery = delivery;
  o.data.assign(data, data + size);
  o.due = schedule(peer, delivery == Delivery::Reliable, &last_out_);
  outgoing_.push_back(std::move(o));
  pump_outgoing(false);
  return true;
}

void SimTransport::disconnect(PeerId peer, std::uint32_t reason) {
  // After everything reliable already queued for this peer.
  Outgoing o;
  o.peer = peer;
  o.is_disconnect = true;
  o.reason = reason;
  o.due = schedule(peer, true, &last_out_);
  outgoing_.push_back(std::move(o));
}

void SimTransport::drop(PeerId peer) {
  outgoing_.erase(std::remove_if(outgoing_.begin(), outgoing_.end(),
                                 [&](const Outgoing& o) { return o.peer == peer; }),
                  outgoing_.end());
  incoming_.erase(std::remove_if(incoming_.begin(), incoming_.end(),
                                 [&](const Incoming& i) { return i.event.peer == peer; }),
                  incoming_.end());
  last_out_.erase(peer);
  last_in_.erase(peer);
  inner_->drop(peer);
}

void SimTransport::pump_outgoing(bool all) {
  const double now = now_ms();
  // Release everything due; unreliable items may pass reliable ones.
  for (auto it = outgoing_.begin(); it != outgoing_.end();) {
    if (!all && it->due > now) {
      ++it;
      continue;
    }
    if (it->is_disconnect) {
      inner_->disconnect(it->peer, it->reason);
    } else {
      inner_->send(it->peer, it->delivery, it->data.data(), it->data.size());
    }
    it = outgoing_.erase(it);
  }
}

void SimTransport::pump_incoming() {
  TransportEvent ev;
  while (inner_->poll(&ev)) {
    const bool unreliable = ev.type == TransportEvent::Type::Received &&
                            ev.delivery == Delivery::Unreliable;
    if (unreliable && lose()) {
      continue;
    }
    Incoming in;
    in.due = schedule(ev.peer, !unreliable, &last_in_);
    in.order = order_++;
    in.event = std::move(ev);
    if (in.event.type == TransportEvent::Type::Disconnected) {
      last_in_.erase(in.event.peer);
    }
    incoming_.push_back(std::move(in));
  }
}

bool SimTransport::poll(TransportEvent* out) {
  pump_outgoing(false);
  pump_incoming();
  const double now = now_ms();
  auto best = incoming_.end();
  for (auto it = incoming_.begin(); it != incoming_.end(); ++it) {
    if (it->due > now) continue;
    if (best == incoming_.end() || it->due < best->due ||
        (it->due == best->due && it->order < best->order)) {
      best = it;
    }
  }
  if (best == incoming_.end()) return false;
  *out = std::move(best->event);
  incoming_.erase(best);
  return true;
}

void SimTransport::flush() {
  pump_outgoing(false);
  inner_->flush();
}

std::uint16_t SimTransport::port() const { return inner_->port(); }

LinkStats SimTransport::stats(PeerId peer) const { return inner_->stats(peer); }

void SimTransport::close() {
  outgoing_.clear();
  incoming_.clear();
  last_out_.clear();
  last_in_.clear();
  inner_->close();
}

}  // namespace net
}  // namespace tombstone
}  // namespace ts
