#include "puplang.hpp"

#include <sstream>

namespace puplang {

auto error_message(PuplangError e) -> std::string_view {
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

const Settings default_settings = [] {
    static constexpr unsigned char data[] = {
#embed "default_settings.txt"
    };
    return Settings::parse(
        std::string_view{ reinterpret_cast<const char *>(data), sizeof(data) });
}();

auto encode(std::string_view text) -> std::expected<std::string, PuplangError> {
    return ::encode(text, default_settings);
}

auto decode(std::string_view text) -> std::expected<std::string, PuplangError> {
    return ::decode(text, default_settings);
}

auto encode(std::string_view text, const Settings &cfg,
            const EncodeOptions &opts)
    -> std::expected<std::string, PuplangError> {
    return ::encode(text, cfg, opts);
}

auto encode(std::string_view text, std::string_view settings,
            const EncodeOptions &opts)
    -> std::expected<std::string, PuplangError> {
    return ::encode(text, Settings::parse(settings), opts);
}

auto decode(std::string_view text, const Settings &cfg)
    -> std::expected<std::string, PuplangError> {
    return ::decode(text, cfg);
}

auto decode(std::string_view text, std::string_view settings)
    -> std::expected<std::string, PuplangError> {
    return ::decode(text, Settings::parse(settings));
}

auto generate_sound_table() -> std::string {
    return ::generate_sound_table(default_settings);
}

auto generate_sound_table(const Settings &cfg) -> std::string {
    return ::generate_sound_table(cfg);
}

auto encode_file(std::string_view in_path, std::string_view out_path,
                 const Settings &cfg, const EncodeOptions &opts,
                 std::function<void(double)> progress, bool no_temp)
    -> std::expected<void, PuplangError> {
    return detail::convert_file(
        [&](std::istream &in, std::ostream &out) {
            return encode_stream(in, out, cfg, opts, progress);
        },
        in_path,
        out_path,
        no_temp);
}

auto encode_file(std::string_view in_path, std::string_view out_path,
                 std::string_view settings, const EncodeOptions &opts,
                 std::function<void(double)> progress, bool no_temp)
    -> std::expected<void, PuplangError> {
    return encode_file(
        in_path, out_path, Settings::parse(settings), opts, progress, no_temp);
}

auto decode_file(std::string_view in_path, std::string_view out_path,
                 const Settings &cfg, std::function<void(double)> progress,
                 bool no_temp) -> std::expected<void, PuplangError> {
    return detail::convert_file(
        [&](std::istream &in, std::ostream &out) {
            return decode_stream(in, out, cfg, progress);
        },
        in_path,
        out_path,
        no_temp);
}

auto decode_file(std::string_view in_path, std::string_view out_path,
                 std::function<void(double)> progress, bool no_temp)
    -> std::expected<void, PuplangError> {
    return decode_file(in_path, out_path, default_settings, progress, no_temp);
}

} // namespace puplang
