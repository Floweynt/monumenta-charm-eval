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

##### Prepare Dependencies

- Clang: install the latest `LLVM-(VersionNumberHere)-win64.exe` from [LLVM Releases](https://github.com/llvm/llvm-project/releases)
- Meson & Ninja
    - Install [Python](https://www.python.org/downloads/) first
	- Type `pip install meson` and `pip install ninja` in cmd (Win+R, type `cmd` and enter)
- C++ Library
	- Download [VS Build Tool](https://visualstudio.microsoft.com/visual-cpp-build-tools/). During installation, select the "Desktop development with C++" workload.

##### Build

Install [Git](https://git-scm.com/download/win) first. During installation, choose the option to “Add Git to PATH” (usually it is by default).

Open PowerShell (Win+X) and type
```PowerShell
git clone https://github.com/Floweynt/monumenta-charm-eval.git
cd monumenta-charm-eval
```

Wait till it's done, then type 
```PowerShell
$env:CC="clang"
$env:CXX="clang++"
meson setup build --buildtype=release -Dcpp_args="-O3 -march=native"
```

Wait till it's done, then type 
```PowerShell
cd build
ninja -j4
```

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

1. Write up the config (example file at `monumenta-charm-eval/samples`) and save it as a file at `monumenta-charm-eval/build`.
2. Grab charm data with Flowey's Monumenta Addons mod (v1.8+), press the keybind (I by default) when hovering your mouse on each cz charms you'd like to use, then type the command the Toast just told you, paste and save it as .txt file at `monumenta-charm-eval/build`

Run the program from the command line:

Linux:
```sh
$ ./mtce --config YourConfigNameHere.conf --in YourCharmDataSetNameHere.txt
```

Windows:
```Powershell
.\mtce.exe --config YourConfigNameHere.conf --in YourCharmDataSetNameHere.txt
```

Advanced options (specifying different algorithms, etc) can be done with additional CLI flags. Use `--help` to see available options:
```sh
$ ./mtce --help
```

## Future Work

- [ ] Tree pruning  
- [ ] GPU acceleration

Submit a PR if you'd like to help write the Windows guide or add examples for config files or datasets.
