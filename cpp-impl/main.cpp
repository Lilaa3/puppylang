#include <exception>
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

#include "decode.hpp"
#include "encode.hpp"
#include "utility.hpp"

enum class Mode { Encode, Decode, Help };

struct CliOptions {
    Mode mode = Mode::Help;
    std::string input = "-";               // raw text (or "-" for stdin)
    std::optional<std::string> input_file; // if set, read from this file
    std::string output = "";
    std::string settings = "settings.txt";
    EncodeOptions enc_opts;
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

void add_common_args(argparse::ArgumentParser &parser, CliOptions &opts) {
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
        .help("Settings file (default: settings.txt)")
        .default_value(std::string("settings.txt"));
}

} // namespace

std::optional<CliOptions> parse_cli(int argc, char *argv[]) {
    argparse::ArgumentParser program("puplang-cli");
    program.add_description(
        "Encode text to puplang or decode puplang back to text.");

    argparse::ArgumentParser encode_parser("encode");
    argparse::ArgumentParser decode_parser("decode");

    CliOptions enc_opts;
    CliOptions dec_opts;

    add_common_args(encode_parser, enc_opts);
    add_common_args(decode_parser, dec_opts);

    encode_parser.add_argument("--seed")
        .help("RNG seed for encoding (default: 64)")
        .default_value(uint64_t{64})
        .scan<'u', uint64_t>();
    encode_parser.add_argument("--howl-chance")
        .help("Initial howl probability 0-1 (default: 0.2)")
        .default_value(0.2)
        .scan<'g', double>();
    encode_parser.add_argument("--howl-decay")
        .help("Howl chance multiplier after each howl (default: 0.5)")
        .default_value(0.5)
        .scan<'g', double>();
    encode_parser.add_argument("--short-limit")
        .help("Extra letters counted as a tiny change (default: 2)")
        .default_value(2)
        .scan<'i', int>();
    encode_parser.add_argument("--min-howl")
        .help("Floor for howl chance so long forms stay reachable (default: 0.1)")
        .default_value(0.1)
        .scan<'g', double>();
    encode_parser.add_argument("--shape")
        .help("Per-letter decay; closer to 1 = flatter short variation "
              "(default: 0.9)")
        .default_value(0.9)
        .scan<'g', double>();

    program.add_subparser(encode_parser);
    program.add_subparser(decode_parser);

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception &e) {
        std::println(stderr, "{}", e.what());
        return std::nullopt;
    }

    if (!program.is_subcommand_used("encode") &&
        !program.is_subcommand_used("decode")) {
        // No subcommand given: show help and exit.
        std::cout << program;
        return CliOptions{}; // mode defaults to Help
    }

    argparse::ArgumentParser *used;
    CliOptions opts;
    if (program.is_subcommand_used("encode")) {
        used = &encode_parser;
        opts.mode = Mode::Encode;
        opts.enc_opts.seed = encode_parser.get<uint64_t>("--seed");
        opts.enc_opts.howl_chance =
            encode_parser.get<double>("--howl-chance");
        opts.enc_opts.howl_decay = encode_parser.get<double>("--howl-decay");
        opts.enc_opts.howl.short_limit =
            encode_parser.get<int>("--short-limit");
        opts.enc_opts.howl.min_howl = encode_parser.get<double>("--min-howl");
        opts.enc_opts.howl.shape = encode_parser.get<double>("--shape");
    } else {
        used = &decode_parser;
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

    auto cfg = Settings::load(opts->settings);
    if (!cfg) {
        std::println(
            stderr, "Failed to load settings file: {}", opts->settings);
        return 1;
    }

    // Resolve the input source
    std::ifstream in_file;
    std::istringstream in_arg;
    std::istream *in = &std::cin;
    if (opts->input_file) {
        in_file.open(*opts->input_file);
        if (!in_file) {
            std::println(
                stderr, "Failed to open input file: {}", *opts->input_file);
            return 1;
        }
        in = &in_file;
    } else if (opts->input != "-") {
        in_arg.str(opts->input);
        in = &in_arg;
    }

    // Resolve the output sink
    std::ofstream out_file;
    std::ostream *out = &std::cout;
    std::string output_path = opts->output;
    if (output_path.empty() && opts->input_file) {
        output_path = derive_output_name(*opts->input_file,
                                         opts->mode == Mode::Encode ? "encode"
                                                                    : "decode");
    }
    if (!output_path.empty() && output_path != "-") {
        out_file.open(output_path);
        if (!out_file) {
            std::println(stderr, "Failed to open output file: {}", output_path);
            return 1;
        }
        out = &out_file;
    }

    auto result = (opts->mode == Mode::Encode)
                      ? encode_stream(*in, *out, *cfg, opts->enc_opts)
                      : decode_stream(*in, *out, *cfg);
    if (result)
        *out << '\n';
    if (!result) {
        std::println(stderr,
                     "{} error: {}",
                     opts->mode == Mode::Encode ? "Encode" : "Decode",
                     static_cast<int>(result.error()));
        return 1;
    }

    return 0;
}
