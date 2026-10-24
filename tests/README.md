# Native checks

Build with a native C/C++ compiler and CMake:

```sh
cmake -S tests -B build/host-checks
cmake --build build/host-checks
ctest --test-dir build/host-checks --output-on-failure
```