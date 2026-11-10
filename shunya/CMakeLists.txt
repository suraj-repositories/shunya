cmake_minimum_required(VERSION 3.16)

project(shunya VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)

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

qt_add_executable(appshunya
    main.cpp
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
)

qt_add_qml_module(appshunya
    URI shunya
    QML_FILES
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
)

set_target_properties(appshunya PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_include_directories(appshunya
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/base/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/domain/include
)

target_link_libraries(appshunya
    PRIVATE
        Qt6::Quick
        shunya_core_base
        shunya_core_domain
)

install(TARGETS appshunya
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
