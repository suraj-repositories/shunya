#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
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

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

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
