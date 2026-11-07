#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "studio_controller.hpp"
#include "menu_bar_controller.hpp"
#include "docking_controller.hpp"
#include "ribbon_controller.hpp"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    auto* studio = new ShunyaStudioController(&app);
    auto* menuCtrl = new ShunyaMenuBarController(&app);
    auto* dockCtrl = new shunya::gui::ShunyaDockingController(&app);
    auto* ribbonCtrl = new shunya::gui::ShunyaRibbonController(&app);

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

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("shunya", "Main");

    return QGuiApplication::exec();
}
