#pragma once

#include <expected>
#include <iterator>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "utf8.h"
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
auto parse_encoded_word(std::string_view token, const Settings &cfg)
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

// Appends a UTF-8 codepoint to any string-like or stream-like output.
// Works with std::string (push_back) and std::ostream (put).
template <typename Out>
    requires requires(Out o, char c) { o.push_back(c); } ||
             requires(Out o, char c) { o.put(c); }
void append_utf8(Out &out, uint32_t cp) {
    if constexpr (requires { out.push_back(char{}); }) {
        utf8::append(cp, std::back_inserter(out));
    } else {
        utf8::append(cp, std::ostreambuf_iterator<char>(out));
    }
}

/// State maintained across token decoding.
struct DecodeState {
    CodePointMode current_mode = CodePointMode::ASCII;
    uint32_t current_cp = 0;
    uint8_t accumulated_chunks = 0;
    bool seen_header = false;
    std::string last_token; // For footer verification at EOF
};

/// Processes a single token, appending decoded output to `out`.
auto decode_token(std::string_view token, const Settings &cfg,
                  DecodeState &state, std::ostream &out)
    -> std::expected<void, PuplangError> {
    auto [lead, sound, trail, mode_change] = parse_encoded_word(token, cfg);

    if (!state.seen_header) {
        if (token != cfg.header)
            return std::unexpected(PuplangError::invalid_structure);
        state.seen_header = true;
        return {};
    }

    out << lead;

    if (mode_change != 0)
        state.current_mode = static_cast<CodePointMode>(mode_change);

    // Bare punctuation token, nothing to decode
    if (sound.empty()) {
        out << trail;
        state.last_token = std::string(token);
        return {};
    }

    auto sound_val = get_sound_value(sound, cfg.sounds);
    if (!sound_val)
        return std::unexpected(sound_val.error());

    state.current_cp = (state.current_cp << BITS_PER_SOUND) |
                       (static_cast<uint32_t>(*sound_val) & SOUND_MASK);
    state.accumulated_chunks++;

    // Once the required number of chunks for the current mode are buffered,
    // emit the codepoint
    if (state.accumulated_chunks == state.current_mode) {
        append_utf8(out, state.current_cp);
        state.current_cp = 0;
        state.accumulated_chunks = 0;
    }

    out << trail;
    state.last_token = std::string(token);
    return {};
}

auto decode_stream(std::istream &in, std::ostream &out, const Settings &cfg)
    -> std::expected<void, PuplangError> {
    DecodeState state;
    std::string token;

    // Read first token (header)
    if (!(in >> token) || token != cfg.header)
        return std::unexpected(PuplangError::invalid_structure);
    state.seen_header = true;

    // Read second token (first payload or footer if empty message)
    std::string next_token;
    if (!(in >> next_token))
        return std::unexpected(PuplangError::invalid_structure);

    // Process tokens with one-token lookahead
    while (in >> token) {
        auto result = decode_token(next_token, cfg, state, out);
        if (!result)
            return result;
        next_token = std::move(token);
    }

    if (next_token != cfg.footer) // next_token is now the footer
        return std::unexpected(PuplangError::invalid_structure);
    // incomplete multibyte sequence at EOF.. oh.
    if (state.accumulated_chunks != 0)
        return std::unexpected(PuplangError::missformed);

    return {};
}

auto decode(std::string_view encoded_text, const Settings &cfg)
    -> std::expected<std::string, PuplangError> {
    std::istringstream in{ std::string(encoded_text) };
    std::ostringstream out;
    auto result = decode_stream(in, out, cfg);
    if (!result)
        return std::unexpected(result.error());
    return out.str();
}