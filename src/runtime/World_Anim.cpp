// Sprite animation on the fixed tick: clip timing, rider clip picking by
// name (idle_ / walk_ + _down / _up / _left / _right) and the play-by-name
// API scripts will drive.

#include "runtime/World.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string_view>

namespace ts {
namespace tombstone {
namespace runtime {

const char* to_string(Facing facing) {
  switch (facing) {
    case Facing::Up:
      return "up";
    case Facing::Left:
      return "left";
    case Facing::Right:
      return "right";
    case Facing::Down:
    default:
      return "down";
  }
}

namespace {

constexpr Facing kFacings[] = {Facing::Down, Facing::Up, Facing::Left,
                               Facing::Right};

// "walk_left" -> "walk"; names without a direction suffix stay whole.
std::string_view clip_base(std::string_view name) {
  for (Facing f : kFacings) {
    const std::string_view suffix = to_string(f);
    if (name.size() > suffix.size() + 1 &&
        name.substr(name.size() - suffix.size()) == suffix &&
        name[name.size() - suffix.size() - 1] == '_') {
      return name.substr(0, name.size() - suffix.size() - 1);
    }
  }
  return name;
}

// A clip name ending in a direction gives the heading it shows.
bool facing_of(std::string_view name, Facing* out) {
  const std::string_view base = clip_base(name);
  if (base.size() == name.size()) {
    return false;
  }
  const std::string_view suffix = name.substr(base.size() + 1);
  for (Facing f : kFacings) {
    if (suffix == to_string(f)) {
      *out = f;
      return true;
    }
  }
  return false;
}

}  // namespace

std::string World::pick_clip(const AnimSet& set, Facing dir, int facing,
                             bool walking, const std::string& fallback,
                             bool* flip_x) {
  *flip_x = false;
  auto has = [&](const std::string& name) { return set.find(name) != nullptr; };
  // `base`_`d`, or the opposite side mirrored for left / right.
  auto try_dir = [&](const std::string& base, Facing d, std::string* out) {
    const std::string exact = base + "_" + to_string(d);
    if (has(exact)) {
      *out = exact;
      *flip_x = false;
      return true;
    }
    if (d == Facing::Left || d == Facing::Right) {
      const std::string other =
          base + "_" + to_string(d == Facing::Left ? Facing::Right : Facing::Left);
      if (has(other)) {
        *out = other;
        *flip_x = true;
        return true;
      }
    }
    return false;
  };
  const Facing side = facing < 0 ? Facing::Left : Facing::Right;
  const char* const walk_bases[] = {"walk", "idle"};
  const char* const idle_bases[] = {"idle"};
  const std::size_t n = walking ? 2 : 1;
  for (std::size_t i = 0; i < n; ++i) {
    const std::string base = walking ? walk_bases[i] : idle_bases[i];
    std::string out;
    if (try_dir(base, dir, &out)) {
      return out;
    }
    // Up / down with only side views: look the way we last rode.
    if ((dir == Facing::Up || dir == Facing::Down) && try_dir(base, side, &out)) {
      return out;
    }
    // Fewer directions: the front view stands in for everything.
    if (has(base + "_down")) {
      *flip_x = false;
      return base + "_down";
    }
    // A bare clip faces right; mirror it when heading left.
    if (has(base)) {
      *flip_x = side == Facing::Left;
      return base;
    }
  }
  *flip_x = false;
  if (has(fallback)) {
    return fallback;
  }
  return set.start_clip();
}

const AnimLibrary::Entry* World::anim_set(const Actor& a) const {
  if (a.anim.set < 0 || static_cast<std::size_t>(a.anim.set) >= anim_sets_.size()) {
    return nullptr;
  }
  return &anim_sets_[static_cast<std::size_t>(a.anim.set)];
}

void World::set_clip(AnimState& st, const std::string& clip, bool flip_x,
                     bool restart) {
  st.flip_x = flip_x;
  if (clip == st.clip && !restart) {
    return;
  }
  // walk_left -> walk_up keeps the stride; anything else starts over.
  const bool same_move = !restart && !st.clip.empty() &&
                         clip_base(clip) == clip_base(st.clip) &&
                         clip_base(clip).size() != clip.size();
  st.clip = clip;
  if (!same_move) {
    st.time = 0.0;
    st.frame = 0;
    st.finished = false;
  }
}

void World::setup_animation() {
  anim_sets_.clear();
  anim_paths_.clear();
  for (Actor& a : actors_) {
    setup_actor_animation(a);
  }
}

void World::setup_actor_animation(Actor& a) {
  a.anim = AnimState{};
  if (!a.data.animator || a.data.animator->set.empty()) {
    return;
  }
  const std::string& rel = a.data.animator->set;
  const auto it = std::find(anim_paths_.begin(), anim_paths_.end(), rel);
  if (it != anim_paths_.end()) {
    a.anim.set = static_cast<int>(it - anim_paths_.begin());
  } else {
    const AnimLibrary::Entry* e = anim_lib_.get(project_dir_, rel);
    anim_sets_.push_back(e ? *e : AnimLibrary::Entry{});
    anim_paths_.push_back(rel);
    a.anim.set = static_cast<int>(anim_sets_.size() - 1);
  }
  const AnimLibrary::Entry& entry = anim_sets_[static_cast<std::size_t>(a.anim.set)];
  if (!entry.ok) {
    return;
  }
  const AnimatorData& an = *a.data.animator;
  // The ride starts on the authored clip, else the default.
  std::string start = an.clip;
  if (!entry.set.find(start)) {
    start = entry.set.find(an.default_clip) ? an.default_clip
                                            : entry.set.start_clip();
  }
  a.anim.clip = start;
  Facing f = Facing::Down;
  if (facing_of(start, &f)) {
    a.dir = f;
    if (f == Facing::Left) a.facing = -1;
    if (f == Facing::Right) a.facing = 1;
  }
}

void World::step_animation() {
  for (Actor& a : actors_) {
    if (a.data.player) {
      // Four-way heading from move intent. On a diagonal keep the current
      // heading when it is one of the pressed ways, else side-on wins.
      const float dead = 0.2f;
      const bool h = std::fabs(a.in_x) > dead;
      const bool v = std::fabs(a.in_y) > dead;
      const Facing hd = a.in_x < 0.0f ? Facing::Left : Facing::Right;
      const Facing vd = a.in_y < 0.0f ? Facing::Up : Facing::Down;
      if (h && v) {
        if (a.dir != hd && a.dir != vd) a.dir = hd;
      } else if (h) {
        a.dir = hd;
      } else if (v) {
        a.dir = vd;
      }
      a.anim.walking = h || v;
    }
    const AnimLibrary::Entry* entry = anim_set(a);
    if (!entry || !entry->ok || !a.data.animator) {
      continue;
    }
    const AnimatorData& an = *a.data.animator;
    if (a.data.player && !a.anim.hold) {
      bool flip = false;
      const std::string fallback =
          entry->set.find(an.default_clip) ? an.default_clip : entry->set.start_clip();
      const std::string clip = pick_clip(entry->set, a.dir, a.facing,
                                         a.anim.walking, fallback, &flip);
      set_clip(a.anim, clip, flip, false);
    }
    const AnimClip* clip = entry->set.find(a.anim.clip);
    if (!clip) {
      continue;
    }
    if (an.playing) {
      a.anim.time += kTickSeconds * static_cast<double>(an.speed);
    }
    const AnimSample s = sample_clip(*clip, a.anim.time);
    a.anim.frame = s.frame;
    a.anim.finished = s.finished;
  }
}

bool World::play_clip(std::uint64_t id, const std::string& clip, bool restart,
                      bool hold) {
  Actor* a = find(id);
  if (!a) {
    return false;
  }
  const AnimLibrary::Entry* entry = anim_set(*a);
  if (!entry || !entry->ok || !entry->set.find(clip)) {
    return false;
  }
  set_clip(a->anim, clip, false, restart);
  a->anim.hold = hold;
  const AnimSample s = sample_clip(*entry->set.find(clip), a->anim.time);
  a->anim.frame = s.frame;
  a->anim.finished = s.finished;
  return true;
}

void World::release_clip(std::uint64_t id) {
  if (Actor* a = find(id)) {
    a->anim.hold = false;
  }
}

}  // namespace runtime
}  // namespace tombstone
}  // namespace ts
