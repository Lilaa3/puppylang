#include <expected>
#include <string>

#include "puplang.hpp"

namespace {

std::string g_last_error;
std::string g_result;

EncodeOptions g_opts;

auto run_encode(std::string_view text, std::string_view settings) -> const
    char * {
    std::expected<std::string, PuplangError> result =
        settings.empty()
            ? puplang::encode(text, puplang::default_settings, g_opts)
            : puplang::encode(text, settings, g_opts);
    if (!result) {
        g_last_error = std::string(puplang::error_message(result.error()));
        return nullptr;
    }
    g_last_error.clear();
    g_result = *result;
    return g_result.c_str();
}

auto run_decode(std::string_view text, std::string_view settings) -> const
    char * {
    std::expected<std::string, PuplangError> result =
        settings.empty() ? puplang::decode(text)
                         : puplang::decode(text, settings);
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
    return run_encode(text, settings);
}

extern "C" const char *puplang_decode(const char *text, const char *settings) {
    return run_decode(text, settings);
}

extern "C" void puplang_set_seed(uint64_t seed) { g_opts.seed = seed; }

extern "C" void puplang_set_initial_howl_chance(double chance) {
    g_opts.initial_howl_chance = chance;
}

extern "C" void puplang_set_howl_decay(double decay) {
    g_opts.howl_decay = decay;
}

extern "C" void puplang_set_free_extension_limit(int limit) {
    g_opts.free_extension_limit = limit;
}

extern "C" void puplang_set_min_howl(double min_howl) {
    g_opts.min_howl = min_howl;
}

extern "C" void puplang_set_length_decay_rate(double length_decay_rate) {
    g_opts.length_decay_rate = length_decay_rate;
}

extern "C" void puplang_set_howl_enabled(bool enabled) {
    g_opts.howl_enabled = enabled;
}

extern "C" void puplang_set_uppercase_weight(double decay) {
    g_opts.uppercase_weight = decay;
}

extern "C" void puplang_set_max_short_extension(int limit) {
    g_opts.max_short_extension = limit;
}