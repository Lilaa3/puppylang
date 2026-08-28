#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <expected>
#include <fstream>
#include <functional>
#include <ios>
#include <numeric>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

enum class Casing : int { Lower = 0, Title = 1, Upper = 2, Step = 3 };
enum class PuplangError {
    empty_input,
    unrecognized,
    missformed,
    unknown_casing,
    invalid_structure,
    bark_collision,
    invalid_utf8,
    io_error
};

constexpr auto CASING_MULTIPLIER = static_cast<int>(Casing::Step);
constexpr auto BITS_PER_SOUND = 7;
constexpr uint32_t VALUE_MODULUS = 1u << BITS_PER_SOUND;
constexpr uint32_t SOUND_MASK = VALUE_MODULUS - 1;

class ProgressTracker {
    std::function<void(double)> sink;
    double last = -1.0;
    double threshold = 0.0001;
    double total = 0.0;

  public:
    explicit ProgressTracker(std::function<void(double)> sink, std::istream &in)
        : sink(sink) {
        calculate_total(in);
    }

    void report(double frac) {
        if (!sink)
            return;
        if (frac >= 1.0 || frac - last > threshold) {
            sink(frac);
            last = frac;
        }
    }

    void report_fraction(std::streamoff current) {
        if (total <= 0)
            return;
        report(static_cast<double>(current) / total);
    }

  private:
    void calculate_total(std::istream &in) {
        in.clear();
        in.seekg(0, std::ios::end);
        const auto endpos = in.tellg();
        in.clear();
        in.seekg(0, std::ios::beg);
        if (endpos > std::streampos(0))
            total = static_cast<double>(std::streamoff(endpos));
    }
};

// Represents a run of identical characters
struct Run {
    char c;
    int count;
    int max_extra = 2; // Default repetition limit when unspecified in pattern
};

struct WordInfo {
    std::vector<Run> runs;
    int base_index;
    int var_offset;
};

enum CodePointMode { ASCII = 1, LATIN, BMP, SPECIAL };

auto is_punct(char c) {
    return std::string_view(",!?:;'\".()[]{}").contains(c);
}

// Computes the total combinations that a sound can have.
auto run_combinations(const std::vector<Run> &runs) noexcept {
    return std::accumulate(
        runs.begin(), runs.end(), 1, [](int acc, const Run &r) {
            return acc * (r.max_extra + 1);
        });
}

auto determine_casing(std::string_view s)
    -> std::expected<Casing, PuplangError> {
    if (std::ranges::all_of(s, [](unsigned char c) { return std::isupper(c); }))
        return Casing::Upper;
    if (std::ranges::all_of(s, [](unsigned char c) { return std::islower(c); }))
        return Casing::Lower;
    if (!s.empty() && std::isupper(static_cast<unsigned char>(s.front())))
        return Casing::Title;
    // oh,., bad puppy
    return std::unexpected(PuplangError::unknown_casing);
}

// Groups consecutive identical characters
// Casing is evaluated separately
auto run_length_encode(std::string_view s) -> std::vector<Run> {
    auto same_char_ci = [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) ==
               std::tolower(static_cast<unsigned char>(b));
    };

    std::vector<Run> runs;
    for (auto chunk : s | std::views::chunk_by(same_char_ci)) {
        char lower = static_cast<char>(
            std::tolower(static_cast<unsigned char>(chunk.front())));
        runs.push_back(
            { .c = lower,
              .count = static_cast<int>(std::ranges::distance(chunk)),
              .max_extra = 2 });
    }
    return runs;
}

// Matches input runs against a sound's runs
auto match_pattern_variation(const std::vector<Run> &in_runs,
                             const WordInfo &base)
    -> std::expected<std::optional<int>, PuplangError> {
    if (base.runs.size() != in_runs.size())
        return std::nullopt;

    int var_index = 0;
    int multiplier = 1;

    for (size_t i = 0; i < base.runs.size(); ++i) {
        int extra = in_runs[i].count - base.runs[i].count;
        if (in_runs[i].c != base.runs[i].c || extra < 0)
            return std::nullopt;

        // Exceeding defined limit, whine whine
        if (extra > base.runs[i].max_extra) {
            return std::unexpected(PuplangError::missformed);
        }
        var_index += extra * multiplier;
        multiplier *= (base.runs[i].max_extra + 1);
    }
    return var_index;
}

// Base sound, variation index, casing into a unique linear integer before
// modulo.
auto calc_sound_value(const WordInfo &base, int var_index, Casing casing) {
    uint32_t base_val = base.base_index * CASING_MULTIPLIER;
    if (var_index > 0)
        base_val = base.var_offset + (var_index - 1) * CASING_MULTIPLIER;
    return base_val + std::to_underlying(casing);
}

// Identifies the matching archetype and derives the wrapped 7-bit value.
auto get_sound_value(std::string_view input,
                     const std::vector<WordInfo> &sounds)
    -> std::expected<uint32_t, PuplangError> {
    if (input.empty())
        return std::unexpected(PuplangError::empty_input);

    auto casing = determine_casing(input);
    if (!casing)
        return std::unexpected(casing.error());
    auto in_runs = run_length_encode(input);

    for (const auto &base : sounds) {
        if (auto var_idx = match_pattern_variation(in_runs, base)) {
            if (!var_idx)
                return std::unexpected(var_idx.error());
            if (var_idx->has_value()) {
                return calc_sound_value(base, **var_idx, *casing) %
                       VALUE_MODULUS;
            }
        }
    }
    return std::unexpected(PuplangError::unrecognized);
}

// Parses a pattern for a word/sound into a vector of runs
auto parse_pattern(std::string_view pattern) -> std::vector<Run> {
    std::vector<Run> runs;

    auto update_run = [&](char raw_c) {
        if (auto c = static_cast<char>(
                std::tolower(static_cast<unsigned char>(raw_c)));
            !runs.empty() && runs.back().c == c) {
            runs.back().count++;
        } else {
            runs.push_back({ .c = c, .count = 1, .max_extra = 2 });
        }
    };

    for (size_t i = 0; i < pattern.size();) {
        if (pattern[i] == '[') { // max run count open
            if (auto close = pattern.find("]", i);
                close != std::string_view::npos && !runs.empty()) {
                auto digits = pattern.substr(i + 1, close - i - 1);
                std::from_chars(digits.data(),
                                digits.data() + digits.size(),
                                runs.back().max_extra);
                i = close + 1;
                continue;
            }
        }
        if (std::isalpha(static_cast<unsigned char>(pattern[i]))) { // letter
            update_run(pattern[i]);
        }
        ++i;
    }

    return runs;
}

struct Settings {
    std::string header;
    std::string footer;
    std::array<std::string, 5> prefixes;
    std::vector<WordInfo> sounds;

    static auto parse(std::string_view contents) -> Settings {
        Settings cfg;
        std::istringstream file{ std::string(contents) };
        std::string line_buf;
        std::vector<std::string> patterns;
        bool in_sounds = false;

        auto trim = [](std::string_view s) -> std::string_view {
            auto a = s.find_first_not_of(" \t\r\n"),
                 b = s.find_last_not_of(" \t\r\n");
            return (a == std::string_view::npos) ? "" : s.substr(a, b - a + 1);
        };

        while (std::getline(file, line_buf)) {
            auto line = trim(line_buf);
            if (line.empty() || line.starts_with("#"))
                continue;

            if (line == "[sounds]") {
                in_sounds = true;
                continue;
            }

            if (in_sounds) {
                patterns.emplace_back(line);
            } else if (auto eq = line.find("="); eq != std::string_view::npos) {
                auto k = trim(line.substr(0, eq)),
                     v = trim(line.substr(eq + 1));
                if (k == "header")
                    cfg.header = v;
                else if (k == "footer")
                    cfg.footer = v;
                else if (k == "prefix_1")
                    cfg.prefixes[1] = v;
                else if (k == "prefix_2")
                    cfg.prefixes[2] = v;
                else if (k == "prefix_3")
                    cfg.prefixes[3] = v;
                else if (k == "prefix_4")
                    cfg.prefixes[4] = v;
            }
        }

        // layout the numerical space of the happy little barks
        int offset = static_cast<int>(patterns.size()) * CASING_MULTIPLIER;
        for (size_t i = 0; i < patterns.size(); ++i) {
            auto runs = parse_pattern(patterns[i]);
            cfg.sounds.push_back({ .runs = runs,
                                   .base_index = static_cast<int>(i),
                                   .var_offset = offset });
            offset += (run_combinations(runs) - 1) * CASING_MULTIPLIER;
        }
        return cfg;
    }

    static auto load(std::string_view path) -> std::optional<Settings> {
        std::ifstream file{ std::string(path) };
        if (!file)
            return std::nullopt;
        std::ostringstream buffer;
        buffer << file.rdbuf();
        auto cfg = parse(buffer.str());
        if (cfg.sounds.empty())
            return std::nullopt;
        return cfg;
    }
};
