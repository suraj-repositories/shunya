add_executable(shunya_unit_tests
    unit/test_build_smoke.cpp
    unit/test_uuid.cpp
    unit/test_error_result.cpp
    unit/test_logging.cpp
    unit/test_event_bus.cpp
    unit/test_math.cpp
    unit/test_config.cpp
    unit/test_thread_pool.cpp
    unit/test_app_context.cpp
    unit/test_units.cpp
    unit/test_project.cpp
    unit/test_scene_graph.cpp
)

target_include_directories(shunya_unit_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(shunya_unit_tests
    PRIVATE
        shunya_core_base
        shunya_core_domain
)

target_compile_features(shunya_unit_tests PRIVATE cxx_std_20)

add_test(NAME CoreUnitTests COMMAND shunya_unit_tests)
