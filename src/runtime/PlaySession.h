#pragma once

// Play / Pause / Step / Stop around a World with a fixed-timestep
// accumulator. Shared by the editor's Play mode and ts_game; render code
// asks for alpha() to interpolate between ticks.

#include "runtime/Input.h"
#include "runtime/World.h"
#include "scene/SceneData.h"
#include "scene/TileMap.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ts {
namespace tombstone {
namespace runtime {

enum class PlayState { Stopped, Playing, Paused };
const char* to_string(PlayState state);

class PlaySession {
 public:
  // Most ticks one update() may run; a long hitch drops the rest instead
  // of spiralling.
  static constexpr int kMaxCatchUpTicks = 8;
  // Longest frame update() will account for (seconds).
  static constexpr double kMaxFrameSeconds = 0.25;

  // Fresh World from the authored entities (a copy: the caller's data is
  // never touched). Starts Playing. Restarts if already running.
  bool start(const std::vector<Entity2D>& entities,
             const std::string& project_dir, std::string* error_out = nullptr);
  // Same, with the scene's tile solidity (editor Play passes its own).
  bool start(const std::vector<Entity2D>& entities,
             const TileSolidity& solidity, const std::string& project_dir,
             std::string* error_out = nullptr);
  // Same, from <project_dir>/scene.json (ts_game).
  bool start_project(const std::string& project_dir,
                     std::string* error_out = nullptr);
  // Throws every bit of runtime state away.
  void stop();

  void pause();
  void resume();
  void toggle_pause();
  // While paused: run exactly one tick now. False when not paused.
  bool step_once(const InputFrame& input);

  // Feed real elapsed time. Runs 0..kMaxCatchUpTicks ticks while Playing,
  // none while Paused / Stopped. Returns the number of ticks run.
  int update(double real_seconds, const InputFrame& input);
  // Deterministic: n ticks regardless of the clock (tests, --smoke).
  void run_ticks(int n, const InputFrame& input);

  PlayState state() const { return state_; }
  bool active() const { return state_ != PlayState::Stopped; }
  bool playing() const { return state_ == PlayState::Playing; }
  bool paused() const { return state_ == PlayState::Paused; }

  World& world() { return world_; }
  const World& world() const { return world_; }
  // Fraction of a tick banked in the accumulator (render interpolation).
  float alpha() const;
  std::uint64_t tick() const { return world_.tick(); }
  // Real time spent Playing (pauses excluded).
  double play_seconds() const { return play_seconds_; }
  // Ticks per real second over the last measured second.
  double ticks_per_second() const { return measured_tps_; }

 private:
  void reset_clock();

  World world_;
  PlayState state_ = PlayState::Stopped;
  double accumulator_ = 0.0;
  double play_seconds_ = 0.0;
  double window_seconds_ = 0.0;
  int window_ticks_ = 0;
  double measured_tps_ = 0.0;
};

}  // namespace runtime
}  // namespace tombstone
}  // namespace ts
