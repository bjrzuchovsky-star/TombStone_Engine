#include "scene/JsonFlat.h"

#include "core/JsonMini.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace ts {
namespace tombstone {
namespace json_flat {

using json_mini::match_char;
using json_mini::parse_bool;
using json_mini::parse_string;
using json_mini::skip_ws;

std::string format_number(double v) {
  std::ostringstream ss;
  ss.setf(std::ios::fmtflags(0), std::ios::floatfield);
  ss.precision(9);
  ss << v;
  return ss.str();
}

std::string format_float(float v) {
  for (int precision = 6; precision <= 9; ++precision) {
    std::ostringstream ss;
    ss.setf(std::ios::fmtflags(0), std::ios::floatfield);
    ss.precision(precision);
    ss << v;
    const std::string s = ss.str();
    if (std::strtof(s.c_str(), nullptr) == v) {
      return s;
    }
  }
  return format_number(v);
}

std::optional<double> parse_number(std::string_view text, std::size_t& i) {
  skip_ws(text, i);
  if (i >= text.size()) {
    return std::nullopt;
  }
  const std::size_t start = i;
  if (text[i] == '-' || text[i] == '+') {
    ++i;
  }
  bool any_digit = false;
  while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
    any_digit = true;
    ++i;
  }
  if (i < text.size() && text[i] == '.') {
    ++i;
    while (i < text.size() &&
           std::isdigit(static_cast<unsigned char>(text[i]))) {
      any_digit = true;
      ++i;
    }
  }
  if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
    ++i;
    if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
      ++i;
    }
    bool exp_digit = false;
    while (i < text.size() &&
           std::isdigit(static_cast<unsigned char>(text[i]))) {
      exp_digit = true;
      ++i;
    }
    if (!exp_digit) {
      return std::nullopt;
    }
  }
  if (!any_digit) {
    return std::nullopt;
  }
  try {
    return std::stod(std::string(text.substr(start, i - start)));
  } catch (...) {
    return std::nullopt;
  }
}

bool skip_bracketed(std::string_view text, std::size_t& i) {
  int depth = 0;
  bool in_string = false;
  while (i < text.size()) {
    const char c = text[i++];
    if (in_string) {
      if (c == '\\') {
        ++i;
      } else if (c == '"') {
        in_string = false;
      }
      continue;
    }
    if (c == '"') {
      in_string = true;
    } else if (c == '[' || c == '{') {
      ++depth;
    } else if (c == ']' || c == '}') {
      if (--depth == 0) {
        return true;
      }
    }
  }
  return false;
}

std::optional<FlatMap> parse_flat_object(std::string_view text, std::size_t& i,
                                         FlatMap* nested) {
  if (!match_char(text, i, '{')) {
    return std::nullopt;
  }
  FlatMap out;
  skip_ws(text, i);
  if (match_char(text, i, '}')) {
    return out;
  }
  while (true) {
    auto key = parse_string(text, i);
    if (!key || !match_char(text, i, ':')) {
      return std::nullopt;
    }
    skip_ws(text, i);
    if (i < text.size() && text[i] == '"') {
      auto val = parse_string(text, i);
      if (!val) {
        return std::nullopt;
      }
      out.emplace(*key, *val);
    } else if (auto b = parse_bool(text, i)) {
      out.emplace(*key, *b ? "true" : "false");
    } else if (i < text.size() && text[i] == '[') {
      const std::size_t start = i;
      if (!skip_bracketed(text, i)) {
        return std::nullopt;
      }
      if (nested) {
        nested->emplace(*key, std::string(text.substr(start, i - start)));
      }
      out.emplace(*key, "");
    } else if (i < text.size() && text[i] == '{') {
      const std::size_t start = i;
      if (!skip_bracketed(text, i)) {
        return std::nullopt;
      }
      if (nested) {
        nested->emplace(*key, std::string(text.substr(start, i - start)));
      }
      out.emplace(*key, "");
    } else if (text.substr(i, 4) == "null") {
      i += 4;
    } else {
      const std::size_t start = i;
      if (!parse_number(text, i)) {
        return std::nullopt;
      }
      out.emplace(*key, std::string(text.substr(start, i - start)));
    }
    skip_ws(text, i);
    if (match_char(text, i, '}')) {
      return out;
    }
    if (!match_char(text, i, ',')) {
      return std::nullopt;
    }
  }
}

double map_number(const FlatMap& map, std::string_view key, double fallback) {
  const auto it = map.find(std::string(key));
  if (it == map.end() || it->second.empty()) {
    return fallback;
  }
  try {
    return std::stod(it->second);
  } catch (...) {
    return fallback;
  }
}

float map_float(const FlatMap& map, std::string_view key, float fallback) {
  const auto it = map.find(std::string(key));
  if (it == map.end() || it->second.empty()) {
    return fallback;
  }
  const char* begin = it->second.c_str();
  char* end = nullptr;
  const float v = std::strtof(begin, &end);
  if (end == begin || !std::isfinite(v)) {
    return fallback;
  }
  return v;
}

int map_int(const FlatMap& map, std::string_view key, int fallback) {
  const double v = map_number(map, key, static_cast<double>(fallback));
  if (!std::isfinite(v) || v < -2.0e9 || v > 2.0e9) {
    return fallback;
  }
  return static_cast<int>(v);
}

std::uint64_t map_u64(const FlatMap& map, std::string_view key,
                      std::uint64_t fallback) {
  const double v = map_number(map, key, static_cast<double>(fallback));
  if (!(v >= 0.0) || v > 1.8e19) {
    return fallback;
  }
  return static_cast<std::uint64_t>(v + 0.5);
}

bool map_bool(const FlatMap& map, std::string_view key, bool fallback) {
  const auto it = map.find(std::string(key));
  if (it == map.end()) {
    return fallback;
  }
  return it->second == "true" || it->second == "1";
}

std::string map_string(const FlatMap& map, std::string_view key,
                       std::string_view fallback) {
  const auto it = map.find(std::string(key));
  if (it == map.end()) {
    return std::string(fallback);
  }
  return it->second;
}

std::optional<FlatMap> parse_nested(const FlatMap& nested,
                                    const std::string& key) {
  const auto it = nested.find(key);
  if (it == nested.end()) {
    return std::nullopt;
  }
  std::size_t j = 0;
  return parse_flat_object(it->second, j);
}

bool parse_int_array(std::string_view text, std::vector<int>* out) {
  out->clear();
  std::size_t i = 0;
  if (!match_char(text, i, '[')) {
    return false;
  }
  skip_ws(text, i);
  if (match_char(text, i, ']')) {
    return true;
  }
  while (true) {
    auto n = parse_number(text, i);
    if (!n || !std::isfinite(*n) || *n < -2.0e9 || *n > 2.0e9) {
      return false;
    }
    out->push_back(static_cast<int>(*n));
    skip_ws(text, i);
    if (match_char(text, i, ']')) {
      return true;
    }
    if (!match_char(text, i, ',')) {
      return false;
    }
  }
}

bool skip_value(std::string_view text, std::size_t& i) {
  skip_ws(text, i);
  if (i >= text.size()) {
    return false;
  }
  if (text[i] == '"') {
    return parse_string(text, i).has_value();
  }
  if (text[i] == '[' || text[i] == '{') {
    return skip_bracketed(text, i);
  }
  if (parse_bool(text, i)) {
    return true;
  }
  if (text.compare(i, 4, "null") == 0) {
    i += 4;
    return true;
  }
  return parse_number(text, i).has_value();
}

bool split_object_array(std::string_view text, std::vector<std::string>* out) {
  out->clear();
  std::size_t i = 0;
  if (!match_char(text, i, '[')) {
    return false;
  }
  skip_ws(text, i);
  if (match_char(text, i, ']')) {
    return true;
  }
  while (true) {
    skip_ws(text, i);
    if (i >= text.size() || text[i] != '{') {
      return false;
    }
    const std::size_t start = i;
    if (!skip_bracketed(text, i)) {
      return false;
    }
    out->emplace_back(text.substr(start, i - start));
    skip_ws(text, i);
    if (match_char(text, i, ']')) {
      return true;
    }
    if (!match_char(text, i, ',')) {
      return false;
    }
  }
}

}  // namespace json_flat
}  // namespace tombstone
}  // namespace ts
