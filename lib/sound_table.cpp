#include "sound_table.hpp"

#include <cstdint>
#include <format>
#include <sstream>

auto ascii_repr(uint8_t c) -> std::string {
    std::string s = std::format("{:?}", static_cast<char>(c));
    return s.substr(1, s.size() - 2); // strip the single quotes
}

auto generate_sound_table(const Settings &cfg) -> std::string {
    std::ostringstream out;
    for (const auto &base : cfg.sounds) {
        std::string base_word = build_word_variation(base, 0);
        out << '[' << base_word << "]\n";

        int variations = run_combinations(base.runs);
        for (int var = 0; var < variations; ++var) {
            std::string variant = build_word_variation(base, var);
            int extra = static_cast<int>(variant.size()) -
                        static_cast<int>(base_word.size());
            for (auto casing :
                 { Casing::Lower, Casing::Title, Casing::Upper }) {
                std::string word = apply_casing(variant, casing);
                uint32_t value = calc_sound_value(base, var, casing);
                uint8_t v7 = static_cast<uint8_t>(value % 128);
                uint8_t v8 = static_cast<uint8_t>(value % 256);
                out << "word=" << word << " value=" << value
                    << " extra=" << extra << " 7bit=" << static_cast<int>(v7)
                    << " 8bit=" << static_cast<int>(v8)
                    << " ascii=" << ascii_repr(v7) << '\n';
            }
        }
    }
    return out.str();
}
