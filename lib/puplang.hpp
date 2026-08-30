#pragma once

#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <string_view>
#include <system_error>

#include "decode.hpp"
#include "encode.hpp"
#include "sound_table.hpp"
#include "utility.hpp"

namespace puplang {

auto error_message(PuplangError e) -> std::string_view;

// Default sound table, footer, header, etc.
extern const Settings default_settings;

// High-level helpers

auto encode(std::string_view text) -> std::expected<std::string, PuplangError>;

auto decode(std::string_view text) -> std::expected<std::string, PuplangError>;

auto encode(std::string_view text, const Settings &cfg,
            const EncodeOptions &opts = {})
    -> std::expected<std::string, PuplangError>;

auto encode(std::string_view text, std::string_view settings,
            const EncodeOptions &opts = {})
    -> std::expected<std::string, PuplangError>;

auto decode(std::string_view text, const Settings &cfg)
    -> std::expected<std::string, PuplangError>;

auto decode(std::string_view text, std::string_view settings)
    -> std::expected<std::string, PuplangError>;

auto generate_sound_table() -> std::string;

auto generate_sound_table(const Settings &cfg) -> std::string;

namespace detail {

// Handles .tmp file creation, rename and cleanup
template <typename Fx>
auto convert_file(Fx fx, std::string_view in_path, std::string_view out_path,
                  bool no_temp = false) -> std::expected<void, PuplangError> {
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

auto encode_file(std::string_view in_path, std::string_view out_path,
                 const Settings &cfg, const EncodeOptions &opts = {},
                 std::function<void(double)> progress = {},
                 bool no_temp = false) -> std::expected<void, PuplangError>;

auto encode_file(std::string_view in_path, std::string_view out_path,
                 std::string_view settings, const EncodeOptions &opts = {},
                 std::function<void(double)> progress = {},
                 bool no_temp = false) -> std::expected<void, PuplangError>;

auto decode_file(std::string_view in_path, std::string_view out_path,
                 const Settings &cfg, std::function<void(double)> progress = {},
                 bool no_temp = false) -> std::expected<void, PuplangError>;

auto decode_file(std::string_view in_path, std::string_view out_path,
                 std::function<void(double)> progress = {},
                 bool no_temp = false) -> std::expected<void, PuplangError>;

} // namespace puplang
