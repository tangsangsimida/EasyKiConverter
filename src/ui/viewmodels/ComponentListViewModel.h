#ifndef COMPONENTLISTVIEWMODEL_H
#define COMPONENTLISTVIEWMODEL_H

#include "models/ComponentListItemData.h"
#include "services/ComponentService.h"

#include <QAbstractListModel>
#include <QImage>
#include <QMutex>
#include <QPointer>
#include <QRunnable>
#include <QSet>
#include <QStringList>
#include <QThreadPool>
#include <QTimer>

#include <functional>

namespace EasyKiConverter {

class ValidationStateManager;

// 预览图编码任务
class PreviewImageEncodeRunnable : public QRunnable {
public:
    PreviewImageEncodeRunnable(ComponentListItemData* item,
                               const QList<QImage>& images,
                               std::function<void(const QString&, const QStringList&)> callback);
    void run() override;

private:
    QPointer<ComponentListItemData> m_item;
    QList<QImage> m_images;
    std::function<void(const QString&, const QStringList&)> m_callback;
};

// 元件列表视图模型
class ComponentListViewModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int componentCount READ componentCount NOTIFY componentCountChanged)
    Q_PROPERTY(QString bomFilePath READ bomFilePath NOTIFY bomFilePathChanged)
    Q_PROPERTY(QString bomResult READ bomResult NOTIFY bomResultChanged)
    Q_PROPERTY(bool hasInvalidComponents READ hasInvalidComponents NOTIFY hasInvalidComponentsChanged)
    Q_PROPERTY(QString filterMode READ filterMode NOTIFY filterModeChanged)
    Q_PROPERTY(int filteredCount READ filteredCount NOTIFY filteredCountChanged)
    Q_PROPERTY(int validatingCount READ validatingCount NOTIFY filteredCountChanged)
    Q_PROPERTY(int validCount READ validCount NOTIFY filteredCountChanged)
    Q_PROPERTY(int invalidCount READ invalidCount NOTIFY filteredCountChanged)
    Q_PROPERTY(bool isScrolling READ isScrolling NOTIFY isScrollingChanged)
    Q_PROPERTY(bool validationReadyHint READ validationReadyHint NOTIFY attentionStateChanged)
    Q_PROPERTY(bool previewReadyHint READ previewReadyHint NOTIFY attentionStateChanged)

public:
    enum ComponentRoles { ItemDataRole = Qt::UserRole + 1 };

    explicit ComponentListViewModel(ComponentService* service, QObject* parent = nullptr);
    ~ComponentListViewModel() override;

    // QAbstractListModel
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int componentCount() const {
        QMutexLocker locker(&m_listMutex);
        return m_componentList.count();
    }

    QString bomFilePath() const {
        return m_bomFilePath;
    }

    QString bomResult() const {
        return m_bomResult;
    }

    bool hasInvalidComponents() const {
        return m_hasInvalidComponents;
    }

    QSharedPointer<ComponentData> getPreloadedData(const QString& componentId) const;
    QMap<QString, QSharedPointer<ComponentData>> getAllPreloadedData() const;

public slots:
    Q_INVOKABLE void addComponent(const QString& componentId);
    Q_INVOKABLE void removeComponent(int index);
    Q_INVOKABLE void removeComponentById(const QString& componentId);
    Q_INVOKABLE void clearComponentList();
    Q_INVOKABLE void addComponentsBatch(const QStringList& componentIds);
    void processNextBatchAdd();
    Q_INVOKABLE void pasteFromClipboard();
    Q_INVOKABLE void copyAllComponentIds();
    Q_INVOKABLE void selectBomFile(const QString& filePath);
    void fetchComponentData(const QString& componentId, bool fetch3DModel = true);
    void setOutputPath(const QString& path);

    QString outputPath() const {
        return m_outputPath;
    }

    Q_INVOKABLE void refreshComponentInfo(int index);
    Q_INVOKABLE void retryAllInvalidComponents();
    Q_INVOKABLE QStringList getAllComponentIds() const;
    Q_INVOKABLE void copyToClipboard(const QString& text);
    Q_INVOKABLE void setFilterMode(const QString& mode);

    // Getter 方法
    QString filterMode() const {
        return m_filterMode;
    }

    int filteredCount() const;
    int validatingCount() const;
    int validCount() const;
    int invalidCount() const;

    bool isScrolling() const {
        return m_isScrolling;
    }

    bool validationReadyHint() const {
        return m_validationReadyHint;
    }

    bool previewReadyHint() const {
        return m_previewReadyHint;
    }

    Q_INVOKABLE void setScrolling(bool scrolling);
    Q_INVOKABLE void fetchPreviewImages(const QStringList& componentIds);
    Q_INVOKABLE void updateExportStatus(const QString& componentId, int previewImageExported, int datasheetExported);
    Q_INVOKABLE void dismissAttentionHints();

signals:
    void componentCountChanged();
    void bomFilePathChanged();
    void bomResultChanged();
    void hasInvalidComponentsChanged();
    void filterModeChanged();
    void filteredCountChanged();
    void isScrollingChanged();
    void componentAdded(const QString& componentId, bool success, const QString& message);
    void componentRemoved(const QString& componentId);
    void listCleared();
    void pasteCompleted(int added, int skipped);
    void outputPathChanged();
    void attentionStateChanged();
    void previewFetchRequested();

private slots:
    void handleComponentInfoReady(const QString& componentId, const ComponentData& data);
    void handleCadDataReady(const QString& componentId, const ComponentData& data);
    void handleModel3DReady(const QString& uuid, const QString& filePath);
    void handleFetchError(const QString& componentId, const QString& error);
    void handleLcscDataUpdated(const QString& componentId,
                               const QString& manufacturerPart,
                               const QString& datasheetUrl,
                               const QStringList& imageUrls);
    void handleDatasheetReady(const QString& componentId, const QByteArray& datasheetData);

private:
    static bool isNonRetryableValidationError(const QString& error);
    bool componentExists(const QString& componentId) const;
    bool validateComponentId(const QString& componentId) const;
    QStringList extractComponentIdFromText(const QString& text) const;
    ComponentListItemData* findItemData(const QString& componentId) const;
    void recomputeStateCounters();
    void updateHasInvalidComponents();
    void rebuildComponentIdIndex();
    void startValidationQueue();
    void processNextValidation();
    void onValidationComplete(const QString& componentId);
    void fetchAllPreviewImages();
    void batchUpdatePreviewImages();
    void processCachePreviewImages();  // 批量处理缓存预览图（防抖）
    void onPreviewImageEncodingDone(const QString& componentId, const QStringList& encodedImages);
    void scheduleListUpdate();  // 批量模式下的列表更新调度
    void delayedFetchPreviewImages();  // 延迟获取预览图，等待所有验证完成
    void markPreviewFetchCompleted(const QString& componentId);
    void clearAttentionHints();

private:
    ComponentService* m_service;
    QList<ComponentListItemData*> m_componentList;
    QHash<QString, int> m_componentIdIndex;
    mutable QMutex m_listMutex;  // Protects m_componentList and m_componentIdIndex
    QString m_outputPath;
    QString m_bomFilePath;
    QString m_bomResult;
    bool m_hasInvalidComponents = false;

    ValidationStateManager* m_validationStateManager;

    int m_pendingValidationCount = 0;
    QStringList m_validatedComponentIds;

    QString m_filterMode = "all";
    bool m_isScrolling = false;
    int m_validatingCountCache = 0;
    int m_validCountCache = 0;
    int m_invalidCountCache = 0;
    int m_retryableInvalidCountCache = 0;

    QStringList m_validationQueue;
    QSet<QString> m_inFlightComponentIds;  // 追踪正在处理中的组件，避免重复调度
    int m_validationPendingCount = 0;
    int m_validationCompletedCount = 0;
    int m_validationTotalCount = 0;

    QTimer* m_previewImageUpdateTimer;
    QList<QPointer<ComponentListItemData>> m_pendingPreviewImageItems;
    mutable QMutex m_previewImageMutex;  // Protects m_pendingPreviewImageItems

    // 缓存预览图批量更新（防抖）
    QMap<QString, QStringList> m_pendingCachePreviewImages;
    QMap<QString, QMap<int, QString>> m_pendingIncrementalPreviewImages;
    mutable QMutex m_cachePreviewMutex;  // Protects m_pendingCachePreviewImages
    QTimer* m_cachePreviewImageTimer;

    QThreadPool* m_encodingThreadPool;

    QStringList m_pendingComponentIds;
    int m_pendingBatchValidationCount = 0;
    QTimer* m_batchAddTimer;
    // 批处理大小（BOM导入时增大以减少调度开销，配合m_bomImportMode降低UI更新频率）
    static constexpr int BATCH_ADD_SIZE = 50;

    // 批量更新模式（验证期间暂停 UI 更新）
    bool m_batchUpdateMode = false;
    QList<QPointer<ComponentListItemData>> m_batchUpdateItems;
    QTimer* m_batchUpdateTimer;

    // 列表更新批处理（合并 componentCountChanged 和 filteredCountChanged）
    bool m_batchListUpdateMode = false;
    QTimer* m_batchListUpdateTimer;

    // BOM 导入模式（限制 UI 更新频率）
    bool m_bomImportMode = false;
    QTimer* m_bomImportUpdateTimer;
    int m_bomImportPendingUpdates = 0;  // BOM 导入模式下待处理的验证完成计数

    // 延迟获取预览图定时器（等待验证全部完成）
    QTimer* m_delayedFetchPreviewTimer;
    QSet<QString> m_pendingPreviewFetchIds;
    bool m_validationReadyHint = false;
    bool m_previewReadyHint = false;
};

}  // namespace EasyKiConverter

#endif  // COMPONENTLISTVIEWMODEL_H
