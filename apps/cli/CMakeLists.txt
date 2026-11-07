add_executable(shunya-cli
    src/main.cpp
)

target_include_directories(shunya-cli
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../../core/base/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../core/domain/include
)

target_link_libraries(shunya-cli
    PRIVATE
        shunya_core_base
        shunya_core_domain
)

target_compile_features(shunya-cli PRIVATE cxx_std_20)
