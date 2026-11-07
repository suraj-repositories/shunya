#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "studio_controller.hpp"
#include "menu_bar_controller.hpp"
#include "docking_controller.hpp"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    auto* studio = new ShunyaStudioController(&app);
    auto* menuCtrl = new ShunyaMenuBarController(&app);
    auto* dockCtrl = new shunya::gui::ShunyaDockingController(&app);

    studio->setMenuBarController(menuCtrl);

    engine.rootContext()->setContextProperty("studio", studio);
    engine.rootContext()->setContextProperty("menuController", menuCtrl);
    engine.rootContext()->setContextProperty("dockingController", dockCtrl);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("shunya", "Main");

    return QGuiApplication::exec();
}
