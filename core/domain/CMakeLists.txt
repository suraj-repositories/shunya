# ==============================================================================
# Modular Domain Sub-Libraries (Acyclic Dependency Architecture)
# ==============================================================================

function(shunya_configure_domain_target target_name)
    target_include_directories(${target_name}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_compile_features(${target_name} PUBLIC cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    elseif(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    endif()
endfunction()

# 1. Core Domain Foundations
add_library(shunya_domain_core OBJECT
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
    src/simulation_clock.cpp
    src/simulation_runtime.cpp
    src/sparse_matrix.cpp
    src/numerical_solver.cpp
    src/observability.cpp
    src/simulation_execution.cpp
    src/data_exchange.cpp
    src/workspace_ui.cpp
    src/rendering.cpp
    src/mock_hardware_rasterizer.cpp
    src/circuit.cpp
    src/digital_logic.cpp
    src/mechanics.cpp
    src/thermal.cpp
    src/device.cpp
    src/virtual_lab.cpp
    src/scripting.cpp
    src/security.cpp
    src/library.cpp
    src/co_simulation.cpp
    src/fluid_dynamics.cpp
    src/chemistry.cpp
    src/optics.cpp
    src/biology.cpp
    src/performance.cpp
    src/adr.cpp
    src/material_ui.cpp
    src/property_node.cpp
    src/parameter_reference_manager.cpp
    src/multi_property_analyzer.cpp
    src/viewport_interaction.cpp
    src/scientific_plot.cpp
    src/colormap_tables.cpp
    src/oscilloscope.cpp
)
shunya_configure_domain_target(shunya_domain_core)
target_link_libraries(shunya_domain_core PUBLIC shunya_core_base)

# 2. Periodic Elements & Material Modeling
add_library(shunya_domain_mat OBJECT
    src/matdb/elements_118_catalog.cpp
    src/matdb/isotope_repository.cpp
    src/matdb/periodic_table_extended.cpp
    src/matdb/orbital_engine.cpp
    src/matdb/ionization_repository.cpp
    src/matdb/spectral_catalog.cpp
    src/matdb/crystal_database.cpp
    src/matdb/unit_cell_builder.cpp
    src/matdb/thermo_registry.cpp
    src/matdb/shomate_evaluator.cpp
    src/matdb/periodic_heatmap_filter.cpp
    src/matdb/bohr_orbital_model.cpp
    src/matdb/decay_chain_builder.cpp
    src/mat/molecular_graph.cpp
    src/mat/vsepr_conformation.cpp
    src/mat/molecular_mesh_builder.cpp
    src/mat/formula_parser.cpp
    src/mat/smiles_parser.cpp
    src/mat/composite_homogenizer.cpp
    src/mat/alloy_builder.cpp
    src/mat/porous_material.cpp
    src/mat/anisotropic_tensor.cpp
    src/mat/monotonic_spline.cpp
    src/mat/thermal_property_evaluator.cpp
    src/mat/thermal_strain.cpp
    src/mat/phase_change_enthalpy.cpp
    src/mat/viscosity_evaluator.cpp
    src/mat/electrical_properties.cpp
    src/matdb/element_database.cpp
    src/mat/element_behavior.cpp
    src/mat/material_mixing_lab.cpp
    src/mat/formation_scenarios.cpp
    src/mat/compound_validator.cpp
    src/mat/gibbs_free_energy.cpp
    src/mat/hume_rothery.cpp
    src/mat/solution_solubility_engine.cpp
    src/mat/directional_layup_engine.cpp
    src/mat/property_models/chemical_property_model.cpp
    src/mat/property_models/electrical_property_model.cpp
    src/mat/property_models/thermal_property_model.cpp
    src/mat/property_models/mechanical_property_model.cpp
    src/mat/property_models/environmental_property_model.cpp
    src/mat/property_models/material_property_bundle.cpp
    src/mat/calculation_classification.cpp
    src/mat/epistemic_value.cpp
    src/mat/state_dependent_evaluator.cpp
    src/mat/material_boundary_checker.cpp
    src/mat/shomate_engine.cpp
    src/mat/scientific_certification.cpp
    src/mat/iron_oxidation_engine.cpp
    src/mat/copper_patina_engine.cpp
    src/mat/material_mutation_propagator.cpp
    src/mat/thermal_deformation_engine.cpp
    src/mat/shunyamat_serializer.cpp
)
shunya_configure_domain_target(shunya_domain_mat)
target_link_libraries(shunya_domain_mat PUBLIC shunya_domain_core)
target_compile_definitions(shunya_domain_mat PRIVATE SHUNYA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# 3. 2D & 3D CAD Modeling & Rendering
add_library(shunya_domain_cad OBJECT
    src/cad2d/curve_2d.cpp
    src/cad2d/bezier_spline.cpp
    src/cad2d/polygon_boolean.cpp
    src/cad2d/profile_offset.cpp
    src/cad2d/fillet_chamfer.cpp
    src/cad2d/sketch_viewport_2d.cpp
    src/cad2d/construction_layer.cpp
    src/cad2d/snap_manager.cpp
    src/cad2d/polar_guide.cpp
    src/cad2d/constraint_graph.cpp
    src/cad2d/geometric_constraints.cpp
    src/cad2d/dimensional_constraints.cpp
    src/cad2d/sketch_solver_2d.cpp
    src/cad2d/layer_manager.cpp
    src/cad2d/block_manager.cpp
    src/cad2d/dxf_io.cpp
    src/cad2d/svg_io.cpp
    src/cad2d/drawing_tools.cpp
    src/cad2d/dimensioning_tool.cpp
    src/cad2d/vector_text_tool.cpp
    src/cad2d/advanced_curves.cpp
    src/cad2d/associative_fillet_chamfer.cpp
    src/cad2d/trim_extend_tool.cpp
    src/cad2d/alignment_tools.cpp
    src/cad2d/snap_glyph.cpp
    src/cad2d/coordinate_hud.cpp
    src/cad2d/selection_gizmo.cpp
    src/cad2d/group_manager.cpp
    src/cad2d/clipboard_manager.cpp
    src/cad2d/sketch_commands.cpp
    src/cad2d/context_menu_model.cpp
    src/cad2d/schematic_symbol_engine.cpp
    src/cad2d/schematic_wire_router.cpp
    src/cad2d/schematic_pin_tool.cpp
    src/cad2d/schematic_netlabel_bus.cpp
    src/cad2d/schematic_erc_engine.cpp
    src/cad2d/schematic_sim_overlay.cpp
    src/cad3d/solid_mesh.cpp
    src/cad3d/parametric_primitives.cpp
    src/cad3d/mesh_selection_engine.cpp
    src/cad3d/mesh_transform_ops.cpp
    src/cad3d/mesh_topology_ops.cpp
    src/cad3d/polygon_tessellator.cpp
    src/cad3d/extrusion_feature.cpp
    src/cad3d/revolve_feature.cpp
    src/cad3d/trajectory_frame.cpp
    src/cad3d/sweep_feature.cpp
    src/cad3d/loft_feature.cpp
    src/cad3d/bsp_tree.cpp
    src/cad3d/csg_boolean.cpp
    src/cad3d/polyhedral_brep.cpp
    src/cad3d/solid_chamfer.cpp
    src/cad3d/solid_fillet.cpp
    src/cad3d/solid_shell.cpp
    src/cad3d/half_edge_mesh.cpp
    src/cad3d/half_edge_pool.cpp
    src/cad3d/mesh_decimator.cpp
    src/cad3d/subdivision.cpp
    src/cad3d/bvh_tree.cpp
    src/cad3d/tetrahedral_mesher.cpp
    src/cad3d/rhi_renderer_engine.cpp
    src/cad3d/shadow_pass.cpp
    src/cad3d/wboit_pass.cpp
    src/cad3d/pbr_material.cpp
    src/cad3d/ibl_processor.cpp
    src/cad3d/volume_renderer.cpp
    src/cad3d/hierarchical_culler.cpp
    src/cad3d/gpu_vertex_buffer.cpp
    src/cad3d/gpu_compute_pipeline.cpp
    src/cad3d/shader_pipeline_cache.cpp
)
shunya_configure_domain_target(shunya_domain_cad)
target_link_libraries(shunya_domain_cad PUBLIC shunya_domain_core)

# 4. Electronic Design Automation & SPICE Circuits
add_library(shunya_domain_pcb OBJECT
    src/pcb/schematic_sheet.cpp
    src/pcb/wire_router_2d.cpp
    src/pcb/erc_engine.cpp
    src/pcb/symbol_library.cpp
    src/pcb/footprint_generator.cpp
    src/pcb/board_stackup.cpp
    src/pcb/board_outline.cpp
    src/pcb/trace_router.cpp
    src/pcb/via_manager.cpp
    src/pcb/copper_pour.cpp
    src/pcb/drc_engine.cpp
    src/pcb/gerber_exporter.cpp
    src/pcb/excellon_exporter.cpp
    src/pcb/pcb_3d_exporter.cpp
    src/pcb/parasitic_extractor.cpp
    src/pcb/netlist_compiler.cpp
    src/pcb/spice_simulation_engine.cpp
    src/pcb/digital_event_engine.cpp
    src/pcb/mixed_signal_bridge.cpp
)
shunya_configure_domain_target(shunya_domain_pcb)
target_link_libraries(shunya_domain_pcb PUBLIC shunya_domain_core)

# 5. Multiphysics, Chemistry, Environment & Co-Simulation
add_library(shunya_domain_sim OBJECT
    src/phy/symplectic_integrator.cpp
    src/phy/rigid_body_kinematics.cpp
    src/phy/gjk_epa.cpp
    src/phy/collision_engine.cpp
    src/phy/ccd_solver.cpp
    src/phy/constraint_solver.cpp
    src/phy/thermal_solver_3d.cpp
    src/phy/convection_model.cpp
    src/phy/radiation_solver.cpp
    src/phy/phase_change_solver.cpp
    src/phy/thermo_stress_solver.cpp
    src/phy/sph_viscosity.cpp
    src/phy/sph_surface_tension.cpp
    src/phy/sph_solver_3d.cpp
    src/phy/pressure_poisson.cpp
    src/phy/mac_grid_solver_3d.cpp
    src/chem/reaction_network.cpp
    src/chem/arrhenius_evaluator.cpp
    src/chem/catalysis_model.cpp
    src/chem/gibbs_minimizer.cpp
    src/chem/equilibrium_shifter.cpp
    src/chem/reaction_enthalpy.cpp
    src/chem/chemo_thermal_coupler.cpp
    src/chem/combustion_model.cpp
    src/chem/adiabatic_flame.cpp
    src/chem/aqueous_solution.cpp
    src/chem/electrolyte_conductivity.cpp
    src/chem/electrochemical_cell.cpp
    src/chem/battery_cell_model.cpp
    src/env/atmospheric_model.cpp
    src/env/psychrometric_model.cpp
    src/env/wind_field_model.cpp
    src/env/gravity_field.cpp
    src/env/solar_position.cpp
    src/env/climatic_chamber.cpp
    src/env/wind_tunnel_fixture.cpp
    src/env/ambient_convection.cpp
    src/env/deep_space_sink.cpp
    src/integ/adaptive_step_negotiator.cpp
    src/integ/state_checkpoint.cpp
    src/integ/master_co_simulation.cpp
    src/integ/electro_thermal_connector.cpp
    src/integ/thermo_mechanical_connector.cpp
    src/integ/fsi_connector.cpp
    src/integ/chemo_thermal_connector.cpp
    src/integ/fmu_importer.cpp
    src/integ/fmu_exporter.cpp
    src/integ/shared_memory_bus.cpp
    src/integ/telemetry_stream_server.cpp
)
shunya_configure_domain_target(shunya_domain_sim)
target_link_libraries(shunya_domain_sim PUBLIC shunya_domain_mat shunya_domain_core)

# 6. Performance Kernels & Scientific Benchmarks
add_library(shunya_domain_perf OBJECT
    src/perf/gpu_particle_engine.cpp
    src/perf/gpu_spmv.cpp
    src/perf/gpu_transfer_ring.cpp
    src/perf/simd_kernels.cpp
    src/perf/work_stealing_pool.cpp
    src/perf/domain_decomposition.cpp
    src/perf/linear_arena.cpp
    src/perf/simulation_step_arena.cpp
    src/perf/task_graph_engine.cpp
    src/perf/thread_scaling_benchmark.cpp
    src/perf/performance_hud.cpp
    src/perf/simulation_hardware_profiler.cpp
    src/perf/chrome_tracing_exporter.cpp
    src/perf/hot_loop_profiler.cpp
    src/perf/memory_budget.cpp
    src/bench/scientific_ci_runner.cpp
    src/bench/visual_regression_runner.cpp
    src/bench/perf_regression_gate.cpp
)
shunya_configure_domain_target(shunya_domain_perf)
target_link_libraries(shunya_domain_perf PUBLIC shunya_domain_mat shunya_domain_core shunya_domain_pcb)
if (WIN32)
    target_link_libraries(shunya_domain_perf PUBLIC psapi)
endif()

# 7. Phase 3 Unified Engineering Pipeline (Elements -> Materials -> Objects -> Components -> Circuits)
add_library(shunya_domain_eng OBJECT
    src/eng/engineering_pipeline.cpp
    src/eng/entity_relationship_graph.cpp
    src/eng/physical_object.cpp
    src/eng/component_generator.cpp
    src/eng/battery_engineer.cpp
    src/eng/interactive_circuit.cpp
    src/eng/physics_runtime.cpp
)
shunya_configure_domain_target(shunya_domain_eng)
target_link_libraries(shunya_domain_eng
    PUBLIC
        shunya_domain_sim
        shunya_domain_pcb
        shunya_domain_cad
        shunya_domain_mat
        shunya_domain_core
)

# Umbrella target preserving full backward compatibility for all consumers
add_library(shunya_core_domain INTERFACE)
target_sources(shunya_core_domain
    INTERFACE
        $<TARGET_OBJECTS:shunya_domain_core>
        $<TARGET_OBJECTS:shunya_domain_mat>
        $<TARGET_OBJECTS:shunya_domain_cad>
        $<TARGET_OBJECTS:shunya_domain_pcb>
        $<TARGET_OBJECTS:shunya_domain_sim>
        $<TARGET_OBJECTS:shunya_domain_perf>
        $<TARGET_OBJECTS:shunya_domain_eng>
)
target_include_directories(shunya_core_domain
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(shunya_core_domain
    INTERFACE
        shunya_core_base
)
target_compile_features(shunya_core_domain INTERFACE cxx_std_20)
