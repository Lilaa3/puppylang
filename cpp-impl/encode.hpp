#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "utility.hpp"

/// Reconstructs the sound/word by increasing letter repeats from left to right,
/// carrying over to the next letter once a letter reaches its max limit.
inline auto build_word_variation(const WordInfo &base, uint32_t var_index)
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

inline auto apply_casing(std::string word, Casing casing) -> std::string {
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
inline auto build_sound_table(const std::vector<WordInfo> &sounds)
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

// Chooses from the shortest forms by default, or the elongated forms if
// howling.
inline auto pick_candidate_word(std::span<const std::string> candidates,
                                bool howl, auto &rng) -> std::string {
    if (candidates.empty())
        return "";

    // If howling or randomly triggering a flavor variation
    if ((howl || (rng() % 5 == 0)) && candidates.size() > 1) {
        return candidates[rng() % candidates.size()];
    }

    // Default to base/short variations
    size_t min_len = candidates.front().size();
    auto mid =
        std::ranges::find_if(candidates, [min_len](const std::string &w) {
            return w.size() > min_len;
        });

    std::span short_words(candidates.begin(), mid);
    return short_words[rng() % short_words.size()];
}

struct SoundChunk {
    int value;          // 7-bit value
    int mode;           // UTF-8 byte width (1 to 4)
    bool is_first_byte; // True if this chunk starts a new codepoint
    std::string lead_punct;
    std::string trail_punct;
};

inline auto next_utf8(std::string_view s, size_t &i)
    -> std::pair<uint32_t, int> {
    int len = std::countl_one(uint8_t(s[i]));

    // ASCII (starts with "0") or invalid bytes fallback to 1 byte
    if (len <= 1 || len > 4 || i + len > s.size()) {
        return { uint8_t(s[i++]), 1 };
    }

    // Extract payload bits from lead byte
    uint32_t cp = uint8_t(s[i]) & (0xFF >> (len + 1));

    // Shift in the 6 bits from each continuation byte
    for (int off = 1; off < len; ++off) {
        cp = (cp << 6) | (uint8_t(s[i + off]) & 0x3F);
    }

    i += len;
    return { cp, len };
}

// Slices input stream into 7-bit chunks while attaching interleaved punctuation
// to sound boundaries
inline auto tokenize_input_to_chunks(std::string_view text)
    -> std::vector<SoundChunk> {
    std::vector<SoundChunk> chunks;
    std::string pending_lead;

    for (size_t i = 0; i < text.size();) {
        if (is_punct(text[i])) {
            if (chunks.empty())
                pending_lead.push_back(text[i]);
            else
                chunks.back().trail_punct.push_back(text[i]);
            i++;
            continue;
        }

        auto [cp, bytes] = next_utf8(text, i);
        for (int b = 0; b < bytes; ++b) {
            int shift = BITS_PER_SOUND * (bytes - 1 - b);
            chunks.push_back(
                { .value = static_cast<int>((cp >> shift) & SOUND_MASK),
                  .mode = bytes,
                  .is_first_byte = (b == 0),
                  .lead_punct = (b == 0) ? std::move(pending_lead) : "",
                  .trail_punct = "" });
            pending_lead.clear();
        }
    }
    return chunks;
}

inline auto encode(std::string_view text, const Settings &cfg,
                   uint64_t seed = 1337ULL)
    -> std::expected<std::string, PuplangError> {
    auto tablex = build_sound_table(cfg.sounds);
    if (!tablex)
        return std::unexpected(tablex.error());
    auto &table = *tablex;

    auto chunks = tokenize_input_to_chunks(text);

    std::mt19937_64 rng(64);

    // Locate eligible chunks that support stretched "howl" representations
    std::vector<size_t> stretchable;
    for (size_t i = 0; i < chunks.size(); ++i) {
        const auto &list = table[chunks[i].value];
        if (!list.empty() && list.back().size() > list.front().size()) {
            stretchable.push_back(i);
        }
    }
    size_t howl_idx = stretchable.empty()
                          ? static_cast<size_t>(-1)
                          : stretchable[rng() % stretchable.size()];

    std::string result = cfg.header;
    int current_mode = 1;

    for (size_t i = 0; i < chunks.size(); ++i) {
        auto &chunk = chunks[i];
        std::string sound =
            pick_candidate_word(table[chunk.value], i == howl_idx, rng);

        // emit prefix switch only when the codepoint byte-width changes
        if (chunk.is_first_byte && chunk.mode != current_mode) {
            sound = cfg.prefixes[chunk.mode] + sound;
            current_mode = chunk.mode;
        }

        result += " " + chunk.lead_punct + sound + chunk.trail_punct;
    }

    result += " " + cfg.footer;
    return result;
}