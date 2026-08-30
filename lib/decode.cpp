#include "decode.hpp"

#include <ostream>
#include <sstream>

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

auto parse_framing_value(std::string_view token, const Settings &cfg)
    -> std::expected<uint8_t, PuplangError> {
    auto [lead, sound, trail, mode_change] = parse_encoded_word(token, cfg);
    (void)lead;
    (void)trail;
    (void)mode_change;
    auto val = get_sound_value(sound, cfg.sounds);
    if (!val)
        return std::unexpected(PuplangError::invalid_structure);
    return static_cast<uint8_t>(*val % BINARY_VALUE_MODULUS);
}

auto decode_token(std::string_view token, const Settings &cfg,
                  DecodeState &state, std::ostream &out)
    -> std::expected<void, PuplangError> {
    auto [lead, sound, trail, mode_change] = parse_encoded_word(token, cfg);

    out << lead;

    if (mode_change != 0)
        state.current_mode = static_cast<CodePointMode>(mode_change);

    // Bare punctuation token, nothing to decode
    if (sound.empty()) {
        out << trail;
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
    return {};
}

auto decode_stream(std::istream &in, std::ostream &out, const Settings &cfg,
                   std::function<void(double)> progress)
    -> std::expected<void, PuplangError> {
    DecodeState state;
    std::string token;

    ProgressTracker tracker{ progress, in };

    // Read first token (header)
    if (!(in >> token))
        return std::unexpected(PuplangError::invalid_structure);

    uint8_t header_byte = 0;
    bool legacy = false;
    auto header = parse_framing_value(token, cfg);
    if (!header)
        return std::unexpected(header.error());
    header_byte = *header;
    const auto version = header_byte & HEADER_MASK_VERSION;
    // v0 is reserved for explicit woof/yay framing
    if (version == 0) {
        legacy = (token == cfg.header);
        if (!legacy)
            return std::unexpected(PuplangError::invalid_structure);
    }
    if (version > FORMAT_VERSION)
        return std::unexpected(PuplangError::unsupported_version);

    tracker.report_fraction(std::streamoff(in.tellg()));

    // Read second token (first payload or footer if empty message)
    std::string next_token;
    if (!(in >> next_token))
        return std::unexpected(PuplangError::invalid_structure);
    tracker.report_fraction(std::streamoff(in.tellg()));

    // Process tokens with one-token lookahead
    while (in >> token) {
        auto result = decode_token(next_token, cfg, state, out);
        if (!result)
            return result;
        next_token = std::move(token);
        tracker.report_fraction(std::streamoff(in.tellg()));
    }

    // next_token is now the footer.
    // For v0 it must be the literal settings footer word (yay).
    // For the new format it mirrors the header.
    if (legacy) {
        if (next_token != cfg.footer)
            return std::unexpected(PuplangError::invalid_structure);
    } else {
        auto footer = parse_framing_value(next_token, cfg);
        if (!footer || *footer != header_byte)
            return std::unexpected(PuplangError::invalid_structure);
    }
    // incomplete multibyte sequence at EOF.. oh.
    if (state.accumulated_chunks != 0)
        return std::unexpected(PuplangError::missformed);

    tracker.report(1.0);

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
