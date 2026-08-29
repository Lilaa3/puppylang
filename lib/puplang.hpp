#pragma once

#include <expected>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

#include "decode.hpp"
#include "encode.hpp"
#include "sound_table.hpp"
#include "utility.hpp"

namespace puplang {

inline auto error_message(PuplangError e) -> std::string_view {
    switch (e) {
    case PuplangError::empty_input:
        return "empty input";
    case PuplangError::unrecognized:
        return "unrecognized sound";
    case PuplangError::missformed:
        return "missformed sound";
    case PuplangError::unknown_casing:
        return "unknown casing";
    case PuplangError::invalid_structure:
        return "missing woof/yay framing";
    case PuplangError::bark_collision:
        return "sound table collision";
    case PuplangError::invalid_utf8:
        return "invalid or truncated utf-8";
    case PuplangError::io_error:
        return "i/o error";
    }
    return "unknown error";
}

// Default sound table, footer, header, etc.
inline const Settings default_settings = [] {
    static constexpr unsigned char data[] = {
#embed "default_settings.txt"
    };
    return Settings::parse(
        std::string_view{ reinterpret_cast<const char *>(data), sizeof(data) });
}();

// High-level helpers

inline auto encode(std::string_view text)
    -> std::expected<std::string, PuplangError> {
    return ::encode(text, default_settings);
}

inline auto decode(std::string_view text)
    -> std::expected<std::string, PuplangError> {
    return ::decode(text, default_settings);
}

inline auto encode(std::string_view text, const Settings &cfg,
                   const EncodeOptions &opts = {})
    -> std::expected<std::string, PuplangError> {
    return ::encode(text, cfg, opts);
}

inline auto encode(std::string_view text, std::string_view settings,
                   const EncodeOptions &opts = {})
    -> std::expected<std::string, PuplangError> {
    return ::encode(text, Settings::parse(settings), opts);
}

inline auto decode(std::string_view text, const Settings &cfg)
    -> std::expected<std::string, PuplangError> {
    return ::decode(text, cfg);
}

inline auto decode(std::string_view text, std::string_view settings)
    -> std::expected<std::string, PuplangError> {
    return ::decode(text, Settings::parse(settings));
}

inline auto generate_sound_table() -> std::string {
    return ::generate_sound_table(default_settings);
}

inline auto generate_sound_table(const Settings &cfg) -> std::string {
    return ::generate_sound_table(cfg);
}

namespace detail {

// Handles .tmp file creation, rename and cleanup
template <typename Fx>
inline auto convert_file(Fx fx, std::string_view in_path,
                         std::string_view out_path, bool no_temp = false)
    -> std::expected<void, PuplangError> {
    std::ifstream in{ std::string(in_path) };
    if (!in)
        return std::unexpected(PuplangError::io_error);

    const std::filesystem::path dst{ std::string(out_path) };
    const std::filesystem::path target =
        no_temp ? dst : std::filesystem::path{ std::string(out_path) + ".tmp" };

    auto cleanup = [&] {
        if (!no_temp) {
            std::error_code ignored;
            std::filesystem::remove(target, ignored);
        }
    };

    {
        std::ofstream out{ target };
        if (!out)
            return std::unexpected(PuplangError::io_error);
        auto result = fx(in, out);
        out.flush();
        out.close();
        if (!result) {
            cleanup();
            return result;
        }
    } // stream is closed before the rename

    if (no_temp)
        return {};

    std::error_code ec;
    std::filesystem::rename(target, dst, ec);
    if (!ec)
        return {};

    cleanup();
    return std::unexpected(PuplangError::io_error);
}
} // namespace detail

inline auto encode_file(std::string_view in_path, std::string_view out_path,
                        const Settings &cfg, const EncodeOptions &opts = {},
                        std::function<void(double)> progress = {},
                        bool no_temp = false)
    -> std::expected<void, PuplangError> {
    return detail::convert_file(
        [&](std::istream &in, std::ostream &out) {
            return encode_stream(in, out, cfg, opts, progress);
        },
        in_path,
        out_path,
        no_temp);
}

inline auto
encode_file(std::string_view in_path, std::string_view out_path,
            std::string_view settings, const EncodeOptions &opts = {},
            std::function<void(double)> progress = {}, bool no_temp = false)
    -> std::expected<void, PuplangError> {
    return encode_file(
        in_path, out_path, Settings::parse(settings), opts, progress, no_temp);
}

inline auto decode_file(std::string_view in_path, std::string_view out_path,
                        const Settings &cfg,
                        std::function<void(double)> progress = {},
                        bool no_temp = false)
    -> std::expected<void, PuplangError> {
    return detail::convert_file(
        [&](std::istream &in, std::ostream &out) {
            return decode_stream(in, out, cfg, progress);
        },
        in_path,
        out_path,
        no_temp);
}

inline auto decode_file(std::string_view in_path, std::string_view out_path,
                        std::function<void(double)> progress = {},
                        bool no_temp = false)
    -> std::expected<void, PuplangError> {
    return decode_file(in_path, out_path, default_settings, progress, no_temp);
}

} // namespace puplang
