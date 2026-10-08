#pragma once

// Tiny hand-rolled helpers for flat JSON objects used by editor settings,
// project.json and scene.json. Not a general-purpose parser -- only
// string/bool scalars and optional nesting-free objects like
// { "key": "value", "flag": true }.

#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ts {
namespace tombstone {
namespace json_mini {

inline void skip_ws(std::string_view s, std::size_t& i) {
  while (i < s.size() &&
         (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) {
    ++i;
  }
}

inline bool match_char(std::string_view s, std::size_t& i, char c) {
  skip_ws(s, i);
  if (i < s.size() && s[i] == c) {
    ++i;
    return true;
  }
  return false;
}

inline std::optional<std::string> parse_string(std::string_view s,
                                               std::size_t& i) {
  skip_ws(s, i);
  if (i >= s.size() || s[i] != '"') {
    return std::nullopt;
  }
  ++i;
  std::string out;
  while (i < s.size()) {
    const char c = s[i++];
    if (c == '"') {
      return out;
    }
    if (c == '\\' && i < s.size()) {
      const char e = s[i++];
      switch (e) {
        case '"':
        case '\\':
        case '/':
          out.push_back(e);
          break;
        case 'n':
          out.push_back('\n');
          break;
        case 't':
          out.push_back('\t');
          break;
        case 'r':
          out.push_back('\r');
          break;
        default:
          out.push_back(e);
          break;
      }
      continue;
    }
    out.push_back(c);
  }
  return std::nullopt;
}

inline std::optional<bool> parse_bool(std::string_view s, std::size_t& i) {
  skip_ws(s, i);
  if (s.substr(i, 4) == "true") {
    i += 4;
    return true;
  }
  if (s.substr(i, 5) == "false") {
    i += 5;
    return false;
  }
  return std::nullopt;
}

// Parse a flat object into string values. Booleans are stored as "true"/"false".
inline std::optional<std::unordered_map<std::string, std::string>>
parse_object(std::string_view text) {
  std::size_t i = 0;
  if (!match_char(text, i, '{')) {
    return std::nullopt;
  }

  std::unordered_map<std::string, std::string> out;
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
    } else {
      // Skip unrecognized scalars (numbers/null) as empty to stay tolerant.
      while (i < text.size() && text[i] != ',' && text[i] != '}') {
        ++i;
      }
      out.emplace(*key, "");
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

inline std::string escape_string(std::string_view value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (char c : value) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\t':
        out += "\\t";
        break;
      case '\r':
        out += "\\r";
        break;
      default:
        out.push_back(c);
        break;
    }
  }
  return out;
}

inline std::string write_object(
    const std::vector<std::pair<std::string, std::string>>& string_fields,
    const std::vector<std::pair<std::string, bool>>& bool_fields = {}) {
  std::string out = "{\n";
  bool first = true;
  auto comma = [&]() {
    if (!first) {
      out += ",\n";
    }
    first = false;
  };
  for (const auto& [k, v] : string_fields) {
    comma();
    out += "  \"";
    out += escape_string(k);
    out += "\": \"";
    out += escape_string(v);
    out += '"';
  }
  for (const auto& [k, v] : bool_fields) {
    comma();
    out += "  \"";
    out += escape_string(k);
    out += "\": ";
    out += v ? "true" : "false";
  }
  out += "\n}\n";
  return out;
}

inline std::string get_string(
    const std::unordered_map<std::string, std::string>& map,
    std::string_view key,
    std::string_view fallback = {}) {
  const auto it = map.find(std::string(key));
  if (it == map.end()) {
    return std::string(fallback);
  }
  return it->second;
}

inline bool get_bool(const std::unordered_map<std::string, std::string>& map,
                     std::string_view key,
                     bool fallback = false) {
  const auto it = map.find(std::string(key));
  if (it == map.end()) {
    return fallback;
  }
  return it->second == "true" || it->second == "1";
}

}  // namespace json_mini
}  // namespace tombstone
}  // namespace ts
