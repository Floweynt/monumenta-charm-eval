#include "charm.h"
#include "color.h"
#include "eval.h"
#include "src/charm_data.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <ostream>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace mtce
{
    inline static constexpr std::size_t ENCODED_CHARM_STAT_BITS = 26;
    inline static constexpr int32_t ENCODED_CHARM_STAT_SCALE = (1 << ENCODED_CHARM_STAT_BITS) - 1;

#ifdef IS_LANGUAGE_SERVER
    inline static constexpr std::size_t DEFAULT_VECTOR_BLOCK = 512 / 8;
    // tunables
    inline static constexpr std::size_t CHARM_STRUCT_ALGIN = DEFAULT_VECTOR_BLOCK;
    inline static constexpr std::size_t TABLE_SIZE_ALIGN = ABILITY_COUNT;
#else
    inline static constexpr std::size_t DEFAULT_VECTOR_BLOCK = 512 / 8;
    // tunables
    inline static constexpr std::size_t CHARM_STRUCT_ALGIN = DEFAULT_VECTOR_BLOCK;
    inline static constexpr std::size_t TABLE_SIZE_ALIGN = DEFAULT_VECTOR_BLOCK / sizeof(int32_t);
#endif

    // fast encoding of a charm
    // everything is encoded as int32_t for performance reasons (vectorization potential)
    // we take 26 bits of precision from raw floating point values (this is technically too many! we only need 23 for 32-bit floats)
    // alignment for vectorization reasons
    template <std::size_t N>
    struct alignas(CHARM_STRUCT_ALGIN) table_t
    {
        std::array<int32_t, N> stat_table;
    };

    struct alignas(CHARM_STRUCT_ALGIN) charm_set_buffer
    {
        std::array<charm_id, 8> data; // there's only 7 entries but we use eight because it might perform better for vectorization
    };

    template <std::size_t N>
    using charm_buffer = std::vector<table_t<N>>;

    using cp_buffer = std::vector<uint32_t>;

    namespace
    {
        template <std::size_t N>
        auto eval_charms(const charm_buffer<N>& charms, const cp_buffer& cp_table, uint32_t max_cp, table_t<N> weights, size_t n_threads)
            -> std::vector<charm_id>;

        template <std::size_t N>
        class charm_eval_helper
        {
            table_t<N> weights{};
            std::span<const table_t<N>> charms;
            std::span<const uint32_t> cp_table;
            uint32_t max_charm_power{};

            int64_t max_utility_value{};
            charm_set_buffer best_charm_set{};

            template <std::size_t M>
            friend auto eval_charms(const charm_buffer<M>& charms, const cp_buffer& cp_table, uint32_t max_cp, table_t<M> weights, size_t n_threads)
                -> std::vector<charm_id>;

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

                        eval_charm<CharmsLeft - 1>(new_charm_set_stats, curr_cp + cp_table[i], new_charm_set, i + 1);
                    }
                }
            }
        };

        constexpr auto prepare_result(const charm_set_buffer& charm_set)
        {
            std::vector<charm_id> ch_res;
            ch_res.reserve(CHARM_COUNT_MAX);

            for (const auto charm : charm_set.data)
            {
                if (charm != MISSING_ID)
                {
                    ch_res.push_back(charm);
                }
            }

            return ch_res;
        }

        template <std::size_t N>
        auto eval_charms(const charm_buffer<N>& charms, const cp_buffer& cp_table, uint32_t max_cp, table_t<N> weights, size_t n_threads)
            -> std::vector<charm_id>
        {
            if (n_threads <= 1)
            {
                charm_eval_helper<N> helper;
                helper.charms = charms;
                helper.cp_table = cp_table;
                helper.max_charm_power = max_cp;
                helper.weights = weights;
                helper.max_utility_value = -1;
                table_t<N> stats_buffer{};
                charm_set_buffer id_buffer{};
                std::ranges::fill(id_buffer.data, MISSING_ID);

                helper.template eval_charm<CHARM_COUNT_MAX>(stats_buffer, 0, id_buffer, 0);
                return prepare_result(helper.best_charm_set);
            }
            else // NOLINT(readability-else-after-return)
            {
                // TODO: omp parallelize over std::thread creation
                std::vector<charm_eval_helper<N>> results;
                std::vector<std::thread> workers;
                std::atomic<charm_id> jobs = 0;

                for (size_t i = 0; i < n_threads; i++)
                {
                    charm_eval_helper<N> helper;
                    helper.charms = charms;
                    helper.cp_table = cp_table;
                    helper.max_charm_power = max_cp;
                    helper.weights = weights;
                    helper.max_utility_value = -1;
                    results.push_back(std::move(helper));
                }

                for (size_t i = 0; i < n_threads; i++)
                {
                    std::thread worker([&helper = results[i], &jobs, &charms, &cp_table]() {
                        while (true)
                        {
                            auto curr_job = jobs.fetch_add(1);

                            if (curr_job >= charms.size())
                            {
                                return;
                            }

                            table_t<N> stats_buffer = charms[curr_job];
                            charm_set_buffer id_buffer{};
                            std::ranges::fill(id_buffer.data, MISSING_ID);
                            id_buffer.data[0] = curr_job;

                            helper.template eval_charm<CHARM_COUNT_MAX - 1>(stats_buffer, cp_table[curr_job], id_buffer, curr_job + 1);
                        }
                    });
                    workers.emplace_back(std::move(worker));
                }

                for (size_t i = 0; i < n_threads; i++)
                {
                    workers[i].join();
                }

                int64_t max_utility_value{};
                charm_set_buffer best_charm_set{};

                for (size_t i = 0; i < n_threads; i++)
                {
                    if (results[i].max_utility_value > max_utility_value)
                    {
                        max_utility_value = results[i].max_utility_value;
                        best_charm_set = results[i].best_charm_set;
                    }
                }

                return prepare_result(best_charm_set);
            }
        }

        struct charm_compact_dyn
        {
            uint32_t original_index{};
            uint32_t charm_power{};
            std::vector<int32_t> stat_table;
        };

        template <std::size_t N>
        constexpr auto eval_charms_dyn(
            const std::vector<charm_compact_dyn>& charms, uint32_t max_charm_power, const std::vector<int32_t>& weights, size_t n_threads
        )
        {
            charm_buffer<N> _charms;
            cp_buffer cp_table;
            table_t<N> _weights{};

            _charms.reserve(charms.size());
            cp_table.reserve(charms.size());

            for (const auto& charm : charms)
            {
                table_t<N> storage{};
                cp_table.push_back(charm.charm_power);
                std::copy(charm.stat_table.begin(), charm.stat_table.end(), storage.stat_table.begin());
                _charms.emplace_back(std::move(storage));
            }

            std::copy(weights.begin(), weights.end(), _weights.stat_table.begin());

            return eval_charms<N>(_charms, cp_table, max_charm_power, _weights, n_threads);
        }

        using eval_charm_delegate_t = std::vector<charm_id> (*)(
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

        auto prepare(const std::vector<charm>& charms, const charm_weights& weights) -> eval_prep_result
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

    auto evaluate_naive(const std::vector<charm>& charms, uint32_t max_cp, const charm_weights& weights, size_t threads) -> std::vector<charm_id>
    {
        auto [important_abilities, compact_dyn_charms, compact_weights] = prepare(charms, weights);

        std::println(std::cout, gray("prune - only considering abilities:"));
        for (const auto ability : important_abilities)
        {
            std::println(std::cout, gray("  - {}"), EFFECT_NAMES.at(ability));
        }

        std::println(std::cout, gray("prune - only considering charms:"));
        for (const auto& charm : compact_dyn_charms)
        {
            std::println(std::cout, gray("  - {}"), charms.at(charm.original_index).name);
        }

        if (important_abilities.empty())
        {
            return {};
        }

        return table_helper::TABLE[important_abilities.size()](compact_dyn_charms, max_cp, compact_weights, threads);
    }
} // namespace mtce
