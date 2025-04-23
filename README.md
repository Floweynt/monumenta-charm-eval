# Multi-Threaded Zenith Charm Evaluator

A fast Zenith Charm evaluation tool written in C++, optimized for multithreaded performance and modern superscalar processors.

## Usage

### Building

For best performance, it is strongly recommended to build from source. Modern CPUs support features like AVX, which can significantly improve
performance. Building from source allows the compiler to generate optimized code specific to your system.

#### Linux

**Dependencies:** `meson`, `ninja`, `clang`

It is important to use Clang. In benchmarks, Clang 19.1.1 showed a ~4x speedup over GCC 14.2.1.

This project uses C++23, so a relatively modern compiler is required.

Build steps:
```sh
$ CC=clang CXX=clang++ CXXFLAGS="-O3 -march=native" meson setup build --buildtype=custom
$ cd build
$ ninja -j4
```

#### Windows

*TODO (no Windows setup currently available)*

The steps are likely similar to Linux. You can install Clang for Windows, and both Ninja and Meson are cross-platform. Alternatively, use WSL. 
If you successfully build this on Windows, please consider contributing a guide.

### Configuration

The configuration file is simple:

```
charm_power = 15

[weights]
# example weights
volcanic_meteor_cooldown = 10000
volcanic_meteor_damage = 9000
volcanic_meteor_radius = 7000
# ...
```

Assign higher weights to more important effects. Avoid specifying weights for effects you don't care about - performance optimizations rely on
minimizing the number of effects.

### Running

Run the program from the command line:

```sh
$ ./mtce --config config.conf --in charm_dataset.txt
```

Replace `config.conf` with your configuration file, and `charm_dataset.txt` with the charm dataset. You can obtain this dataset using the 
Flowey Monumenta Addons mod (version 1.8 or later).

Advanced options (specifying different algorithms, etc) can be done with additional CLI flags. Use `--help` to see available options:
```sh
$ ./mtce --help
```

## Future Work

- [ ] Tree pruning  
- [ ] GPU acceleration

Submit a PR if you'd like to help write the Windows guide or add examples for config files or datasets.
