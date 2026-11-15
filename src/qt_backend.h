#pragma once

#include "linker_types.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>
#include <memory>
#include <string>

class QWindow;

namespace fl {

class QtLinkBackend : public QObject {
    Q_OBJECT

    // Global & Environment
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
    Q_PROPERTY(int activeTab READ activeTab WRITE setActiveTab NOTIFY activeTabChanged)
    Q_PROPERTY(bool isAdmin READ isAdmin NOTIFY privilegesChanged)
    Q_PROPERTY(bool isDevMode READ isDevMode NOTIFY privilegesChanged)

    // Create & Move Page
    Q_PROPERTY(int operationMode READ operationMode WRITE setOperationMode NOTIFY createStateChanged)
    Q_PROPERTY(int linkType READ linkType WRITE setLinkType NOTIFY createStateChanged)
    Q_PROPERTY(QString linkPath READ linkPath WRITE setLinkPath NOTIFY createStateChanged)
    Q_PROPERTY(QString targetPath READ targetPath WRITE setTargetPath NOTIFY createStateChanged)
    Q_PROPERTY(bool autoCreateParent READ autoCreateParent WRITE setAutoCreateParent NOTIFY createStateChanged)
    Q_PROPERTY(bool createBusy READ createBusy NOTIFY createBusyChanged)
    Q_PROPERTY(int validationLevel READ validationLevel NOTIFY validationChanged)
    Q_PROPERTY(QString validationMessage READ validationMessage NOTIFY validationChanged)
    Q_PROPERTY(QString quickFixLabel READ quickFixLabel NOTIFY validationChanged)
    Q_PROPERTY(bool canExecute READ canExecute NOTIFY validationChanged)
    Q_PROPERTY(QString commandPreview READ commandPreview NOTIFY validationChanged)
    Q_PROPERTY(QString consoleOutput READ consoleOutput NOTIFY consoleOutputChanged)
    Q_PROPERTY(QString lastCreatedLink READ lastCreatedLink NOTIFY consoleOutputChanged)

    // Inspect & Scan Page
    Q_PROPERTY(QString inspectQueryPath READ inspectQueryPath WRITE setInspectQueryPath NOTIFY inspectStateChanged)
    Q_PROPERTY(bool inspectIncludeHardLinks READ inspectIncludeHardLinks WRITE setInspectIncludeHardLinks NOTIFY inspectStateChanged)
    Q_PROPERTY(int inspectMaxDepth READ inspectMaxDepth WRITE setInspectMaxDepth NOTIFY inspectStateChanged)
    Q_PROPERTY(bool inspectScanning READ inspectScanning NOTIFY inspectScanningChanged)
    Q_PROPERTY(QString inspectStatusText READ inspectStatusText NOTIFY inspectStateChanged)
    Q_PROPERTY(bool hasSingleInspect READ hasSingleInspect NOTIFY inspectResultsChanged)
    Q_PROPERTY(QVariantMap singleInspect READ singleInspect NOTIFY inspectResultsChanged)
    Q_PROPERTY(QVariantList scanResults READ scanResults NOTIFY inspectResultsChanged)

    // History Page
    Q_PROPERTY(QString historyFilterText READ historyFilterText WRITE setHistoryFilterText NOTIFY historyStateChanged)
    Q_PROPERTY(int historyCategoryFilter READ historyCategoryFilter WRITE setHistoryCategoryFilter NOTIFY historyStateChanged)
    Q_PROPERTY(int historyTotalCount READ historyTotalCount NOTIFY historyStateChanged)
    Q_PROPERTY(QVariantList historyRows READ historyRows NOTIFY historyStateChanged)

public:
    explicit QtLinkBackend(QObject* parent = nullptr);
    ~QtLinkBackend() override;

    void attachWindow(QWindow* window);

    // Getters
    bool darkMode() const { return state_.darkTheme; }
    int activeTab() const { return state_.activeTab; }
    bool isAdmin() const { return state_.isAdmin; }
    bool isDevMode() const { return state_.isDevMode; }

    int operationMode() const { return static_cast<int>(state_.operationMode); }
    int linkType() const { return static_cast<int>(state_.linkType); }
    QString linkPath() const { return QString::fromStdString(state_.linkPath); }
    QString targetPath() const { return QString::fromStdString(state_.targetPath); }
    bool autoCreateParent() const { return state_.autoCreateParent; }
    bool createBusy() const { return createBusy_; }

    int validationLevel() const { return static_cast<int>(cachedValidation_.level); }
    QString validationMessage() const { return QString::fromStdString(cachedValidation_.message); }
    QString quickFixLabel() const { return QString::fromStdString(cachedValidation_.quickFixLabel); }
    bool canExecute() const { return cachedValidation_.canExecute; }
    QString commandPreview() const { return commandPreview_; }
    QString consoleOutput() const { return QString::fromStdString(state_.consoleOutput); }
    QString lastCreatedLink() const { return QString::fromStdString(lastCreatedLink_); }

    QString inspectQueryPath() const { return QString::fromStdString(state_.inspectQueryPath); }
    bool inspectIncludeHardLinks() const { return state_.inspectIncludeHardLinks; }
    int inspectMaxDepth() const { return state_.inspectMaxDepth; }
    bool inspectScanning() const { return inspectScanning_; }
    QString inspectStatusText() const { return QString::fromStdString(state_.inspectStatusText); }
    bool hasSingleInspect() const { return state_.hasInspectedSingle; }
    QVariantMap singleInspect() const { return singleInspectMap_; }
    QVariantList scanResults() const { return scanResultList_; }

    QString historyFilterText() const { return QString::fromStdString(state_.historyFilterText); }
    int historyCategoryFilter() const { return state_.historyCategoryFilter; }
    int historyTotalCount() const { return static_cast<int>(state_.history.size()); }
    QVariantList historyRows() const { return historyRowList_; }

    // Setters
    void setDarkMode(bool dark);
    void setActiveTab(int tab);
    void setOperationMode(int mode);
    void setLinkType(int type);
    void setLinkPath(const QString& path);
    void setTargetPath(const QString& path);
    void setAutoCreateParent(bool enabled);
    void setInspectQueryPath(const QString& path);
    void setInspectIncludeHardLinks(bool enabled);
    void setInspectMaxDepth(int depth);
    void setHistoryFilterText(const QString& text);
    void setHistoryCategoryFilter(int category);

    // Invokable actions for QML
    Q_INVOKABLE void toggleTheme();
    Q_INVOKABLE void swapPaths();
    Q_INVOKABLE void fillExample();
    Q_INVOKABLE void clearInputs();
    Q_INVOKABLE void browseLinkSave();
    Q_INVOKABLE void browseLinkFolder();
    Q_INVOKABLE void browseLinkFile();
    Q_INVOKABLE void browseTargetFolder();
    Q_INVOKABLE void browseTargetFile();
    Q_INVOKABLE void applyQuickFix();
    Q_INVOKABLE void executeCreateAction();
    Q_INVOKABLE void copyCommand();
    Q_INVOKABLE void revealCreatedLink();
    Q_INVOKABLE void inspectCreatedLink();

    Q_INVOKABLE void browseInspectFolder();
    Q_INVOKABLE void browseInspectFile();
    Q_INVOKABLE void inspectSinglePathNow();
    Q_INVOKABLE void inspectSpecificPath(const QString& path);
    Q_INVOKABLE void startDirectoryScan();
    Q_INVOKABLE void cancelDirectoryScan();
    Q_INVOKABLE void requestUnlink(const QString& path);

    Q_INVOKABLE void requestClearHistory();
    Q_INVOKABLE void backfillHistory(int sourceIndex);
    Q_INVOKABLE void inspectHistoryPath(const QString& path);

    Q_INVOKABLE void relaunchAdmin();
    Q_INVOKABLE void openDeveloperSettings();
    Q_INVOKABLE void refreshPrivileges();
    Q_INVOKABLE void revealPath(const QString& path);
    Q_INVOKABLE void copyText(const QString& text);

    Q_INVOKABLE void confirmAccepted();
    Q_INVOKABLE void confirmCanceled();

signals:
    void darkModeChanged();
    void activeTabChanged();
    void privilegesChanged();
    void createStateChanged();
    void createBusyChanged();
    void validationChanged();
    void consoleOutputChanged();
    void inspectStateChanged();
    void inspectScanningChanged();
    void inspectResultsChanged();
    void historyStateChanged();

    void confirmRequested(const QString& title, const QString& message, const QString& primaryLabel);
    void toastRequested(const QString& title, const QString& message, int tone);

private:
    void persistSettings() const;
    void refreshValidation();
    void rebuildInspectModels();
    void rebuildHistoryModel();
    void applyWindowTitlebarTheme();
    void launchCreateWorker();

    QWindow* mainWindow_ = nullptr;
    AppState state_;
    ValidationResult cachedValidation_;
    QString commandPreview_;
    QVariantMap singleInspectMap_;
    QVariantList scanResultList_;
    QVariantList historyRowList_;
    std::string lastCreatedLink_;
    bool createBusy_ = false;
    bool inspectScanning_ = false;
    std::shared_ptr<std::atomic_bool> scanCancel_;
    std::shared_ptr<std::atomic_bool> alive_ = std::make_shared<std::atomic_bool>(true);
};

} // namespace fl
