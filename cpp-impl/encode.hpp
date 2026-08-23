#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <expected>
#include <fstream>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "utf8.h"
#include "utility.hpp"
#include <assert.h>

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
auto build_sound_table(const std::vector<WordInfo> &sounds)
    -> std::expected<std::array<std::vector<std::string>, VALUE_MODULUS>,
                     PuplangError> {
    std::array<std::vector<std::string>, VALUE_MODULUS> table;

    for (const auto &base : sounds) {
        int variations = run_combinations(base.runs);
        for (int var = 0; var < variations; ++var) {
            std::string base_word = build_word_variation(base, var);
            for (auto casing :
                 { Casing::Lower, Casing::Title, Casing::Upper }) {
                std::string word = apply_casing(base_word, casing);
                // Only keep the word if the decoder can actually read it
                if (auto val = get_sound_value(word, sounds); val) {
                    table[*val].push_back(std::move(word));
                } else {
                    return std::unexpected(PuplangError::bark_collision);
                }
            }
        }
    }

    for (auto &words : table) {
        std::ranges::sort(words, {}, &std::string::size);
        auto [first, last] = std::ranges::unique(words);
        words.erase(first, last);
    }
    return table;
}

struct HowlTuning {
    int short_limit = 2;   // extra letters that still count as a "tiny" change
    double min_howl = 0.1; // floor so long forms stay reachable
    double shape = 0.9;    // gentle per-letter decay
};

/// Picks a candidate word.
///
/// Variations that differ by only a few letters (up to howl.short_limit extra
/// characters) are all "normal" and commonly used with nearly equal odds.
/// Beyond that, longer forms are progressively rarer, and the overall chance of
/// such a long howl equals howl_chance.
auto pick_candidate_word(std::span<const std::string> candidates,
                         double howl_chance, const HowlTuning &howl, auto &rng)
    -> std::string {
    if (candidates.empty())
        return "";

    const double howl_prob = std::max(howl_chance, howl.min_howl);
    const size_t min_len = candidates.front().size();

    // weight = SHAPE^(extra letters)
    std::vector<double> weight(candidates.size());
    double short_w = 0.0, howl_w = 0.0;
    for (size_t i = 0; i < candidates.size(); ++i) {
        int extra =
            static_cast<int>(candidates[i].size()) - static_cast<int>(min_len);
        weight[i] = std::pow(howl.shape, extra);
        if (extra <= howl.short_limit)
            short_w += weight[i];
        else
            howl_w += weight[i];
    }

    // Rescale long forms so the overall howl chance equals howl_prob, keeping
    // their relative ranking.
    if (howl_w > 0.0) {
        const double scale = (short_w * howl_prob / (1.0 - howl_prob)) / howl_w;
        for (size_t i = 0; i < candidates.size(); ++i) {
            int extra = static_cast<int>(candidates[i].size()) -
                        static_cast<int>(min_len);
            if (extra > howl.short_limit)
                weight[i] *= scale;
        }
    }

    return candidates[std::discrete_distribution<size_t>(weight.begin(),
                                                         weight.end())(rng)];
}

struct SoundChunk {
    uint8_t value;      // 7-bit value
    CodePointMode mode; // UTF-8 byte width (1 to 4)
    bool is_first_byte; // True if this chunk starts a new codepoint
    std::string lead_punct;
    std::string trail_punct;
};

/// State maintained across streaming encode calls.
struct EncodeState {
    CodePointMode current_mode = CodePointMode::ASCII;
    float howl_chance;
    const float howl_decay;
    HowlTuning howl;
    std::mt19937_64 rng;
    bool any_chunk_emitted = false;

    EncodeState(uint64_t seed, double hc, double hd, HowlTuning h)
        : howl_chance(hc), howl_decay(hd), howl(std::move(h)), rng(seed) {}
};

void encode_chunk(
    const SoundChunk &chunk,
    const std::array<std::vector<std::string>, VALUE_MODULUS> &table,
    const Settings &cfg, EncodeState &state, std::ostream &out) {
    auto &candidates = table[chunk.value];

    assert(!candidates.empty());

    std::string sound = pick_candidate_word(
        candidates, state.howl_chance, state.howl, state.rng);

    // Only decay if considered a howl
    size_t min_len = candidates.front().size();
    int extra = static_cast<int>(sound.size()) - static_cast<int>(min_len);
    bool is_howl = extra > state.howl.short_limit;
    if (is_howl) {
        state.howl_chance *= state.howl_decay;
    }

    // Emit prefix switch only when the codepoint byte-width changes
    if (chunk.is_first_byte && chunk.mode != state.current_mode) {
        out << ' ' << cfg.prefixes[chunk.mode];
        state.current_mode = chunk.mode;
    }

    out << ' ' << chunk.lead_punct << sound << chunk.trail_punct;
    state.any_chunk_emitted = true;
}

struct EncodeOptions {
    uint64_t seed = 64;
    double howl_chance = 0.2;
    double howl_decay = 0.5;
    HowlTuning howl;
};

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
                   EncodeOptions opts = {})
    -> std::expected<void, PuplangError> {
    auto tablex = build_sound_table(cfg.sounds);
    if (!tablex) {
        return std::unexpected(tablex.error());
    }
    auto &table = *tablex;

    EncodeState state(opts.seed, opts.howl_chance, opts.howl_decay, opts.howl);

    // Read entire input into string
    std::string input((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());

    // Validate UTF-8
    if (!utf8::is_valid(input.begin(), input.end())) {
        return std::unexpected(PuplangError::invalid_utf8);
    }

    out << cfg.header;

    std::string pending_lead;  // punctuation before the next sound
    std::string pending_trail; // punctuation after the previous sound
    bool sound_since_punct = false;

    auto emit_codepoint = [&](uint32_t cp, int bytes) -> void {
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
            encode_chunk(chunk, table, cfg, state, out);
        }
        sound_since_punct = true;
    };

    // Iterate through codepoints using utf8cpp
    auto it = input.begin();
    const auto end = input.end();

    while (it != end) {
        uint32_t cp = utf8::next(it, end);

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
        emit_codepoint(cp, bytes);
    }

    // Flush any trailing punctuation left after the final sound.
    if (!pending_trail.empty())
        out << pending_trail;
    if (!state.any_chunk_emitted && !pending_lead.empty())
        out << ' ' << pending_lead;

    out << ' ' << cfg.footer;
    return {};
}

auto encode(std::string_view text, const Settings &cfg, uint64_t seed = 64)
    -> std::expected<std::string, PuplangError> {
    std::istringstream in{ std::string(text) };
    std::ostringstream out;
    auto result = encode_stream(in, out, cfg, EncodeOptions{ .seed = seed });
    if (!result)
        return std::unexpected(result.error());
    return out.str();
}