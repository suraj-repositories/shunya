#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExtensionPlugin>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "colormap_legend_controller.hpp"
#include "docking_controller.hpp"
#include "icon_provider.hpp"
#include "layer_panel_controller.hpp"
#include "menu_bar_controller.hpp"
#include "multi_property_controller.hpp"
#include "oscilloscope_controller.hpp"
#include "periodic_table_controller.hpp"
#include "pipeline_controller.hpp"
#include "property_tree_model.hpp"
#include "quantity_input_controller.hpp"
#include "ribbon_controller.hpp"
#include "scientific_plot_item.hpp"
#include "sketch_canvas_2d.hpp"
#include "studio_controller.hpp"
#include "viewport_3d_item.hpp"
#include "viewport_controller.hpp"

Q_IMPORT_QML_PLUGIN(shunyaPlugin)

using namespace shunya::gui;

class TestShunyaUi : public QObject {
    Q_OBJECT

private:
    struct UiHarness {
        std::unique_ptr<QQmlApplicationEngine> engine;
        ShunyaStudioController* studio{nullptr};
        ShunyaMenuBarController* menuCtrl{nullptr};
        ShunyaPipelineController* pipeline{nullptr};
        ShunyaOscilloscopeController* scopeCtrl{nullptr};
        ShunyaPropertyTreeModel* propTreeModel{nullptr};
        QStringList warnings;
        QQuickWindow* window{nullptr};

        void init(QObject* parent) {
            engine = std::make_unique<QQmlApplicationEngine>();
            engine->addImageProvider(QStringLiteral("icons"), new ShunyaIconProvider());

            studio = new ShunyaStudioController(parent);
            menuCtrl = new ShunyaMenuBarController(parent);
            auto* dockCtrl = new ShunyaDockingController(parent);
            auto* ribbonCtrl = new ShunyaRibbonController(parent);
            propTreeModel = new ShunyaPropertyTreeModel(parent);
            auto* quantityCtrl = new ShunyaQuantityInputController(parent);
            auto* multiPropCtrl = new ShunyaMultiPropertyController(parent);
            auto* viewportCtrl = new ShunyaViewportController(parent);
            auto* colormapCtrl = new ShunyaColormapLegendController(parent);
            scopeCtrl = new ShunyaOscilloscopeController(parent);
            auto* layerCtrl = new ShunyaLayerPanelController(parent);
            auto* periodicCtrl = new ShunyaPeriodicTableController(parent);
            pipeline = new ShunyaPipelineController(studio->workspace(), parent);

            pipeline->setOscilloscopeController(scopeCtrl);
            studio->setPipelineController(pipeline);
            studio->setMenuBarController(menuCtrl);
            ribbonCtrl->setCommandStack(&studio->commandStack());
            multiPropCtrl->setCommandStack(&studio->commandStack());

            auto paramMgr = std::make_shared<shunya::core::domain::ParameterReferenceManager>();
            propTreeModel->setParameterReferenceManager(paramMgr);
            quantityCtrl->setParameterReferenceManager(paramMgr);

            engine->rootContext()->setContextProperty("studio", studio);
            engine->rootContext()->setContextProperty("pipeline", pipeline);
            engine->rootContext()->setContextProperty("menuController", menuCtrl);
            engine->rootContext()->setContextProperty("dockingController", dockCtrl);
            engine->rootContext()->setContextProperty("ribbonController", ribbonCtrl);
            engine->rootContext()->setContextProperty("propertyTreeModel", propTreeModel);
            engine->rootContext()->setContextProperty("quantityController", quantityCtrl);
            engine->rootContext()->setContextProperty("multiPropertyController", multiPropCtrl);
            engine->rootContext()->setContextProperty("viewportController", viewportCtrl);
            engine->rootContext()->setContextProperty("colormapController", colormapCtrl);
            engine->rootContext()->setContextProperty("oscilloscopeController", scopeCtrl);
            engine->rootContext()->setContextProperty("layerController", layerCtrl);
            engine->rootContext()->setContextProperty("periodicTableController", periodicCtrl);

            QObject::connect(engine.get(), &QQmlEngine::warnings, parent, [this](const QList<QQmlError>& list) {
                for (const auto& e : list) {
                    warnings.append(e.toString());
                }
            });

            engine->loadFromModule("shunya", "Main");
            QVERIFY(!engine->rootObjects().isEmpty());
            window = qobject_cast<QQuickWindow*>(engine->rootObjects().first());
            QVERIFY(window != nullptr);
            window->show();
            QVERIFY(QTest::qWaitForWindowExposed(window, 3000));
            QTest::qWait(80);
        }
    };

private slots:
    void initTestCase() {
        // Ensure crisp readable fonts even under QT_QPA_PLATFORM=offscreen on Windows
        QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/segoeui.ttf"));
        QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/segoeuib.ttf"));
        QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/consola.ttf"));
        QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/consolab.ttf"));

        qmlRegisterType<ShunyaViewport3DItem>("shunya", 1, 0, "Viewport3DItem");
        qmlRegisterType<ShunyaScientificPlotItem>("shunya", 1, 0, "ShunyaScientificPlotItem");
        qmlRegisterType<ShunyaSketchCanvas2D>("shunya", 1, 0, "ShunyaSketchCanvas2D");
    }

    void test_svg_icon_provider_light_and_dark() {
        ShunyaIconProvider provider;
        const QStringList names{"elements", "mixing", "objects", "components", "battery", "circuits", "editor2d", "editor3d", "simulation", "settings", "sun", "moon"};
        for (const auto& name : names) {
            QSize sz;
            const QImage darkImg = provider.requestImage(name + "/FAFAFA", &sz, QSize(24, 24));
            const QImage lightImg = provider.requestImage(name + "/0A0A0A", &sz, QSize(24, 24));
            QVERIFY2(!darkImg.isNull(), qPrintable("Missing dark SVG icon: " + name));
            QVERIFY2(!lightImg.isNull(), qPrintable("Missing light SVG icon: " + name));
            int opaqueCount = 0;
            for (int y = 0; y < darkImg.height(); ++y) {
                for (int x = 0; x < darkImg.width(); ++x) {
                    if (qAlpha(darkImg.pixel(x, y)) > 16) ++opaqueCount;
                }
            }
            QVERIFY2(opaqueCount > 4, qPrintable("Empty SVG pixels for: " + name));
        }
    }

    void test_qml_zero_warnings_navigation_and_theme_switch() {
        QObject holder;
        UiHarness ui;
        ui.init(&holder);

        QObject* theme = ui.engine->singletonInstance<QObject*>("shunya", "Theme");
        QVERIFY(theme != nullptr);
        QCOMPARE(theme->property("isDarkMode").toBool(), true);
        const QColor darkBg = theme->property("bgPrimary").value<QColor>();

        const QStringList workspaces{
            "elements", "mixing_lab", "objects", "components", "circuits",
            "editor_2d", "editor_3d", "simulation", "settings"
        };
        for (const auto& ws : workspaces) {
            ui.pipeline->setActiveWorkspace(ws);
            QTest::qWait(40);
            QCOMPARE(ui.pipeline->activeWorkspace(), ws);
        }

        // Toggle Light Mode & verify contrast inversion
        theme->setProperty("isDarkMode", false);
        QTest::qWait(50);
        QCOMPARE(theme->property("isDarkMode").toBool(), false);
        const QColor lightBg = theme->property("bgPrimary").value<QColor>();
        QVERIFY(darkBg.lightness() < 30);
        QVERIFY(lightBg.lightness() > 220);

        theme->setProperty("isDarkMode", true);
        QTest::qWait(40);

        if (!ui.warnings.isEmpty()) {
            qWarning().noquote() << "QML warnings:\n" << ui.warnings.join("\n");
        }
        QCOMPARE(ui.warnings.count(), 0);
    }

    void test_e2e_ui_controllers_persistence_and_stress() {
        QObject holder;
        UiHarness ui;
        ui.init(&holder);

        QSignalSpy wsSpy(ui.pipeline, &ShunyaPipelineController::workspaceChanged);
        QSignalSpy simSpy(ui.pipeline, &ShunyaPipelineController::physicsChanged);

        // 1. Query 118 CSV elements & evaluate behavior
        const QVariantList elems = ui.pipeline->queryElements();
        QCOMPARE(elems.size(), 118);
        const QVariantMap liBeh = ui.pipeline->evaluateElementBehavior(3, 500.0, 101325.0, true);
        QCOMPARE(liBeh.value("symbol").toString(), QString("Li"));
        QVERIFY(liBeh.value("phase").toString().contains("Liquid"));

        // 2. Mix & promote material -> create Copper rod -> generate Resistor component
        QVariantList inputs;
        QVariantMap cuIn;
        cuIn["symbol"] = "Cu";
        cuIn["basis"] = 1; // MassFraction
        cuIn["amount"] = 1.0;
        inputs.append(cuIn);
        const QVariantMap promoted = ui.pipeline->saveAndPromoteMixture(
            "", "UI_Copper_Bar_Mat", "Alloy", 298.15, 101325.0, false, 0.0, inputs, true);
        QVERIFY2(promoted.value("ok").toBool(), qPrintable(promoted.value("error").toString()));
        const QString matId = promoted.value("materialId").toString();
        QVERIFY(!matId.isEmpty());

        const QVariantMap obj = ui.pipeline->createObject(
            "UI_Busbar", "Conductor", "Cylinder", 0.02, 0.25, 0.02, 0.004, matId);
        QVERIFY(obj.value("ok").toBool());
        const QString objId = obj.value("id").toString();

        const QVariantMap comp = ui.pipeline->generateComponentFromObject(
            "UI_R_Bus", objId, "Resistor", QVariantMap{});
        QVERIFY(comp.value("ok").toBool());

        // 3. Battery charge/discharge simulation
        const QVariantMap batSpec = ui.pipeline->getDefaultBatterySpecMap();
        const QVariantMap batSim = ui.pipeline->simulateBatteryChargeDischarge(
            batSpec, 1.0, 60.0, 0.5, 60.0, 2.0);
        QVERIFY(batSim.value("ok").toBool());
        QVERIFY(!batSim.value("voltageSeries").toList().isEmpty());

        // 4. Circuit DC & Transient execution
        const QVariantMap dcRes = ui.pipeline->runCircuitDc();
        QVERIFY(dcRes.value("ok").toBool());
        const QVariantMap trRes = ui.pipeline->runCircuitTransient(5.0, 0.25);
        QVERIFY(trRes.value("ok").toBool());

        // 5. Simulation lifecycle & rapid start/stop/switch stress
        ui.studio->startSimulation();
        QCOMPARE(ui.studio->simStatus(), QString("Running"));
        QTest::qWait(80);
        ui.studio->pauseSimulation();
        QCOMPARE(ui.studio->simStatus(), QString("Paused"));
        ui.studio->stepSimulation();
        ui.studio->stopSimulation();
        ui.studio->resetSimulation();
        QCOMPARE(ui.studio->simStatus(), QString("Stopped"));
        QVERIFY(simSpy.count() >= 1);
        QVERIFY(wsSpy.count() >= 1);

        const QStringList allWs{"elements", "mixing_lab", "objects", "components", "circuits", "editor_2d", "editor_3d", "simulation", "settings"};
        for (int i = 0; i < 18; ++i) {
            ui.pipeline->setActiveWorkspace(allWs[i % allWs.size()]);
        }

        // 6. v3.0 Bundle persistence round-trip
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        const QString bundlePath = tmpDir.filePath("ui_roundtrip.shunya3.json");
        const QVariantMap saveRes = ui.pipeline->saveProjectBundle(bundlePath);
        QVERIFY(saveRes.value("ok").toBool());

        const int beforeObjs = ui.pipeline->getObjects().size();
        const QVariantMap loadRes = ui.pipeline->loadProjectBundle(bundlePath);
        QVERIFY(loadRes.value("ok").toBool());
        QCOMPARE(ui.pipeline->getObjects().size(), beforeObjs);
        QCOMPARE(ui.warnings.count(), 0);
    }

    void test_capture_visual_screenshots_when_requested() {
        QString outDir = qEnvironmentVariable("SHUNYA_SCREENSHOT_DIR");
        QTemporaryDir fallbackDir;
        if (outDir.isEmpty()) {
            QVERIFY(fallbackDir.isValid());
            outDir = fallbackDir.path();
        }
        QDir().mkpath(outDir);

        QObject holder;
        UiHarness ui;
        ui.init(&holder);
        ui.window->resize(1440, 900);
        ui.pipeline->runCircuitDc();
        ui.pipeline->runCircuitTransient(5.0, 0.25);
        for (int s = 0; s < 12; ++s) ui.studio->stepSimulation();

        QObject* theme = ui.engine->singletonInstance<QObject*>("shunya", "Theme");
        QVERIFY(theme != nullptr);

        auto snap = [&](const QString& ws, bool dark, const QString& filename) {
            theme->setProperty("isDarkMode", dark);
            ui.pipeline->setActiveWorkspace(ws);
            QTest::qWait(180);
            const QImage img = ui.window->grabWindow();
            QVERIFY(!img.isNull());
            QVERIFY(img.save(outDir + "/" + filename));
        };

        snap("elements",   true,  "01_elements_dark.png");
        snap("mixing_lab", true,  "02_mixing_dark.png");
        snap("components", true,  "03_battery_dark.png");
        snap("circuits",   true,  "04_circuits_dark.png");
        snap("editor_3d",  true,  "08_editor3d_dark.png");
        snap("simulation", true,  "05_simulation_dark.png");
        snap("elements",   false, "06_elements_light.png");
        snap("circuits",   false, "07_circuits_light.png");

        QCOMPARE(ui.warnings.count(), 0);
    }
};

QTEST_MAIN(TestShunyaUi)
#include "test_shunya_ui.moc"
