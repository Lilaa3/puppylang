#include "decode.hpp"

#include <ostream>
#include <sstream>
#include <vector>

enum class TokenSegKind { Punct, Mode, Sound };

struct TokenSeg {
    TokenSegKind kind;
    std::string text;
    int mode = 0;
};

struct PrefixMatch {
    int mode;
    std::string_view prefix;
};

// Checks if the beginning of a string matches any defined mode prefix.
static std::optional<PrefixMatch> match_prefix(std::string_view s,
                                               const Settings &cfg) {
    if (cfg.prefixes.size() <= 1) {
        return std::nullopt;
    }

    auto begin = cfg.prefixes.begin() + 1;
    auto count = std::min<size_t>(4, cfg.prefixes.size() - 1);
    auto end = begin + count;

    // Check each prefix to see if the string starts with it
    int index = 1;
    for (auto it = begin; it != end; ++it, ++index) {
        const auto &p = *it;
        if (!p.empty() && s.starts_with(p)) {
            return PrefixMatch{ index, p };
        }
    }

    return std::nullopt;
}

// Splits a single section into a sequence of punctuation, mode prefix, and
// sound parts.
static auto tokenize_section(std::string_view section, const Settings &cfg)
    -> std::expected<std::vector<TokenSeg>, PuplangError> {
    std::vector<TokenSeg> out;

    while (!section.empty()) {
        // Prefix Match
        if (auto match = match_prefix(section, cfg)) {
            out.push_back({ .kind = TokenSegKind::Mode,
                            .text = std::string(match->prefix),
                            .mode = match->mode });
            section.remove_prefix(match->prefix.size());
            continue;
        }
        // Punctuation
        if (is_punct(section.front())) {
            out.push_back({ .kind = TokenSegKind::Punct,
                            .text = std::string(1, section.front()) });
            section.remove_prefix(1);
            continue;
        }

        // Sound Segment
        // Look ahead for the next boundary (either punctuation or a new prefix)
        size_t len = 0;
        while (len < section.size()) {
            auto remaining = section.substr(len);
            if (is_punct(remaining.front()) || match_prefix(remaining, cfg)) {
                break;
            }
            ++len;
        }

        if (len == 0)
            return std::unexpected(PuplangError::invalid_structure);

        out.push_back({ .kind = TokenSegKind::Sound,
                        .text = std::string(section.substr(0, len)) });
        section.remove_prefix(len);
    }

    return out;
}

// Parses only one token from a section
auto parse_framing_value(std::string_view section, const Settings &cfg)
    -> std::expected<uint8_t, PuplangError> {
    auto segs = tokenize_section(section, cfg);
    if (!segs)
        return std::unexpected(segs.error());

    std::optional<std::string_view> sound_text;
    for (const auto &seg : *segs) {
        if (seg.kind == TokenSegKind::Sound) {
            if (sound_text
                    .has_value()) { // More than one sound segment is invalid
                return std::unexpected(PuplangError::invalid_structure);
            }
            sound_text = seg.text;
            auto val = get_sound_value(*sound_text, cfg.sounds);
            if (!val)
                return std::unexpected(PuplangError::invalid_structure);
            return static_cast<uint8_t>(*val % BINARY_VALUE_MODULUS);
        } else { // Framing tokens shouldn't switch modes or have any
                 // punctuation
            return std::unexpected(PuplangError::invalid_structure);
        }
    }
    // Token contained no sound segment
    return std::unexpected(PuplangError::invalid_structure);
}

auto decode_tokens(std::string_view section, const Settings &cfg,
                   DecodeState &state, std::ostream &out)
    -> std::expected<void, PuplangError> {
    auto segsx = tokenize_section(section, cfg);
    if (!segsx)
        return std::unexpected(segsx.error());
    auto segs = segsx.value();

    for (const auto &seg : segs) {
        switch (seg.kind) {
        case TokenSegKind::Punct:
            out << seg.text;
            break;
        case TokenSegKind::Mode:
            state.current_mode = static_cast<CodePointMode>(seg.mode);
            break;
        case TokenSegKind::Sound: {
            auto sound_val = get_sound_value(seg.text, cfg.sounds);
            if (!sound_val)
                return std::unexpected(sound_val.error());
            state.current_cp = (state.current_cp << BITS_PER_SOUND) |
                               (static_cast<uint32_t>(*sound_val) & SOUND_MASK);
            state.accumulated_chunks++;
            // Once the required number of chunks for the current mode are
            // buffered, emit the codepoint
            if (state.accumulated_chunks == state.current_mode) {
                append_utf8(out, state.current_cp);
                state.current_cp = 0;
                state.accumulated_chunks = 0;
            }
            break;
        }
        }
    }
    return {};
}

auto decode_stream(std::istream &in, std::ostream &out, const Settings &cfg,
                   std::function<void(double)> progress)
    -> std::expected<void, PuplangError> {
    DecodeState state;
    std::string section;

    ProgressTracker tracker{ progress, in };

    // Read first section (header)
    if (!(in >> section))
        return std::unexpected(PuplangError::invalid_structure);

    uint8_t header_byte = 0;
    bool legacy = false;
    auto header = parse_framing_value(section, cfg);
    if (!header)
        return std::unexpected(header.error());
    header_byte = *header;
    const auto version = header_byte & HEADER_MASK_VERSION;
    // v0 is reserved for explicit woof/yay framing
    if (version == 0) {
        legacy = (section == cfg.header);
        if (!legacy)
            return std::unexpected(PuplangError::invalid_structure);
    }
    if (version > FORMAT_VERSION)
        return std::unexpected(PuplangError::unsupported_version);

    tracker.report_fraction(std::streamoff(in.tellg()));

    // Read second section (first payload or footer if empty message)
    std::string next_section;
    if (!(in >> next_section))
        return std::unexpected(PuplangError::invalid_structure);
    tracker.report_fraction(std::streamoff(in.tellg()));

    // Process sections with lookahead
    while (in >> section) {
        auto result = decode_tokens(next_section, cfg, state, out);
        if (!result)
            return result;
        next_section = std::move(section);
        tracker.report_fraction(std::streamoff(in.tellg()));
    }

    // next_section is now the footer.
    // For v0 it must be the literal settings footer word (yay).
    // For the new format it mirrors the header.
    if (legacy) {
        if (next_section != cfg.footer)
            return std::unexpected(PuplangError::invalid_structure);
    } else {
        auto footer = parse_framing_value(next_section, cfg);
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
