# Multi-Threaded Zenith Charm Evaluator
A fast zenith charm evaluation tool written in c++, optimized for multithreaded performance and modern superscalar processors.

## Usage

### Building 

To optimize for performance, it is highly recommended you build from source. Modern processors have features like AVX which massively improve 
performance, and building from source allows the compiler to build a version of the program tailored to your CPU.

TODO: make a writeup for this 

### Configuration

The configuration file is relatively simple:

```
charm_power = 15

[weights]
# for comments
volcanic_meteor_cooldown = 10000
volcanic_meteor_damage = 9000
volcanic_meteor_radius = 7000
# ...
```

More important effects should be weighted higher. Avoid adding weights for things you don't care about, since there are optimizations that rely on 
having fewer effects.

### Running 

The program can be run from the command line like:

```sh
$ ./mtce --config config.conf --in charm_dataset.txt
```

Replace `config.conf` with the actual configuration file, and `charm_dataset.txt` with the actual charm dataset. You can obtain the charm dataset by 
using Flowey Monumenta Addons mod, version `1.8` or above.

## Future work 
- [ ] Tree pruning 
- [ ] GPU acceleration
