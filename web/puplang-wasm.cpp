#include <string>

#include "decode.hpp"
#include "encode.hpp"
#include "utility.hpp"

namespace {

std::string g_last_error;
std::string g_result;

auto run(std::string_view text, const Settings &cfg, bool do_encode) -> const
    char * {
    auto result = do_encode ? encode(text, cfg) : decode(text, cfg);
    if (!result) {
        switch (result.error()) {
        case PuplangError::empty_input:
            g_last_error = "empty input";
            break;
        case PuplangError::unrecognized:
            g_last_error = "unrecognized sound";
            break;
        case PuplangError::missformed:
            g_last_error = "missformed sound";
            break;
        case PuplangError::unknown_casing:
            g_last_error = "unknown casing";
            break;
        case PuplangError::invalid_structure:
            g_last_error = "missing woof/yay framing";
            break;
        case PuplangError::bark_collision:
            g_last_error = "sound table collision";
            break;
        case PuplangError::invalid_utf8:
            g_last_error = "invalid or truncated utf-8";
            break;
        }
        return nullptr;
    }
    g_last_error.clear();
    g_result = *result;
    return g_result.c_str();
}

} // namespace

extern "C" const char *puplang_last_error() { return g_last_error.c_str(); }

extern "C" const char *puplang_encode(const char *text, const char *settings) {
    return run(text, Settings::parse(settings), true);
}

extern "C" const char *puplang_decode(const char *text, const char *settings) {
    return run(text, Settings::parse(settings), false);
}