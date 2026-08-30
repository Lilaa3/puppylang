#pragma once

#include <string>
#include <string_view>

#include "encode.hpp"

/// Renders a 7-bit value as its c repr, for example "\n"
auto ascii_repr(uint8_t c) -> std::string;

/// Generates a human-readable listing of every possible sound.
auto generate_sound_table(const Settings &cfg) -> std::string;
