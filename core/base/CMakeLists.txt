add_library(shunya_core_base)

target_sources(shunya_core_base
    PRIVATE
        src/version.cpp
        src/uuid.cpp
        src/logging.cpp
    PUBLIC FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/shunya/core/base/version.hpp
            include/shunya/core/base/uuid.hpp
            include/shunya/core/base/error.hpp
            include/shunya/core/base/result.hpp
            include/shunya/core/base/logging.hpp
            include/shunya/core/base/event_bus.hpp
            include/shunya/core/base/math.hpp
)

target_include_directories(shunya_core_base
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(shunya_core_base PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(shunya_core_base PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(shunya_core_base PRIVATE /W4 /permissive-)
endif()
