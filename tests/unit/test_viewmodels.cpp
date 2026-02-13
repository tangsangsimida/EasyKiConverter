#include <QtTest>
#include <QSignalSpy>
#include <QCoreApplication>
#include <QThreadPool>
#include "ui/viewmodels/ComponentListViewModel.h"
#include "services/ComponentService.h"
#include "core/easyeda/EasyedaApi.h"
#include "tests/common/MockNetworkAdapter.hpp"

using namespace EasyKiConverter;
using namespace EasyKiConverter::Test;

class TestViewModels : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        // 创建一个全局 Application 以支持信号/槽
        static int argc = 1;
        static char arg0[] = "test";
        static char *argv[] = {arg0, nullptr};
        new QCoreApplication(argc, argv);
    }

    void cleanupTestCase() {}

    void testComponentListAdditionRemoval() {
        auto adapter = new MockNetworkAdapter();
        auto api = new EasyedaApi(adapter);  // adapter 成为 api 的子对象
        auto service = new ComponentService(api);  // api 成为 service 的子对象
        auto viewModel = new ComponentListViewModel(service);

        QCOMPARE(viewModel->rowCount(), 0);

        const QString validId1 = "C12345";
        const QString validId2 = "C45678";

        viewModel->addComponent(validId1);
        QCoreApplication::processEvents();
        QCOMPARE(viewModel->rowCount(), 1);

        viewModel->addComponent(validId1);
        QCoreApplication::processEvents();
        QCOMPARE(viewModel->rowCount(), 1);

        viewModel->addComponent(validId2);
        QCoreApplication::processEvents();
        QCOMPARE(viewModel->rowCount(), 2);

        viewModel->removeComponent(0);
        QCOMPARE(viewModel->rowCount(), 1);

        viewModel->clearComponentList();
        QCOMPARE(viewModel->rowCount(), 0);

        // 只删除顶层对象，子对象会自动删除
        // 对象树：service -> api -> adapter
        delete viewModel;
        delete service;
    }

    void testViewModelSignalHandling() {
        auto adapter = new MockNetworkAdapter();
        const QString testId = "C12345";

        QJsonObject componentJson;
        componentJson["title"] = "Test Resistor";
        componentJson["package"] = "0603";
        QJsonObject root;
        root["result"] = componentJson;
        root["success"] = true;

        adapter->addJsonResponse("https://easyeda.com/api/products/" + testId + "/components?version=6.5.51", root);

        auto api = new EasyedaApi(adapter);
        auto service = new ComponentService(api);
        auto viewModel = new ComponentListViewModel(service);

        QSignalSpy spy(viewModel, &ComponentListViewModel::componentAdded);
        viewModel->addComponent(testId);

        QCOMPARE(spy.count(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(viewModel->rowCount(), 1, 1000);

        delete viewModel;
        delete service;
    }

    void testBatchAdditionWithUserIds() {
        auto adapter = new MockNetworkAdapter();
        auto api = new EasyedaApi(adapter);
        auto service = new ComponentService(api);
        auto viewModel = new ComponentListViewModel(service);

        QStringList ids = {
            "C8734", "C52717", "C8323", "C28730", "C35556",
            "C724040", "C432211", "C14877", "C12345", "C2040",
            "C915663", "C33993", "C2858491", "C19156", "C80713",
            "C13622", "C60420", "C507118", "C3018718", "C414042",
            "C7420375"
        };

        viewModel->addComponentsBatch(ids);

        QCOMPARE(viewModel->rowCount(), 21);

        delete viewModel;
        delete service;
    }
};

QTEST_GUILESS_MAIN(TestViewModels)
#include "test_viewmodels.moc"