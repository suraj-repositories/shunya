add_library(shunya_core_domain)

target_sources(shunya_core_domain
    PRIVATE
        src/project.cpp
        src/scene_graph.cpp
        src/command_stack.cpp
        src/selection.cpp
        src/migration.cpp
        src/validator.cpp
        src/packaging.cpp
        src/element_definition.cpp
    PUBLIC FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/shunya/core/domain/project.hpp
            include/shunya/core/domain/scene_graph.hpp
            include/shunya/core/domain/command_stack.hpp
            include/shunya/core/domain/selection.hpp
            include/shunya/core/domain/migration.hpp
            include/shunya/core/domain/validator.hpp
            include/shunya/core/domain/packaging.hpp
            include/shunya/core/domain/element_definition.hpp
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
