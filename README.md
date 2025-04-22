# Multi-Threaded Zenith Charm Evaluator
A fast zenith charm evaluation tool written in c++, optimized for multithreaded performance and modern superscalar processors.

## Details 

### Module: `naive`

This module implements a *high-performance linear combinatorial optimization algorithm* for evaluating sets of zenith charms. The goal is to find the 
highest-utility subset of charms within a maximum total power constraint. The algorithm operates on a fixed number of charms and evaluates all 
possible combinations (up to a fixed maximum `CHARM_COUNT_MAX`) to find the one with the best score under a given utility function.
The score is computed as a *weighted sum* of normalized ability stats.

Optimizations:
- Layout & alignment for SIMD
  - Alignment with `alignas(CHARM_STRUCT_ALGIN)` to ensure `table_t<N>` is properly aligned in memory to enable vectorization.
  - Stat table is `std::array<int32_t, N>`, since `int32_t`s instead of `float`s allows faster vector ops.
    - Floating-point stats are quantized to `int32_t` by scaling (via `ENCODED_CHARM_STAT_SCALE`), sacrificing *no* precision
  - Padded table size (`TABLE_SIZE_ALIGN`) - ensures `table_t` size is always a multiple of a vector block width, allowing:
    - Efficient loop unrolling
    - Elimination of masking logic in vectorized operations.
- Recursive backtracking with static unrolling
  - The recursive method `eval_charm<CharmsLeft>` is *template-recursive* with compile-time constant unrolling:
    - Eliminates loop branches at runtime.
    - Enables full inlining and loop unrolling by the compiler (see `[[gnu::always_inline]]`).
- (Basic) pruning
  - If accumulated charm power exceeds `max_cp`, recursion terminates early:
    ```cpp
    if (curr_cp > max_charm_power) return;
    ```
- Redundant work elimination via preprocessing
  - Zero-weight abilities are excluded entirely.
  - Charms with no contribution to weighted utility are ignored.
  - Dimensionality reduction reduces:
    - Table sizes
    - Total computation
    - Memory use and cache pressure
- Dynamic dispatch to static size evaluator
  - This is the layer that translates dynamic inputs to the fast static evaluator
  - Instead of branching on the number of abilities, the algorithm uses:
    ```cpp
    table_helper::TABLE[important_abilities.size()]
    ```
  - This provides:
    - Constant-time dispatch to a specialized evaluator.
    - No dynamic loop over ability counts.
    - Reduced instruction overhead.
  - Each `eval_charms_dyn<TABLE_SIZE<N>>` instantiation is type-specialized for a specific ability count (branch-free)

### Module: `naive-prune`

Same as `naive` but prune all charm sets if their *utility per charm* is less than some `factor_upc` times the best *utility per charm*, or if the 
*utility per charm power* is less than some `factor_upp` times the best *utility per charm power*.

## Future work 
- [ ] Tree pruning 
- [ ] GPU acceleration
