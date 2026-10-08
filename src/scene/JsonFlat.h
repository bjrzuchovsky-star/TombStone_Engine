#pragma once

// Small JSON helpers shared by the scene.json and .anim.json codecs: flat
// objects whose nested arrays / objects come back as raw text, number
// parsing that keeps floats bit-exact, and shortest round-trip float
// formatting. Scene data only (no UI).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ts {
namespace tombstone {
namespace json_flat {

using FlatMap = std::unordered_map<std::string, std::string>;

std::string format_number(double v);
// Shortest text that reads back as the same float ("0.35", not
// "0.349999994"), so hand-edited files stay readable.
std::string format_float(float v);

std::optional<double> parse_number(std::string_view text, std::size_t& i);
// Skip one bracketed value ([...] or {...}), honouring quoted strings.
// i must sit on the opener.
bool skip_bracketed(std::string_view text, std::size_t& i);
// Skip any one JSON value (string, bool, number, null, array, object).
bool skip_value(std::string_view text, std::size_t& i);

// Flat object of scalars. Nested objects and arrays are captured as raw
// JSON text in *nested (key -> "{...}" / "[...]") when provided. Numbers
// are kept as their original text so floats read back bit-exact.
std::optional<FlatMap> parse_flat_object(std::string_view text, std::size_t& i,
                                         FlatMap* nested = nullptr);
// nested[key] parsed as a flat object (nullopt when absent or malformed).
std::optional<FlatMap> parse_nested(const FlatMap& nested,
                                    const std::string& key);
// "[1, 2, 3]" -> ints. False on anything that is not a flat number array.
bool parse_int_array(std::string_view text, std::vector<int>* out);
// "[{...}, {...}]" -> the raw text of each object. False when malformed.
bool split_object_array(std::string_view text, std::vector<std::string>* out);

double map_number(const FlatMap& map, std::string_view key, double fallback);
// Floats parse straight from the text (strtof), so a written value reads
// back as the identical float.
float map_float(const FlatMap& map, std::string_view key, float fallback);
int map_int(const FlatMap& map, std::string_view key, int fallback);
std::uint64_t map_u64(const FlatMap& map, std::string_view key,
                      std::uint64_t fallback);
bool map_bool(const FlatMap& map, std::string_view key, bool fallback);
std::string map_string(const FlatMap& map, std::string_view key,
                       std::string_view fallback);

}  // namespace json_flat
}  // namespace tombstone
}  // namespace ts
