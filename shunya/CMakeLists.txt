cmake_minimum_required(VERSION 3.16)

project(shunya VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Svg Test)

qt_standard_project_setup(REQUIRES 6.10)

# Content of downloaded design from Figma to Qt or similar
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

if(NOT TARGET shunya_core_base)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../core/base ${CMAKE_CURRENT_BINARY_DIR}/core_base)
endif()
if(NOT TARGET shunya_core_domain)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../core/domain ${CMAKE_CURRENT_BINARY_DIR}/core_domain)
endif()

qt_add_library(shunya_gui_lib STATIC
    studio_controller.hpp
    studio_controller.cpp
    menu_bar_controller.hpp
    menu_bar_controller.cpp
    docking_controller.hpp
    docking_controller.cpp
    ribbon_controller.hpp
    ribbon_controller.cpp
    property_tree_model.hpp
    property_tree_model.cpp
    quantity_input_controller.hpp
    quantity_input_controller.cpp
    multi_property_controller.hpp
    multi_property_controller.cpp
    viewport_controller.hpp
    viewport_controller.cpp
    scientific_plot_item.hpp
    scientific_plot_item.cpp
    colormap_legend_controller.hpp
    colormap_legend_controller.cpp
    oscilloscope_controller.hpp
    oscilloscope_controller.cpp
    sketch_canvas_2d.hpp
    sketch_canvas_2d.cpp
    layer_panel_controller.hpp
    layer_panel_controller.cpp
    periodic_table_controller.hpp
    periodic_table_controller.cpp
    pipeline_controller.hpp
    pipeline_controller.cpp
    icon_provider.hpp
    icon_provider.cpp
    viewport_3d_item.hpp
    viewport_3d_item.cpp
)

set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(shunya_gui_lib
    URI shunya
    QML_FILES
        Theme.qml
        ShunyaIcon.qml
        MonoButton.qml
        MonoInput.qml
        MonoBadge.qml
        ElementsWorkspace.qml
        MixingLabWorkspace.qml
        ObjectsWorkspace.qml
        ComponentsBatteryWorkspace.qml
        CircuitWorkspace.qml
        Editor2DWorkspace.qml
        Editor3DWorkspace.qml
        SimulationWorkspace.qml
        ProjectSettingsWorkspace.qml
        Main.qml
        MainMenuBar.qml
        NewProjectWizard.qml
        UnsavedChangesGuardDialog.qml
        DockContainer.qml
        DockOverlay.qml
        RibbonBar.qml
        RibbonTab.qml
        RibbonGroup.qml
        CommandPalette.qml
        UndoHistoryDropdown.qml
        PropertyInspector.qml
        PropertyDelegateFactory.qml
        QuantitySpinBox.qml
        MultiPropertyInspector.qml
        GizmoOverlay.qml
        ViewCube.qml
        ViewportHud.qml
        ScientificPlot.qml
        ColormapLegend.qml
        OscilloscopePanel.qml
        LogicAnalyzerTrack.qml
        SketchCanvas.qml
        SnapGlyphOverlay.qml
        SketchToolbar.qml
        DimensionDisplayWidget.qml
        LayerPanel.qml
        BlockLibraryPanel.qml
        UnitCellViewer.qml
        ElementCell.qml
        HeatmapLegend.qml
        PeriodicTableWindow.qml
        BohrModelView.qml
        OrbitalPreviewItem.qml
        IsotopeDecayView.qml
        MoleculeViewer.qml
        AlloyComposer.qml
        SchematicView.qml
        WireInteractionTool.qml
        ErcReportPanel.qml
        SymbolEditorView.qml
        FootprintPreview.qml
        StackupEditor.qml
        PcbRoutingCanvas.qml
        ViaDialog.qml
        DrcReportView.qml
        ChamberControlPanel.qml
        WindTunnelView.qml
)

target_include_directories(shunya_gui_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/base/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/domain/include
)

target_link_libraries(shunya_gui_lib
    PUBLIC
        Qt6::Quick
        Qt6::Svg
        shunya_core_base
        shunya_core_domain
)

qt_add_executable(appshunya
    main.cpp
    icons.qrc
)

set_target_properties(appshunya PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appshunya
    PRIVATE
        shunya_gui_lib
        shunya_gui_libplugin
)

qt_add_executable(shunya_ui_tests
    ../tests/ui/test_shunya_ui.cpp
    icons.qrc
)

target_link_libraries(shunya_ui_tests
    PRIVATE
        shunya_gui_lib
        shunya_gui_libplugin
        Qt6::Test
)

add_test(NAME QtUiTests COMMAND shunya_ui_tests)
set_tests_properties(QtUiTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

install(TARGETS appshunya
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
