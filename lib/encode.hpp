#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <functional>
#include <iosfwd>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "utility.hpp"

struct SoundChunk {
    uint8_t value;      // 7-bit value
    CodePointMode mode; // UTF-8 byte width (1 to 4)
    bool is_first_byte; // True if this chunk starts a new codepoint
    bool join = false;  // Suppress the leading separator space
    std::string lead_punct;
    std::string trail_punct;
    bool sep_after_lead = false; // Separate lead punctuation from the sound
};

struct EncodeOptions {
    uint64_t seed = 64;
    int free_extension_limit = 0; // extra letter count that won't get penalty
    int max_short_extension = 2;
    double length_decay_rate = 0.9; // gentle per-letter decay
    double uppercase_weight = 0.4;

    bool howl_enabled = true;
    double howl_decay = 0.5;
    double initial_howl_chance = 0.2;
    double min_howl = 0.1; // floor so long forms stay reachable

    bool punctuation_as_char = false; // treat punctuation like any character
};

/// State maintained across streaming encode calls.
struct EncodeState {
    CodePointMode current_mode = CodePointMode::ASCII;
    EncodeOptions opts;
    double howl_chance;
    std::mt19937_64 rng;
    bool any_chunk_emitted = false;

    EncodeState(uint64_t seed, EncodeOptions opts)
        : opts(opts), rng(seed), howl_chance(opts.initial_howl_chance) {}
};

/// Reconstructs the sound/word by increasing letter repeats from left to right,
/// carrying over to the next letter once a letter reaches its max limit.
auto build_word_variation(const WordInfo &base, uint32_t var_index)
    -> std::string;

auto apply_casing(std::string word, Casing casing) -> std::string;

/// Generates all possible sounds and groups them by their 0–127 value,
/// sorted shortest-first so we can easily pick default/short OR elongated very
/// excited howls
struct SoundEntry {
    std::string word;
    Casing casing;
};

auto build_sound_table(const std::vector<WordInfo> &sounds)
    -> std::expected<std::array<std::vector<SoundEntry>, VALUE_MODULUS>,
                     PuplangError>;

/// Binary (full-byte) variant.
auto build_binary_sound_table(const std::vector<WordInfo> &sounds)
    -> std::expected<std::array<std::vector<SoundEntry>, BINARY_VALUE_MODULUS>,
                     PuplangError>;

/// Picks a candidate word.
///
/// Variations that differ by only a few letters (up to
/// howl.free_extension_limit extra characters) are all "normal" and commonly
/// used with nearly equal odds. Beyond that, longer forms are progressively
/// rarer, and the overall chance of such a long howl equals
/// initial_howl_chance.
auto pick_candidate_word(std::span<const SoundEntry> candidates,
                         EncodeState &state)
    -> std::expected<std::pair<std::string, bool>,
                     PuplangError>; // (word, is_howl)

/// Picks a sound and dampens the howl chance.
auto pick_sound(std::span<const SoundEntry> candidates, EncodeState &state)
    -> std::expected<std::string, PuplangError>;

auto encode_chunk(
    const SoundChunk &chunk,
    const std::array<std::vector<SoundEntry>, VALUE_MODULUS> &table,
    const Settings &cfg, EncodeState &state, std::ostream &out)
    -> std::expected<void, PuplangError>;

// Returns the UTF-8 byte width (1-4) for a valid codepoint.
int codepoint_byte_width(uint32_t cp);

auto encode_stream(std::istream &in, std::ostream &out, const Settings &cfg,
                   EncodeOptions opts = {},
                   std::function<void(double)> progress = {})
    -> std::expected<void, PuplangError>;

auto encode(std::string_view text, const Settings &cfg,
            const EncodeOptions &opts = {})
    -> std::expected<std::string, PuplangError>;
