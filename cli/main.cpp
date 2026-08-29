#include <cassert>
#include <exception>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <print>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <argparse/argparse.hpp>

#ifdef PUPLANG_EMBED_LICENSES
#include "licenses.hpp"
#endif

#include "puplang.hpp"

enum class Mode {
    Encode,
    Decode,
    GenSounds,
#ifdef PUPLANG_EMBED_LICENSES
    Licenses,
#endif
    Help
};

struct CliOptions {
    Mode mode = Mode::Help;
    std::string input = "-";               // raw text (or "-" for stdin)
    std::optional<std::string> input_file; // if set, read from this file
    std::string output = "";
    std::string settings = "settings.txt";
    EncodeOptions enc_opts;
#ifdef PUPLANG_EMBED_LICENSES
    std::string license_target = "";
#endif
};

namespace {

// Folds a list of input positionals into the single string
auto join_input(const std::vector<std::string> &parts) -> std::string {
    if (parts.empty())
        return "-";
    std::string out;
    for (const auto &p : parts) {
        if (!out.empty())
            out += ' ';
        out += p;
    }
    return out;
}

void add_common_args(argparse::ArgumentParser &parser) {
    parser.add_argument("input")
        .help("Raw text to process (default: stdin, use '-')")
        .default_value(std::vector<std::string>{})
        .nargs(argparse::nargs_pattern::any);

    parser.add_argument("-i", "--input")
        .help("Read input from a file instead of the positional argument")
        .default_value(std::string(""));

    parser.add_argument("-o", "--output")
        .help("Output file (default: auto-derive for files, stdout otherwise)")
        .default_value(std::string(""));

    parser.add_argument("-s", "--settings")
        .help("Settings file (default: built-in)")
        .default_value(std::string(""));
}

} // namespace

std::optional<CliOptions> parse_cli(int argc, char *argv[]) {
    argparse::ArgumentParser program(
        "puplang-cli", "1.0", argparse::default_arguments::help);
    program.add_description(
        "Encode text to puplang or decode puplang back to text.");

    argparse::ArgumentParser encode_command(
        "encode", "1.0", argparse::default_arguments::help);
    argparse::ArgumentParser decode_command(
        "decode", "1.0", argparse::default_arguments::help);
    argparse::ArgumentParser sounds_command(
        "sounds", "1.0", argparse::default_arguments::help);

    add_common_args(encode_command);
    add_common_args(decode_command);

    sounds_command.add_argument("-s", "--settings")
        .help("Settings file (default: built-in)")
        .default_value(std::string(""));
    sounds_command.add_argument("-o", "--output")
        .help("Output file (default: stdout)")
        .default_value(std::string(""));

    encode_command.add_argument("--seed")
        .help("RNG seed for encoding (default: 64)")
        .default_value((uint64_t)64)
        .scan<'u', uint64_t>();
    encode_command.add_argument("--free-extension-limit")
        .help("Extra letters counted as a penalty-less change (default: 0)")
        .default_value(0u)
        .scan<'u', uint>();
    encode_command.add_argument("--max-short-extension")
        .help("Extra letters that don't count as a howl. Past these, the "
              "chance gets very unlikely, and impossible if howls are "
              "disabled. (default: 2)")
        .default_value(2u)
        .scan<'u', uint>();
    encode_command.add_argument("--length-decay-rate")
        .help("Past the free extension limit, the chance of each extra letter "
              "gets decreased exponentially by this value."
              "(default: 0.4)")
        .default_value(0.4)
        .scan<'g', double>();
    encode_command.add_argument("--uppercase-weight")
        .help("Weight of uppercase letters. Less is less common. Titled words "
              "are only affected by half of this value. (default: 1.0)")
        .default_value(1.0)
        .scan<'g', double>();

    encode_command.add_argument("--howl-enabled")
        .help("Enable howls. A howl is a sound past the free extension limit "
              "thats made less uncommon. (default: true)")
        .default_value(true)
        .implicit_value(true);

    encode_command.add_argument("--initial-howl-chance")
        .help("Initial howl probability 0-1 (default: 0.2)")
        .default_value(0.2)
        .scan<'g', double>();
    encode_command.add_argument("--howl-decay")
        .help("Howl chance decay multiplier after each howl (default: 0.5)")
        .default_value(0.5)
        .scan<'g', double>();
    encode_command.add_argument("--min-howl")
        .help("Floor for howl chance (default: 0.1)")
        .default_value(0.1)
        .scan<'g', double>();

    program.add_subparser(encode_command);
    program.add_subparser(decode_command);
    program.add_subparser(sounds_command);

#ifdef PUPLANG_EMBED_LICENSES
    argparse::ArgumentParser licenses_command(
        "licenses", "1.0", argparse::default_arguments::help);

    std::vector<std::unique_ptr<argparse::ArgumentParser>> license_subparsers;
    license_subparsers.reserve(licenses::all_licenses.size());

    for (const auto &lic : licenses::all_licenses) {
        auto sub = std::make_unique<argparse::ArgumentParser>(
            std::string(lic.id), "1.0", argparse::default_arguments::help);
        licenses_command.add_subparser(*sub);
        license_subparsers.push_back(std::move(sub));
    }

    program.add_subparser(licenses_command);
#endif

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception &e) {
        std::println(stderr, "{}", e.what());
        return std::nullopt;
    }

    if (!program.is_subcommand_used("encode") &&
        !program.is_subcommand_used("decode") &&
        !program.is_subcommand_used("sounds") &&
        !program.is_subcommand_used("licenses")) {
        // No subcommand given: show help and exit.
        std::cout << program;
        return CliOptions{}; // mode defaults to Help
    }

    argparse::ArgumentParser *used;
    CliOptions opts;
    if (program.is_subcommand_used("encode")) {
        used = &encode_command;
        opts.mode = Mode::Encode;
        opts.enc_opts.seed = encode_command.get<uint64_t>("--seed");
        opts.enc_opts.free_extension_limit =
            encode_command.get<uint>("--free-extension-limit");
        opts.enc_opts.max_short_extension =
            encode_command.get<uint>("--max-short-extension");
        opts.enc_opts.length_decay_rate =
            encode_command.get<double>("--length-decay-rate");
        opts.enc_opts.uppercase_weight =
            encode_command.get<double>("--uppercase-weight");
        opts.enc_opts.howl_enabled = encode_command.get<bool>("--howl-enabled");
        opts.enc_opts.initial_howl_chance =
            encode_command.get<double>("--initial-howl-chance");
        opts.enc_opts.howl_decay = encode_command.get<double>("--howl-decay");
        opts.enc_opts.min_howl = encode_command.get<double>("--min-howl");
    } else if (program.is_subcommand_used("sounds")) {
        used = &sounds_command;
        opts.mode = Mode::GenSounds;
        opts.output = sounds_command.get<std::string>("--output");
        opts.settings = sounds_command.get<std::string>("--settings");
        return opts;
    }
#ifdef PUPLANG_EMBED_LICENSES
    else if (program.is_subcommand_used("licenses")) {
        opts.mode = Mode::Licenses;
        opts.settings = "";
        opts.license_target = "";

        for (const auto &lic : licenses::all_licenses) {
            if (licenses_command.is_subcommand_used(std::string(lic.id))) {
                opts.license_target = lic.id;
                break;
            }
        }
        return opts;
    }
#endif
    else {
        used = &decode_command;
        opts.mode = Mode::Decode;
    }

    opts.input = join_input(used->get<std::vector<std::string>>("input"));
    auto in_file = used->get<std::string>("--input");
    if (!in_file.empty())
        opts.input_file = in_file;
    opts.output = used->get<std::string>("--output");
    opts.settings = used->get<std::string>("--settings");

    return opts;
}

std::string derive_output_name(const std::string &input,
                               std::string_view direction) {
    std::filesystem::path p(input);
    return (p.parent_path() / (p.stem().string() + "_" +
                               std::string(direction) + p.extension().string()))
        .string();
}

int main(int argc, char *argv[]) {
    auto opts = parse_cli(argc, argv);
    if (!opts)
        return 1;
    if (opts->mode == Mode::Help) {
        return 0;
    }

    std::optional<Settings> loaded_settings;
    if (!opts->settings.empty()) {
        loaded_settings = Settings::load(opts->settings);
        if (!loaded_settings) {
            std::println(
                stderr, "Failed to load settings file: {}", opts->settings);
            return 1;
        }
    }
    const Settings &cfg =
        loaded_settings ? *loaded_settings : puplang::default_settings;

#ifdef PUPLANG_EMBED_LICENSES
    if (opts->mode == Mode::Licenses) {
        licenses::print_license(std::cout, opts->license_target);
        return 0;
    }
#endif

    if (opts->mode == Mode::GenSounds) {
        if (!opts->output.empty() && opts->output != "-") {
            std::ofstream out_file;
            out_file.open(opts->output);
            if (!out_file) {
                std::println(
                    stderr, "i/o error: cannot open '{}'", opts->output);
                return 1;
            }
            out_file << puplang::generate_sound_table(cfg);
            out_file.close();
        } else {
            std::cout << puplang::generate_sound_table(cfg);
        }
        return 0;
    }

    // Resolve the output sink
    std::string output_path = opts->output;
    if (output_path.empty() && opts->input_file) {
        output_path = derive_output_name(*opts->input_file,
                                         opts->mode == Mode::Encode ? "encode"
                                                                    : "decode");
    }
    bool to_file = !output_path.empty() && output_path != "-";

    // File-to-file runs go through the library's streaming helper, which
    // stages the output and only renames it into place on success.
    std::expected<void, PuplangError> result =
        std::unexpected(PuplangError::empty_input);
    if (to_file && opts->input_file) {
        result =
            (opts->mode == Mode::Encode)
                ? puplang::encode_file(
                      *opts->input_file, output_path, cfg, opts->enc_opts)
                : puplang::decode_file(*opts->input_file, output_path, cfg);
    } else {
        // Resolve the input source
        std::ifstream in_file;
        std::istringstream in_arg;
        std::istream *in = &std::cin;
        if (opts->input_file) {
            in_file.open(*opts->input_file);
            if (!in_file) {
                std::println(
                    stderr, "i/o error: cannot open '{}'", *opts->input_file);
                return 1;
            }
            in = &in_file;
        } else if (opts->input != "-") {
            in_arg.str(opts->input);
            in = &in_arg;
        }

        std::ofstream out_file;
        std::ostream *out = &std::cout;
        if (to_file) {
            out_file.open(output_path);
            if (!out_file) {
                std::println(
                    stderr, "i/o error: cannot open '{}'", output_path);
                return 1;
            }
            out = &out_file;
        }

        result = (opts->mode == Mode::Encode)
                     ? encode_stream(*in, *out, cfg, opts->enc_opts)
                     : decode_stream(*in, *out, cfg);
        if (result)
            *out << '\n';
    }

    if (!result) {
        if (result.error() == PuplangError::io_error && to_file &&
            opts->input_file)
            std::println(
                stderr, "i/o error: {} -> {}", *opts->input_file, output_path);
        else
            std::println(stderr,
                         "{} error: {}",
                         opts->mode == Mode::Encode ? "Encode" : "Decode",
                         puplang::error_message(result.error()));
        return 1;
    }

    return 0;
}
