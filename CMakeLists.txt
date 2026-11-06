cmake_minimum_required(VERSION 3.20)

project(shunya_workspace
    VERSION 0.1.0
    DESCRIPTION "Shunya Virtual Engineering & Multiphysics Platform"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Global compiler warning policies across targets
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
elseif(MSVC)
    add_compile_options(/W4 /permissive-)
endif()

enable_testing()

# Core non-GUI foundational scientific modules
add_subdirectory(core/base)
add_subdirectory(core/domain)

# Automated unit testing suite
add_subdirectory(tests)

# Desktop application shell (Qt 6)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shunya/CMakeLists.txt")
    add_subdirectory(shunya)
endif()
