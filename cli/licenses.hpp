#pragma once

#include <array>
#include <string>
#include <string_view>

namespace licenses {

struct LicenseMeta {
    std::string_view id;
    std::string_view name;
    std::string_view repo_url;
    std::string_view license_name;
    std::string_view license_url;
    std::string_view copyright;
    std::string_view author_url;
    std::string_view description;
    std::string_view text;
    bool is_main_project = false;

    std::string format_header() const;
};

uint get_terminal_width();
std::string encode_into_cli(std::string_view sv);

extern const std::array<LicenseMeta, 3> all_licenses;

const LicenseMeta *find_license(std::string_view id);
std::string build_credits_summary();
void print_license(std::ostream &out, std::string_view target_id);
} // namespace licenses