#include "charm.h"
#include "eval.h"
#include <chrono>
#include <iostream>
#include <vector>

using namespace mtce;

namespace
{
    void print_res(const eval_result& ids)
    {
        for (const auto c_id : ids.charms)
        {
            std::cout << c_id << " ";
        }

        std::cout << "\n";
    }

    void test_empty()
    {
        std::vector<charm> charms = {};
        charm_weights weights = {1, 1, 1, 1, 1, 1, 1, 1, 1};
        print_res(evaluate_naive(charms, 15, weights));
    }

    void test_single()
    {
        std::vector<charm> charms = {
            {.charm_power = 1, .charm_data = {-30}},
        };
        charm_weights weights = {1};
        print_res(evaluate_naive(charms, 15, weights));
    }

    void test_choice()
    {
        std::vector<charm> charms = {
            {.charm_power = 1, .charm_data = {-29}},
            {.charm_power = 1, .charm_data = {-30}},
        };
        charm_weights weights = {1};
        print_res(evaluate_naive(charms, 1, weights));
    }

    void test_choice_many()
    {
        std::vector<charm> charms = {
            {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-1}},
            {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-1}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
        };
        charm_weights weights = {1};
        print_res(evaluate_naive(charms, 15, weights));
    }
    void test_perf_output()
    {
        std::vector<charm> charms = {
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-1}},
            {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-1}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-1}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-1}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
        };
        charm_weights weights = {1};
        print_res(evaluate_naive(charms, 15, weights));
    }

    void test_perf()
    {
        std::vector<charm> charms = {
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-1}},
            {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-1}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-1}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-1}}, {.charm_power = 1, .charm_data = {-1}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
            {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}}, {.charm_power = 1, .charm_data = {-2}},
        };
        charm_weights weights = {1};
        evaluate_naive(charms, 15, weights);
    }
} // namespace

auto main() -> int
{
    test_empty();
    test_single();
    test_choice();
    test_choice_many();
    test_perf_output();

    for(int i = 0; i < 50; i++)
    {
        const auto start = std::chrono::high_resolution_clock::now();
        test_perf();
        const auto end = std::chrono::high_resolution_clock::now();
        std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() << "\n";
    }
}
