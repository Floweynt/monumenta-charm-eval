#include "common/aligned_eval.h"
#include "common/charm.h"
#include "common/eval.h"
#include "common/gen/charm_data.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace mtce
{
    using namespace vec;

    namespace
    {
        using cp_buffer = std::vector<uint32_t>;

        using offset_buffer = std::vector<uint32_t>;

        using internal_result_t = std::pair<int64_t, charm_set_buffer>;

        template <std::size_t N>
        struct eval_config_static
        {
            const charm_buffer<N>& charms;
            const cp_buffer& cp_table;
            const offset_buffer& offset_table;
            uint32_t max_cp;
            table_t<N> weights;
            size_t n_threads;
        };

        template <std::size_t N>
        struct charm_eval_helper
        {
            table_t<N> weights;
            std::span<const table_t<N>> charms;
            std::span<const uint32_t> cp_table;
            std::span<const uint32_t> offset_table;
            uint32_t max_charm_power;

            int64_t max_utility_value = -1;
            charm_set_buffer best_charm_set;

            charm_eval_helper(const eval_config_static<N>& cfg)
                : weights(cfg.weights), charms(cfg.charms), cp_table(cfg.cp_table), offset_table(cfg.offset_table), max_charm_power(cfg.max_cp)
            {
            }

            [[gnu::always_inline]] constexpr auto eval_stats(const table_t<N>& stats) -> int64_t
            {
                // this gets optimized into really good unrolled vectorized stuff
                int64_t result = 0;
                for (std::size_t i = 0; i < N; i++)
                {
                    // bounds check is trivially optimized
                    result += (int64_t)std::min(stats.stat_table.at(i), ENCODED_CHARM_STAT_SCALE) * weights.stat_table.at(i);
                }

                return result;
            }

            template <int CharmsLeft>
            [[gnu::always_inline]] constexpr auto eval_charm(const table_t<N>& curr, uint32_t curr_cp, const charm_set_buffer& set, size_t prev_idx)
            {
                if (curr_cp > max_charm_power)
                {
                    return;
                }

                // always check utility
                int64_t utility = eval_stats(curr);

                // this branch is really, really, really slow - but we have no way of making it faster :P
                // even using branchfree is slower! ~0.8 vs ~0.86
                if (utility > max_utility_value) [[unlikely]]
                {
                    max_utility_value = utility;
                    best_charm_set = set;
                }

                if constexpr (CharmsLeft > 0)
                {
                    for (size_t i = prev_idx; i < charms.size(); i++)
                    {
                        charm_set_buffer new_charm_set = set;
                        new_charm_set.data[CHARM_COUNT_MAX - CharmsLeft] = i;
                        table_t<N> new_charm_set_stats = curr;
                        table_t<N> charm = charms[i];

                        for (std::size_t j = 0; j < N; j++)
                        {
                            // bounds check is trivially optimized out
                            new_charm_set_stats.stat_table.at(j) += charm.stat_table.at(j);
                        }

                        eval_charm<CharmsLeft - 1>(new_charm_set_stats, curr_cp + cp_table[i], new_charm_set, i + offset_table[i]);
                    }
                }
            }
        };

        template <std::size_t N>
        auto eval_charms_serial(const eval_config_static<N>& cfg) -> internal_result_t
        {
            charm_eval_helper<N> helper(cfg);
            table_t<N> stats_buffer{};

            charm_set_buffer id_buffer;
            helper.template eval_charm<CHARM_COUNT_MAX>(stats_buffer, 0, id_buffer, 0);
            return {helper.max_utility_value, helper.best_charm_set};
        }

        template <std::size_t N>
        auto eval_charms_parallel(const eval_config_static<N>& cfg) -> internal_result_t
        {
            std::vector<charm_eval_helper<N>> results;
            std::vector<std::thread> workers;
            std::atomic<charm_id> jobs = 0;

            results.reserve(cfg.n_threads);
            for (size_t i = 0; i < cfg.n_threads; i++)
            {
                results.emplace_back(charm_eval_helper<N>(cfg));
            }

            for (size_t i = 0; i < cfg.n_threads; i++)
            {
                std::thread worker([&helper = results[i], &jobs, &charms = cfg.charms, &cp_table = cfg.cp_table, &offs = cfg.offset_table]() {
                    while (true)
                    {
                        auto curr_job = jobs.fetch_add(1);

                        if (curr_job >= charms.size())
                        {
                            return;
                        }

                        table_t<N> stats_buffer = charms[curr_job];
                        charm_set_buffer id_buffer;
                        id_buffer.data[0] = curr_job;

                        helper.template eval_charm<CHARM_COUNT_MAX - 1>(stats_buffer, cp_table[curr_job], id_buffer, curr_job + offs[curr_job]);
                    }
                });
                workers.emplace_back(std::move(worker));
            }

            for (size_t i = 0; i < cfg.n_threads; i++)
            {
                workers[i].join();
            }

            int64_t max_utility_value{};
            charm_set_buffer best_charm_set{};

            for (size_t i = 0; i < cfg.n_threads; i++)
            {
                if (results[i].max_utility_value > max_utility_value)
                {
                    max_utility_value = results[i].max_utility_value;
                    best_charm_set = results[i].best_charm_set;
                }
            }

            return {max_utility_value, best_charm_set};
        }

        template <std::size_t N>
        auto eval_charms(const eval_config_static<N>& cfg) -> internal_result_t
        {
            return cfg.n_threads <= 1 ? eval_charms_serial(cfg) : eval_charms_parallel(cfg);
        }

        struct charm_compact_dyn
        {
            charm_id original_index{};
            uint32_t charm_power{};
            bool has_upgrade{};
            std::vector<int32_t> stat_table;
        };

        // a bridge between the dynamic "input" space and the specialized "evaluation" space
        template <std::size_t N>
        constexpr auto eval_charms_dyn(
            const std::vector<charm_compact_dyn>& charms, uint32_t max_charm_power, const std::vector<int32_t>& weights, size_t n_threads
        )
        {
            charm_buffer<N> _charms;
            cp_buffer cp_table;
            offset_buffer offset_table;
            table_t<N> _weights{};

            _charms.reserve(charms.size());
            cp_table.reserve(charms.size());
            offset_table.reserve(charms.size());

            for (const auto& charm : charms)
            {
                table_t<N> storage{};
                cp_table.push_back(charm.charm_power);
                offset_table.push_back(charm.has_upgrade ? 2 : 1);
                std::copy(charm.stat_table.begin(), charm.stat_table.end(), storage.stat_table.begin());
                _charms.emplace_back(std::move(storage));
            }

            std::copy(weights.begin(), weights.end(), _weights.stat_table.begin());

            return eval_charms<N>(eval_config_static<N>{
                .charms = _charms,
                .cp_table = cp_table,
                .offset_table = offset_table,
                .max_cp = max_charm_power,
                .weights = _weights,
                .n_threads = n_threads,
            });
        }

        using eval_charm_delegate_t = internal_result_t (*)(
            const std::vector<charm_compact_dyn>& charms, uint32_t max_charm_power, const std::vector<int32_t>& weights, size_t n_threads
        );

        template <std::size_t N>
        inline constexpr auto TABLE_SIZE_FOR = ((N + TABLE_SIZE_ALIGN - 1) / TABLE_SIZE_ALIGN) * TABLE_SIZE_ALIGN;

        template <typename T>
        struct _table_helper
        {
        };

        template <std::size_t... Counts>
        struct _table_helper<std::index_sequence<Counts...>>
        {
            static constexpr eval_charm_delegate_t TABLE[] = {eval_charms_dyn<TABLE_SIZE_FOR<Counts>>...};
        };

        using table_helper = _table_helper<std::make_index_sequence<ABILITY_COUNT + 1>>;

        struct eval_prep_result
        {
            std::vector<size_t> important_abilities;
            std::vector<charm_compact_dyn> compact_dyn_charms;
            std::vector<int32_t> compact_weights;
        };

        // pre-processing step for charm data
        auto prepare_charm_data(const std::vector<charm>& charms, const charm_weights& weights) -> eval_prep_result
        {
            std::vector<size_t> important_abilities;

            for (std::size_t ability_id = 0; ability_id < ABILITY_COUNT; ability_id++)
            {
                if (weights.at(ability_id) == 0)
                {
                    continue;
                }

                bool has_nonzero = false;
                for (const auto& charm : charms)
                {
                    if (charm.charm_data.at(ability_id) != 0)
                    {
                        has_nonzero = true;
                        break;
                    }
                }

                if (!has_nonzero)
                {
                    continue;
                }

                important_abilities.push_back(ability_id);
            }

            std::vector<charm_compact_dyn> compact_dyn_charms;
            compact_dyn_charms.reserve(charms.size());

            for (size_t i = 0; i < charms.size(); i++)
            {
                const auto& charm = charms[i];
                charm_compact_dyn charm_compact;
                charm_compact.original_index = i;
                charm_compact.charm_power = charm.charm_power;
                charm_compact.has_upgrade = charm.has_upgrade;
                charm_compact.stat_table.reserve(important_abilities.size());

                bool has_nonzero = false;

                for (const auto ability_id : important_abilities)
                {
                    const auto rel_value = charm.charm_data.at(ability_id) / EFFECT_CAPS.at(ability_id);
                    has_nonzero |= (charm.charm_data.at(ability_id) != 0);
                    charm_compact.stat_table.push_back((int32_t)(rel_value * ENCODED_CHARM_STAT_SCALE));
                }

                if (has_nonzero)
                {
                    compact_dyn_charms.emplace_back(std::move(charm_compact));
                }
            }

            std::vector<int32_t> compact_weights;
            compact_weights.reserve(important_abilities.size());
            for (const auto ability_id : important_abilities)
            {
                compact_weights.push_back(weights.at(ability_id));
            }

            return {
                .important_abilities = std::move(important_abilities),
                .compact_dyn_charms = std::move(compact_dyn_charms),
                .compact_weights = std::move(compact_weights),
            };
        }
    } // namespace

    auto evaluate_naive(const eval_config& config, const naive_tracing_config& trace) -> eval_result
    {
        auto [important_abilities, compact_dyn_charms, compact_weights] = prepare_charm_data(config.charms, config.weights);

        if (trace.trace_prune)
        {
            std::vector<std::string_view> abilities;
            std::vector<std::string_view> charms;

            abilities.reserve(important_abilities.size());
            charms.reserve(compact_dyn_charms.size());

            for (auto important_ability : important_abilities)
            {
                abilities.push_back(EFFECT_NAMES.at(important_ability));
            }

            for (const auto& important_charm : compact_dyn_charms)
            {
                charms.push_back(config.charms.at(important_charm.original_index).name);
            }

            trace.trace_prune(abilities, charms);
        }

        // dynamically select the implementation based on the amount of abilities
        auto [utility, charm_set] =
            table_helper::TABLE[important_abilities.size()](compact_dyn_charms, config.max_cp, compact_weights, config.threads);

        // translate static result -> dynamic result
        std::vector<charm_id> ch_res;
        ch_res.reserve(CHARM_COUNT_MAX);

        for (const auto charm : charm_set.data)
        {
            if (charm != MISSING_ID)
            {
                ch_res.push_back(compact_dyn_charms[charm].original_index);
            }
        }

        return {
            .utility_value = utility,
            .charms = ch_res,
        };
    }
} // namespace mtce
