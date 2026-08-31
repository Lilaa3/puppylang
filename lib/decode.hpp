#pragma once

#include <expected>
#include <functional>
#include <iosfwd>
#include <iterator>
#include <string>
#include <string_view>

#include "utf8.h"
#include "utility.hpp"

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
};

/// Processes a single token, appending decoded output to `out`.
auto decode_token(std::string_view token, const Settings &cfg,
                  DecodeState &state, std::ostream &out)
    -> std::expected<void, PuplangError>;

auto decode_stream(std::istream &in, std::ostream &out, const Settings &cfg,
                   std::function<void(double)> progress = {})
    -> std::expected<void, PuplangError>;

auto decode(std::string_view encoded_text, const Settings &cfg)
    -> std::expected<std::string, PuplangError>;
