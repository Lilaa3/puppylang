#pragma once

#include <expected>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "utility.hpp"

struct TokenParts {
    std::string_view lead;
    std::string_view body;
    std::string_view trail;
};

// Slices punctuation boundaries from a token
auto split_punctuation(std::string_view token) -> TokenParts {
    size_t start = 0;
    while (start < token.size() && is_punct(token[start]))
        start++;

    size_t end = token.size();
    while (end > start && is_punct(token[end - 1]))
        end--;

    return { .lead = token.substr(0, start),
             .body = token.substr(start, end - start),
             .trail = token.substr(end) };
}

struct ParsedWord {
    std::string_view lead_punct;
    std::string_view sound;
    std::string_view trail_punct;
    int mode_change = 0;
};

// Deconstructs a formatted token into punctuation, mode prefix, and the core
// sound
inline auto parse_encoded_word(std::string_view token, const Settings &cfg)
    -> ParsedWord {
    auto [lead, body, trail] = split_punctuation(token);
    int mode_change = 0;

    // Check for mode prefixes while preserving the casing of the sound itself
    for (int m = 1; m <= 4; ++m) {
        if (!cfg.prefixes[m].empty() && body.starts_with(cfg.prefixes[m])) {
            mode_change = m;
            body.remove_prefix(cfg.prefixes[m].size());
            break;
        }
    }

    return { .lead_punct = lead,
             .sound = body,
             .trail_punct = trail,
             .mode_change = mode_change };
}

// Appends a UTF-8 codepoint to a string
inline void append_utf8(std::string &out, uint32_t cp) {
    auto emit = [&](auto... bytes) {
        (out.push_back(static_cast<char>(bytes)), ...);
    };

    if (cp <= 0x7F) {
        emit(cp);
    } else if (cp <= 0x7FF) {
        emit(0xC0 | (cp >> 6), 0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        emit(0xE0 | (cp >> 12), 0x80 | ((cp >> 6) & 0x3F), 0x80 | (cp & 0x3F));
    } else {
        emit(0xF0 | (cp >> 18),
             0x80 | ((cp >> 12) & 0x3F),
             0x80 | ((cp >> 6) & 0x3F),
             0x80 | (cp & 0x3F));
    }
}

inline auto decode(std::string_view encoded_text, const Settings &cfg)
    -> std::expected<std::string, PuplangError> {
    auto tokens =
        encoded_text | std::views::split(" ") |
        std::views::transform([](auto r) { return std::string_view(r); }) |
        std::views::filter([](auto sv) { return !sv.empty(); }) |
        std::ranges::to<std::vector>();

    // Message must be bounded by configured header and footer
    if (tokens.size() < 2 || tokens.front() != cfg.header ||
        tokens.back() != cfg.footer) {
        return std::unexpected(PuplangError::invalid_structure);
    }

    std::string result;
    auto current_mode = 1;   // Default mode is 1 chunk per codepoint (ASCII)
    uint32_t current_cp = 0; // Bit-buffer accumulating 7-bit chunks
    auto accumulated_chunks = 0;

    // Process only payload tokens, excluding framing header and footer
    for (size_t i = 1; i + 1 < tokens.size(); ++i) {
        auto [lead, sound, trail, mode_change] =
            parse_encoded_word(tokens[i], cfg);
        result.append(lead);

        if (mode_change != 0)
            current_mode = mode_change;

        auto sound_val = get_sound_value(sound, cfg.sounds);
        if (!sound_val)
            return std::unexpected(sound_val.error());

        current_cp = (current_cp << BITS_PER_SOUND) |
                     (static_cast<uint32_t>(*sound_val) & SOUND_MASK);
        accumulated_chunks++;

        // Once the required number of chunks for the current mode are buffered,
        // emit the codepoint
        if (accumulated_chunks == current_mode) {
            append_utf8(result, current_cp);
            current_cp = 0;
            accumulated_chunks = 0;
        }

        result.append(trail);
    }

    // incomplete multibyte sequence at EOF.. oh.
    if (accumulated_chunks != 0) {
        return std::unexpected(PuplangError::missformed);
    }

    return result;
}