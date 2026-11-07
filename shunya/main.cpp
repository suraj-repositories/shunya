#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "studio_controller.hpp"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    auto* studio = new ShunyaStudioController(&app);
    engine.rootContext()->setContextProperty("studio", studio);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("shunya", "Main");

    return QGuiApplication::exec();
}
