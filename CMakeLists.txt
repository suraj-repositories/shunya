cmake_minimum_required(VERSION 3.20)

project(shunya_workspace
    VERSION 0.1.0
    DESCRIPTION "Shunya Virtual Engineering & Multiphysics Platform"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Global compiler warning policies across targets (Phase 4 zero-warning policy)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror=return-type -Wno-error=maybe-uninitialized)
elseif(MSVC)
    add_compile_options(/W4 /permissive- /we4715)
endif()

# Automated Sanitizer Support (ASan / LSan / UBSan) (PH4-02.01.006)
option(SHUNYA_ENABLE_SANITIZERS "Enable AddressSanitizer, LeakSanitizer, and UndefinedBehaviorSanitizer" OFF)
if(SHUNYA_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Configuring AddressSanitizer and UndefinedBehaviorSanitizer (-fsanitize=address,undefined)")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
        add_link_options(-fsanitize=address,undefined)
    elseif(MSVC)
        message(STATUS "Configuring MSVC AddressSanitizer (/fsanitize=address)")
        add_compile_options(/fsanitize=address /Zi)
    endif()
endif()

# WebAssembly / Emscripten headless solver target readiness (IMP-04)
if(EMSCRIPTEN)
    message(STATUS "Configuring Shunya headless solver kernel for WebAssembly target via Emscripten")
    add_compile_options(-sWASM=1 -sDISABLE_EXCEPTION_CATCHING=0)
endif()

enable_testing()

# Core non-GUI foundational scientific modules
add_subdirectory(core/base)
add_subdirectory(core/domain)

# Automated unit testing suite
add_subdirectory(tests)

# Headless simulation command-line interface (CLI)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/apps/cli/CMakeLists.txt")
    add_subdirectory(apps/cli)
endif()

# Desktop application shell (Qt 6)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shunya/CMakeLists.txt")
    add_subdirectory(shunya)
endif()
