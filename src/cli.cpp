#include "cli.h"
#include "build.h"
#include "src/charm.h"
#include "src/sv_manip.h"
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace mtce
{
    namespace
    {
        void print_help(std::string_view prog, std::ostream& out)
        {
            std::println(out, "usage: {} <args>", prog);
            std::println(out, "cli flags:");
            std::println(out, "--help, -h               print this message");
            std::println(out, "--version                print version");
            std::println(out, "--config, -c [file]      specify configuration file");
            std::println(out, "--in, -i [file]          specify input file");
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

        constexpr auto create_charm_from_data(
            uint8_t charm_power, uint32_t color, std::string name, std::span<const size_t> index_map, std::string_view values, size_t line_no
        ) -> charm
        {
            charm instance{
                .charm_power = charm_power,
                .color = color,
                .name = std::move(name),
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
        cli_options args;
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
                args.config = parse_arg_generic(arg, i, argc, argv);
            }
            else if (arg == "--in" || arg == "-i")
            {
                args.charm_input_file = parse_arg_generic(arg, i, argc, argv);
            }
        }

        check(!args.config.empty(), "missing --config (config), try --help?");
        check(!args.charm_input_file.empty(), "missing --in (charm input file), try --help?");
        return args;
    }

    auto read_config(const std::string& path) -> config
    {
        std::ifstream ifs(path);

        enum : uint8_t
        {
            GLOBAL,
            WEIGHTS
        } section = GLOBAL;

        config cfg{
            .max_cp = 0,
        };

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
                        cfg.max_cp = read_cfg_int(line_no, value);
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
                    cfg.ability_weights[std::string(key)] = read_cfg_int(line_no, value);
                }
                break;
                }
            }
        }

        return cfg;
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

            res.push_back(create_charm_from_data(charm_power, COLOR_BY_RARITY.at(rarity), std::string(name), effect_ids, parts[4], line_no));

            /*
            if (parts.size() == 6)
            {
                check(rarity < COLOR_BY_RARITY.size() - 1, "bad charm data on line {}: illegal rarity {}", line_no, rarity);
                res.push_back(
                    create_charm_from_data(charm_power, COLOR_BY_RARITY.at(rarity + 1), std::string(name) + " (u)", effect_ids, parts[5], line_no)
                );
            }*/
        }

        return res;
    }
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
} // namespace mtce
