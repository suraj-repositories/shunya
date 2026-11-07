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
        src/element_instance.cpp
        src/material_curve.cpp
        src/material.cpp
        src/material_assignment.cpp
        src/material_dictionary.cpp
        src/periodic_table.cpp
        src/dataset_release.cpp
        src/material_runtime.cpp
        src/material_adapters.cpp
        src/component.cpp
        src/component_runtime.cpp
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
            include/shunya/core/domain/element_instance.hpp
            include/shunya/core/domain/material_curve.hpp
            include/shunya/core/domain/material.hpp
            include/shunya/core/domain/material_assignment.hpp
            include/shunya/core/domain/material_dictionary.hpp
            include/shunya/core/domain/periodic_table.hpp
            include/shunya/core/domain/dataset_release.hpp
            include/shunya/core/domain/material_runtime.hpp
            include/shunya/core/domain/material_adapters.hpp
            include/shunya/core/domain/component.hpp
            include/shunya/core/domain/component_runtime.hpp
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
