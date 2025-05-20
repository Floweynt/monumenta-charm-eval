#include "cli/cli.h"
#include "build_config.h"
#include "cli/sv_manip.h"
#include "common/charm.h"
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <fstream>
#include <iostream>
#include <ostream>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace mtce
{
    namespace
    {
        void print_help(std::string_view prog, std::ostream& out)
        {
            std::println(out, "usage: {} <args>", prog);
            std::println(out, "cli flags:");
            std::println(out, "  --help, -h               print this message");
            std::println(out, "  --version                print version");
            std::println(out, "  --config, -c [file]      specify configuration file");
            std::println(out, "  --in, -i [file]          specify input file");
            std::println(out, "  --algo [name]            specify the charm evaluation algorithm");
            std::println(out, "  --benchmark [n]          enables benchmarking mode, specifying number of times to run for data");
            std::println(out, "                           available options: naive");
            std::println(out, "algorithm specific flags:");
            std::println(out, "  --naive-threads [n]      [naive] specifies the number of threads to use");
            std::println(out, "  --naive-trace            [naive] enables tracing of pruning & other optimizations");
        }

        auto parse_arg_generic(std::string_view arg, int& idx, int argc, const char* const* argv) -> std::string_view
        {
            if (idx + 1 >= argc)
            {
                std::println(std::cerr, "{} must be followed by an argument", arg);
                std::exit(-1);
            }

            const auto* res = argv[idx + 1];
            idx++;
            return res;
        }

        template <typename T>
        auto parse_arg_typed(std::string_view arg, int& idx, int argc, const char* const* argv) -> T
        {
            auto arg_value = parse_arg_generic(arg, idx, argc, argv);
            T value{};
            auto [ptr, res] = std::from_chars(arg_value.begin(), arg_value.end(), value);

            if (ptr != arg_value.end() || res != std::errc{})
            {
                std::println(std::cerr, "failed to parse argument {}", arg_value);
                std::exit(-1);
            }

            return value;
        }

        constexpr auto read_cfg_int(size_t line_no, std::string_view value) -> int32_t
        {
            int32_t res = 0;
            auto [ptr, ec] = std::from_chars(value.begin(), value.end(), res);

            if (ptr != value.end() || ec != std::errc{})
            {
                std::println(std::cerr, "malformed config on line {}: failed to parse '{}' as int", line_no, value);
                std::exit(-1);
            }
            return res;
        }

        template <typename T>
        constexpr auto read_charm_val(size_t line_no, std::string_view data) -> T
        {
            T res{};
            auto [ptr, ec] = std::from_chars(data.begin(), data.end(), res);

            if (ptr != data.end() || ec != std::errc{})
            {
                std::println(std::cerr, "malformed charm data on line {}: failed to parse '{}'", line_no, data);
                std::exit(-1);
            }
            return res;
        }

        constexpr auto create_charm(
            uint8_t charm_power, uint32_t color, std::string name, bool has_upgrade, std::span<const size_t> index_map, std::string_view values,
            size_t line_no
        ) -> charm
        {
            charm instance{
                .charm_power = charm_power,
                .color = color,
                .name = std::move(name),
                .has_upgrade = has_upgrade,
            };

            auto entries = split_string_view(values, ':');

            for (size_t i = 0; i < index_map.size(); i++)
            {
                instance.charm_data.at(index_map[i]) = read_charm_val<float>(line_no, entries[i]);
            }

            return instance;
        }

        template <typename... Args>
        void check(bool predicate, const std::format_string<Args...>& fmt, Args&&... args)
        {
            if (!predicate)
            {
                std::println(std::cerr, fmt, std::forward<Args>(args)...);
                std::exit(-1);
            }
        }
    } // namespace

    auto parse_args(int argc, const char* const* argv) -> cli_options
    {
        static constexpr std::string_view CLI_WEIGHT_PREFIX = "--weight-";

        cli_options args;
        args.algo = naive_algo_flags{
            .threads = std::thread::hardware_concurrency(),
        };

        std::string_view prog_name = argv[0];

        for (int i = 1; i < argc; i++)
        {
            std::string_view arg = argv[i];

            if (arg == "--help" || arg == "-h")
            {
                print_help(prog_name, std::cout);
                std::exit(0);
            }
            if (arg == "--version")
            {
                std::println(std::cout, "{}: version " VERSION, prog_name);
                std::println(std::cout, "Copyright (C) 2024 Floweynt and contributors.");
                std::println(std::cout, "This is free software; see the source for copying conditions.  There is NO");
                std::println(std::cout, "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.");
                std::exit(0);
            }
            else if (arg == "--config" || arg == "-c")
            {
                auto config_file = parse_arg_generic(arg, i, argc, argv);
                read_config(std::string(config_file), args.config);
            }
            else if (arg == "--config-charm-power")
            {
                auto value = parse_arg_typed<uint8_t>(arg, i, argc, argv);
                check(value <= CHARM_POWER_MAX, "cp must be less than or equal to {}", CHARM_POWER_MAX);
                args.config.max_cp = value;
            }
            else if (arg.starts_with(CLI_WEIGHT_PREFIX))
            {
                auto name = std::string(arg.substr(CLI_WEIGHT_PREFIX.size()));
                check(NAME_TO_ID.contains(name), "unknown effect type");
                args.config.ability_weights[name] = parse_arg_typed<int32_t>(arg, i, argc, argv);
            }
            else if (arg == "--in" || arg == "-i")
            {
                args.charm_input_file = parse_arg_generic(arg, i, argc, argv);
            }
            else if (arg == "--benchmark")
            {
                check((args.benchmark = parse_arg_typed<uint32_t>(arg, i, argc, argv)) > 0, "--benchmark must be followed by number > 0");
            }
            else if (arg == "--bot-mode")
            {
                args.bot_mode = true;
            }
            else if (arg == "--algo")
            {
                auto algo_name = parse_arg_generic(arg, i, argc, argv);
                if (algo_name == "naive")
                {
                    args.algo = naive_algo_flags{
                        .threads = std::thread::hardware_concurrency(),
                    };
                }
                else
                {
                    check(false, "unknown charm evaluation algorithm: {}", algo_name);
                }
            }
            else if (arg == "--naive-threads" && std::holds_alternative<naive_algo_flags>(args.algo))
            {
                std::get<naive_algo_flags>(args.algo).threads = parse_arg_typed<uint16_t>(arg, i, argc, argv);
            }
            else if (arg == "--naive-trace" && std::holds_alternative<naive_algo_flags>(args.algo))
            {
                std::get<naive_algo_flags>(args.algo).enable_trace = true;
            }
            else
            {
                check(false, "unknown cli argument {}", arg);
            }
        }

        check(!args.charm_input_file.empty(), "missing --in (charm input file), try --help?");
        return args;
    }

    void read_config(const std::string& path, config& out)
    {
        std::ifstream ifs(path);

        enum : uint8_t
        {
            GLOBAL,
            WEIGHTS
        } section = GLOBAL;

        std::string raw_line;

        size_t line_no = 0;

        while (std::getline(ifs, raw_line))
        {
            line_no++;
            auto line = trim(std::string_view(raw_line).substr(0, raw_line.find('#')));

            if (line.empty())
            {
                continue;
            }

            // section
            if (line.front() == '[')
            {
                if (line == "[weights]")
                {
                    section = WEIGHTS;
                }
                else
                {
                    check(false, "malformed config on line {}: illegal section '{}'", line_no, line);
                }
            }
            else
            {
                // try and tokenize line
                auto index = line.find('=');
                check(index != std::string::npos, "malformed entry on line {}: '{}'", line_no, line);

                auto key = trim(line.substr(0, index));
                auto value = trim(line.substr(index + 1));

                switch (section)
                {
                case GLOBAL: {
                    if (key == "charm_power")
                    {
                        out.max_cp = read_cfg_int(line_no, value);
                        check(out.max_cp <= CHARM_POWER_MAX, "cp must be less than or equal to {}", CHARM_POWER_MAX);
                    }
                    else
                    {
                        check(false, "unknown key on line {}: '{}'", line_no, key);
                    }
                    break;
                }
                case WEIGHTS: {
                    auto iter = mtce::NAME_TO_ID.find(key);
                    check(iter != mtce::NAME_TO_ID.end(), "unknown charm effect on line {}: '{}'", line_no, key);
                    out.ability_weights[std::string(key)] = read_cfg_int(line_no, value);
                }
                break;
                }
            }
        }
    }

    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
    auto read_charms(const std::string& path) -> std::vector<charm>
    {
        constexpr static std::array COLOR_BY_RARITY = {0x9f929cU, 0x70bc6dU, 0x705ecaU, 0xcd5ecaU, 0xe49b20U};

        std::vector<charm> res;
        std::ifstream ifs(path);

        std::string curr_line;

        size_t line_no = 0;

        while (std::getline(ifs, curr_line))
        {
            line_no++;
            const auto parts = split_string_view(curr_line, ';');

            if (parts.size() != 5 && parts.size() != 6)
            {
                std::println(std::cerr, "bad charm data on line {}", line_no);
                std::exit(-1);
            }

            if (curr_line.empty())
            {
                continue;
            }

            auto rarity = read_charm_val<uint8_t>(line_no, parts[0]);

            check(rarity < COLOR_BY_RARITY.size(), "bad charm data on line {}: illegal rarity {}", line_no, rarity);

            auto name = parts[1];
            auto charm_power = read_charm_val<uint8_t>(line_no, parts[2]);
            auto effect_names = split_string_view(parts[3], ':');

            std::vector<size_t> effect_ids;
            effect_ids.reserve(effect_names.size());
            for (const auto effect : effect_names)
            {
                const auto iter = NAME_TO_ID.find(effect);
                check(iter != NAME_TO_ID.end(), "bad charm data on line {}: unknown effect {}", line_no, effect);
                effect_ids.push_back(iter->second);
            }

            res.push_back(create_charm(charm_power, COLOR_BY_RARITY.at(rarity), std::string(name), parts.size() == 6, effect_ids, parts[4], line_no));

            if (parts.size() == 6)
            {
                check(rarity < COLOR_BY_RARITY.size() - 1, "bad charm data on line {}: illegal rarity {}", line_no, rarity);
                res.push_back(
                    create_charm(charm_power, COLOR_BY_RARITY.at(rarity + 1), std::string(name) + " (u)", false, effect_ids, parts[5], line_no)
                );
            }
        }

        return res;
    }
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
} // namespace mtce
