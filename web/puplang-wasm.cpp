#include <expected>
#include <string>

#include "puplang.hpp"

namespace {

std::string g_last_error;
std::string g_result;

auto run(std::string_view text, std::string_view settings, bool do_encode)
    -> const char * {
    // Empty settings selects the compile-time embedded default sound table.
    std::expected<std::string, PuplangError> result =
        do_encode ? (settings.empty() ? puplang::encode(text)
                                      : puplang::encode(text, settings))
                  : (settings.empty() ? puplang::decode(text)
                                      : puplang::decode(text, settings));
    if (!result) {
        g_last_error = std::string(puplang::error_message(result.error()));
        return nullptr;
    }
    g_last_error.clear();
    g_result = *result;
    return g_result.c_str();
}

} // namespace

extern "C" const char *puplang_last_error() { return g_last_error.c_str(); }

extern "C" const char *puplang_encode(const char *text, const char *settings) {
    return run(text, settings, true);
}

extern "C" const char *puplang_decode(const char *text, const char *settings) {
    return run(text, settings, false);
}
