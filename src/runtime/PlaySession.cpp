#include "runtime/PlaySession.h"

#include <algorithm>

namespace ts {
namespace tombstone {
namespace runtime {

const char* to_string(PlayState state) {
  switch (state) {
    case PlayState::Stopped:
      return "Stopped";
    case PlayState::Playing:
      return "Playing";
    case PlayState::Paused:
      return "Paused";
  }
  return "?";
}

void PlaySession::reset_clock() {
  accumulator_ = 0.0;
  play_seconds_ = 0.0;
  window_seconds_ = 0.0;
  window_ticks_ = 0;
  measured_tps_ = 0.0;
}

bool PlaySession::start(const std::vector<Entity2D>& entities,
                        const std::string& project_dir,
                        std::string* error_out) {
  stop();
  if (!world_.build(entities, project_dir, error_out)) {
    world_.clear();
    return false;
  }
  state_ = PlayState::Playing;
  return true;
}

bool PlaySession::start_project(const std::string& project_dir,
                                std::string* error_out) {
  stop();
  if (!world_.load_project(project_dir, error_out)) {
    world_.clear();
    return false;
  }
  state_ = PlayState::Playing;
  return true;
}

void PlaySession::stop() {
  world_.clear();
  state_ = PlayState::Stopped;
  reset_clock();
}

void PlaySession::pause() {
  if (state_ == PlayState::Playing) {
    state_ = PlayState::Paused;
  }
}

void PlaySession::resume() {
  if (state_ == PlayState::Paused) {
    state_ = PlayState::Playing;
  }
}

void PlaySession::toggle_pause() {
  if (state_ == PlayState::Playing) {
    pause();
  } else {
    resume();
  }
}

bool PlaySession::step_once(const InputFrame& input) {
  if (state_ != PlayState::Paused) {
    return false;
  }
  world_.step(input);
  accumulator_ = 0.0;  // show the new tick as-is
  return true;
}

int PlaySession::update(double real_seconds, const InputFrame& input) {
  if (state_ != PlayState::Playing) {
    return 0;
  }
  const double dt = std::clamp(real_seconds, 0.0, kMaxFrameSeconds);
  play_seconds_ += dt;
  accumulator_ += dt;
  int ran = 0;
  while (accumulator_ >= kTickSeconds && ran < kMaxCatchUpTicks) {
    world_.step(input);
    accumulator_ -= kTickSeconds;
    ++ran;
  }
  if (ran == kMaxCatchUpTicks && accumulator_ >= kTickSeconds) {
    accumulator_ = 0.0;  // fell behind: drop the backlog
  }
  window_seconds_ += dt;
  window_ticks_ += ran;
  if (window_seconds_ >= 1.0) {
    measured_tps_ = static_cast<double>(window_ticks_) / window_seconds_;
    window_seconds_ = 0.0;
    window_ticks_ = 0;
  }
  return ran;
}

void PlaySession::run_ticks(int n, const InputFrame& input) {
  if (state_ == PlayState::Stopped) {
    return;
  }
  for (int i = 0; i < n; ++i) {
    world_.step(input);
  }
}

float PlaySession::alpha() const {
  return static_cast<float>(std::clamp(accumulator_ / kTickSeconds, 0.0, 1.0));
}

}  // namespace runtime
}  // namespace tombstone
}  // namespace ts
