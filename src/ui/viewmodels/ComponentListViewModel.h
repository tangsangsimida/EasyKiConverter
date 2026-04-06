#ifndef COMPONENTLISTVIEWMODEL_H
#define COMPONENTLISTVIEWMODEL_H

#include "models/ComponentListItemData.h"
#include "services/ComponentService.h"

#include <QAbstractListModel>
#include <QImage>
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

public:
    enum ComponentRoles { ItemDataRole = Qt::UserRole + 1 };

    explicit ComponentListViewModel(ComponentService* service, QObject* parent = nullptr);
    ~ComponentListViewModel() override;

    // QAbstractListModel
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int componentCount() const {
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

    Q_INVOKABLE void setScrolling(bool scrolling);
    Q_INVOKABLE void updateExportStatus(const QString& componentId, int previewImageExported, int datasheetExported);

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
    void handleAllImagesReady(const QString& componentId, const QStringList& imagePaths);

private:
    bool componentExists(const QString& componentId) const;
    bool validateComponentId(const QString& componentId) const;
    QStringList extractComponentIdFromText(const QString& text) const;
    ComponentListItemData* findItemData(const QString& componentId) const;
    void updateHasInvalidComponents();
    void rebuildComponentIdIndex();
    void startValidationQueue();
    void processNextValidation();
    void onValidationComplete(const QString& componentId);
    void fetchAllPreviewImages();
    void batchUpdatePreviewImages();
    void onPreviewImageEncodingDone(const QString& componentId, const QStringList& encodedImages);
    void scheduleListUpdate();  // 批量模式下的列表更新调度
    void delayedFetchPreviewImages();  // 延迟获取预览图，等待所有验证完成

private:
    ComponentService* m_service;
    QList<ComponentListItemData*> m_componentList;
    QHash<QString, int> m_componentIdIndex;
    QString m_outputPath;
    QString m_bomFilePath;
    QString m_bomResult;
    bool m_hasInvalidComponents = false;

    ValidationStateManager* m_validationStateManager;

    int m_pendingValidationCount = 0;
    QStringList m_validatedComponentIds;

    QString m_filterMode = "all";
    bool m_isScrolling = false;

    QStringList m_validationQueue;
    int m_validationPendingCount = 0;
    int m_validationCompletedCount = 0;
    int m_validationTotalCount = 0;

    QTimer* m_previewImageUpdateTimer;
    QList<QPointer<ComponentListItemData>> m_pendingPreviewImageItems;

    QThreadPool* m_encodingThreadPool;

    QStringList m_pendingComponentIds;
    QTimer* m_batchAddTimer;
    static constexpr int BATCH_ADD_SIZE = 10;

    // 批量更新模式（验证期间暂停 UI 更新）
    bool m_batchUpdateMode = false;
    QList<QPointer<ComponentListItemData>> m_batchUpdateItems;
    QTimer* m_batchUpdateTimer;

    // 列表更新批处理（合并 componentCountChanged 和 filteredCountChanged）
    bool m_batchListUpdateMode = false;
    QTimer* m_batchListUpdateTimer;

    // 延迟获取预览图定时器（等待验证全部完成）
    QTimer* m_delayedFetchPreviewTimer;
};

}  // namespace EasyKiConverter

#endif  // COMPONENTLISTVIEWMODEL_H
