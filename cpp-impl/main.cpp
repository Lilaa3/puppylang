#include <print>
#include <string>

#include "decode.hpp"
#include "encode.hpp"
#include "utility.hpp"

int main(int argc, char *argv[]) {
    std::vector<std::string> args{ argv, argv + argc };
    if (args.size() > 1) {
        std::println("Only one argument is allowed.");
        return -1;
    }
    if (args.empty()) {
        std::println("No input provided.");
        return -1;
    }
    auto config = Settings::load("settings.txt");

    std::string original = args[0];
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