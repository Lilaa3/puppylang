#include <print>
#include <string>

#include "utility.hpp"
#include "encode.hpp"
#include "decode.hpp"

int main() {
    auto config = Settings::load("settings.txt");

    std::string original = "Wow supporte para VARIOS caracteres! O ç, o ª e até o ワンワン";
    auto encoded = encode(original, config).value();
    
    std::println("Original: {}", original);
    std::println("Encoded:  {}", encoded);

    auto decoded = decode(encoded, config);
    if (decoded) {
        std::println("Decoded:  {}", *decoded);
    } else {
        std::println("Decode failed!");
    }
}