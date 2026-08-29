#include "licenses.hpp"

#include <cassert>
#include <format>
#include <ostream>

#if __has_include(<sys/ioctl.h>) && __has_include(<unistd.h>)
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace licenses {

using namespace std::string_view_literals;

uint get_terminal_width() {
#if defined(TIOCGWINSZ)
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        return w.ws_col;
    }
#endif
    return 80;
}

std::string encode_into_cli(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());

    while (!sv.empty()) {
        if (sv.starts_with('<')) {
            if (auto gt = sv.find('>'); gt != std::string_view::npos) {
                auto tag_body = sv.substr(1, gt - 1);
                auto sep = tag_body.find_first_of("= ");
                auto name = tag_body.substr(0, sep);
                auto val = (sep != std::string_view::npos)
                               ? tag_body.substr(sep + 1)
                               : "";

                if (name == "hr") {
                    out.append(get_terminal_width(), '-');
                    sv.remove_prefix(gt + 1);
                    continue;
                } else if (name == "link") {
                    if (auto close = sv.find("</link>", gt + 1);
                        close != std::string_view::npos) {
                        auto text = sv.substr(gt + 1, close - (gt + 1));
                        out += std::format(
                            "\x1b]8;;{}\x07{}\x1b]8;;\x07", val, text);
                        sv.remove_prefix(close + "</link>"sv.size());
                        continue;
                    }
                }
            }
        }
        out.push_back(sv[0]);
        sv.remove_prefix(1);
    }

    return out;
}

// clang-format off
constexpr unsigned char puplang_data[] = {
    #embed @PUPLANG_EMBED_LINE@
};
constexpr unsigned char argparse_data[] = {
    #embed @ARGPARSE_EMBED_LINE@
};
constexpr unsigned char utfcpp_data[]  = {
    #embed @UTFCPP_EMBED_LINE@
};
// clang-format on

static inline std::string_view to_sv(const unsigned char *data,
                                     std::size_t size) {
    return std::string_view(reinterpret_cast<const char *>(data), size);
}

std::string LicenseMeta::format_header() const {
    return std::format(
        "<link={}>{}</link> | <link={}>{}</link> | <link={}>{}</link>",
        repo_url,
        name,
        license_url,
        license_name,
        author_url,
        copyright);
}

const std::array<LicenseMeta, 3> all_licenses{
    LicenseMeta{
        .id = "puplang",
        .name = "puplang",
        .repo_url = "https://github.com/Lilaa3/puppylang",
        .license_name = "MIT License",
        .license_url = "https://github.com/Lilaa3/puppylang/blob/main/LICENSE",
        .copyright = "© 2026 Liliana (Lilaa3)",
        .author_url = "https://github.com/Lilaa3",
        .description = "The project you are looking at right now!",
        .text = to_sv(puplang_data, sizeof(puplang_data)),
        .is_main_project = true,
    },
    LicenseMeta{
        .id = "argparse",
        .name = "argparse",
        .repo_url = "https://github.com/p-ranav/argparse",
        .license_name = "MIT License",
        .license_url =
            "https://github.com/p-ranav/argparse/blob/master/LICENSE",
        .copyright = "© 2018 Pranav Srinivas Kumar",
        .author_url = "https://github.com/p-ranav",
        .description = "Fantastic modern C++ command line argument parser.",
        .text = to_sv(argparse_data, sizeof(argparse_data)),
    },
    LicenseMeta{
        .id = "utfcpp",
        .name = "utfcpp",
        .repo_url = "https://github.com/nemtrif/utfcpp",
        .license_name = "Boost Software License 1.0",
        .license_url = "https://github.com/nemtrif/utfcpp/blob/master/LICENSE",
        .copyright = "© Nemanja Trifunovic",
        .author_url = "https://github.com/nemtrif",
        .description = "Library for working with UTF-8 strings.",
        .text = to_sv(utfcpp_data, sizeof(utfcpp_data)),
    },
};

const LicenseMeta *find_license(std::string_view id) {
    for (const auto &lic : all_licenses) {
        if (lic.id == id)
            return &lic;
    }
    return nullptr;
}

std::string build_credits_summary() {
    auto format_row = [](const LicenseMeta &meta,
                         size_t name_width = 8,
                         size_t license_width = 26) {
        std::string name_spaces(
            name_width > meta.name.size() ? name_width - meta.name.size() : 0,
            ' ');
        std::string license_spaces(license_width > meta.license_name.size()
                                       ? license_width -
                                             meta.license_name.size()
                                       : 0,
                                   ' ');

        return std::format(
            "<link={}>{}</link>{} | <link={}>{}</link>{} | <link={}>{}</link>",
            meta.repo_url,
            meta.name,
            name_spaces,
            meta.license_url,
            meta.license_name,
            license_spaces,
            meta.author_url,
            meta.copyright);
    };

    std::string out;
    out += "<hr>\nThe library and CLI:\n";
    for (const auto &lic : all_licenses) {
        if (lic.is_main_project) {
            out += format_row(lic) + "\n";
        }
    }

    out += "\n<hr>\nThe thirdparty libraries:\n";
    for (const auto &lic : all_licenses) {
        if (!lic.is_main_project) {
            out += format_row(lic) + "\n";
        }
    }

    out += "\n<hr>\n\nTo read the individual licenses, use their individual "
           "subcommands. E.g. `puplang licenses argparse`";
    return out;
}

void print_license(std::ostream &out, std::string_view target_id) {
    if (target_id.empty()) {
        out << licenses::encode_into_cli(licenses::build_credits_summary())
            << "\n\n";
        return;
    }

    const licenses::LicenseMeta *meta = licenses::find_license(target_id);
    if (!meta)
        assert(false);

    out << licenses::encode_into_cli("<hr>")
        << licenses::encode_into_cli(meta->format_header()) << "\n"
        << "> " << licenses::encode_into_cli(meta->description) << "\n"
        << licenses::encode_into_cli("<hr>")
        << licenses::encode_into_cli(meta->text) << "\n"
        << licenses::encode_into_cli("<hr>") << "\n\n";
}

} // namespace licenses