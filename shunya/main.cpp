#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "studio_controller.hpp"
#include "menu_bar_controller.hpp"
#include "docking_controller.hpp"
#include "ribbon_controller.hpp"
#include "property_tree_model.hpp"
#include "quantity_input_controller.hpp"

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

    auto paramMgr = std::make_shared<shunya::core::domain::ParameterReferenceManager>();
    propTreeModel->setParameterReferenceManager(paramMgr);
    quantityCtrl->setParameterReferenceManager(paramMgr);

    studio->setMenuBarController(menuCtrl);
    ribbonCtrl->setCommandStack(&studio->commandStack());

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
