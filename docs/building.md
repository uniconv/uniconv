# Building from Source

## Requirements

- CMake 3.20+
- C++20 compiler (Clang 14+, GCC 12+, MSVC 17+)
- libmagic (`brew install libmagic` on macOS, `apt install libmagic-dev` on Ubuntu)

## Build

```bash
git clone https://github.com/anthropics/uniconv.git
cd uniconv/uniconv
mkdir build && cd build
cmake ..
cmake --build .
```

## Install

```bash
sudo cmake --install .
```

## Run tests

```bash
cd build
ctest        # All tests
ctest -V     # Verbose output
```
