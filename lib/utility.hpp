#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <functional>
#include <ios>
#include <istream>
#include <optional>
#include <string>
#include <string_view>
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

bool is_punct(char c);

// Computes the total combinations that a sound can have.
int run_combinations(const std::vector<Run> &runs) noexcept;

auto determine_casing(std::string_view s)
    -> std::expected<Casing, PuplangError>;

// Groups consecutive identical characters
// Casing is evaluated separately
auto run_length_encode(std::string_view s) -> std::vector<Run>;

// Matches input runs against a sound's runs
auto match_pattern_variation(const std::vector<Run> &in_runs,
                             const WordInfo &base)
    -> std::expected<std::optional<int>, PuplangError>;

// Base sound, variation index, casing into a unique linear integer before
// modulo.
auto calc_sound_value(const WordInfo &base, int var_index, Casing casing)
    -> uint32_t;

// Identifies the matching archetype and derives the wrapped 7-bit value.
auto get_sound_value(std::string_view input,
                     const std::vector<WordInfo> &sounds)
    -> std::expected<uint32_t, PuplangError>;

// Parses a pattern for a word/sound into a vector of runs
auto parse_pattern(std::string_view pattern) -> std::vector<Run>;

struct Settings {
    std::string header;
    std::string footer;
    std::array<std::string, 5> prefixes;
    std::vector<WordInfo> sounds;

    static auto parse(std::string_view contents) -> Settings;
    static auto load(std::string_view path) -> std::optional<Settings>;
};
