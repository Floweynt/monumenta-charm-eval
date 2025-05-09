#pragma once

#include "build_config.h"
#include "common/charm.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// a bunch of utilities that are useful for vectorization
namespace mtce::vec
{
    // fast encoding of a charm
    // everything is encoded as int32_t for performance reasons (vectorization potential)
    // we take 26 bits of precision from raw floating point values (this is technically too many! we only need 23 for 32-bit floats)
    // alignment for vectorization reasons
    inline static constexpr std::size_t ENCODED_CHARM_STAT_BITS = 26;
    inline static constexpr int32_t ENCODED_CHARM_STAT_SCALE = (1 << ENCODED_CHARM_STAT_BITS) - 1;
    inline static constexpr std::size_t DEFAULT_VECTOR_BLOCK = VECTORIZED_BIT_SIZE / 8;

#ifdef IS_LANGUAGE_SERVER
    // tunables
    inline static constexpr std::size_t CHARM_STRUCT_ALGIN = DEFAULT_VECTOR_BLOCK;
    inline static constexpr std::size_t TABLE_SIZE_ALIGN = ABILITY_COUNT;
#else
    // tunables
    inline constexpr std::size_t CHARM_STRUCT_ALGIN = DEFAULT_VECTOR_BLOCK;
    inline constexpr std::size_t TABLE_SIZE_ALIGN = DEFAULT_VECTOR_BLOCK / sizeof(int32_t);
#endif

    template <std::size_t N>
    struct alignas(CHARM_STRUCT_ALGIN) table_t
    {
        std::array<int32_t, N> stat_table;
    };

    struct alignas(CHARM_STRUCT_ALGIN) charm_set_buffer
    {
        std::array<charm_id, 8> data{}; // there's only 7 entries but we use eight because it might perform better for vectorization

        constexpr charm_set_buffer() { std::ranges::fill(data, MISSING_ID); }
    };

    template <std::size_t N>
    using charm_buffer = std::vector<table_t<N>>;
} // namespace mtce::vec
