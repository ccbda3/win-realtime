#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QTimer>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QCloseEvent>
#include <QLabel>
#include <QWebEngineView>
#include <QTabWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QStackedWidget>
#include <QScrollArea>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonArray>
#include <QProgressDialog>
#include <QThread>
#include <QMutex>
#include <QWebEnginePage>
#include <functional>

extern "C" void setGpsRunning(int v);
extern "C" int getGpsRunning();
extern void setCpuRender(bool v);
extern bool isCpuRender();
extern QString g_mid, g_mac, g_cpu;
extern void setDeviceType(const QString &v);
extern QString getDeviceType();
extern void setAuthPermanent(int v);
extern int getAuthPermanent();
extern void setAuthRemaining(double v);
extern double getAuthRemaining();
extern void setStoredSig(const QString &v);
extern QString getStoredSig();
extern QString reActivateLicense(const QString &filePath);
typedef void (*iq_callback)(float *data, int n_floats, void *user);
typedef void (*log_callback)(const char *msg, void *user);
typedef struct { iq_callback on_iq; log_callback on_log; void (*on_disconnect)(void *user); void *user; volatile int running; int power_level; char device_name[64]; } rf_context;
extern "C" int rf_run(rf_context *ctx);

class ToggleSwitch : public QWidget {
    Q_OBJECT
public:
    explicit ToggleSwitch(QWidget *parent = nullptr);
    bool isChecked() const { return m_checked; }
    void setChecked(bool c);

signals:
    void toggled(bool checked);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;

private:
    bool m_checked = true;
};

class SatelliteStatusWidget : public QWidget {
    Q_OBJECT
public:
    explicit SatelliteStatusWidget(QWidget *parent = nullptr);
    void setSignalLevel(float level);
    void setConnected(bool c);
    void setGpsRunning(bool r);
    void setSatellitePRN(int prn, bool active);
    int activeSatelliteCount() const;

protected:
    void paintEvent(QPaintEvent *) override;
    QSize minimumSizeHint() const override { return QSize(240, 200); }

private:
    void drawPRNGrid(QPainter &p, int w, int h);
    void drawInfoBar(QPainter &p, int w, int y, int barH);
    void drawLinkStatus(QPainter &p, int w, int y, int barH);
    float signalLevel = 0.0f;
    bool sdrConnected = false;
    bool gpsRunning = false;
    bool prnActive[33] = {false};
    QTimer *refreshTimer;
};

class UsrpThread : public QThread {
    Q_OBJECT
public:
    explicit UsrpThread(QObject *parent = nullptr);
    ~UsrpThread();
    void stopUsrp();
    void setDeviceName(const QString &name) { manualDevice = name; }
    void setPowerLevel(int level) { manualPower = level; }
    const char* deviceName() const { return ctx.device_name; }
signals:
    void iqData(const QVector<float> &samples);
    void logMsg(const QString &msg);
    void gpssimConnected();
    void gpssimDisconnected();
    void deviceDisconnected();
protected:
    void run() override;
private:
    rf_context ctx;
    QString manualDevice;
    int manualPower = 1;
    static void iqCallback(float *data, int n, void *user);
    static void logCallback(const char *msg, void *user);
    static void deviceDisconnectCallback(void *user);
};

class QTcpServer;
class QGroupBox;

class MapPage : public QWebEnginePage {
    Q_OBJECT
public:
    explicit MapPage(QObject *parent = nullptr) : QWebEnginePage(parent) {}
    std::function<void(int)> onModeSwitch;
    std::function<void(double,double)> onSinglePreview;
    std::function<void(double,double)> onRouteTarget;
    std::function<void(int,int)> onPathUpdate;
protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &message, int lineNumber, const QString &sourceID) override {
        Q_UNUSED(level); Q_UNUSED(lineNumber); Q_UNUSED(sourceID);
        if (message.startsWith("MODE_SWITCH:")) {
            int jsMode = message.mid(12).toInt();
            int cppMode = jsMode - 1;
            if (cppMode >= 0 && cppMode <= 2 && onModeSwitch) {
                onModeSwitch(cppMode);
            }
        } else if (message.startsWith("SINGLE_PREVIEW:")) {
            QStringList parts = message.mid(15).split(',');
            if (parts.size() == 2 && onSinglePreview) {
                onSinglePreview(parts[0].toDouble(), parts[1].toDouble());
            }
        } else if (message.startsWith("ROUTE_TARGET:")) {
            QStringList parts = message.mid(13).split(',');
            if (parts.size() == 2 && onRouteTarget) {
                onRouteTarget(parts[0].toDouble(), parts[1].toDouble());
            }
        } else if (message.startsWith("PATH_UPDATE:")) {
            QStringList parts = message.mid(12).split(':');
            if (parts.size() == 2 && onPathUpdate) {
                onPathUpdate(parts[0].toInt(), parts[1].toInt());
            }
        }
    }
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *e) override;
    void changeEvent(QEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *e) override;
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

public slots:
    void onStartGps();
    void onStopGps();
    void pollStatus();
    void refreshBrdc();
    void onDownloadBrdc();
    void updatePRNsFromBrdc();
    void onMapLoaded(bool ok);
    void onMapTypeChanged(const QString &type);
    void onLoadTxtPath();
    void onExportPath();
    void onImportPath();
    void onRouteExportPath();
    void onRouteImportPath();
    void onClearPath();
    void updateBeijingTime();
    void onToggleCycle();
    void setCycleOn();
    void onHeightSliderChanged(int val);
    void onSpeedSliderChanged(int val);
    void onRandomRangeChanged(int val);
    void onUsrpIq(const QVector<float> &samples);
    void onLogMsg(const QString &msg);
    void onGpssimConnected();
    void onGpssimDisconnected();
    void onModeSwitchChanged(int val);
    void onConfirmRoute();
    void onCancelRoute();
    void onConfirmSingle();
    void onCancelSingle();
    void ensureTileCached(const QString &z, const QString &x, const QString &y, const QString &tileFile, const QString &tileDir);

private:
    QNetworkAccessManager *tileNetMgr = nullptr;
    QTimer *tileRetryTimer = nullptr;
    struct TileJob { QString z, x, y, tileFile, tileDir; int cdnIdx; int retry; };
    QList<TileJob> tileJobQueue;
    QSet<QString> tileInFlight;
    QSet<QString> tileFailed;
    bool tileJobRunning = false;
    void processNextTileJob();

private:
    void log(const QString &msg, const char *tag = "UI");
    void setTracking(bool on);
    void buildLeftPanel();
    void buildRightPanel();
    void syncModeButtons();
    void doClose();

    QProcess *gpsProc = nullptr;
    QTcpServer *httpServer = nullptr;
    QWebEngineView *webView = nullptr;
    int mapRetryCount = 0;
    bool gpsEmitted = false;

    void checkForUpdate(bool manual = false);
    void downloadNextUpdateFile();
    void applyUpdateAndRestart();
    void confirmAndApply();
    QNetworkAccessManager *m_updateMgr = nullptr;
    QJsonArray m_updateFiles;
    QStringList m_updateRemove;
    int m_updateIdx = 0;
    int m_updateRetry = 0;
    QString m_remoteVersion;
    QString m_updateNote;
    QString m_updateTmpDir;
    QProgressDialog *m_updateProgress = nullptr;

    QLineEdit *searchEdit = nullptr;
    QListWidget *searchResults = nullptr;
    QNetworkAccessManager *netMgr = nullptr;
    QNetworkReply *searchReply = nullptr;

    QPushButton *btnGpsToggle = nullptr;
    QLabel *statusLight = nullptr;
    QLabel *m_statusText = nullptr;
    ToggleSwitch *realtimeCheck = nullptr;
    QLabel *beijingTimeLabel = nullptr;
    QTimer *beijingTimer = nullptr;
    QComboBox *brdcCombo = nullptr;
    QComboBox *deviceCombo = nullptr;
    bool m_bitDepth8 = false;
    QPlainTextEdit *logBox = nullptr;
    QPlainTextEdit *logMAP = nullptr;
    QPlainTextEdit *logGPS = nullptr;
    SatelliteStatusWidget *satStatus = nullptr;
    QTimer *pollTimer = nullptr;
    QTimer *posInfoTimer = nullptr;
    QLabel *gpsStatus = nullptr;
    QLabel *usrpStatus = nullptr;
    QLabel *authStatus = nullptr;
    QLabel *midLabel = nullptr;
    QLabel *sigLabel = nullptr;
    QPushButton *btnChangeAuth = nullptr;
    QPushButton *btnCopyMid = nullptr;
    QPushButton *btnCheckUpdate = nullptr;
    UsrpThread *usrpThread = nullptr;

    QSlider *heightSlider = nullptr;
    QSlider *speedSlider = nullptr;
    QLabel *posInfo = nullptr;
    QWidget *m_bottomBar = nullptr;
    QLabel *m_barLat = nullptr;
    QLabel *m_barLon = nullptr;
    QLabel *m_barAlt = nullptr;
    QLabel *m_barSpeed = nullptr;
    QLabel *m_barTime = nullptr;
    QLabel *speedLabel = nullptr;
    QLabel *heightLabel = nullptr;
    QSlider *randomRangeSlider = nullptr;
    QLabel *randRangeLabel = nullptr;
    QPushButton *btnClearPath = nullptr;
    QPushButton *btnCycle = nullptr;
    QPushButton *btnJitter = nullptr;
    QPushButton *btnAutoCenter = nullptr;
    QPushButton *btnNormalMap = nullptr;
    QPushButton *btnSatMap = nullptr;
    QPushButton *btnHybridMap = nullptr;

    QString selDevice;

    bool gpsRunning = false;
    bool usrpRunning = false;
    bool m_autoCenter = false;
    bool gpssimConnected = false;
    bool m_manualStop = false;

    QStackedWidget *modeStack = nullptr;
    QWidget *pageSingle = nullptr;
    QWidget *pageRoute = nullptr;
    QWidget *pageCustom = nullptr;
    QPushButton *btnLoadTxtPath = nullptr;
    QPushButton *btnRoutePlan = nullptr;
    QPushButton *btnRouteStart = nullptr;
    QPushButton *btnRoutePause = nullptr;
    QPushButton *btnRouteStop = nullptr;
    QPushButton *btnRouteImport = nullptr;
    QPushButton *btnRouteExport = nullptr;
    QPushButton *btnCustomStart = nullptr;
    QPushButton *btnCustomPause = nullptr;
    QPushButton *btnCustomStop = nullptr;

    double posLastLat = 0.0;
    double posLastLon = 0.0;
    QDateTime posLastTime;
    QString appDir;
    QString dataDir;
    QString xingliDir;
    QPoint m_dragPos;
    QWidget *titleBar = nullptr;
    QTimer *m_gpsRestartTimer = nullptr;
    int m_gpsRestartCount = 0;

    int httpPort = 8080;
    bool m_mapReady = false;

    int m_mode = 0;
    int m_pathPointCount = 0;
    double centerLat = 0.0, centerLon = 0.0;
    /* 后台推算相关：窗口最小化/WebEngine 冻结时，C++ 端用上次读到的位置+速度继续推进 */
    double cppLat = 0.0, cppLon = 0.0, cppHgt = 0.0;
    double cppCurrentSpeed = 0.0;
    double cppTargetSpeed = 0.0;
    QDateTime cppLastTick;
    QDateTime cppLastFrontierRead;
    QTimer *posPushTimer = nullptr; /* C++ 独立位置推进引擎，不依赖 WebEngine */
    QFile *m_logFile = nullptr;     /* 文件日志 */
    void sendUdpPos(double lat, double lon, double hgt);

    /* C++ 路径跟随引擎（完全独立于 JS，避免 WebEngine 冻结导致 GPS 信号中断） */
    std::vector<double> m_ppx, m_ppy, m_ppz;  /* 路径点 (线性单位) */
    double m_cx = 0, m_cy = 0, m_cz = 0;      /* 当前位置 (线性单位) */
    double m_tx = 0, m_ty = 0, m_tz = 0;      /* 目标路点 (线性单位) */
    double m_vx = 0, m_vy = 0, m_vz = 0;      /* 方向向量 */
    double m_cs = 0, m_ts = 0;                /* 当前速度/目标速度 km/h */
    int m_pi = 0;                             /* 路径索引 */
    bool m_pf = false;                        /* pathFollowing */
    bool m_pp = false;                        /* paused */
    bool m_cyc = true;                        /* cycleMode */
    int m_pathLen = 0;                        /* 路径点数 */
    double m_jitLon = 0, m_jitLat = 0;        /* 抖动量 */
    bool m_jitEn = false;                     /* 抖动开关 */
    double m_jitR = 10;                       /* 抖动半径 */
    void advancePathCpp();
    void startPathCpp();
    double jitterRadiusMeters = 2.0;
    QPushButton *btnMode1 = nullptr;
    QPushButton *btnMode2 = nullptr;
    QPushButton *btnMode3 = nullptr;

    QLabel *modeLabel = nullptr;
    QTimer *jitterTimer = nullptr;
    QTimer *trackingTimer = nullptr;
    QTimer *keepaliveTimer = nullptr;  /* 窗口最小化时保持 WebEngine 活跃 */
    QPushButton *btnLockTrack = nullptr;
    bool jitterEnabled = false;
    QList<QPushButton*> pathButtons;
    bool trackJitterEnabled = false;
    bool m_pathCompleted = false;
    bool m_tracking = false;
    bool userStoppedGps = false;
    bool m_syncingMode = false;

    QScrollArea *m_leftPanel = nullptr;
    QWidget *rightPanelWidget = nullptr;
    QGroupBox *gbParam = nullptr;
    QWidget *gbAuthWidget = nullptr;

    QPushButton *btnQuickPath[3] = {nullptr};
    QString quickPathName[3];
    QString quickPathFile[3];

    double m_routeTargetLat = 0.0;
    double m_routeTargetLon = 0.0;
    bool m_routePending = false;

    /* API Key management */
    QStringList m_apiKeys;
    int m_apiKeyIndex = -1;
    QString m_sessionApiKey;
    QLabel *m_apiKeyLabel = nullptr;
    QString getApiKeysPath();
    void loadApiKeys();
    void saveApiKeys();
    QString getNextApiKey();
    void apiKeyAdd(const QString &key);
    void apiKeyRemove(int index);
    void showApiKeyDialog();

    /* Map source switching */
    QString m_mapSource = "amap";
    QString m_amapSecurityCode = "ce7104c91210214f7d2e3f8d74b0485a";  /* 高德 JS API 安全码 */
    QStringList m_apiKeysTianditu;
    int m_apiKeyIndexTianditu = -1;
    QComboBox *m_mapSourceCombo = nullptr;
    QPushButton *m_btnApiKey = nullptr;
};

#endif
