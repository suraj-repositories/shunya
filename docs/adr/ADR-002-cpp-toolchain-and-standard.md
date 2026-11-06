# ADR-002: C++ Standard Baseline and Toolchain Configuration

## Status
Accepted

## Context
The platform requires modern language features (concepts, ranges, `std::span`, `std::format`, `std::expected`) for type safety and expressiveness while maintaining broad compiler support across Windows (MinGW-w64 GCC 13.1+/MSVC) and Linux (GCC 13+/Clang 16+).

## Decision
1. **Language Standard:** C++20 is established as the strict mandatory baseline (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`).
2. **C++23 Adoption Policy:** Adopt C++23 standard library features selectively where fully supported across all tier-1 target compilers without proprietary or experimental compiler flags.
3. **Build System:** CMake (minimum version 3.20) with Ninja and standard `CMakePresets.json` configurations.
4. **Compiler Warnings:** Enforce `-Wall -Wextra -Wpedantic` on GCC/Clang and `/W4 /permissive-` on MSVC to prevent undefined behavior and portability traps.

## Consequences
- **Positive:** Uniform cross-platform compilation, zero non-standard compiler dialect lock-in, modern RAII and zero-cost abstractions.
- **Negative:** Older compilers without full C++20 support cannot build the codebase.
