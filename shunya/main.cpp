#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExtensionPlugin>
Q_IMPORT_QML_PLUGIN(shunyaPlugin)
#include "studio_controller.hpp"
#include "menu_bar_controller.hpp"
#include "docking_controller.hpp"
#include "ribbon_controller.hpp"
#include "property_tree_model.hpp"
#include "quantity_input_controller.hpp"
#include "multi_property_controller.hpp"
#include "viewport_controller.hpp"
#include "colormap_legend_controller.hpp"
#include "oscilloscope_controller.hpp"
#include "layer_panel_controller.hpp"
#include "periodic_table_controller.hpp"
#include "pipeline_controller.hpp"
#include "icon_provider.hpp"
#include "viewport_3d_item.hpp"
#include "scientific_plot_item.hpp"
#include "sketch_canvas_2d.hpp"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("icons"), new shunya::gui::ShunyaIconProvider());
    qmlRegisterType<shunya::gui::ShunyaViewport3DItem>("shunya", 1, 0, "Viewport3DItem");
    qmlRegisterType<shunya::gui::ShunyaScientificPlotItem>("shunya", 1, 0, "ShunyaScientificPlotItem");
    qmlRegisterType<shunya::gui::ShunyaSketchCanvas2D>("shunya", 1, 0, "ShunyaSketchCanvas2D");

    auto* studio = new ShunyaStudioController(&app);
    auto* menuCtrl = new ShunyaMenuBarController(&app);
    auto* dockCtrl = new shunya::gui::ShunyaDockingController(&app);
    auto* ribbonCtrl = new shunya::gui::ShunyaRibbonController(&app);
    auto* propTreeModel = new shunya::gui::ShunyaPropertyTreeModel(&app);
    auto* quantityCtrl = new shunya::gui::ShunyaQuantityInputController(&app);
    auto* multiPropCtrl = new shunya::gui::ShunyaMultiPropertyController(&app);
    auto* viewportCtrl = new shunya::gui::ShunyaViewportController(&app);
    auto* colormapCtrl = new shunya::gui::ShunyaColormapLegendController(&app);
    auto* scopeCtrl = new shunya::gui::ShunyaOscilloscopeController(&app);
    auto* layerCtrl = new shunya::gui::ShunyaLayerPanelController(&app);
    auto* periodicCtrl = new shunya::gui::ShunyaPeriodicTableController(&app);
    auto* pipelineCtrl = new shunya::gui::ShunyaPipelineController(studio->workspace(), &app);

    pipelineCtrl->setOscilloscopeController(scopeCtrl);
    studio->setPipelineController(pipelineCtrl);

    auto paramMgr = std::make_shared<shunya::core::domain::ParameterReferenceManager>();
    propTreeModel->setParameterReferenceManager(paramMgr);
    quantityCtrl->setParameterReferenceManager(paramMgr);

    studio->setMenuBarController(menuCtrl);
    ribbonCtrl->setCommandStack(&studio->commandStack());
    multiPropCtrl->setCommandStack(&studio->commandStack());

    // Connect ribbon action triggers to studio
    QObject::connect(ribbonCtrl, &shunya::gui::ShunyaRibbonController::actionTriggered, [studio](const QString& act) {
        if (act == "sim.run") studio->startSimulation();
        else if (act == "sim.pause") studio->pauseSimulation();
        else if (act == "sim.step") studio->stepSimulation();
        else if (act == "sim.stop") studio->stopSimulation();
        else if (act == "sim.reset") studio->resetSimulation();
    });

    engine.rootContext()->setContextProperty("studio", studio);
    engine.rootContext()->setContextProperty("pipeline", pipelineCtrl);
    engine.rootContext()->setContextProperty("menuController", menuCtrl);
    engine.rootContext()->setContextProperty("dockingController", dockCtrl);
    engine.rootContext()->setContextProperty("ribbonController", ribbonCtrl);
    engine.rootContext()->setContextProperty("propertyTreeModel", propTreeModel);
    engine.rootContext()->setContextProperty("quantityController", quantityCtrl);
    engine.rootContext()->setContextProperty("multiPropertyController", multiPropCtrl);
    engine.rootContext()->setContextProperty("viewportController", viewportCtrl);
    engine.rootContext()->setContextProperty("colormapController", colormapCtrl);
    engine.rootContext()->setContextProperty("oscilloscopeController", scopeCtrl);
    engine.rootContext()->setContextProperty("layerController", layerCtrl);
    engine.rootContext()->setContextProperty("periodicTableController", periodicCtrl);

    QObject::connect(&engine, &QQmlEngine::warnings, [](const QList<QQmlError>& warnings) {
        for (const auto& w : warnings) {
            fprintf(stderr, "QML WARN: %s\n", w.toString().toLocal8Bit().constData());
        }
        fflush(stderr);
    });

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [](const QUrl& url) {
            fprintf(stderr, "Object creation failed for: %s\n", url.toString().toLocal8Bit().constData());
            fflush(stderr);
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    engine.loadFromModule("shunya", "Main");

    return QGuiApplication::exec();
}
