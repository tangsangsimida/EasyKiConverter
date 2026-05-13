#include "core/utils/GzipUtils.h"
#include "core/utils/KiCadTableUtils.h"
#include "core/utils/LayerMapper.h"
#include "core/utils/UrlUtils.h"

#include <QByteArray>
#include <QTest>

#include <cmath>

using namespace EasyKiConverter;

class TestCoreUtils : public QObject {
    Q_OBJECT

private slots:

    void normalizePreviewImageUrls() {
        QCOMPARE(UrlUtils::normalizePreviewImageUrl(QStringLiteral(" //image.lceda.cn/files/a.png ")),
                 QStringLiteral("https://image.lceda.cn/files/a.png"));
        QCOMPARE(UrlUtils::normalizePreviewImageUrl(QStringLiteral("/web1/M00/00/00/a.jpg")),
                 QStringLiteral("https://file.elecfans.com/web1/M00/00/00/a.jpg"));
        QCOMPARE(UrlUtils::normalizePreviewImageUrl(QStringLiteral("/M00/00/00/a.jpg")),
                 QStringLiteral("https://file.elecfans.com/M00/00/00/a.jpg"));
        QCOMPARE(UrlUtils::normalizePreviewImageUrl(QStringLiteral("/some/path/a.png")),
                 QStringLiteral("https://image.lceda.cn/some/path/a.png"));
        QCOMPARE(UrlUtils::normalizePreviewImageUrl(QStringLiteral("relative/a.png")),
                 QStringLiteral("https://image.lceda.cn/relative/a.png"));
        QCOMPARE(UrlUtils::normalizePreviewImageUrl(QStringLiteral("alimg.szlcsc.com/upload/a.png")),
                 QStringLiteral("https://alimg.szlcsc.com/upload/a.png"));
    }

    void deduplicateAndNormalizeUrlsKeepsFirstOccurrenceOrder() {
        const QStringList urls = {
            QStringLiteral("//image.lceda.cn/a.png"),
            QStringLiteral("https://image.lceda.cn/a.png"),
            QStringLiteral("relative/b.png"),
            QStringLiteral(""),
            QStringLiteral(" relative/b.png "),
        };

        QCOMPARE(UrlUtils::deduplicateAndNormalizeUrls(urls),
                 QStringList({QStringLiteral("https://image.lceda.cn/a.png"),
                              QStringLiteral("https://image.lceda.cn/relative/b.png")}));
    }

    void gzipDetectionAndDecompression() {
        const QByteArray plain = QByteArrayLiteral("plain text payload long enough");
        QVERIFY(!GzipUtils::isGzipped(plain));

        const GzipUtils::DecompressResult plainResult = GzipUtils::decompress(plain);
        QVERIFY(plainResult.success);
        QCOMPARE(plainResult.data, plain);

        const QByteArray gzipped = QByteArray::fromHex("1f8b08001f3c036a02ffcb48cdc9c95748afca2c0000196ad2df0a000000");
        QVERIFY(GzipUtils::isGzipped(gzipped));

        const GzipUtils::DecompressResult gzipResult = GzipUtils::decompress(gzipped);
        QVERIFY(gzipResult.success);
        QCOMPARE(gzipResult.data, QByteArrayLiteral("hello gzip"));

        const GzipUtils::DecompressResult invalidResult = GzipUtils::decompress(QByteArray::fromHex("1f8b"));
        QVERIFY(!invalidResult.success);
        QVERIFY(invalidResult.data.isEmpty());
    }

    void kicadTableValueEscaping() {
        QCOMPARE(KiCadTableUtils::escapeTableValue(QStringLiteral("Plain")), QStringLiteral("Plain"));
        QCOMPARE(KiCadTableUtils::escapeTableValue(QStringLiteral("A \"quoted\" value")),
                 QStringLiteral("A \\\"quoted\\\" value"));
        QCOMPARE(KiCadTableUtils::escapeTableValue(QStringLiteral("C:\\KiCad\\Lib")),
                 QStringLiteral("C:\\\\KiCad\\\\Lib"));
    }

    void layerMapperMapsIdsNamesAndFallbacks() {
        const LayerMapper mapper;

        QCOMPARE(mapper.mapToKiCadLayer(1), static_cast<int>(LayerMapper::F_Cu));
        QCOMPARE(mapper.mapToKiCadLayer(2), static_cast<int>(LayerMapper::B_Cu));
        QCOMPARE(mapper.mapToKiCadLayer(3), static_cast<int>(LayerMapper::F_SilkS));
        QCOMPARE(mapper.mapToKiCadLayer(10), static_cast<int>(LayerMapper::Edge_Cuts));
        QCOMPARE(mapper.mapToKiCadLayer(9999), static_cast<int>(LayerMapper::Dwgs_User));

        QCOMPARE(mapper.mapToKiCadLayer(QStringLiteral("TopLayer")), static_cast<int>(LayerMapper::F_Cu));
        QCOMPARE(mapper.mapToKiCadLayer(QStringLiteral("BottomAssembly")), static_cast<int>(LayerMapper::B_Fab));
        QCOMPARE(mapper.mapToKiCadLayer(QStringLiteral("UnknownLayer")), static_cast<int>(LayerMapper::Dwgs_User));

        QCOMPARE(mapper.getKiCadLayerName(LayerMapper::F_Cu), QStringLiteral("F.Cu"));
        QCOMPARE(mapper.getKiCadLayerName(LayerMapper::B_Cu), QStringLiteral("B.Cu"));
        QCOMPARE(mapper.getKiCadLayerName(9999), QStringLiteral("Unknown(9999)"));
    }

    void layerMapperClassifiesLayersAndConvertsUnits() {
        QVERIFY(LayerMapper::isSignalLayer(LayerMapper::F_Cu));
        QVERIFY(LayerMapper::isSignalLayer(LayerMapper::B_Cu));
        QVERIFY(!LayerMapper::isSignalLayer(LayerMapper::F_SilkS));

        QVERIFY(LayerMapper::isSilkLayer(LayerMapper::F_SilkS));
        QVERIFY(LayerMapper::isMaskLayer(LayerMapper::B_Mask));
        QVERIFY(LayerMapper::isPasteLayer(LayerMapper::F_Paste));
        QVERIFY(LayerMapper::isMechanicalLayer(LayerMapper::Edge_Cuts));

        QVERIFY(LayerMapper::isCourtYardLayer(48));
        QVERIFY(!LayerMapper::isCourtYardLayer(47));
        QCOMPARE(LayerMapper::getCourtYardLayerName(48), QStringLiteral("F.CrtYd"));
        QVERIFY(LayerMapper::getCourtYardLayerName(47).isEmpty());

        QVERIFY(LayerMapper::isInnerLayer(15));
        QVERIFY(LayerMapper::isInnerLayer(46));
        QVERIFY(!LayerMapper::isInnerLayer(47));

        QVERIFY(LayerMapper::isPadLayer(1));
        QVERIFY(LayerMapper::isPadLayer(11));
        QVERIFY(LayerMapper::isPadLayer(15));
        QVERIFY(!LayerMapper::isPadLayer(47));

        QVERIFY(std::abs(LayerMapper::milToMm(100.0) - 2.54) < 0.0001);
        QVERIFY(std::abs(LayerMapper::mmToMil(2.54) - 100.0) < 0.001);
    }
};

QTEST_GUILESS_MAIN(TestCoreUtils)
#include "test_core_utils.moc"
