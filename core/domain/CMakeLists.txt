add_library(shunya_core_domain)

target_sources(shunya_core_domain
    PRIVATE
        src/project.cpp
    PUBLIC FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/shunya/core/domain/project.hpp
)

target_include_directories(shunya_core_domain
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(shunya_core_domain
    PUBLIC
        shunya_core_base
)

target_compile_features(shunya_core_domain PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(shunya_core_domain PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(shunya_core_domain PRIVATE /W4 /permissive-)
endif()
