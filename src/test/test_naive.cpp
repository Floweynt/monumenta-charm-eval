#include "common/eval.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <vector>

using namespace mtce;
using namespace testing;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
TEST(naive, empty)
{

    for (int i = 0; i < 15; i++)
    {
        auto [utilty, charm_set] = evaluate_naive({
            .charms = {},
            .max_cp = static_cast<std::uint32_t>(i),
            .weights = {1, 1, 1, 1, 1, 1, 1, 1, 1},
        });
        ASSERT_TRUE(charm_set.empty());
        ASSERT_TRUE(utilty == 0);
    }
}

TEST(naive, single)
{
    auto [utility, charm_set] = evaluate_naive({
        .charms =
            {
                {.charm_power = 1, .charm_data = {-30}},
            },
        .max_cp = 15,
        .weights = {1},
    });

    ASSERT_THAT(charm_set, ElementsAre(0));
}

TEST(naive, choice)
{
    auto [utility, charm_set] = evaluate_naive({
        .charms =
            {
                {.charm_power = 1, .charm_data = {-29}},
                {.charm_power = 1, .charm_data = {-30}},
            },
        .max_cp = 1,
        .weights = {1},
    });

    ASSERT_THAT(charm_set, ElementsAre(1));
}

TEST(naive, choice_many)
{
    auto [utility, charm_set] = evaluate_naive({
        .charms =
            {
                {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-1}},
                {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-1}},
                {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
                {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
                {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-2}},
                {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
                {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
                {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            },
        .max_cp = 15,
        .weights = {1},
    });

    ASSERT_THAT(charm_set, ElementsAre(1, 6, 7, 8, 10, 11, 12));
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
