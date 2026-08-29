#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <expected>
#include <fstream>
#include <functional>
#include <iterator>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "utf8.h"
#include "utility.hpp"
#include <assert.h>

struct SoundChunk {
    uint8_t value;      // 7-bit value
    CodePointMode mode; // UTF-8 byte width (1 to 4)
    bool is_first_byte; // True if this chunk starts a new codepoint
    std::string lead_punct;
    std::string trail_punct;
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
    -> std::string {
    std::string word;
    int rem = var_index;
    for (const auto &r : base.runs) {
        int radix = r.max_extra + 1;
        word.append(r.count + (rem % radix), r.c);
        rem /= radix;
    }
    return word;
}

auto apply_casing(std::string word, Casing casing) -> std::string {
    if (casing == Casing::Title && !word.empty()) {
        word.front() = static_cast<char>(
            std::toupper(static_cast<unsigned char>(word.front())));
    } else if (casing == Casing::Upper) {
        std::ranges::transform(word, word.begin(), ::toupper);
    }
    return word;
}

/// Generates all possible sounds and groups them by their 0–127 value,
/// sorted shortest-first so we can easily pick default/short OR elongated very
/// excited howls
struct SoundEntry {
    std::string word;
    Casing casing;
};

auto build_sound_table(const std::vector<WordInfo> &sounds)
    -> std::expected<std::array<std::vector<SoundEntry>, VALUE_MODULUS>,
                     PuplangError> {
    std::array<std::vector<SoundEntry>, VALUE_MODULUS> table;

    for (const auto &base : sounds) {
        int variations = run_combinations(base.runs);
        for (int var = 0; var < variations; ++var) {
            std::string base_word = build_word_variation(base, var);
            for (auto casing :
                 { Casing::Lower, Casing::Title, Casing::Upper }) {
                std::string word = apply_casing(base_word, casing);
                // Only keep the word if the decoder can actually read it
                if (auto val = get_sound_value(word, sounds); val) {
                    table[*val].push_back({ std::move(word), casing });
                } else {
                    return std::unexpected(PuplangError::bark_collision);
                }
            }
        }
    }

    for (auto &words : table) {
        std::ranges::sort(words.begin(),
                          words.end(),
                          [](const SoundEntry &a, const SoundEntry &b) {
                              return a.word.size() < b.word.size();
                          });
        auto [first, last] = std::ranges::unique(
            words, [](const SoundEntry &a, const SoundEntry &b) {
                return a.word == b.word;
            });
        words.erase(first, last);
    }
    return table;
}

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
                     PuplangError> { // (word, is_howl)
    if (candidates.empty())
        return std::unexpected(PuplangError::empty_input);

    auto &options = state.opts;

    const double howl_prob = std::max(state.howl_chance, options.min_howl);
    const size_t min_len = candidates.front().word.size();

    auto max_word_i = 0;
    // weight = SHAPE^(extra letters)
    std::vector<double> weight(candidates.size());
    double short_w = 0.0, howl_w = 0.0;
    for (size_t i = 0; i < candidates.size(); ++i) {
        int extra = static_cast<int>(candidates[i].word.size()) -
                    static_cast<int>(min_len);
        auto past_limit = std::max(0, extra - options.free_extension_limit);

        // if past limit, each letter past the short limit loses weight
        // exponentially
        if (past_limit > 0)
            weight[i] = std::pow(options.length_decay_rate, past_limit);
        else
            weight[i] = 1.0;

        // if the candidate is upper, decay according to uppercase_weight
        if (candidates[i].casing == Casing::Upper)
            weight[i] *= options.uppercase_weight;
        else if (candidates[i].casing == Casing::Title)
            weight[i] *= options.uppercase_weight * 0.5;

        // if the candidate is a howl, add it to the howl weight
        if (extra <= options.max_short_extension)
            short_w += weight[i];
        else
            howl_w += weight[i];

        if (past_limit <= options.max_short_extension)
            max_word_i = i;
    }
    if (!options.howl_enabled) {
        auto start = weight.begin();
        auto end = weight.begin() + max_word_i + 1;
        auto index = std::discrete_distribution<size_t>(start, end)(state.rng);
        return std::pair{ candidates[index].word, false };
    }

    // Rescale long forms so the overall howl chance equals howl_prob, keeping
    // their relative ranking.
    if (howl_w > 0.0) {
        const double scale = (short_w * howl_prob / (1.0 - howl_prob)) / howl_w;
        for (size_t i = 0; i < candidates.size(); ++i) {
            int extra = static_cast<int>(candidates[i].word.size()) -
                        static_cast<int>(min_len);
            if (i > max_word_i)
                weight[i] *= scale;
        }
    }

    auto start = weight.begin();
    auto end = weight.end();
    auto index = std::discrete_distribution<size_t>(start, end)(state.rng);
    return std::pair{ candidates[index].word, index > max_word_i };
}

auto encode_chunk(
    const SoundChunk &chunk,
    const std::array<std::vector<SoundEntry>, VALUE_MODULUS> &table,
    const Settings &cfg, EncodeState &state, std::ostream &out)
    -> std::expected<void, PuplangError> {
    auto &candidates = table[chunk.value];

    assert(!candidates.empty());

    auto sound_result = pick_candidate_word(candidates, state);
    if (!sound_result)
        return std::unexpected(sound_result.error());
    auto [sound, is_howl] = sound_result.value();
    if (is_howl) {
        state.howl_chance *= state.opts.howl_decay;
    }

    // Emit prefix switch only when the codepoint byte-width changes
    if (chunk.is_first_byte && chunk.mode != state.current_mode) {
        out << ' ' << cfg.prefixes[chunk.mode];
        state.current_mode = chunk.mode;
    }

    out << ' ' << chunk.lead_punct << sound << chunk.trail_punct;
    state.any_chunk_emitted = true;

    return {};
}

// Returns the UTF-8 byte width (1-4) for a valid codepoint.
inline auto codepoint_byte_width(uint32_t cp) -> int {
    if (cp <= 0x7F)
        return 1;
    if (cp <= 0x7FF)
        return 2;
    if (cp <= 0xFFFF)
        return 3;
    return 4;
}

auto encode_stream(std::istream &in, std::ostream &out, const Settings &cfg,
                   EncodeOptions opts = {},
                   std::function<void(double)> progress = {})
    -> std::expected<void, PuplangError> {
    auto tablex = build_sound_table(cfg.sounds);
    if (!tablex) {
        return std::unexpected(tablex.error());
    }
    auto &table = *tablex;

    EncodeState state(opts.seed, opts);

    out << cfg.header;

    ProgressTracker tracker{ progress, in };

    std::string pending_lead;  // punctuation before the next sound
    std::string pending_trail; // punctuation after the previous sound
    bool sound_since_punct = false;

    auto emit_codepoint = [&](uint32_t cp,
                              int bytes) -> std::expected<void, PuplangError> {
        // Trailing punctuation belongs after the previous sound, so it is
        // flushed right before the following sound starts.
        if (!pending_trail.empty()) {
            out << pending_trail;
            pending_trail.clear();
        }
        for (int b = 0; b < bytes; ++b) {
            int shift = BITS_PER_SOUND * (bytes - 1 - b);
            SoundChunk chunk{
                .value = static_cast<uint8_t>((cp >> shift) & SOUND_MASK),
                .mode = static_cast<CodePointMode>(bytes),
                .is_first_byte = (b == 0),
                .lead_punct = (b == 0) ? std::move(pending_lead) : "",
                .trail_punct = ""
            };
            if (b == 0)
                pending_lead.clear();
            auto result = encode_chunk(chunk, table, cfg, state, out);
            if (!result)
                return std::unexpected(result.error());
        }
        sound_since_punct = true;
        return {};
    };

    std::istreambuf_iterator<char> it(in), end;
    while (it != end) {
        uint32_t cp;
        try {
            cp = utf8::next(it, end);
        } catch (const utf8::exception &) {
            return std::unexpected(PuplangError::invalid_utf8);
        }

        tracker.report_fraction(std::streamoff(in.tellg()));

        // Check if this codepoint is ASCII punctuation
        if (cp <= 0x7F && is_punct(static_cast<char>(cp))) {
            if (sound_since_punct)
                pending_trail.push_back(static_cast<char>(cp));
            else
                pending_lead.push_back(static_cast<char>(cp));
            continue;
        }

        // Emit the codepoint
        int bytes = codepoint_byte_width(cp);
        auto result = emit_codepoint(cp, bytes);
        if (!result)
            return std::unexpected(result.error());
    }

    tracker.report(1.0);

    // Flush any trailing punctuation left after the final sound.
    if (!pending_trail.empty())
        out << pending_trail;
    if (!state.any_chunk_emitted && !pending_lead.empty())
        out << ' ' << pending_lead;

    out << ' ' << cfg.footer;
    return {};
}

auto encode(std::string_view text, const Settings &cfg,
            const EncodeOptions &opts = {})
    -> std::expected<std::string, PuplangError> {
    std::istringstream in{ std::string(text) };
    std::ostringstream out;
    auto result = encode_stream(in, out, cfg, opts);
    if (!result)
        return std::unexpected(result.error());
    return out.str();
}