# Building from Source

## Requirements

- CMake 3.20+
- C++20 compiler (Clang 14+, GCC 12+, MSVC 17+)

## Build (macOS / Linux)

```bash
git clone https://github.com/uniconv/uniconv.git
cd uniconv/uniconv
mkdir build && cd build
cmake ..
cmake --build .
```

## Build (Windows)

Visual Studio 2022 with "Desktop development with C++" workload is required.

```powershell
git clone https://github.com/uniconv/uniconv.git
cd uniconv\uniconv
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

The resulting binary is at `build\Release\uniconv.exe`.

## Install

```bash
# macOS / Linux
sudo cmake --install .
```

On Windows, copy `build\Release\uniconv.exe` to a directory in your PATH, or add the build directory to PATH.

## Run tests

```bash
cd build
ctest        # All tests
ctest -V     # Verbose output
```

On Windows, specify the configuration:

```powershell
ctest -C Release
ctest -C Release -V
```
