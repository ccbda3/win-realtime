#include "mainwindow.h"
static bool g_closing = false;
#ifdef Q_OS_WIN
#include <winsock2.h>
#include <windows.h>
#endif
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>

static const QStringList kBuiltinAmapKeys = {
    "3ecf56d2c71663821d44b9019ce4a9b7",
    "iSt3RxwUToDWHfU66Kru3UxKYt1bgZU2"
};
static const QStringList kBuiltinTiandituKeys = {
    "c8668a8aaf9e07cf8c21433a9d13ce0a"
};
static const QString kBuiltinSecurityCode = "ce7104c91210214f7d2e3f8d74b0485a";
#include <QSettings>
#include <QUrl>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QInputDialog>
#include <QtConcurrent>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QThread>
#include <QFileInfo>
#include <QRandomGenerator>
#include <functional>
#include <QMap>
#include <QDialog>
#include <QLineEdit>
#include <QStyleFactory>
#include <QWheelEvent>
#include <QIcon>
#include <QSizeGrip>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QPainterPath>
#include <QScrollArea>
#include <QGroupBox>
#include <QButtonGroup>
#include <QSlider>
#include <QFileDialog>
#include <QFrame>
#include <QWebEngineSettings>
#include <QWebEngineProfile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <cmath>
#include <QNetworkReply>
#include <QProcess>
#include <QProgressDialog>

#ifndef UPDATE_BASE_URL
#ifdef GPS_VERSION_GAODUAN
#define UPDATE_BASE_URL "https://pub-70aa5c65c4264947b35cc03acafcfc1d.r2.dev/update"
#else
#define UPDATE_BASE_URL "https://pub-70aa5c65c4264947b35cc03acafcfc1d.r2.dev/update/quanneng"
#endif
#endif

static void parseVersion(const QString &s, int out[3]) {
    out[0] = 1; out[1] = 0; out[2] = 0;
    QStringList parts = s.trimmed().split('.');
    int idx = 0;
    for (int i = 0; i < parts.size() && i < 3; i++) {
        bool ok = false;
        int n = parts[i].toInt(&ok);
        if (!ok) return;
        out[idx++] = n;
    }
    if (idx == 1) { out[2] = out[0]; out[0] = 0; out[1] = 0; }
}

static int compareVersion(const QString &a, const QString &b) {
    int va[3], vb[3];
    parseVersion(a, va);
    parseVersion(b, vb);
    for (int i = 0; i < 3; i++) {
        if (va[i] != vb[i]) return va[i] > vb[i] ? 1 : -1;
    }
    return 0;
}

// ===== ToggleSwitch =====
ToggleSwitch::ToggleSwitch(QWidget *parent) : QWidget(parent) {
    setFixedSize(140, 30);
    setCursor(Qt::PointingHandCursor);
}

void ToggleSwitch::setChecked(bool c) {
    if (m_checked == c) return;
    m_checked = c;
    update();
    emit toggled(c);
}

void ToggleSwitch::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    int w = width(), h = height();
    int trackH = h - 4;
    int trackW = w - 4;
    int trackY = 2;

    // Track background
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#e8ecf1"));
    p.drawRoundedRect(2, trackY, trackW, trackH, trackH / 2, trackH / 2);

    // Active side highlight
    if (m_checked) {
        p.setBrush(QColor("#005fb8"));
        p.drawRoundedRect(2, trackY, trackW / 2, trackH, trackH / 2, trackH / 2);
    } else {
        p.setBrush(QColor("#005fb8"));
        p.drawRoundedRect(trackW / 2 + 2, trackY, trackW / 2, trackH, trackH / 2, trackH / 2);
    }

    // Labels
    QFont f("Microsoft YaHei", 10, QFont::Bold);
    p.setFont(f);
    p.setPen(m_checked ? QColor("#ffffff") : QColor("#999999"));
    p.drawText(QRect(0, 0, w / 2, h), Qt::AlignCenter, "其他");
    p.setPen(m_checked ? QColor("#999999") : QColor("#ffffff"));
    p.drawText(QRect(w / 2, 0, w / 2, h), Qt::AlignCenter, "手机");
}

void ToggleSwitch::mousePressEvent(QMouseEvent *) {
    setChecked(!m_checked);
}

SatelliteStatusWidget::SatelliteStatusWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(240, 200);
    refreshTimer = new QTimer(this);
    refreshTimer->setTimerType(Qt::CoarseTimer);
    connect(refreshTimer, &QTimer::timeout, this, [this]() { update(); });
    refreshTimer->start(500);
}

void SatelliteStatusWidget::setSignalLevel(float level) {
    signalLevel = level;
    if (!refreshTimer->isActive()) update();
}
void SatelliteStatusWidget::setConnected(bool c) { sdrConnected = c; update(); }
void SatelliteStatusWidget::setGpsRunning(bool r) {
    gpsRunning = r;
    if (!r) {
        for (int i = 1; i <= 32; i++) prnActive[i] = false;
    }
    update();
}
void SatelliteStatusWidget::setSatellitePRN(int prn, bool active) {
    if (prn >= 1 && prn <= 32) { prnActive[prn] = active; update(); }
}

void SatelliteStatusWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    int w = width(), h = height();
    p.fillRect(rect(), QColor(243, 243, 243));
    int infoBarH = 28;
    int linkBarH = 16;
    int gridH = h - infoBarH - linkBarH;
    if (gridH < 40) gridH = 40;
    drawPRNGrid(p, w, gridH);
    drawInfoBar(p, w, gridH, infoBarH);
    drawLinkStatus(p, w, gridH + infoBarH, linkBarH);
}

void SatelliteStatusWidget::drawPRNGrid(QPainter &p, int w, int h) {
    const int cols = 8, rows = 4;
    int cellW = w / cols;
    int cellH = h / rows;
    int radius = qMin(cellW, cellH) / 3 - 2;
    if (radius < 3) radius = 3;

    QFont f = p.font();
    f.setPixelSize(qMax(8, radius));
    f.setBold(true);
    p.setFont(f);

    for (int i = 0; i < 32; i++) {
        int prn = i + 1;
        int col = i % cols;
        int row = i / cols;
        int cx = col * cellW + cellW / 2;
        int cy = row * cellH + cellH / 2;

        if (prnActive[prn]) {
            p.setBrush(gpsRunning ? QColor(0, 160, 70, 200) : QColor(180, 180, 180, 150));
            p.setPen(QPen(gpsRunning ? QColor(0, 220, 100) : QColor(160, 160, 160), 1));
        } else {
            p.setBrush(QColor(229, 229, 229, 180));
            p.setPen(QPen(QColor(209, 209, 209), 1));
        }
        p.drawEllipse(QPointF(cx, cy), radius, radius);

        p.setPen(prnActive[prn] ? (gpsRunning ? QColor(200, 255, 220) : QColor(160, 160, 160))
                                : QColor(209, 209, 209));
        p.drawText(QRectF(cx - radius, cy - radius, radius*2, radius*2),
                   Qt::AlignCenter, QString::number(prn));
    }
}

void SatelliteStatusWidget::drawInfoBar(QPainter &p, int w, int y, int barH) {
    p.fillRect(0, y, w, barH, QColor(229, 229, 229));
    int sigX = 6;
    int sigY = y + 4;
    int sigW = 60;
    int sigH = barH - 8;

    p.setPen(QColor(209, 209, 209));
    p.setBrush(QColor(250, 250, 250));
    p.drawRect(sigX, sigY, sigW, sigH);

    int fillW = (int)(sigW * signalLevel);
    if (fillW > 0) {
        QColor c = signalLevel > 0.7f ? QColor(0, 200, 80) :
                   signalLevel > 0.3f ? QColor(200, 180, 0) : QColor(200, 50, 50);
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawRect(sigX + 1, sigY + 1, fillW - 1, sigH - 2);
    }

    QFont f = p.font(); f.setPixelSize(9); p.setFont(f);
    p.setPen(QColor(97, 97, 97));
    p.drawText(sigX + sigW + 4, y + 12, "信号");

    int active = 0;
    for (int i = 1; i <= 32; i++) if (prnActive[i]) active++;
    p.drawText(w / 2 - 40, y + 12, 80, 12, Qt::AlignCenter,
               QString("已锁定 %1").arg(active));

    QString status;
    QColor statusColor;
    if (gpsRunning && sdrConnected) {
        status = "已发射"; statusColor = QColor(0, 200, 80);
    } else if (gpsRunning) {
        status = "等待SDR"; statusColor = QColor(200, 180, 0);
    } else {
        status = "待命"; statusColor = QColor(160, 160, 160);
    }
    p.setPen(statusColor);
    p.drawText(w - 56, y + 12, 50, 12, Qt::AlignRight, status);
}

void SatelliteStatusWidget::drawLinkStatus(QPainter &p, int w, int y, int barH) {
    p.fillRect(0, y, w, barH, QColor(243, 243, 243));
    struct { int x; bool ok; const char *label; } links[] = {
        {w/6,       gpsRunning,   "GPS"},
        {w/2,       sdrConnected, "SDR"},
        {w*5/6,     true,         "天线"},
    };
    QFont f = p.font(); f.setPixelSize(8); p.setFont(f);
    for (auto &l : links) {
        p.setPen(l.ok ? QColor(0, 180, 70) : QColor(180, 50, 50));
        p.setBrush(l.ok ? QColor(0, 200, 80) : QColor(200, 50, 50));
        p.drawEllipse(QPointF(l.x - 12, y + barH/2), 2, 2);
        p.drawText(l.x - 16, y + 1, 32, barH, Qt::AlignCenter, l.label);
    }
}

class ChartWheelForwarder : public QObject {
    QMap<QWidget*, SatelliteStatusWidget*> mapping;
    bool forwarding = false;
public:
    void addMapping(QWidget *container, SatelliteStatusWidget *chart) {
        mapping[container] = chart;
        container->installEventFilter(this);
    }
    bool eventFilter(QObject *obj, QEvent *e) override {
        if (forwarding) return false;
        if (e->type() == QEvent::Wheel) {
            QWidget *w = qobject_cast<QWidget*>(obj);
            if (w && mapping.contains(w)) {
                forwarding = true;
                QApplication::sendEvent(mapping[w], e);
                forwarding = false;
                return true;
            }
        }
        return QObject::eventFilter(obj, e);
    }
};

UsrpThread::UsrpThread(QObject *parent) : QThread(parent) {
    memset(&ctx, 0, sizeof(ctx));
    ctx.on_iq = iqCallback;
    ctx.on_log = logCallback;
    ctx.on_disconnect = deviceDisconnectCallback;
    ctx.user = this;
    ctx.running = 0;
}
UsrpThread::~UsrpThread() { stopUsrp(); }
void UsrpThread::stopUsrp() { ctx.running = 0; wait(8000); }
void UsrpThread::run() {
    ctx.running = 1;
    strncpy(ctx.device_name, manualDevice.toLatin1().constData(), sizeof(ctx.device_name)-1);
    ctx.power_level = manualPower;
    rf_run(&ctx);
}
void UsrpThread::iqCallback(float *data, int n, void *user) {
    UsrpThread *self = (UsrpThread *)user;
    QVector<float> vec(n);
    memcpy(vec.data(), data, n * sizeof(float));
    emit self->iqData(vec);
}
void UsrpThread::logCallback(const char *msg, void *user) {
    UsrpThread *self = (UsrpThread *)user;
    emit self->logMsg(QString::fromUtf8(msg));
}
void UsrpThread::deviceDisconnectCallback(void *user) {
    UsrpThread *self = (UsrpThread *)user;
    if (self) emit self->deviceDisconnected();
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    appDir = QApplication::applicationDirPath();
    dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/PosNavTech";
    QDir().mkpath(dataDir);
    xingliDir = dataDir + "/xingli";
    QDir().mkpath(xingliDir);
    QDir().mkpath(appDir + "/手动星历");

    {
        QSettings oldS(appDir + "/settings.ini", QSettings::IniFormat);
        QSettings newS(dataDir + "/settings.ini", QSettings::IniFormat);
        if (!newS.contains("deviceType") && oldS.contains("deviceType"))
            newS.setValue("deviceType", oldS.value("deviceType"));
        if (!newS.contains("savedPathFile") && oldS.contains("savedPathFile"))
            newS.setValue("savedPathFile", oldS.value("savedPathFile"));
    }

    resize(1280, 720);
    setMinimumSize(1100, 600);
    setWindowTitle("行迹自录");
    setWindowFlags(Qt::FramelessWindowHint);
    QIcon appIcon(appDir + "/卫星.png");
    if (appIcon.isNull()) appIcon = QIcon(appDir + "/appicon.ico");
    setWindowIcon(appIcon);
    qApp->setWindowIcon(appIcon);

    QWidget *central = new QWidget;
    central->setStyleSheet("background:#e8ecf1");
    setCentralWidget(central);
    QVBoxLayout *rootV = new QVBoxLayout(central);
    rootV->setContentsMargins(0, 0, 0, 0);
    rootV->setSpacing(0);

    titleBar = new QWidget;
    titleBar->setFixedHeight(40);
    titleBar->setStyleSheet("background:#e8ecf1;border-bottom:1px solid #d0d5dd");
    QHBoxLayout *titleL = new QHBoxLayout(titleBar);
    titleL->setContentsMargins(6, 0, 2, 0);
    titleL->setSpacing(4);

    QLabel *iconLbl = new QLabel;
    QPixmap iconPx(appDir + "/卫星.png");
    if (iconPx.isNull()) iconPx = QPixmap(appDir + "/appicon.ico");
    if (!iconPx.isNull())
        iconLbl->setPixmap(iconPx.scaled(18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLbl->setFixedSize(18, 18);
    titleL->addWidget(iconLbl);

    QString localVerStr;
    {
        QFile vf(appDir + "/version.txt");
        if (vf.open(QIODevice::ReadOnly)) {
            localVerStr = QString::fromUtf8(vf.readAll()).trimmed();
            vf.close();
        }
        bool isNum = false;
        int verNum = localVerStr.toInt(&isNum);
        if (isNum && verNum >= 10000) {
            localVerStr = QString("%1.%2.%3").arg(verNum / 10000).arg((verNum / 100) % 100).arg(verNum % 100);
            QFile vf2(appDir + "/version.txt");
            if (vf2.open(QIODevice::WriteOnly)) {
                vf2.write(localVerStr.toUtf8());
                vf2.close();
            }
        }
    }
    QLabel *titleText = new QLabel(localVerStr.isEmpty() ? "行迹自录" : QString("行迹自录 v%1").arg(localVerStr));
    titleText->setStyleSheet("font-size:15px;color:#1a1a1a;background:transparent");
    titleL->addWidget(titleText);
    titleL->addSpacing(8);

    authStatus = new QLabel("●授权获取中...");
    authStatus->setStyleSheet("font-size:10px;color:#005fb8;background:transparent;padding:1px 4px");
    authStatus->setFixedHeight(16);
    titleL->addWidget(authStatus);
    {
        QLabel *renderLbl = new QLabel(isCpuRender() ? "渲染:CPU" : "渲染:GPU");
        renderLbl->setStyleSheet(isCpuRender()
            ? "font-size:10px;color:#f59e0b;background:transparent;padding:1px 4px"
            : "font-size:10px;color:#22c55e;background:transparent;padding:1px 4px");
        renderLbl->setFixedHeight(16);
        titleL->addWidget(renderLbl);
    }
    std::function<void()> refreshAuthStatus = [this]() {
        QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
        QString licType = s.value("licType").toString();
        if (licType == "perm") {
            authStatus->setText("●永久授权");
            authStatus->setStyleSheet("font-size:10px;color:#22c55e;background:transparent;padding:1px 4px");
        } else if (licType == "24h" || licType == "48h") {
            qint64 activatedAt = s.value("licActivatedAt", 0).toLongLong();
            qint64 hours = (licType == "24h") ? 24 : 48;
            qint64 elapsed = (QDateTime::currentSecsSinceEpoch() - activatedAt) / 3600;
            if (elapsed >= hours) {
                authStatus->setText("●授权已过期");
                authStatus->setStyleSheet("font-size:10px;color:#ef4444;background:transparent;padding:1px 4px");
            } else {
                double remain = hours - elapsed;
                authStatus->setText(QString("●剩余%1小时").arg(remain, 0, 'f', 1));
                authStatus->setStyleSheet("font-size:10px;color:#22c55e;background:transparent;padding:1px 4px");
            }
        } else {
            authStatus->setText("●未授权");
            authStatus->setStyleSheet("font-size:10px;color:#f59e0b;background:transparent;padding:1px 4px");
        }
    };
    refreshAuthStatus();
    usrpStatus = new QLabel("●设备待命");
    usrpStatus->setStyleSheet("font-size:10px;color:#616161;background:transparent;padding:1px 4px");
    usrpStatus->setFixedHeight(16);
    titleL->addWidget(usrpStatus);
    titleL->addSpacing(4);

    sigLabel = new QLabel(QString::fromUtf8("\u7b7e\u540d: ..."));
    sigLabel->setObjectName("licenseSig");
    sigLabel->setStyleSheet("font-size:10px;color:#616161;background:transparent");
    sigLabel->setFixedHeight(16);
    titleL->addWidget(sigLabel);

    btnChangeAuth = new QPushButton(QString::fromUtf8("\u66f4\u6362"));
    btnChangeAuth->setFixedSize(38, 18);
    btnChangeAuth->setCursor(Qt::PointingHandCursor);
    btnChangeAuth->setStyleSheet("QPushButton{background:transparent;color:#005fb8;border:1px solid #d0d5dd;border-radius:3px;padding:0;font-size:9px}QPushButton:hover{background:#ffffff}");
    titleL->addWidget(btnChangeAuth);

    midLabel = new QLabel(QString::fromUtf8("\u673a\u5668\u7801: ") + g_mid.left(8) + QString::fromUtf8("..."));
    midLabel->setStyleSheet("font-size:10px;color:#616161;background:transparent");
    midLabel->setFixedHeight(16);
    titleL->addWidget(midLabel);

    btnCopyMid = new QPushButton(QString::fromUtf8("\u590d\u5236"));
    btnCopyMid->setFixedSize(30, 16);
    btnCopyMid->setCursor(Qt::PointingHandCursor);
    btnCopyMid->setStyleSheet("QPushButton{background:transparent;color:#005fb8;border:1px solid #d0d5dd;border-radius:3px;padding:0;font-size:8px}QPushButton:hover{background:#ffffff}");
    titleL->addWidget(btnCopyMid);
    connect(btnCopyMid, &QPushButton::clicked, this, [this]() {
        QString mid;
        for (int i = 0; i < g_mid.size(); i++) {
            if (i > 0 && i % 4 == 0) mid += '-';
            mid += g_mid[i];
        }
        QApplication::clipboard()->setText(mid);
        QMessageBox::information(nullptr, QString::fromUtf8("\u5df2\u590d\u5236"), QString::fromUtf8("\u673a\u5668\u7801\u5df2\u590d\u5236\u5230\u526a\u8d34\u677f:\n") + mid);
    });
    connect(btnChangeAuth, &QPushButton::clicked, this, [this, refreshAuthStatus]() {
        QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        QString path = QFileDialog::getOpenFileName(nullptr, QString::fromUtf8("\u9009\u62e9\u65b0\u6388\u6743\u6587\u4ef6"), desktop + "/", QString::fromUtf8("\u6388\u6743\u6587\u4ef6 (*.lic)"));
        if (path.isEmpty()) return;
        QString result = reActivateLicense(path);
        QString title = result.startsWith(QString::fromUtf8("\u9a8c\u8bc1\u5931\u8d25")) || result.startsWith(QString::fromUtf8("\u8be5\u6388\u6743\u6587\u4ef6")) ? QString::fromUtf8("\u66f4\u6362\u5931\u8d25") : QString::fromUtf8("\u66f4\u6362\u6210\u529f");
        QMessageBox::information(nullptr, title, result);
        if (!result.startsWith(QString::fromUtf8("\u9a8c\u8bc1\u5931\u8d25")) && !result.startsWith(QString::fromUtf8("\u8be5\u6388\u6743\u6587\u4ef6")))
            refreshAuthStatus();
    });
    /* API Key management button */
    m_btnApiKey = new QPushButton("密钥");
    m_btnApiKey->setFixedSize(50, 18);
    m_btnApiKey->setCursor(Qt::PointingHandCursor);
    m_btnApiKey->setStyleSheet("QPushButton{background:transparent;color:#005fb8;border:1px solid #d0d5dd;border-radius:3px;padding:0 2px;font-size:9px}QPushButton:hover{background:#ffffff}");
    titleL->addWidget(m_btnApiKey);
    connect(m_btnApiKey, &QPushButton::clicked, this, &MainWindow::showApiKeyDialog);

    /* Map source selector - Gaode and Tianditu */
    m_mapSourceCombo = new QComboBox;
    m_mapSourceCombo->addItem("高德地图", "amap");
    m_mapSourceCombo->addItem("天地图", "tianditu");
    m_mapSourceCombo->setFixedHeight(18);
    m_mapSourceCombo->setStyleSheet("QComboBox{background:#ffffff;border:1px solid #a0a4ab;border-radius:3px;padding:0 2px;font-size:9px;color:#1a1a1a}QComboBox:hover{background:#f0f2f5}QComboBox::drop-down{width:14px}QComboBox QAbstractItemView{background:#ffffff;color:#1a1a1a;selection-background-color:#e8ecf1}");
    titleL->addWidget(m_mapSourceCombo);
    btnCheckUpdate = new QPushButton("检查更新");
    btnCheckUpdate->setFixedSize(56, 18);
    btnCheckUpdate->setCursor(Qt::PointingHandCursor);
    btnCheckUpdate->setStyleSheet("QPushButton{background:#005fb8;color:#ffffff;border:none;border-radius:3px;padding:0;font-size:9px}QPushButton:hover{background:#004a91}");
    titleL->addWidget(btnCheckUpdate);
    connect(btnCheckUpdate, &QPushButton::clicked, this, [this]() { checkForUpdate(true); });
    connect(m_mapSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        m_mapSource = m_mapSourceCombo->currentData().toString();
        QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
        s.setValue("mapSource", m_mapSource);
        m_amapSecurityCode = s.value("amapSecurityCode", kBuiltinSecurityCode).toString();
        m_btnApiKey->setEnabled(true);
        m_btnApiKey->setToolTip("");
        loadApiKeys();
        if (m_mapSource == "tianditu") {
            if (m_apiKeysTianditu.isEmpty()) m_apiKeysTianditu << "c8668a8aaf9e07cf8c21433a9d13ce0a";
            if (m_apiKeyIndexTianditu < 0 || m_apiKeyIndexTianditu >= m_apiKeysTianditu.size()) m_apiKeyIndexTianditu = 0;
            m_sessionApiKey = m_apiKeysTianditu[m_apiKeyIndexTianditu];
            m_apiKeyIndexTianditu = (m_apiKeyIndexTianditu + 1) % m_apiKeysTianditu.size();
        } else {
            if (m_apiKeys.isEmpty()) m_apiKeys = kBuiltinAmapKeys;
            if (m_apiKeyIndex < 0 || m_apiKeyIndex >= m_apiKeys.size()) m_apiKeyIndex = 0;
            m_sessionApiKey = m_apiKeys[m_apiKeyIndex];
            m_apiKeyIndex = (m_apiKeyIndex + 1) % m_apiKeys.size();
        }
        saveApiKeys();
        if (m_apiKeyLabel) m_apiKeyLabel->setText("Key:" + m_sessionApiKey.left(5));
        if (m_btnApiKey) m_btnApiKey->setText(m_sessionApiKey.left(8) + "...");
        /* Switch map source */
        if (m_mapReady && webView && webView->page()) {
            QString js = QString("qtSwitchMapSource('%1','%2')").arg(m_mapSource).arg(m_sessionApiKey);
            webView->page()->runJavaScript(js);
        }
    });

    QSettings qs(dataDir + "/settings.ini", QSettings::IniFormat);
    for (int i = 0; i < 3; i++) {
        quickPathName[i] = qs.value(QString("quickPathName%1").arg(i), QString("路径%1").arg(i+1)).toString();
        quickPathFile[i] = qs.value(QString("quickPathFile%1").arg(i)).toString();
    }

    std::function<void(int)> loadQuickPath = [this](int idx) {
        if (quickPathFile[idx].isEmpty()) {
            log("快捷路径" + QString::number(idx+1) + " 未设置文件");
            return;
        }
        QFile file(quickPathFile[idx]);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            log("无法打开: " + quickPathFile[idx]);
            return;
        }
        QString content = QString::fromUtf8(file.readAll());
        file.close();
        QString jsonStr = QString::fromUtf8(QJsonDocument(QJsonArray{content}).toJson(QJsonDocument::Compact));
        jsonStr = jsonStr.mid(1, jsonStr.length() - 2);
        QString js = QString("qtImportPath(%1)").arg(jsonStr);
        QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
        s.setValue("savedPathFile", quickPathFile[idx]);
        s.sync();
        webView->page()->runJavaScript(js, [this](const QVariant &v) {
            QString result = v.toString();
            log(result);
            if (result.startsWith("SPEED:")) {
                int sep = result.indexOf('|');
                if (sep > 0) {
                    QString speedStr = result.mid(6, sep - 6);
                    bool ok = false;
                    double speedKmH = speedStr.toDouble(&ok);
                    if (ok && speedKmH >= 0 && speedSlider) {
                        if (speedSlider) speedSlider->setValue(qBound(0, (int)speedKmH, 300));
                    }
                }
            }
            if (btnCycle) setCycleOn();
        });
        log("已加载 " + quickPathName[idx] + ": " + QFileInfo(quickPathFile[idx]).fileName());
    };

    std::function<void(int)> editQuickPath = [this](int idx) {
        QDialog dlg(this);
        dlg.setWindowTitle("编辑快捷路径");
        dlg.setFixedSize(400, 180);
        dlg.setStyleSheet("background:#ffffff;color:#1a1a1a;font-size:13px");
        auto *vl = new QVBoxLayout(&dlg);
        vl->setSpacing(8);

        auto *nameEdit = new QLineEdit(quickPathName[idx]);
        nameEdit->setStyleSheet("QLineEdit{background:#ffffff;color:#1a1a1a;border:1px solid #d0d5dd;border-radius:4px;padding:6px 8px;font-size:13px}");
        nameEdit->selectAll();
        auto *pathEdit = new QLineEdit(quickPathFile[idx]);
        pathEdit->setReadOnly(true);
        pathEdit->setStyleSheet("QLineEdit{background:#f5f5f5;color:#616161;border:1px solid #d0d5dd;border-radius:4px;padding:6px 8px;font-size:12px}");

        auto *btnBrowse = new QPushButton("选择文件...");
        btnBrowse->setStyleSheet("QPushButton{background:#0f7b0f;color:#fff;border:none;border-radius:4px;padding:6px 16px;font-size:12px}QPushButton:hover{background:#107c10}");
        connect(btnBrowse, &QPushButton::clicked, &dlg, [&]() {
            QString fp = QFileDialog::getOpenFileName(&dlg, "选择JSON路径文件", QString(), "JSON文件 (*.json);;所有文件(*)");
            if (!fp.isEmpty()) pathEdit->setText(fp);
        });

        auto *hl = new QHBoxLayout;
        hl->addWidget(pathEdit, 1);
        hl->addWidget(btnBrowse);

        auto *btnSave = new QPushButton("保存");
        btnSave->setStyleSheet("QPushButton{background:#0f7b0f;color:#fff;border:none;border-radius:4px;padding:8px 20px;font-size:13px}QPushButton:hover{background:#107c10}");
        auto *btnCancel = new QPushButton("取消");
        btnCancel->setStyleSheet("QPushButton{background:#c42b1c;color:#fff;border:none;border-radius:4px;padding:8px 20px;font-size:13px}QPushButton:hover{background:#a42617}");

        auto *hl2 = new QHBoxLayout;
        hl2->addStretch();
        hl2->addWidget(btnSave);
        hl2->addWidget(btnCancel);

        vl->addWidget(new QLabel("名称:"));
        vl->addWidget(nameEdit);
        vl->addWidget(new QLabel("文件:"));
        vl->addLayout(hl);
        vl->addLayout(hl2);

        connect(btnSave, &QPushButton::clicked, &dlg, &QDialog::accept);
        connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);

        if (dlg.exec() == QDialog::Accepted) {
            quickPathName[idx] = nameEdit->text().trimmed();
            quickPathFile[idx] = pathEdit->text().trimmed();
            if (quickPathName[idx].isEmpty()) quickPathName[idx] = QString("路径%1").arg(idx+1);
            btnQuickPath[idx]->setText(quickPathName[idx]);
            QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
            s.setValue(QString("quickPathName%1").arg(idx), quickPathName[idx]);
            s.setValue(QString("quickPathFile%1").arg(idx), quickPathFile[idx]);
            s.sync();
        }
    };

    for (int i = 0; i < 3; i++) {
        btnQuickPath[i] = new QPushButton(quickPathName[i]);
        btnQuickPath[i]->setFixedSize(60, 18);
        btnQuickPath[i]->setCursor(Qt::PointingHandCursor);
        btnQuickPath[i]->setStyleSheet(
            "QPushButton{background:#ffffff;border:1px solid #d0d5dd;border-radius:3px;padding:0 4px;color:#1a1a1a;font-size:9px;text-align:center}"
            "QPushButton:hover{background:#f0f4f8}");
        btnQuickPath[i]->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(btnQuickPath[i], &QPushButton::clicked, this, [this, i, loadQuickPath]() { loadQuickPath(i); });
        connect(btnQuickPath[i], &QPushButton::customContextMenuRequested, this, [this, i, editQuickPath](const QPoint &) { editQuickPath(i); });
        titleL->addWidget(btnQuickPath[i]);
    }

    titleL->addStretch();

    QTimer *authTimer = new QTimer(this);
    connect(authTimer, &QTimer::timeout, this, [this, refreshAuthStatus, authTimer]() {
        authTimer->setInterval(60000);
        refreshAuthStatus();
    });
    authTimer->start(0);
    authTimer->setTimerType(Qt::CoarseTimer);

    QPushButton *btnLogToggle = new QPushButton("📋");
    btnLogToggle->setFixedSize(28, 22);
    btnLogToggle->setCursor(Qt::ArrowCursor);
    btnLogToggle->setCheckable(true);
    btnLogToggle->setStyleSheet(
        "QPushButton{background:#e8ecf1;border:1px solid #d0d5dd;border-radius:3px;color:#616161;font-size:11px;padding:0}"
        "QPushButton:hover{background:#d0d5dd;color:#1a1a1a}"
        "QPushButton:checked{background:#d0d5dd;color:#005fb8}");

    auto mkTtlBtn = [&](const QString &text, const QString &color) -> QPushButton * {
        QPushButton *b = new QPushButton(text);
        b->setFixedSize(28, 22);
        b->setCursor(Qt::ArrowCursor);
        b->setStyleSheet(QString(
            "QPushButton{background:#e8ecf1;border:1px solid #d0d5dd;border-radius:3px;color:%1;font-size:13px;padding:0}"
            "QPushButton:hover{background:%2}").arg(color, color == "#ef4444" ? "#c42b1c20" : "#d0d5dd"));
        return b;
    };
    QPushButton *btnMin = mkTtlBtn("─", "#1a1a1a");
    QPushButton *btnMax = mkTtlBtn("□", "#1a1a1a");
    QPushButton *btnClose = mkTtlBtn("✕", "#ef4444");
    connect(btnMin, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(btnMax, &QPushButton::clicked, this, [this]() {
        if (isMaximized()) showNormal(); else showMaximized();
    });
    connect(btnClose, &QPushButton::clicked, this, &QWidget::close);
    titleL->addWidget(btnLogToggle);
    titleL->addWidget(btnMin);
    titleL->addWidget(btnMax);
    titleL->addWidget(btnClose);

    rootV->addWidget(titleBar);
    titleBar->installEventFilter(this);

    QWidget *body = new QWidget;
    QVBoxLayout *bodyL = new QVBoxLayout(body);
    bodyL->setContentsMargins(0, 0, 0, 0);
    bodyL->setSpacing(0);
    QHBoxLayout *mainH = new QHBoxLayout;
    mainH->setContentsMargins(4, 4, 4, 0);
    mainH->setSpacing(4);
    bodyL->addLayout(mainH, 1);

    m_leftPanel = new QScrollArea;
    m_leftPanel->setWidgetResizable(true);
    m_leftPanel->setFixedWidth(290);
    m_leftPanel->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_leftPanel->setStyleSheet("QScrollArea{background:#e8ecf1;border:none}");

    QWidget *leftCol = new QWidget;
    leftCol->setStyleSheet("background:#e8ecf1;");
    QVBoxLayout *leftVl = new QVBoxLayout(leftCol);
    leftVl->setSpacing(4);
    leftVl->setContentsMargins(4, 2, 2, 2);

    netMgr = new QNetworkAccessManager(this);

    QGroupBox *gbTx = new QGroupBox(QStringLiteral("📡 发射器"));
    gbTx->setStyleSheet(
        "QGroupBox{background:#ffffff;border:1px solid #d0d5dd;border-radius:6px;"
        "margin-top:12px;padding-top:12px;padding-left:6px;padding-right:6px;"
        "padding-bottom:4px;font-size:11px;color:#005fb8;font-weight:bold}"
        "QGroupBox::title{subcontrol-origin:margin;left:6px;padding:0 3px}");
    QVBoxLayout *vTx = new QVBoxLayout(gbTx);
    vTx->setContentsMargins(3, 3, 3, 3);
    vTx->setSpacing(4);

#ifndef GPS_VERSION_GAODUAN
    QLabel *brdcLbl = new QLabel("星历");
    brdcLbl->setStyleSheet("font-size:11px;color:#616161;padding:0;margin:0;background:transparent");
    vTx->addWidget(brdcLbl);
    brdcCombo = new QComboBox;
    brdcCombo->setFixedHeight(24);
    brdcCombo->setStyleSheet("QComboBox{background:#ffffff;color:#1a1a1a;border:1px solid #d0d5dd;border-radius:4px;padding:2px 8px;font-size:12px}"
                             "QComboBox QAbstractItemView{background:#ffffff;color:#1a1a1a;border:1px solid #d0d5dd;selection-background-color:#005fb8;selection-color:#ffffff}");
    vTx->addWidget(brdcCombo);
#endif

#ifndef GPS_VERSION_GAODUAN
    deviceCombo = new QComboBox;
    deviceCombo->addItem("USRP B210", "b210");
    deviceCombo->addItem("USRP B200mini", "b200mini");
    deviceCombo->addItem("HackRF", "hackrf");
    deviceCombo->addItem("PlutoSDR", "pluto");
    deviceCombo->setFixedHeight(24);
    deviceCombo->setMaxVisibleItems(4);
    deviceCombo->setStyleSheet("QComboBox{background:#ffffff;color:#1a1a1a;border:1px solid #d0d5dd;border-radius:4px;padding:2px 8px;font-size:12px}"
                               "QComboBox QAbstractItemView{background:#ffffff;color:#1a1a1a;border:1px solid #d0d5dd;selection-background-color:#f0f4f8}");
    deviceCombo->setCurrentIndex(deviceCombo->findData("pluto"));
    vTx->addWidget(deviceCombo);
    connect(deviceCombo, &QComboBox::currentIndexChanged, [this](int) {
        QString dev = deviceCombo->currentData().toString();
        setDeviceType(dev);
        QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
        s.setValue("deviceType", dev);
        s.sync();
    });
    {
        QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
        QString savedDev = s.value("deviceType").toString();
        int di = deviceCombo->findData(savedDev);
        if (di >= 0) deviceCombo->setCurrentIndex(di);
    }
#else
    // 高端版固定使用 PlutoSDR
    setDeviceType("pluto");
#endif

#ifndef GPS_VERSION_GAODUAN
    // Toggle switch: 手机模式 <-> 其他设备
    realtimeCheck = new ToggleSwitch;
    realtimeCheck->setChecked(true);
    vTx->addWidget(realtimeCheck);
    connect(realtimeCheck, &ToggleSwitch::toggled, this, [this](bool checked) {
        QSettings s(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/PosNavTech/settings.ini", QSettings::IniFormat);
        s.setValue("realtimeDefault", checked);
        s.sync();
    });
    {
        QLabel *depthLbl = new QLabel("采样位深");
        depthLbl->setStyleSheet("font-size:11px;color:#616161;padding:0;margin:0;background:transparent");
        vTx->addWidget(depthLbl);
        auto mkDepthBtn = [&](const QString &text, const QString &color) -> QPushButton * {
            QPushButton *b = new QPushButton(text);
            b->setFixedHeight(24);
            b->setCursor(Qt::PointingHandCursor);
            b->setCheckable(true);
            b->setStyleSheet(QString(
                "QPushButton{background:#ffffff;border:2px solid %1;border-radius:4px;color:%1;font-size:11px;font-weight:bold}"
                "QPushButton:checked{background:%1;color:#fff}"
                "QPushButton:hover{background:%2}").arg(color, color + "20"));
            return b;
        };
        QPushButton *btnDepth16 = mkDepthBtn("16位", "#005fb8");
        QPushButton *btnDepth8 = mkDepthBtn("8位", "#f59e0b");
        QButtonGroup *depthGrp = new QButtonGroup(this);
        depthGrp->setExclusive(true);
        depthGrp->addButton(btnDepth16);
        depthGrp->addButton(btnDepth8);
        QHBoxLayout *hDepth = new QHBoxLayout;
        hDepth->setSpacing(6);
        hDepth->addWidget(btnDepth16);
        hDepth->addWidget(btnDepth8);
        vTx->addLayout(hDepth);
        int savedDepth = 16;
        {
            QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
            savedDepth = s.value("bitDepth", 16).toInt();
        }
        m_bitDepth8 = (savedDepth == 8);
        (m_bitDepth8 ? btnDepth8 : btnDepth16)->setChecked(true);
        connect(depthGrp, &QButtonGroup::buttonClicked, this, [this](QAbstractButton *b) {
            m_bitDepth8 = (b->text() == "8位");
            QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
            s.setValue("bitDepth", m_bitDepth8 ? 8 : 16);
            s.sync();
        });
    }
#endif

#ifdef GPS_VERSION_GAODUAN
    {
        QLabel *pwrLbl = new QLabel("功率");
        pwrLbl->setStyleSheet("font-size:11px;color:#616161;padding:0;margin:0;background:transparent");
        auto mkPwrBtn = [&](const QString &text, int val, const QString &color) -> QPushButton * {
            QPushButton *b = new QPushButton(text);
            b->setFixedHeight(24);
            b->setCursor(Qt::PointingHandCursor);
            b->setCheckable(true);
            b->setStyleSheet(QString(
                "QPushButton{background:#ffffff;border:2px solid %1;border-radius:4px;color:%1;font-size:11px;font-weight:bold}"
                "QPushButton:checked{background:%1;color:#fff}"
                "QPushButton:hover{background:%2}").arg(color, color + "20"));
            return b;
        };
        QPushButton *btnPwrS = mkPwrBtn("小", 2, "#22c55e");
        QPushButton *btnPwrM = mkPwrBtn("中", 5, "#f59e0b");
        QPushButton *btnPwrL = mkPwrBtn("大", 10, "#ef4444");
        int savedPwr = 5;
        {
            QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
            savedPwr = s.value("powerLevel", 5).toInt();
        }
        if (savedPwr <= 2) btnPwrS->setChecked(true);
        else if (savedPwr <= 5) btnPwrM->setChecked(true);
        else btnPwrL->setChecked(true);
        auto applyPwr = [this](int v) {
            QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
            s.setValue("powerLevel", v);
            s.sync();
            if (usrpThread) usrpThread->setPowerLevel(v);
        };
        connect(btnPwrS, &QPushButton::clicked, this, [btnPwrS, btnPwrM, btnPwrL, applyPwr]() {
            btnPwrS->setChecked(true); btnPwrM->setChecked(false); btnPwrL->setChecked(false); applyPwr(2);
        });
        connect(btnPwrM, &QPushButton::clicked, this, [btnPwrS, btnPwrM, btnPwrL, applyPwr]() {
            btnPwrS->setChecked(false); btnPwrM->setChecked(true); btnPwrL->setChecked(false); applyPwr(5);
        });
        connect(btnPwrL, &QPushButton::clicked, this, [btnPwrS, btnPwrM, btnPwrL, applyPwr]() {
            btnPwrS->setChecked(false); btnPwrM->setChecked(false); btnPwrL->setChecked(true); applyPwr(10);
        });
        QHBoxLayout *pwrRow = new QHBoxLayout;
        pwrRow->setContentsMargins(0, 0, 0, 0);
        pwrRow->setSpacing(4);
        pwrRow->addWidget(pwrLbl);
        pwrRow->addWidget(btnPwrS);
        pwrRow->addWidget(btnPwrM);
        pwrRow->addWidget(btnPwrL);
        vTx->addLayout(pwrRow);
        applyPwr(savedPwr);
    }
#endif

    btnGpsToggle = new QPushButton("●发射 GPS");
    btnGpsToggle->setFixedHeight(28);
    btnGpsToggle->setCursor(Qt::PointingHandCursor);
    btnGpsToggle->setCheckable(true);
    btnGpsToggle->setStyleSheet(
        "QPushButton{background:#22c55e;border:none;border-radius:4px;color:#fff;font-size:12px;font-weight:bold}"
        "QPushButton:hover{background:#16a34a}"
        "QPushButton:checked{background:#ef4444;color:#fff}"
        "QPushButton:checked:hover{background:#dc2626}");
    vTx->addWidget(btnGpsToggle);
    leftVl->addWidget(gbTx);

    QGroupBox *gbMode = new QGroupBox(QStringLiteral("🧭 模式"));
    gbMode->setStyleSheet(
        "QGroupBox{background:#ffffff;border:1px solid #d0d5dd;border-radius:6px;"
        "margin-top:12px;padding-top:12px;padding-left:6px;padding-right:6px;"
        "padding-bottom:4px;font-size:12px;color:#005fb8;font-weight:bold}"
        "QGroupBox::title{subcontrol-origin:margin;left:8px;padding:0 4px}");
    QVBoxLayout *vMode = new QVBoxLayout(gbMode);
    vMode->setContentsMargins(3, 3, 3, 3);
    vMode->setSpacing(4);

    auto mkModeBtn = [&](const QString &text) -> QPushButton * {
        QPushButton *b = new QPushButton(text);
        b->setFixedHeight(28);
        b->setCursor(Qt::PointingHandCursor);
        b->setCheckable(true);
        b->setStyleSheet(
            "QPushButton{background:#ffffff;border:1px solid #d0d5dd;border-radius:4px;color:#616161;font-size:11px;font-weight:bold;padding:2px 0}"
            "QPushButton:checked{background:#005fb8;border-color:#005fb8;color:#fff}"
            "QPushButton:hover{background:#f0f4f8}");
        return b;
    };
    QHBoxLayout *modeRow = new QHBoxLayout;
    modeRow->setSpacing(4);
    btnMode1 = mkModeBtn("单点");
    btnMode2 = mkModeBtn("公路");
    btnMode1->setChecked(true);
    modeRow->addWidget(btnMode1);
    modeRow->addWidget(btnMode2);
#ifndef GPS_VERSION_GAODUAN
    btnMode3 = mkModeBtn("自定义");
    modeRow->addWidget(btnMode3);
#endif
    vMode->addLayout(modeRow);

    modeLabel = new QLabel("● 单点模式");
    modeLabel->setStyleSheet("font-size:11px;color:#005fb8;background:transparent");
    vMode->addWidget(modeLabel);

    modeStack = new QStackedWidget;
    modeStack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    modeStack->setStyleSheet("background:transparent;");

    pageSingle = new QWidget;
    QVBoxLayout *vS = new QVBoxLayout(pageSingle);
    vS->setContentsMargins(0, 0, 0, 0);
    vS->setSpacing(4);
    QLabel *lblSingle = new QLabel("双击地图选择目标 或 输入坐标:");
    lblSingle->setStyleSheet("font-size:11px;color:#616161;background:transparent");
    lblSingle->setWordWrap(true);
    vS->addWidget(lblSingle);
    QHBoxLayout *coordRow = new QHBoxLayout;
    coordRow->setSpacing(4);
    QLineEdit *coordInput = new QLineEdit;
    coordInput->setPlaceholderText("纬度, 经度 例: 39.906, 116.397");
    coordInput->setStyleSheet("QLineEdit{background:#ffffff;border:1px solid #d0d5dd;border-radius:4px;padding:4px 8px;color:#1a1a1a;font-size:11px}");
    coordInput->setFixedHeight(28);
    coordRow->addWidget(coordInput, 1);
    QPushButton *btnLocate = new QPushButton("定位");
    btnLocate->setStyleSheet("QPushButton{background:#005fb8;color:#fff;border:none;border-radius:4px;padding:4px 12px;font-size:11px;font-weight:bold}QPushButton:hover{background:#004a8a}");
    btnLocate->setFixedHeight(28);
    coordRow->addWidget(btnLocate);
    vS->addLayout(coordRow);
    vS->addStretch();
    modeStack->addWidget(pageSingle);
    connect(btnLocate, &QPushButton::clicked, this, [this, coordInput]() {
        QString text = coordInput->text().trimmed();
        QStringList parts = text.split(',');
        if (parts.size() != 2) {
            QMessageBox::warning(this, "格式错误",
                "请输入 纬度,经度 格式，如: 39.906, 116.397");
            return;
        }
        bool ok1, ok2;
        double lat = parts[0].trimmed().toDouble(&ok1);
        double lon = parts[1].trimmed().toDouble(&ok2);
        if (!ok1 || !ok2 || lat < -90 || lat > 90 || lon < -180 || lon > 180) {
            QMessageBox::warning(this, "坐标无效",
                "纬度范围 -90~90，经度范围 -180~180");
            return;
        }
        webView->page()->runJavaScript(QString("qtShowPreview(%1,%2)").arg(lat).arg(lon));
        webView->page()->runJavaScript(QString("qtConfirmSinglePos()"));
    });

    pageRoute = new QWidget;
    QVBoxLayout *vR = new QVBoxLayout(pageRoute);
    vR->setContentsMargins(0, 0, 0, 0);
    vR->setSpacing(4);
    QLabel *lblRoute = new QLabel("单击地图选择终点\n确认后自动规划路线");
    lblRoute->setStyleSheet("font-size:11px;color:#616161;background:transparent");
    lblRoute->setWordWrap(true);
    vR->addWidget(lblRoute);
    QHBoxLayout *routeRow = new QHBoxLayout;
    routeRow->setSpacing(4);
    btnRouteStart = new QPushButton("▶ 开始");
    btnRouteStart->setFixedHeight(24);
    btnRouteStart->setStyleSheet("QPushButton{background:#22c55e;color:#fff;border:none;border-radius:4px;font-size:11px}QPushButton:hover{background:#16a34a}");
    btnRoutePause = new QPushButton("⏸ 暂停");
    btnRoutePause->setFixedHeight(24);
    btnRoutePause->setStyleSheet("QPushButton{background:#f59e0b;color:#fff;border:none;border-radius:4px;font-size:11px}QPushButton:hover{background:#d97706}");
    btnRouteStop = new QPushButton("■ 停止");
    btnRouteStop->setFixedHeight(24);
    btnRouteStop->setStyleSheet("QPushButton{background:#ef4444;color:#fff;border:none;border-radius:4px;font-size:11px}QPushButton:hover{background:#dc2626}");
    routeRow->addWidget(btnRouteStart);
    routeRow->addWidget(btnRoutePause);
    routeRow->addWidget(btnRouteStop);
    vR->addLayout(routeRow);
    QHBoxLayout *routeIO = new QHBoxLayout;
    routeIO->setSpacing(4);
    btnRouteImport = new QPushButton("导入路线");
    btnRouteImport->setFixedHeight(24);
    btnRouteImport->setStyleSheet("QPushButton{background:#005fb8;border:1px solid #005fb8;border-radius:4px;padding:2px 8px;color:#fff;font-size:11px}QPushButton:hover{background:#004c8c}");
    btnRouteExport = new QPushButton("导出路线");
    btnRouteExport->setFixedHeight(24);
    btnRouteExport->setStyleSheet("QPushButton{background:#0f7b0f;border:1px solid #0f7b0f;border-radius:4px;padding:2px 8px;color:#fff;font-size:11px}QPushButton:hover{background:#107c10}");
    routeIO->addWidget(btnRouteImport);
    routeIO->addWidget(btnRouteExport);
    vR->addLayout(routeIO);
    vR->addStretch();
    modeStack->addWidget(pageRoute);

#ifndef GPS_VERSION_GAODUAN
    pageCustom = new QWidget;
    QVBoxLayout *vC = new QVBoxLayout(pageCustom);
    vC->setContentsMargins(0, 0, 0, 0);
    vC->setSpacing(4);
    QLabel *lblCustom = new QLabel("单击地图添加路径点\n右键撤回上一个点");
    lblCustom->setStyleSheet("font-size:11px;color:#616161;background:transparent");
    lblCustom->setWordWrap(true);
    vC->addWidget(lblCustom);
    QHBoxLayout *customRow = new QHBoxLayout;
    customRow->setSpacing(4);
    btnCustomStart = new QPushButton("▶ 开始");
    btnCustomStart->setFixedHeight(24);
    btnCustomStart->setStyleSheet("QPushButton{background:#22c55e;color:#fff;border:none;border-radius:4px;font-size:11px}QPushButton:hover{background:#16a34a}");
    btnCustomPause = new QPushButton("⏸ 暂停");
    btnCustomPause->setFixedHeight(24);
    btnCustomPause->setStyleSheet("QPushButton{background:#f59e0b;color:#fff;border:none;border-radius:4px;font-size:11px}QPushButton:hover{background:#d97706}");
    btnCustomStop = new QPushButton("■ 停止");
    btnCustomStop->setFixedHeight(24);
    btnCustomStop->setStyleSheet("QPushButton{background:#ef4444;color:#fff;border:none;border-radius:4px;font-size:11px}QPushButton:hover{background:#dc2626}");
    customRow->addWidget(btnCustomStart);
    customRow->addWidget(btnCustomPause);
    customRow->addWidget(btnCustomStop);
    vC->addLayout(customRow);
    QHBoxLayout *customRow2 = new QHBoxLayout;
    customRow2->setSpacing(4);
    QPushButton *btnCustomImport = new QPushButton("导入路径");
    btnCustomImport->setFixedHeight(24);
    btnCustomImport->setStyleSheet("QPushButton{background:#005fb8;border:1px solid #005fb8;border-radius:4px;padding:2px 8px;color:#fff;font-size:11px}QPushButton:hover{background:#004c8c}");
    QPushButton *btnCustomExport = new QPushButton("导出路径");
    btnCustomExport->setFixedHeight(24);
    btnCustomExport->setStyleSheet("QPushButton{background:#0f7b0f;border:1px solid #0f7b0f;border-radius:4px;padding:2px 8px;color:#fff;font-size:11px}QPushButton:hover{background:#107c10}");
    btnLoadTxtPath = new QPushButton("加载TXT");
    btnLoadTxtPath->setFixedHeight(24);
    btnLoadTxtPath->setStyleSheet("QPushButton{background:#ffffff;border:1px solid #d0d5dd;border-radius:4px;padding:2px 8px;color:#1a1a1a;font-size:11px}QPushButton:hover{background:#f0f4f8}");
    customRow2->addWidget(btnCustomImport);
    customRow2->addWidget(btnCustomExport);
    customRow2->addWidget(btnLoadTxtPath);
    vC->addLayout(customRow2);
    vC->addStretch();
    modeStack->addWidget(pageCustom);
#endif

    vMode->addWidget(modeStack);
    leftVl->addWidget(gbMode);

#ifdef GPS_VERSION_GAODUAN
    // ===== 参数滑块 =====
    {
        QGroupBox *gbParam = new QGroupBox(QStringLiteral("🎛️ 参数"));
        gbParam->setStyleSheet(
            "QGroupBox{background:#ffffff;border:1px solid #d0d5dd;border-radius:6px;"
            "margin-top:12px;padding-top:12px;padding-left:6px;padding-right:6px;"
            "padding-bottom:4px;font-size:12px;color:#005fb8;font-weight:bold}"
            "QGroupBox::title{subcontrol-origin:margin;left:8px;padding:0 4px}");
        QVBoxLayout *vP = new QVBoxLayout(gbParam);
        vP->setContentsMargins(3, 3, 3, 3);
        vP->setSpacing(4);
        auto addSliderRow = [&](const QString &labelText, QSlider *&slider, QLabel *&valueLabel, int min, int max, int val, const QString &suffix, std::initializer_list<int> ticks) {
            QFrame *frame = new QFrame;
            frame->setStyleSheet("QFrame{background:#f8f9fa;border:1px solid #e0e3e8;border-radius:5px;padding:4px}");
            QVBoxLayout *frameL = new QVBoxLayout(frame);
            frameL->setContentsMargins(6, 4, 6, 2);
            frameL->setSpacing(0);

            QHBoxLayout *row = new QHBoxLayout;
            row->setContentsMargins(0, 0, 0, 0);
            row->setSpacing(4);
            QLabel *lbl = new QLabel(labelText);
            lbl->setStyleSheet("font-size:11px;color:#1a1a1a;background:transparent;font-weight:bold");
            slider = new QSlider(Qt::Horizontal);
            slider->setRange(min, max);
            slider->setValue(val);
            slider->setTickPosition(QSlider::TicksBothSides);
            slider->setTickInterval(qMax(1, (max - min) / 10));
            slider->setSingleStep(1);
            slider->setPageStep(1);
            slider->setStyle(QStyleFactory::create("Fusion"));
            valueLabel = new QLabel(suffix);
            valueLabel->setFixedWidth(50);
            valueLabel->setStyleSheet("font-size:12px;color:#005fb8;background:transparent;text-decoration:underline;cursor:pointer;font-weight:bold");
            valueLabel->setCursor(Qt::PointingHandCursor);
            valueLabel->installEventFilter(this);
            row->addWidget(lbl);
            row->addWidget(slider, 1);
            row->addWidget(valueLabel);
            frameL->addLayout(row);

            QHBoxLayout *tickRow = new QHBoxLayout;
            tickRow->setContentsMargins(6, 0, 6, 0);
            tickRow->setSpacing(3);
            for (int t : ticks) {
                int tickVal = t;
                QPushButton *tickBtn = new QPushButton(QString::number(t));
                tickBtn->setFixedHeight(22);
                tickBtn->setMinimumWidth(28);
                tickBtn->setCursor(Qt::PointingHandCursor);
                tickBtn->setStyleSheet(
                    "QPushButton{background:#e8ecf1;border:1px solid #d0d5dd;border-radius:4px;color:#1a1a1a;font-size:10px;font-weight:bold;padding:0 4px}"
                    "QPushButton:hover{background:#005fb8;color:#fff;border-color:#005fb8}");
                connect(tickBtn, &QPushButton::clicked, this, [slider, tickVal]() { slider->setValue(tickVal); });
                tickRow->addWidget(tickBtn);
            }
            tickRow->addStretch();
            frameL->addLayout(tickRow);
            vP->addWidget(frame);
        };
        addSliderRow("高度:", heightSlider, heightLabel, 1, 1000, 10, "10米", {1, 10, 50, 100, 200, 500, 1000});
        addSliderRow("速度:", speedSlider, speedLabel, 0, 300, 0, "0.0 km/h", {0, 10, 30, 60, 100, 200, 300});
        addSliderRow("随机±:", randomRangeSlider, randRangeLabel, 0, 30, 0, "关", {0, 1, 3, 5, 10, 20, 30});
        connect(heightSlider, &QSlider::valueChanged, this, &MainWindow::onHeightSliderChanged);
        connect(speedSlider, &QSlider::valueChanged, this, &MainWindow::onSpeedSliderChanged);
        connect(randomRangeSlider, &QSlider::valueChanged, this, &MainWindow::onRandomRangeChanged);
        // 抖动 + 跟踪按钮行
        {
            QFrame *btnFrame = new QFrame;
            btnFrame->setStyleSheet("QFrame{background:#f8f9fa;border:1px solid #e0e3e8;border-radius:5px;padding:4px}");
            QHBoxLayout *btnRow = new QHBoxLayout(btnFrame);
            btnRow->setContentsMargins(6, 4, 6, 4);
            btnRow->setSpacing(4);
            btnJitter = new QPushButton("▶ 抖动");
            btnJitter->setFixedHeight(24);
            btnJitter->setCursor(Qt::PointingHandCursor);
            btnJitter->setStyleSheet(
                "QPushButton{background:#ffffff;border:1px solid #d0d5dd;border-radius:4px;color:#616161;font-size:11px;font-weight:bold;padding:0 8px}"
                "QPushButton:hover{background:#f0f4f8;color:#1a1a1a}");
            btnRow->addWidget(btnJitter);
            connect(btnJitter, &QPushButton::clicked, this, [this]() {
                jitterEnabled = !jitterEnabled;
                if (jitterEnabled) {
                    btnJitter->setText("■ 抖动");
                    btnJitter->setStyleSheet(
                        "QPushButton{background:#005fb8;border:1px solid #005fb8;border-radius:4px;color:#fff;font-size:11px;font-weight:bold;padding:0 8px}"
                        "QPushButton:hover{background:#004a8a}");
                    if (webView && webView->page()) webView->page()->runJavaScript("g_jitterEnabled=true");
                } else {
                    btnJitter->setText("▶ 抖动");
                    btnJitter->setStyleSheet(
                        "QPushButton{background:#ffffff;border:1px solid #d0d5dd;border-radius:4px;color:#616161;font-size:11px;font-weight:bold;padding:0 8px}"
                        "QPushButton:hover{background:#f0f4f8;color:#1a1a1a}");
                    if (webView && webView->page()) {
                        webView->page()->runJavaScript("g_jitterEnabled=false");
                        webView->page()->runJavaScript("delete window._jitState");
                    }
                }
            });
            btnAutoCenter = new QPushButton("▶ 跟踪");
            btnAutoCenter->setFixedHeight(24);
            btnAutoCenter->setCursor(Qt::PointingHandCursor);
            btnAutoCenter->setStyleSheet(
                "QPushButton{background:#ffffff;border:1px solid #d0d5dd;border-radius:4px;color:#616161;font-size:11px;font-weight:bold;padding:0 8px}"
                "QPushButton:hover{background:#f0f4f8;color:#1a1a1a}");
            btnRow->addWidget(btnAutoCenter);
            connect(btnAutoCenter, &QPushButton::clicked, this, [this]() {
                m_autoCenter = !m_autoCenter;
                if (m_autoCenter) {
                    btnAutoCenter->setText("■ 跟踪");
                    btnAutoCenter->setStyleSheet(
                        "QPushButton{background:#005fb8;border:1px solid #005fb8;border-radius:4px;color:#fff;font-size:11px;font-weight:bold;padding:0 8px}"
                        "QPushButton:hover{background:#004a8a}");
                    if (webView && webView->page()) webView->page()->runJavaScript("qtSetTracking(true)");
                } else {
                    btnAutoCenter->setText("▶ 跟踪");
                    btnAutoCenter->setStyleSheet(
                        "QPushButton{background:#ffffff;border:1px solid #d0d5dd;border-radius:4px;color:#616161;font-size:11px;font-weight:bold;padding:0 8px}"
                        "QPushButton:hover{background:#f0f4f8;color:#1a1a1a}");
                    if (webView && webView->page()) webView->page()->runJavaScript("qtSetTracking(false)");
                }
            });
            vP->addWidget(btnFrame);
            // 抖动范围滑块
            {
                QFrame *jitRangeFrame = new QFrame;
                jitRangeFrame->setStyleSheet("QFrame{background:#f8f9fa;border:1px solid #e0e3e8;border-radius:5px;padding:2px}");
                QHBoxLayout *jitRangeRow = new QHBoxLayout(jitRangeFrame);
                jitRangeRow->setContentsMargins(6, 2, 6, 2);
                jitRangeRow->setSpacing(4);
                QLabel *jitRangeLbl = new QLabel("抖动范围:");
                jitRangeLbl->setStyleSheet("font-size:10px;color:#1a1a1a;background:transparent;font-weight:bold");
                QSlider *jitRangeSlider = new QSlider(Qt::Horizontal);
                jitRangeSlider->setRange(0, 20);
                jitRangeSlider->setValue((int)jitterRadiusMeters);
                jitRangeSlider->setStyleSheet(
                    "QSlider::groove:horizontal{height:2px;background:#d0d5dd;border-radius:1px}"
                    "QSlider::handle:horizontal{background:#005fb8;width:6px;height:6px;margin:-2px 0;border-radius:3px}");
                QLabel *jitRangeVal = new QLabel(QString::number((int)jitterRadiusMeters) + "米");
                jitRangeVal->setFixedWidth(40);
                jitRangeVal->setStyleSheet("font-size:10px;color:#005fb8;background:transparent;font-weight:bold");
                connect(jitRangeSlider, &QSlider::valueChanged, this, [this, jitRangeVal](int val) {
                    jitterRadiusMeters = val;
                    jitRangeVal->setText(QString::number(val) + "米");
                    if (webView && webView->page())
                        webView->page()->runJavaScript(QString("jitterRadiusMeters=%1").arg(val));
                });
                jitRangeRow->addWidget(jitRangeLbl);
                jitRangeRow->addWidget(jitRangeSlider, 1);
                jitRangeRow->addWidget(jitRangeVal);
                vP->addWidget(jitRangeFrame);
            }
            QFrame *mapTypeFrame = new QFrame;
            mapTypeFrame->setStyleSheet("QFrame{background:#f8f9fa;border:1px solid #e0e3e8;border-radius:5px;padding:2px}");
            QHBoxLayout *mapTypeRow = new QHBoxLayout(mapTypeFrame);
            mapTypeRow->setContentsMargins(4, 2, 4, 2);
            mapTypeRow->setSpacing(2);
            auto mkMapTypeBtn = [&](const QString &text, const QString &type) -> QPushButton * {
                QPushButton *b = new QPushButton(text);
                b->setFixedHeight(22);
                b->setStyleSheet("QPushButton{background:#ffffff;border:1px solid #d0d5dd;border-radius:4px;padding:2px 4px;color:#1a1a1a;font-size:10px}QPushButton:hover{background:#f0f4f8}QPushButton:checked{background:#005fb8;color:#fff;border-color:#005fb8}");
                b->setCheckable(true);
                connect(b, &QPushButton::clicked, this, [this, type]() { onMapTypeChanged(type); });
                return b;
            };
            btnNormalMap = mkMapTypeBtn("普通", "normal");
            btnSatMap = mkMapTypeBtn("卫星", "satellite");
            btnHybridMap = mkMapTypeBtn("混合", "hybrid");
            QButtonGroup *mapTypeGroup = new QButtonGroup(this);
            mapTypeGroup->addButton(btnNormalMap);
            mapTypeGroup->addButton(btnSatMap);
            mapTypeGroup->addButton(btnHybridMap);
            btnNormalMap->setChecked(true);
            mapTypeRow->addWidget(btnNormalMap);
            mapTypeRow->addWidget(btnSatMap);
            mapTypeRow->addWidget(btnHybridMap);
            vP->addWidget(mapTypeFrame);
        }
        leftVl->addWidget(gbParam);
    }
#endif

#ifndef GPS_VERSION_GAODUAN
    beijingTimeLabel = new QLabel("北京时间: --:--:--");
    beijingTimeLabel->setStyleSheet("font-size:12px;color:#005fb8;background:transparent;padding:2px 6px");
    beijingTimeLabel->setFixedHeight(18);
    leftVl->addWidget(beijingTimeLabel, 0, Qt::AlignRight);

    // ===== 路径 group (非高端版放在左侧) =====
    QGroupBox *gbPath = new QGroupBox(QStringLiteral("🔄 路径"));
    gbPath->setStyleSheet(
        "QGroupBox{background:#ffffff;border:1px solid #d0d5dd;border-radius:6px;"
        "margin-top:12px;padding-top:12px;padding-left:6px;padding-right:6px;"
        "padding-bottom:4px;font-size:11px;color:#005fb8;font-weight:bold}"
        "QGroupBox::title{subcontrol-origin:margin;left:6px;padding:0 3px}");
    QVBoxLayout *vPath = new QVBoxLayout(gbPath);
    vPath->setContentsMargins(4, 4, 4, 4);
    vPath->setSpacing(4);
    btnCycle = new QPushButton("开启循环");
    btnCycle->setFixedHeight(24);
    btnCycle->setStyleSheet("QPushButton{background:#ffffff;border:1px solid #d0d5dd;border-radius:4px;padding:2px 8px;color:#1a1a1a;font-size:11px}QPushButton:hover{background:#f0f4f8}");
    vPath->addWidget(btnCycle);
    connect(btnCycle, &QPushButton::clicked, this, &MainWindow::onToggleCycle);
    btnClearPath = new QPushButton("清除轨迹");
    btnClearPath->setFixedHeight(24);
    btnClearPath->setStyleSheet("QPushButton{background:#ef4444;border:1px solid #ef4444;border-radius:4px;padding:2px 8px;color:#fff;font-size:11px}QPushButton:hover{background:#dc2626}");
    vPath->addWidget(btnClearPath);
    leftVl->addWidget(gbPath);
#endif

    // 状态灯分组
    {
        QGroupBox *gbStatus = new QGroupBox(QStringLiteral("📡 状态"));
        gbStatus->setStyleSheet(
            "QGroupBox{background:#ffffff;border:1px solid #d0d5dd;border-radius:6px;"
            "margin-top:10px;padding-top:10px;padding-left:6px;padding-right:6px;"
            "padding-bottom:4px;font-size:11px;color:#005fb8;font-weight:bold}"
            "QGroupBox::title{subcontrol-origin:margin;left:6px;padding:0 3px}");
        QHBoxLayout *statusL = new QHBoxLayout(gbStatus);
        statusL->setContentsMargins(6, 2, 6, 6);
        statusL->setSpacing(4);
        statusLight = new QLabel;
        statusLight->setFixedSize(18, 18);
        statusLight->setStyleSheet("background:#ef4444;border:1px solid #dc2626;border-radius:9px;min-width:18px;min-height:18px;max-width:18px;max-height:18px");
        QLabel *statusText = new QLabel("待发射");
        m_statusText = statusText;
        statusText->setObjectName("statusText");
        statusText->setStyleSheet("font-size:11px;color:#616161;background:transparent;font-weight:bold");
        statusL->addWidget(statusLight);
        statusL->addWidget(statusText);
        statusL->addStretch();
        leftVl->addWidget(gbStatus);
    }

    satStatus = new SatelliteStatusWidget;
    leftVl->addWidget(satStatus);

    leftVl->addStretch();

    m_leftPanel->setWidget(leftCol);

    QWidget *leftWrapper = new QWidget;
    leftWrapper->setStyleSheet("background:#e8ecf1;");
    leftWrapper->setFixedWidth(290);
    leftWrapper->setMinimumWidth(290);
    leftWrapper->setMaximumWidth(290);
    leftWrapper->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    QVBoxLayout *leftWrapperL = new QVBoxLayout(leftWrapper);
    leftWrapperL->setContentsMargins(0, 0, 0, 0);
    leftWrapperL->setSpacing(0);
    leftWrapperL->addWidget(m_leftPanel);

    mainH->addWidget(leftWrapper, 0);

    auto *profile = QWebEngineProfile::defaultProfile();
    QString cacheDir = dataDir + "/webcache";
    QDir().mkpath(cacheDir);
    profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    profile->setHttpCacheMaximumSize(1024 * 1024 * 1024); // 1GB
    profile->setPersistentStoragePath(cacheDir);
    profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);

    webView = new QWebEngineView;
    /* 关键：禁止 WebEngine 在窗口最小化/后台时把页面设为 Frozen 状态
       否则 Chromium 会冻结所有 setInterval/setTimeout，导致 mainMarker 不动 */
    tileNetMgr = new QNetworkAccessManager(this);
    tileRetryTimer = new QTimer(this);
    tileRetryTimer->setSingleShot(true);
    connect(tileRetryTimer, &QTimer::timeout, this, &MainWindow::processNextTileJob);
    {
        MapPage *page = new MapPage(this);
        page->onModeSwitch = [this](int cppMode) {
            if (cppMode >= 0 && cppMode <= 2 && cppMode != m_mode) {
                m_syncingMode = true;
                onModeSwitchChanged(cppMode);
                m_syncingMode = false;
            }
        };
        page->onSinglePreview = [this](double lat, double lon) {
            if (m_mode != 0) return;
            int ret = QMessageBox::question(this, "确认位置",
                QString("要将位置移动到\n纬度: %1\n经度: %2 吗?").arg(lat,0,'f',6).arg(lon,0,'f',6));
            if (ret == QMessageBox::Yes) {
                onConfirmSingle();
            } else {
                onCancelSingle();
            }
        };
        page->onRouteTarget = [this](double lat, double lon) {
            if (m_mode != 1) return;
            m_routeTargetLat = lat;
            m_routeTargetLon = lon;
            m_routePending = true;
            onConfirmRoute();
        };
        page->onPathUpdate = [this](int jsMode, int count) {
            int cppMode = jsMode - 1;
            if (cppMode >= 0 && cppMode <= 2) m_mode = cppMode;
            m_pathPointCount = count;
        };
        webView->setPage(page);
    }
    webView->setUrl(QUrl("about:blank"));
    connect(webView, &QWebEngineView::loadFinished, this, &MainWindow::onMapLoaded);
    QWidget *mapContainer = new QWidget;
    QVBoxLayout *mapContainerL = new QVBoxLayout(mapContainer);
    mapContainerL->setContentsMargins(0, 0, 0, 0);
    mapContainerL->setSpacing(0);

    m_bottomBar = new QWidget;
    m_bottomBar->setFixedHeight(28);
    m_bottomBar->setStyleSheet("background:#e8ecf1;border-top:1px solid #d0d5dd");
    QHBoxLayout *barL = new QHBoxLayout(m_bottomBar);
    barL->setContentsMargins(10, 0, 10, 0);
    barL->setSpacing(2);

    QString barLabelStyle = "font-size:11px;color:#616161;background:transparent;padding:0 8px";

    m_barLat = new QLabel("纬度: --");
    m_barLon = new QLabel("经度: --");
    m_barAlt = new QLabel("高度: 10m");
    m_barSpeed = new QLabel("速度: 0km/h");
    m_barTime = new QLabel("----:--:-- --:--:--");

    m_barLat->setStyleSheet(barLabelStyle);
    m_barLon->setStyleSheet(barLabelStyle);
    m_barAlt->setStyleSheet(barLabelStyle);
    m_barSpeed->setStyleSheet(barLabelStyle);
    m_barTime->setStyleSheet("font-size:11px;color:#005fb8;background:transparent;padding:0 8px");

    barL->addWidget(m_barLat);
    barL->addWidget(m_barLon);
    barL->addWidget(m_barAlt);
    barL->addWidget(m_barSpeed);
    barL->addStretch();
    barL->addWidget(m_barTime);

    mapContainerL->addWidget(webView, 1);
    mapContainerL->addWidget(m_bottomBar);

    mainH->addWidget(mapContainer, 1);

    /* 保持 WebEngine 活跃定时器：PreciseTimer 每 500ms 执行空 JS 调用，
       防止 Chromium 挂起 WebEngine 进程 */
    keepaliveTimer = new QTimer(this);
    keepaliveTimer->setTimerType(Qt::PreciseTimer);
    connect(keepaliveTimer, &QTimer::timeout, this, [this]() {
        if (webView && webView->page()) {
            webView->page()->runJavaScript("void 0");
        }
    });
    keepaliveTimer->start(500);

#ifndef GPS_VERSION_GAODUAN
    QScrollArea *rightPanel = new QScrollArea;
    rightPanel->setWidgetResizable(true);
    rightPanel->setMinimumWidth(240);
    rightPanel->setMaximumWidth(310);
    rightPanel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    rightPanel->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rightPanel->setStyleSheet("QScrollArea{background:#e8ecf1;border:none}");
    rightPanelWidget = rightPanel;

    QWidget *rightCol = new QWidget;
    rightCol->setStyleSheet("background:#e8ecf1;");
    QVBoxLayout *rightVl = new QVBoxLayout(rightCol);
    rightVl->setContentsMargins(8, 4, 4, 4);
    rightVl->setSpacing(4);

    gbParam = new QGroupBox(QStringLiteral("🎛️ 参数"));
    gbParam->setStyleSheet(
        "QGroupBox{background:#ffffff;border:1px solid #d0d5dd;border-radius:6px;"
        "margin-top:12px;padding-top:12px;padding-left:8px;padding-right:8px;"
        "padding-bottom:4px;font-size:12px;color:#005fb8;font-weight:bold}"
        "QGroupBox::title{subcontrol-origin:margin;left:6px;padding:0 3px}");
    // Helper to create a styled group box
    auto mkGroup = [&](const QString &title) -> QGroupBox * {
        QGroupBox *gb = new QGroupBox(title);
        gb->setStyleSheet(
            "QGroupBox{background:#ffffff;border:1px solid #d0d5dd;border-radius:6px;"
            "margin-top:12px;padding-top:12px;padding-left:6px;padding-right:6px;"
            "padding-bottom:4px;font-size:11px;color:#005fb8;font-weight:bold}"
            "QGroupBox::title{subcontrol-origin:margin;left:6px;padding:0 3px}");
        return gb;
    };

    auto addSliderRow = [&](QVBoxLayout *parentL, const QString &labelText, QSlider *&slider, QLabel *&valueLabel, int min, int max, int val, const QString &suffix) {
        QHBoxLayout *row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);
        QLabel *lbl = new QLabel(labelText);
        lbl->setStyleSheet("font-size:12px;color:#1a1a1a;background:transparent");
        slider = new QSlider(Qt::Horizontal);
        slider->setRange(min, max);
        slider->setValue(val);
        slider->setSingleStep(1);
        slider->installEventFilter(this);
        slider->setStyleSheet(
            "QSlider::groove:horizontal{height:4px;background:#d0d5dd;border-radius:2px}"
            "QSlider::handle:horizontal{background:#005fb8;width:10px;height:10px;margin:-3px 0;border-radius:5px}");
        valueLabel = new QLabel(suffix);
        valueLabel->setFixedWidth(64);
        valueLabel->setStyleSheet("font-size:12px;color:#1a1a1a;background:transparent");
        row->addWidget(lbl);
        row->addWidget(slider, 1);
        row->addWidget(valueLabel);
        parentL->addLayout(row);
    };

    // ===== 参数 group =====
    QVBoxLayout *vP = new QVBoxLayout(gbParam);
    vP->setContentsMargins(4, 1, 4, 1);
    vP->setSpacing(1);

    addSliderRow(vP, "高度:", heightSlider, heightLabel, 1, 1000, 10, "10米");
    connect(heightSlider, &QSlider::valueChanged, this, &MainWindow::onHeightSliderChanged);

    addSliderRow(vP, "速度:", speedSlider, speedLabel, 0, 300, 0, "0.0 km/h");
    speedSlider->setSingleStep(1);
    connect(speedSlider, &QSlider::valueChanged, this, &MainWindow::onSpeedSliderChanged);

    addSliderRow(vP, "随机±:", randomRangeSlider, randRangeLabel, 0, 30, 0, "关");
    connect(randomRangeSlider, &QSlider::valueChanged, this, &MainWindow::onRandomRangeChanged);
    rightVl->addWidget(gbParam);

    // ===== 功率 group =====
    QGroupBox *gbPwr = mkGroup(QStringLiteral("📡 功率"));
    QVBoxLayout *vPwr = new QVBoxLayout(gbPwr);
    vPwr->setContentsMargins(4, 4, 4, 4);
    vPwr->setSpacing(4);

    QLabel *pwrLbl = new QLabel("功率");
    pwrLbl->setStyleSheet("font-size:11px;color:#1a1a1a;background:transparent");
    auto *pwrRow = new QHBoxLayout;
    pwrRow->setContentsMargins(0, 0, 0, 0);
    pwrRow->setSpacing(4);

    auto mkPwrBtn = [&](const QString &text, int val, const QString &color) -> QPushButton * {
        QPushButton *b = new QPushButton(text);
        b->setFixedHeight(24);
        b->setCursor(Qt::PointingHandCursor);
        b->setCheckable(true);
        b->setStyleSheet(QString(
            "QPushButton{background:#ffffff;border:2px solid %1;border-radius:4px;color:%1;font-size:11px;font-weight:bold}"
            "QPushButton:checked{background:%1;color:#fff}"
            "QPushButton:hover{background:%2}").arg(color, color + "20"));
        return b;
    };
    QPushButton *btnPwrS = mkPwrBtn("小", 2, "#22c55e");
    QPushButton *btnPwrM = mkPwrBtn("中", 5, "#f59e0b");
    QPushButton *btnPwrL = mkPwrBtn("大", 10, "#ef4444");

    int savedPwr = 5;
    {
        QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
        savedPwr = s.value("powerLevel", 5).toInt();
    }
    if (savedPwr <= 2) btnPwrS->setChecked(true);
    else if (savedPwr <= 5) btnPwrM->setChecked(true);
    else btnPwrL->setChecked(true);

    auto applyPwr = [this](int v) {
        QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
        s.setValue("powerLevel", v);
        s.sync();
        if (usrpThread) usrpThread->setPowerLevel(v);
    };
    connect(btnPwrS, &QPushButton::clicked, this, [btnPwrS, btnPwrM, btnPwrL, applyPwr]() {
        btnPwrS->setChecked(true); btnPwrM->setChecked(false); btnPwrL->setChecked(false); applyPwr(2);
    });
    connect(btnPwrM, &QPushButton::clicked, this, [btnPwrS, btnPwrM, btnPwrL, applyPwr]() {
        btnPwrS->setChecked(false); btnPwrM->setChecked(true); btnPwrL->setChecked(false); applyPwr(5);
    });
    connect(btnPwrL, &QPushButton::clicked, this, [btnPwrS, btnPwrM, btnPwrL, applyPwr]() {
        btnPwrS->setChecked(false); btnPwrM->setChecked(false); btnPwrL->setChecked(true); applyPwr(10);
    });
    pwrRow->addWidget(pwrLbl);
    pwrRow->addWidget(btnPwrS);
    pwrRow->addWidget(btnPwrM);
    pwrRow->addWidget(btnPwrL);
    vPwr->addLayout(pwrRow);
    applyPwr(savedPwr);

    rightVl->addWidget(gbPwr);

    // ===== 地图 group =====
    QGroupBox *gbMap = mkGroup(QStringLiteral("🗺️ 地图"));
    QVBoxLayout *vMap = new QVBoxLayout(gbMap);
    vMap->setContentsMargins(4, 4, 4, 4);
    vMap->setSpacing(4);

    QHBoxLayout *mapTypeRow = new QHBoxLayout;
    mapTypeRow->setContentsMargins(0, 0, 0, 0);
    mapTypeRow->setSpacing(4);
    btnNormalMap = new QPushButton("普通");
    btnNormalMap->setFixedHeight(24);
    btnNormalMap->setStyleSheet("QPushButton{background:#ffffff;border:1px solid #d0d5dd;border-radius:4px;padding:2px 4px;color:#1a1a1a;font-size:11px}QPushButton:hover{background:#f0f4f8}");
    btnSatMap = new QPushButton("卫星");
    btnSatMap->setFixedHeight(24);
    btnSatMap->setStyleSheet("QPushButton{background:#ffffff;border:1px solid #d0d5dd;border-radius:4px;padding:2px 4px;color:#1a1a1a;font-size:11px}QPushButton:hover{background:#f0f4f8}");
    btnHybridMap = new QPushButton("混合");
    btnHybridMap->setFixedHeight(24);
    btnHybridMap->setStyleSheet("QPushButton{background:#ffffff;border:1px solid #d0d5dd;border-radius:4px;padding:2px 4px;color:#1a1a1a;font-size:11px}QPushButton:hover{background:#f0f4f8}");
    mapTypeRow->addWidget(btnNormalMap);
    mapTypeRow->addWidget(btnSatMap);
    mapTypeRow->addWidget(btnHybridMap);
    vMap->addLayout(mapTypeRow);

    rightVl->addWidget(gbMap);

    // ===== 跟踪 group =====
    QGroupBox *gbTrack = mkGroup(QStringLiteral("📍 跟踪"));
    QVBoxLayout *vTrack = new QVBoxLayout(gbTrack);
    vTrack->setContentsMargins(4, 4, 4, 4);
    vTrack->setSpacing(4);

    btnLockTrack = new QPushButton("📍 跟踪坐标");
    btnLockTrack->setFixedHeight(24);
    btnLockTrack->setCursor(Qt::PointingHandCursor);
    btnLockTrack->setStyleSheet("QPushButton{background:#ffffff;border:1px solid #d0d5dd;border-radius:4px;color:#616161;font-size:11px}QPushButton:hover{background:#f0f4f8;color:#1a1a1a}");
    vTrack->addWidget(btnLockTrack);
    connect(btnLockTrack, &QPushButton::clicked, this, [this]() { setTracking(!m_tracking); });
    rightVl->addWidget(gbTrack);

    // Coordinate info in the right panel
    posInfo = new QLabel("纬度: --\n经度: --\n高度: 10 米\n速度: 0 km/h");
    posInfo->setStyleSheet("font-size:13px;color:#1a1a1a;background:#ffffff;border:1px solid #d0d5dd;border-radius:6px;padding:8px");
    posInfo->setMaximumHeight(80);
    rightVl->addWidget(posInfo);

    rightVl->addStretch();

    // GPS status at bottom-right
    gpsStatus = new QLabel("GPS待命");
    if (gpsStatus) gpsStatus->setStyleSheet(
        "QLabel{background:#ffffff;border:1px solid #d0d5dd;border-radius:8px;"
        "font-size:14px;font-weight:bold;color:#616161;"
        "padding:12px 20px;}"
    );
    if (gpsStatus) gpsStatus->setAlignment(Qt::AlignCenter);
    gpsStatus->setMinimumHeight(50);
    rightVl->addWidget(gpsStatus);

    /* API key indicator at bottom-right */
    m_apiKeyLabel = new QLabel("");
    m_apiKeyLabel->setStyleSheet("font-size:9px;color:#555;background:transparent;padding:2px 4px");
    m_apiKeyLabel->setAlignment(Qt::AlignRight);
    rightVl->addWidget(m_apiKeyLabel);

    rightPanel->setWidget(rightCol);
    mainH->addWidget(rightPanel, 0);
#endif
    rootV->addWidget(body);

    logBox = new QPlainTextEdit;
    logBox->setReadOnly(true);
    logBox->setMaximumBlockCount(500);
    logBox->setPlaceholderText("等待日志...");
    logBox->setStyleSheet("QPlainTextEdit{background:#ffffff;color:#616161;border:1px solid #d0d5dd;border-radius:4px;font:9px Consolas;padding:2px}");

    logMAP = new QPlainTextEdit;
    logMAP->setReadOnly(true);
    logMAP->setMaximumBlockCount(500);
    logMAP->setPlaceholderText("等待日志...");
    logMAP->setStyleSheet("QPlainTextEdit{background:#ffffff;color:#616161;border:1px solid #d0d5dd;border-radius:4px;font:9px Consolas;padding:2px}");

    logGPS = new QPlainTextEdit;
    logGPS->setReadOnly(true);
    logGPS->setMaximumBlockCount(500);
    logGPS->setPlaceholderText("等待日志...");
    logGPS->setStyleSheet("QPlainTextEdit{background:#ffffff;color:#616161;border:1px solid #d0d5dd;border-radius:4px;font:9px Consolas;padding:2px}");

    QWidget *logPanel = new QWidget;
    logPanel->setStyleSheet("background:#ffffff;border-top:1px solid #d0d5dd;");
    logPanel->setFixedHeight(0);
    QVBoxLayout *logPanelL = new QVBoxLayout(logPanel);
    logPanelL->setContentsMargins(0, 0, 0, 0);
    logPanelL->setSpacing(0);

    QHBoxLayout *logTabs = new QHBoxLayout;
    logTabs->setContentsMargins(0, 0, 0, 0);
    logTabs->setSpacing(1);
    QLabel *tabUI = new QLabel("[UI]");
    tabUI->setStyleSheet("font-size:11px;color:#005fb8;background:#d0d5dd;padding:1px 8px;border-radius:2px");
    tabUI->setFixedHeight(16);
    QLabel *tabGPS = new QLabel("[GPS]");
    tabGPS->setStyleSheet("font-size:11px;color:#22c55e;background:#d0d5dd;padding:1px 8px;border-radius:2px");
    tabGPS->setFixedHeight(16);
    QLabel *tabMAP = new QLabel("[MAP]");
    tabMAP->setStyleSheet("font-size:11px;color:#f59e0b;background:#d0d5dd;padding:1px 8px;border-radius:2px");
    tabMAP->setFixedHeight(16);
    logTabs->addWidget(tabUI);
    logTabs->addWidget(tabGPS);
    logTabs->addWidget(tabMAP);
    logTabs->addStretch();
    logPanelL->addLayout(logTabs);

    QSplitter *logSplitter = new QSplitter(Qt::Horizontal);
    logSplitter->setChildrenCollapsible(false);
    logSplitter->addWidget(logBox);
    logSplitter->addWidget(logGPS);
    logSplitter->addWidget(logMAP);
    logSplitter->setSizes({300, 300, 300});
    logPanelL->addWidget(logSplitter, 1);

    logPanel->hide();
    rootV->addWidget(logPanel);

    connect(btnLogToggle, &QPushButton::toggled, this, [btnLogToggle, logPanel](bool checked) {
        logPanel->setVisible(checked);
        logPanel->setFixedHeight(checked ? 160 : 0);
        btnLogToggle->setText(checked ? "📄" : "📋");
    });

    connect(btnGpsToggle, &QPushButton::clicked, this, [this]() {
        if (btnGpsToggle->isChecked()) {
            onStartGps();
        } else {
            onStopGps();
        }
    });
    connect(btnNormalMap, &QPushButton::clicked, this, [this]() { onMapTypeChanged("normal"); });
    connect(btnSatMap, &QPushButton::clicked, this, [this]() { onMapTypeChanged("satellite"); });
    connect(btnHybridMap, &QPushButton::clicked, this, [this]() { onMapTypeChanged("hybrid"); });
    connect(btnClearPath, &QPushButton::clicked, this, &MainWindow::onClearPath);
    connect(btnMode1, &QPushButton::clicked, this, [this]() { onModeSwitchChanged(0); });
    connect(btnMode2, &QPushButton::clicked, this, [this]() { onModeSwitchChanged(1); });
#ifndef GPS_VERSION_GAODUAN
    connect(btnMode3, &QPushButton::clicked, this, [this]() { onModeSwitchChanged(2); });
#endif
#ifndef GPS_VERSION_GAODUAN
    connect(btnCustomImport, &QPushButton::clicked, this, &MainWindow::onImportPath);
    connect(btnCustomExport, &QPushButton::clicked, this, &MainWindow::onExportPath);
    connect(btnLoadTxtPath, &QPushButton::clicked, this, &MainWindow::onLoadTxtPath);
#endif
    connect(btnRouteImport, &QPushButton::clicked, this, &MainWindow::onRouteImportPath);
    connect(btnRouteExport, &QPushButton::clicked, this, &MainWindow::onRouteExportPath);

    connect(btnRouteStart, &QPushButton::clicked, this, [this]() {
        if (webView && webView->page()) webView->page()->runJavaScript("qtStartTrack()");
        log("公路导航已开始");
    });
    connect(btnRoutePause, &QPushButton::clicked, this, [this]() {
        if (webView && webView->page()) webView->page()->runJavaScript("qtPauseTrack()");
        log("公路导航已暂停");
    });
    connect(btnRouteStop, &QPushButton::clicked, this, [this]() {
        if (webView && webView->page()) webView->page()->runJavaScript("qtStopTrack()");
        log("公路导航已停止");
    });
#ifndef GPS_VERSION_GAODUAN
    connect(btnCustomStart, &QPushButton::clicked, this, [this]() {
        if (webView && webView->page()) webView->page()->runJavaScript("qtConfirmMultiPath()");
        log("自定义路径已开始");
    });
    connect(btnCustomPause, &QPushButton::clicked, this, [this]() {
        if (webView && webView->page()) webView->page()->runJavaScript("qtPauseTrack()");
        log("自定义路径已暂停");
    });
    connect(btnCustomStop, &QPushButton::clicked, this, [this]() {
        if (webView && webView->page()) webView->page()->runJavaScript("qtStopPath()");
        log("自定义路径已停止");
    });
#endif


    usrpThread = new UsrpThread(this);
    connect(usrpThread, &UsrpThread::iqData, this, &MainWindow::onUsrpIq);
    connect(usrpThread, &UsrpThread::logMsg, this, &MainWindow::onLogMsg);
    connect(usrpThread, &UsrpThread::gpssimConnected, this, [this]() {
        usrpRunning = true;
        MainWindow::onGpssimConnected();
    });
    connect(usrpThread, &UsrpThread::gpssimDisconnected, this, &MainWindow::onGpssimDisconnected);
    connect(usrpThread, &UsrpThread::deviceDisconnected, this, [this]() {
        onStopGps();
        QMessageBox::critical(this, "发射器断连", "发射器断开连接！请检查连接情况");
        log("发射器断开连接，已停止发射");
    });
    connect(usrpThread, &QThread::finished, this, [this]() {
        bool hadStarted = usrpRunning;
        usrpRunning = false;
        if (!hadStarted) {
            usrpStatus->setText("●设备待命");
            onStopGps();
        } else {
            log("USRP 已停止");
        }
    });

    jitterTimer = new QTimer(this);
    jitterTimer->setTimerType(Qt::CoarseTimer);
    connect(jitterTimer, &QTimer::timeout, this, [this]() {
        if (g_closing) return;
        if ((m_mode == 1 || m_mode == 2) && (trackJitterEnabled || jitterEnabled) && webView && webView->page()) {
            webView->page()->runJavaScript("qtIsStationary()", [this](const QVariant &v) {
                if (v.toBool()) {
                    double angle = (double)rand() / RAND_MAX * 2.0 * 3.14159265;
                    double dist = (double)rand() / RAND_MAX * jitterRadiusMeters * 0.3;
                    double refLat = 30.0 * 3.14159265 / 180.0;
                    double dlat = dist * cos(angle) / 111111.0;
                    double dlon = dist * sin(angle) / (111111.0 * cos(refLat));
                    QString js = QString("qtSetJitterOffset(%1,%2)").arg(dlat,0,'f',9).arg(dlon,0,'f',9);
                    webView->page()->runJavaScript(js);
                } else {
                    webView->page()->runJavaScript("qtResetJitterOffset()");
                }
            });
        }
    });

    trackingTimer = new QTimer(this);
    trackingTimer->setTimerType(Qt::CoarseTimer);
    connect(trackingTimer, &QTimer::timeout, this, [this]() {
        if (g_closing) return;
        if (!m_tracking || !webView || !webView->page()) return;
        webView->page()->runJavaScript("qtGetPosForUDP()", [this](const QVariant &v) {
            if (!m_tracking) return;
            QStringList parts = v.toString().split(",");
            if (parts.size() >= 2) {
                double lat = parts[0].toDouble();
                double lon = parts[1].toDouble();
                centerLat = lat; centerLon = lon;
                QString js = QString("qtTrackingPan(%1,%2)").arg(lat,0,'f',7).arg(lon,0,'f',7);
                webView->page()->runJavaScript(js);
            }
        });
    });

    pollTimer = new QTimer(this);
    connect(pollTimer, &QTimer::timeout, this, &MainWindow::pollStatus);
    pollTimer->start(2000);

    posInfoTimer = new QTimer(this);
    posInfoTimer->setTimerType(Qt::PreciseTimer);
    connect(posInfoTimer, &QTimer::timeout, this, [this]() {
        if (g_closing) return;
        double lat = centerLat, lon = centerLon;
        double hgt = heightSlider ? (double)heightSlider->value() : 10.0;
        double baseSpeed = speedSlider ? (double)speedSlider->value() : 0.0;
        if (speedLabel) speedLabel->setText(QString("%1 km/h").arg(baseSpeed, 0, 'f', 1));
        double displaySpeed = baseSpeed;
        /* 立刻同步目标速度（不依赖前端 setInterval 计算） */
        cppTargetSpeed = baseSpeed;
        if (webView && webView->page()) {
            webView->page()->runJavaScript(
                "var _a=typeof window.lat==='number'?window.lat:0,"
                "_b=typeof window.lon==='number'?window.lon:0,"
                "_c=typeof window.hgt==='number'?window.hgt:0,"
                "_d=typeof currentSpeedKmH==='number'?currentSpeedKmH:0;"
                "String(_a)+','+String(_b)+','+String(_c)+','+String(_d)",
                [this](const QVariant &v) {
                    QStringList p = v.toString().split(",");
                    if (p.size() >= 4) {
                        double jlat = p[0].toDouble();
                        double jlon = p[1].toDouble();
                        double jhgt = p[2].toDouble();
                        double jspeed = p[3].toDouble();
                        if (jlat != 0 || jlon != 0) { centerLat = jlat; centerLon = jlon; }
                        if (posInfo) posInfo->setText(QString("纬度: %1\n经度: %2\n高度: %3 米\n速度: %4 km/h")
                            .arg(jlat, 0, 'f', 6).arg(jlon, 0, 'f', 6).arg(jhgt, 0, 'f', 1).arg(jspeed, 0, 'f', 1));
                        if (m_barLat) m_barLat->setText(QString("纬度: %1").arg(jlat, 0, 'f', 6));
                        if (m_barLon) m_barLon->setText(QString("经度: %1").arg(jlon, 0, 'f', 6));
                        if (m_barAlt) m_barAlt->setText(QString("高度: %1m").arg(jhgt, 0, 'f', 1));
                        if (m_barSpeed) m_barSpeed->setText(QString("速度: %1km/h").arg(jspeed, 0, 'f', 1));
                        /* 跟踪坐标位置开启时每200ms回中一次 */
                        if (m_autoCenter && (jlat != 0 || jlon != 0)) {
                            webView->page()->runJavaScript(QString("var pt=mapApi.wgs84ToDisplay(%1,%2);mapApi.panTo(pt);").arg(jlon,0,'f',7).arg(jlat,0,'f',7));
                        }
                        /* 更新 C++ 端缓存，posPushThread 会用这些值推位置给 gpssim */
                        cppLat = jlat;
                        cppLon = jlon;
                        cppHgt = jhgt;
                        cppCurrentSpeed = jspeed;
                        cppLastFrontierRead = QDateTime::currentDateTime();
                    }
                });
        } else if (lat != 0.0 || lon != 0.0) {
            if (posInfo) posInfo->setText(QString("纬度: %1\n经度: %2\n高度: %3 米\n速度: %4 km/h")
                .arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6).arg(hgt, 0, 'f', 1).arg(displaySpeed, 0, 'f', 1));
            if (m_barLat) m_barLat->setText(QString("纬度: %1").arg(lat, 0, 'f', 6));
            if (m_barLon) m_barLon->setText(QString("经度: %1").arg(lon, 0, 'f', 6));
            if (m_barAlt) m_barAlt->setText(QString("高度: %1m").arg(hgt, 0, 'f', 1));
            if (m_barSpeed) m_barSpeed->setText(QString("速度: %1km/h").arg(displaySpeed, 0, 'f', 1));
        }
    });
    posInfoTimer->start(200);

    /* posPushTimer：C++ 独立位置引擎，任何模式下都持续发送位置到 gps-core。
       即使 WebEngine 冻结，GPS 信号也不中断。
       路径跟随模式下，C++ 独立计算位置推进。 */
    posPushTimer = new QTimer(this);
    posPushTimer->setTimerType(Qt::PreciseTimer);
    connect(posPushTimer, &QTimer::timeout, this, [this]() {
        if (g_closing) return;
        /* 读 JS 当前状态 */
        if (!webView || !webView->page()) return;
        webView->page()->runJavaScript(
            "var _lat=typeof window.lat==='number'?window.lat:0,"
            "_lon=typeof window.lon==='number'?window.lon:0,"
            "_hgt=typeof window.hgt==='number'?window.hgt:0,"
            "_spd=typeof currentSpeedKmH==='number'?currentSpeedKmH:0,"
            "_pf=typeof pathFollowing==='boolean'?pathFollowing:false,"
            "_pp=typeof pausedPathFollowing==='boolean'?pausedPathFollowing:false,"
            "_pc=typeof pathCompleted==='boolean'?pathCompleted:false,"
            "_idx=typeof currentPathIndex==='number'?currentPathIndex:0,"
            "_len=typeof originalPathLinear==='object'&&originalPathLinear!==null?originalPathLinear.length:0,"
            "_cs=typeof currentSpeedKmH==='number'?currentSpeedKmH:0;"
            "String(_lat)+','+String(_lon)+','+String(_hgt)+','+String(_spd)+','+"
            "String(_pf)+','+String(_pp)+','+String(_pc)+','+String(_idx)+','+String(_len)",
            [this](const QVariant &v) {
                QStringList p = v.toString().split(",");
                if (p.size() < 9) return;
                double jlat = p[0].toDouble();
                double jlon = p[1].toDouble();
                double jhgt = p[2].toDouble();
                double jspd = p[3].toDouble();
                bool jpf = p[4] == "true";
                bool jpp = p[5] == "true";
                bool jpc = p[6] == "true";
                int jidx = p[7].toInt();
                int jlen = p[8].toInt();

                if (jpc != m_pathCompleted) {
                    m_pathCompleted = jpc;
                    if (btnRouteStart) {
                        if (jpc) {
                            btnRouteStart->setText("▶ 开始");
                            btnRouteStart->setStyleSheet("QPushButton{background:#9e9e9e;color:#fff;border:none;border-radius:4px;font-size:11px}");
                            btnRouteStart->setEnabled(false);
                        } else {
                            btnRouteStart->setText("▶ 开始");
                            btnRouteStart->setStyleSheet("QPushButton{background:#22c55e;color:#fff;border:none;border-radius:4px;font-size:11px}QPushButton:hover{background:#16a34a}");
                            btnRouteStart->setEnabled(true);
                        }
                    }
                }

                /* 缓存位置 */
                cppLat = jlat; cppLon = jlon; cppHgt = jhgt;
                cppCurrentSpeed = jspd;

                /* C++ 不再独立模拟位置推进，只负责转发 JS 位置到 gps-core */
                sendUdpPos(jlat, jlon, jhgt);
            });
    });
    posPushTimer->start(100);

    refreshBrdc();
    log("控制台已启动，正在初始化...");
    log("DEBUG-1");

    QSettings ssInit(dataDir + "/settings.ini", QSettings::IniFormat);
    m_mapSource = ssInit.value("mapSource", "amap").toString();
    m_amapSecurityCode = ssInit.value("amapSecurityCode", kBuiltinSecurityCode).toString();
    m_apiKeys = kBuiltinAmapKeys;
    m_apiKeysTianditu = kBuiltinTiandituKeys;
    if (m_mapSource == "tianditu") {
        m_apiKeyIndexTianditu = 0;
        m_sessionApiKey = m_apiKeysTianditu.value(0, "c8668a8aaf9e07cf8c21433a9d13ce0a");
    } else {
        m_apiKeyIndex = 0;
        m_sessionApiKey = m_apiKeys.value(0, "3ecf56d2c71663821d44b9019ce4a9b7");
    }

    beijingTimer = new QTimer(this);
    connect(beijingTimer, &QTimer::timeout, this, &MainWindow::updateBeijingTime);
    beijingTimer->start(1000);
    updateBeijingTime();

    httpServer = new QTcpServer(this);
    httpPort = 8080;
    for (int p = 8080; p <= 8085; p++) {
        if (httpServer->listen(QHostAddress::LocalHost, p)) {
            httpPort = p;
            break;
        }
    }
    if (httpPort > 8085) {
        log("HTTP 服务器端口全部被占用 (8080-8085)");
    } else {
        log(QString("HTTP 服务器已启动 (port %1)").arg(httpPort));
        log(QString("HTTP 服务器已启动 (port %1)").arg(httpPort), "MAP");
        connect(httpServer, &QTcpServer::newConnection, this, [this]() {
            QTcpSocket *s = httpServer->nextPendingConnection();
            connect(s, &QTcpSocket::readyRead, this, [this, s]() {
                QByteArray req = s->readAll();
                QStringList lines = QString::fromUtf8(req).split("\r\n");
                if (lines.isEmpty()) { s->close(); return; }
                QStringList parts = lines[0].split(' ');
                if (parts.size() < 2) { s->close(); return; }
                QString method = parts[0], rawPath = parts[1];
                /* Strip query string from path */
                QString path = rawPath;
                int qp = path.indexOf('?');
                if (qp >= 0) path = path.left(qp);

                if (method == "GET") {
                    if (path == "/") path = "/static/qtmap.html";
                    if (path.startsWith("/static/") && !path.contains("..")) {
                        QString filePath = appDir + path;
                        QFile f(filePath);
                        if (f.open(QIODevice::ReadOnly)) {
                            QByteArray data = f.readAll();
                            f.close();
                            QString ext = QFileInfo(filePath).suffix();
                            QString mime = "text/plain";
                            if (ext == "html") mime = "text/html; charset=utf-8";
                            else if (ext == "js") mime = "application/javascript";
                            else if (ext == "css") mime = "text/css";
                            else if (ext == "png") mime = "image/png";
                            if (filePath.endsWith("qtmap.html")) {
                                QSettings ss(dataDir + "/settings.ini", QSettings::IniFormat);
                                double svLat = ss.value("initLat", 0.0).toDouble();
                                double svLon = ss.value("initLon", 0.0).toDouble();
                                QString htmlContent = QString::fromUtf8(data);
                                if (svLat != 0.0 && svLon != 0.0) {
                                    htmlContent.replace("INIT_LAT = 39.906217", QString("INIT_LAT = %1").arg(QString::number(svLat, 'f', 7)));
                                    htmlContent.replace("INIT_LON = 116.397507", QString("INIT_LON = %1").arg(QString::number(svLon, 'f', 7)));
                                }
                                htmlContent.replace("__API_KEY__", m_sessionApiKey);
                                htmlContent.replace("__MAP_SOURCE__", m_mapSource);
                                htmlContent.replace("__AMAP_SECURITY_CODE__", m_amapSecurityCode);
                                htmlContent.replace("__TILE_PROXY__", QString("http://127.0.0.1:%1/tile").arg(httpPort));
#ifdef GPS_VERSION_GAODUAN
                                htmlContent.replace("__VERSION_TYPE__", "gaoduan");
#else
                                htmlContent.replace("__VERSION_TYPE__", "quanneng");
#endif
                                data = htmlContent.toUtf8();
                            }
                            s->write(QString("HTTP/1.1 200 OK\r\nContent-Type: %1\r\nContent-Length: %2\r\nCache-Control: no-cache, no-store, must-revalidate\r\nPragma: no-cache\r\nConnection: close\r\n\r\n").arg(mime).arg(data.size()).toUtf8());
                            s->write(data);
                        } else {
                            s->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                        }
                    } else if (path.startsWith("/tile/")) {
                        QStringList tp = path.mid(6).split('/');
                        if (tp.size() == 3) {
                            QString z = tp[0], x = tp[1], y = tp[2];
                            if (y.endsWith(".png")) y.chop(4);
                            QString tileRel = z + "/" + x + "/" + y + ".png";
                            QString tileDir = dataDir + "/tiles/" + z + "/" + x;
                            QString tileFile = dataDir + "/tiles/" + tileRel;
                            QByteArray tileData;
                            QFile tf(tileFile);
                            if (tf.open(QIODevice::ReadOnly)) {
                                tileData = tf.readAll();
                                tf.close();
                            } else {
                                QString localBase = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
                                localBase.replace("Roaming", "Local");
                                QString localFile = localBase + "/PosNavTech/tiles/" + tileRel;
                                QFile lf(localFile);
                                if (lf.open(QIODevice::ReadOnly)) {
                                    tileData = lf.readAll();
                                    lf.close();
                                }
                            }
                            if (!tileData.isEmpty()) {
                                s->write(QString("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: %1\r\nCache-Control: max-age=86400\r\nConnection: close\r\n\r\n").arg(tileData.size()).toUtf8());
                                s->write(tileData);
                            } else {
                                /* 关键修复：绝对不能阻塞主事件循环！直接返回404，由前端异步加载 */
                                s->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nCache-Control: max-age=60\r\nConnection: close\r\n\r\n");
                                /* 触发后台异步下载任务（单线程队列，不阻塞 UI） */
                                QString zKey = z; QString xKey = x; QString yKey = y; QString tileFileKey = tileFile; QString tileDirKey = tileDir;
                                QMetaObject::invokeMethod(this, [this, zKey, xKey, yKey, tileFileKey, tileDirKey]() {
                                    ensureTileCached(zKey, xKey, yKey, tileFileKey, tileDirKey);
                                }, Qt::QueuedConnection);
                            }
                        } else {
                            s->write("HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                        }
                    } else {
                        s->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                    }
                } else if (method == "POST" && path == "/post") {
                    int bodyStart = req.indexOf("\r\n\r\n") + 4;
                    if (bodyStart > 4) {
                        QByteArray body = req.mid(bodyStart);
                        QString bodyStr = QString::fromUtf8(body);
                        double lat = 0, lon = 0, hgt = 0;
                        QStringList pairs = bodyStr.split('&');
                        for (const QString &pair : pairs) {
                            QStringList kv = pair.split('=');
                            if (kv.size() == 2) {
                                QString key = kv[0], val = kv[1];
                                if (key == "lon") lon = val.toDouble();
                                else if (key == "lat") lat = val.toDouble();
                                else if (key == "hgt") hgt = val.toDouble();
                            }
                        }
                        QUdpSocket udp;
                        double data[3] = {lat, lon, hgt};
                        udp.writeDatagram((const char*)data, sizeof(data), QHostAddress::LocalHost, 5678);
                    }
                    s->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK");
                } else if (method == "POST" && path == "/cpppath") {
                    int bodyStart = req.indexOf("\r\n\r\n") + 4;
                    if (bodyStart > 4) {
                        QByteArray body = req.mid(bodyStart);
                        QString bodyStr = QString::fromUtf8(body);
                        /* body 格式: points=JSON_ENCODED_PATH_DATA */
                        int eq = bodyStr.indexOf('=');
                        if (eq >= 0) {
                            QString json = QUrl::fromPercentEncoding(bodyStr.mid(eq + 1).toUtf8());
                            QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
                            if (doc.isObject()) {
                                QJsonObject obj = doc.object();
                                m_ppx.clear(); m_ppy.clear(); m_ppz.clear();
                                QJsonArray pts = obj["pts"].toArray();
                                for (const QJsonValue &v : pts) {
                                    if (v.isArray()) {
                                        QJsonArray p = v.toArray();
                                        if (p.size() >= 3) {
                                            m_ppx.push_back(p[0].toDouble());
                                            m_ppy.push_back(p[1].toDouble());
                                            m_ppz.push_back(p[2].toDouble());
                                        }
                                    } else if (v.isObject()) {
                                        QJsonObject p = v.toObject();
                                        m_ppx.push_back(p["x"].toDouble());
                                        m_ppy.push_back(p["y"].toDouble());
                                        m_ppz.push_back(p["z"].toDouble());
                                    }
                                }
                                m_pathLen = (int)m_ppx.size();
                                m_pi = obj["idx"].toInt();
                                m_pf = obj["pf"].toBool();
                                m_pp = obj["pp"].toBool();
                                m_cyc = obj["cyc"].toBool(true);
                                if (obj.contains("cx")) m_cx = obj["cx"].toDouble();
                                if (obj.contains("cy")) m_cy = obj["cy"].toDouble();
                                if (obj.contains("cz")) m_cz = obj["cz"].toDouble();
                                if (obj.contains("tx")) m_tx = obj["tx"].toDouble();
                                if (obj.contains("ty")) m_ty = obj["ty"].toDouble();
                                if (obj.contains("tz")) m_tz = obj["tz"].toDouble();
                                log(QString("C++ path sync: %1 pts, pf=%2 pi=%3").arg(m_pathLen).arg(m_pf).arg(m_pi));
                            }
                        }
                    }
                    s->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK");
                } else if (method == "POST" && path == "/log") {
                    int bodyStart = req.indexOf("\r\n\r\n") + 4;
                    if (bodyStart > 4) {
                        QByteArray body = req.mid(bodyStart);
                        QString bodyStr = QString::fromUtf8(body);
                        /* 解析 msg 和 tag 参数 */
                        QString msg, tag = "MAP";
                        QStringList pairs = bodyStr.split('&');
                        for (const QString &pair : pairs) {
                            QStringList kv = pair.split('=');
                            if (kv.size() == 2) {
                                QString key = QUrl::fromPercentEncoding(kv[0].toUtf8());
                                QString val = QUrl::fromPercentEncoding(kv[1].toUtf8());
                                if (key == "msg") msg = val;
                                else if (key == "tag") tag = val;
                            }
                        }
                        if (!msg.isEmpty()) {
                            log(msg, tag.toUtf8().constData());
                        }
                    }
                    s->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK");
                }
                s->close();
            });
            connect(s, &QTcpSocket::disconnected, s, &QTcpSocket::deleteLater);
        });
    }

    /* Load saved map source, migrate old values */
    QSettings ss2(dataDir + "/settings.ini", QSettings::IniFormat);
    m_mapSource = ss2.value("mapSource", "amap").toString();
    if (m_mapSource == "baidu") {
        m_mapSource = "amap";
        ss2.setValue("mapSource", "amap");
    }
    m_amapSecurityCode = ss2.value("amapSecurityCode", kBuiltinSecurityCode).toString();
    int idx = m_mapSourceCombo->findData(m_mapSource);
    if (idx >= 0) m_mapSourceCombo->setCurrentIndex(idx);

    QTimer::singleShot(200, this, [this]() {
        if (!webView) return;
        loadApiKeys();
        if (m_apiKeys.isEmpty()) m_apiKeys << "3ecf56d2c71663821d44b9019ce4a9b7";
        if (m_apiKeysTianditu.isEmpty()) m_apiKeysTianditu << "c8668a8aaf9e07cf8c21433a9d13ce0a";
        m_btnApiKey->setEnabled(true);
        m_btnApiKey->setToolTip("");
        if (m_mapSource == "tianditu") {
            if (m_apiKeyIndexTianditu < 0 || m_apiKeyIndexTianditu >= m_apiKeysTianditu.size()) m_apiKeyIndexTianditu = 0;
            m_sessionApiKey = m_apiKeysTianditu[m_apiKeyIndexTianditu];
            m_apiKeyIndexTianditu = (m_apiKeyIndexTianditu + 1) % m_apiKeysTianditu.size();
        } else {
            if (m_apiKeyIndex < 0 || m_apiKeyIndex >= m_apiKeys.size()) m_apiKeyIndex = 0;
            m_sessionApiKey = m_apiKeys[m_apiKeyIndex];
            m_apiKeyIndex = (m_apiKeyIndex + 1) % m_apiKeys.size();
        }
        saveApiKeys();
        if (m_apiKeyLabel) m_apiKeyLabel->setText("Key:" + m_sessionApiKey.left(5));
        if (m_btnApiKey) m_btnApiKey->setText(m_sessionApiKey.left(8) + "...");
        webView->setUrl(QUrl(QString("http://127.0.0.1:%1/static/qtmap.html").arg(httpPort)));
        log("地图正在加载到主界面...");
        connect(webView->page(), &QWebEnginePage::loadFinished, this, [this](bool) {
            m_mapReady = true;
        });
    });

    QTimer::singleShot(1000, this, [this]() {
        log("正在自动下载最新星历...");
        onDownloadBrdc();
    });
}

MainWindow::~MainWindow() {
    g_closing = true;
    if (keepaliveTimer) keepaliveTimer->stop();
    if (posPushTimer) posPushTimer->stop();
    if (posInfoTimer) posInfoTimer->stop();
    if (pollTimer) pollTimer->stop();
    if (beijingTimer) beijingTimer->stop();
    if (jitterTimer) jitterTimer->stop();
    if (trackingTimer) trackingTimer->stop();
    if (tileRetryTimer) tileRetryTimer->stop();
    if (httpServer) httpServer->close();
    if (netMgr) { netMgr->disconnect(); netMgr->deleteLater(); }
    if (tileNetMgr) { tileNetMgr->disconnect(); tileNetMgr->deleteLater(); }
    onStopGps();
}

void MainWindow::closeEvent(QCloseEvent *e) {
#ifndef GPS_VERSION_GAODUAN
    if (m_mode == 1 && m_pathPointCount > 0) {
        e->ignore();
        int ret = QMessageBox::question(this, "保存轨迹",
            "当前公路轨迹是否保存为自定义路径JSON？\n保存后下次打开将自动作为自定义路径导入。",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (ret == QMessageBox::Cancel) return;
        if (ret == QMessageBox::Yes) {
            QString path = QFileDialog::getSaveFileName(this, "保存路径", dataDir + "/route_path.json", "JSON (*.json)");
            if (path.isEmpty()) return;
            if (webView && webView->page()) {
                webView->page()->runJavaScript("qtExportPath()", [this, path](const QVariant &v) {
                    QString json = v.toString();
                    if (json.isEmpty()) {
                        QMessageBox::warning(this, "保存失败", "没有可保存的路径");
                        doClose();
                        return;
                    }
                    QFile f(path);
                    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        f.write(json.toUtf8());
                        f.close();
                        QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
                        s.setValue("savedPathFile", path);
                        s.setValue("lastMode", 2);
                        s.sync();
                    }
                    doClose();
                });
            }
        } else {
            QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
            s.setValue("lastMode", m_mode);
            s.sync();
            doClose();
        }
    } else
#endif
    {
        QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
        s.setValue("lastMode", m_mode);
        s.sync();
        doClose();
    }
}

void MainWindow::doClose() {
    g_closing = true;
    QFile::remove(QCoreApplication::applicationDirPath() + "/.crash_marker");
    if (keepaliveTimer) keepaliveTimer->stop();
    if (posPushTimer) posPushTimer->stop();
    if (posInfoTimer) posInfoTimer->stop();
    if (pollTimer) pollTimer->stop();
    if (beijingTimer) beijingTimer->stop();
    if (jitterTimer) jitterTimer->stop();
    if (trackingTimer) trackingTimer->stop();
    if (tileRetryTimer) tileRetryTimer->stop();
    if (httpServer) httpServer->close();
    if (netMgr) { netMgr->disconnect(); netMgr->deleteLater(); }
    if (tileNetMgr) { tileNetMgr->disconnect(); tileNetMgr->deleteLater(); }
    hide();
    if (usrpThread) {
        usrpThread->stopUsrp();
        if (!usrpThread->wait(3000)) usrpThread->terminate();
    }
    onStopGps();
    QSettings ss(dataDir + "/settings.ini", QSettings::IniFormat);
    if (centerLat != 0.0 || centerLon != 0.0) {
        ss.setValue("initLat", QString::number(centerLat, 'f', 7));
        ss.setValue("initLon", QString::number(centerLon, 'f', 7));
    }
    ss.sync();
    qApp->quit();
}

void MainWindow::changeEvent(QEvent *e) {
    if (e->type() == QEvent::WindowStateChange) {
        /* keepaliveTimer 每秒执行 runJavaScript("void 0") 保持页面活跃，
           JS 端已覆写 Page Visibility API 始终报告可见，无需额外处理 */
    }
    QMainWindow::changeEvent(e);
}

void MainWindow::ensureTileCached(const QString &z, const QString &x, const QString &y, const QString &tileFile, const QString &tileDir) {
    QString key = z + "/" + x + "/" + y;
    if (tileInFlight.contains(key) || tileFailed.contains(key)) return;
    {
        QFile chk(tileFile);
        if (chk.exists() && chk.size() > 100) return;
    }
    /* 入队 */
    TileJob job;
    job.z = z; job.x = x; job.y = y;
    job.tileFile = tileFile; job.tileDir = tileDir;
    job.cdnIdx = 0; job.retry = 0;
    tileJobQueue.append(job);
    if (!tileJobRunning) processNextTileJob();
}

void MainWindow::processNextTileJob() {
    if (tileJobQueue.isEmpty()) {
        tileJobRunning = false;
        return;
    }
    static const char* cdnList[] = {
        "https://tile-a.openstreetmap.fr/hot",
        "https://a.tile.openstreetmap.fr/osmfr",
        "https://b.tile.openstreetmap.fr/osmfr",
        "https://c.tile.openstreetmap.fr/osmfr",
        "https://tile.openstreetmap.org",
        "https://a.tile.openstreetmap.org",
        "https://b.tile.openstreetmap.org",
        "https://c.tile.openstreetmap.org"
    };
    const int cdnCount = (int)(sizeof(cdnList)/sizeof(cdnList[0]));

    TileJob job = tileJobQueue.first();
    tileJobQueue.removeFirst();
    tileJobRunning = true;
    tileInFlight.insert(job.z + "/" + job.x + "/" + job.y);

    if (job.cdnIdx >= cdnCount) {
        tileInFlight.remove(job.z + "/" + job.x + "/" + job.y);
        tileFailed.insert(job.z + "/" + job.x + "/" + job.y);
        tileJobRunning = false;
        /* 50ms 后处理下一个任务，限制并发 */
        tileRetryTimer->start(50);
        return;
    }

    QString cdnUrl = QString("%1/%2/%3/%4.png").arg(cdnList[job.cdnIdx]).arg(job.z).arg(job.x).arg(job.y);
    QUrl tileUrl(cdnUrl);
    QNetworkRequest req(tileUrl);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("GPS-Realtime/1.0"));
    req.setRawHeader("Referer", "https://www.openstreetmap.org/");
    req.setTransferTimeout(5000);

    QNetworkReply *r = tileNetMgr->get(req);
    connect(r, &QNetworkReply::finished, this, [this, r, job, cdnCount]() {
        r->deleteLater();
        tileInFlight.remove(job.z + "/" + job.x + "/" + job.y);
        bool ok = false;
        if (r->error() == QNetworkReply::NoError &&
            r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            QByteArray data = r->readAll();
            if (data.size() > 100) {
                QDir().mkpath(job.tileDir);
                QFile cf(job.tileFile);
                if (cf.open(QIODevice::WriteOnly)) { cf.write(data); cf.close(); }
                QString localBase2 = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
                localBase2.replace("Roaming", "Local");
                QString localDir2 = localBase2 + "/PosNavTech/tiles/" + job.z + "/" + job.x;
                QString localFile2 = localBase2 + "/PosNavTech/tiles/" + job.z + "/" + job.x + "/" + job.y + ".png";
                QDir().mkpath(localDir2);
                QFile lf2(localFile2);
                if (lf2.open(QIODevice::WriteOnly)) { lf2.write(data); lf2.close(); }
                ok = true;
            }
        }
        if (!ok) {
            TileJob next = job;
            next.cdnIdx++;
            tileJobQueue.prepend(next);
        }
        tileRetryTimer->start(100);
    });
}

bool MainWindow::eventFilter(QObject *obj, QEvent *e) {
    if (e->type() == QEvent::Wheel) {
        QSlider *sl = qobject_cast<QSlider*>(obj);
        if (sl) {
            QWheelEvent *we = static_cast<QWheelEvent*>(e);
            sl->setValue(sl->value() + ((we->angleDelta().y() > 0) ? 1 : -1));
            return true;
        }
    } else if (obj == titleBar) {
        if (e->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent*>(e);
            if (me->button() == Qt::LeftButton)
                m_dragPos = me->globalPosition().toPoint() - frameGeometry().topLeft();
        } else if (e->type() == QEvent::MouseMove) {
            QMouseEvent *me = static_cast<QMouseEvent*>(e);
            if (me->buttons() & Qt::LeftButton && !m_dragPos.isNull() && !isMaximized())
                move(me->globalPosition().toPoint() - m_dragPos);
        } else if (e->type() == QEvent::MouseButtonRelease) {
            m_dragPos = QPoint();
        }
    }
    return QMainWindow::eventFilter(obj, e);
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_NCHITTEST) {
            QPoint pos = QCursor::pos();
            QRect rect = frameGeometry();
            int x = pos.x() - rect.x();
            int y = pos.y() - rect.y();
            int w = rect.width();
            int h = rect.height();
            int border = 6;

            bool top = y < border;
            bool bottom = y > h - border;
            bool left = x < border;
            bool right = x > w - border;

            if (top && left) { *result = HTTOPLEFT; return true; }
            if (top && right) { *result = HTTOPRIGHT; return true; }
            if (bottom && left) { *result = HTBOTTOMLEFT; return true; }
            if (bottom && right) { *result = HTBOTTOMRIGHT; return true; }
            if (left) { *result = HTLEFT; return true; }
            if (right) { *result = HTRIGHT; return true; }
            if (top) { *result = HTTOP; return true; }
            if (bottom) { *result = HTBOTTOM; return true; }
        }
    }
    return false;
}
#endif

void MainWindow::log(const QString &msg, const char *tag) {
    QPlainTextEdit *target = logBox;
    if (QString(tag) == "MAP") target = logMAP;
    else if (QString(tag) == "GPS") target = logGPS;
    if (target) target->appendPlainText(QString("[%1] %2").arg(tag, msg));
    /* 写文件日志 */
    if (!m_logFile) {
        QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        QString logPath = desktop + "/日志.log";
        QFile::remove(logPath);
        m_logFile = new QFile(logPath, this);
        if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
            m_logFile->write(QString("=== 日志 %1 ===\n")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")).toUtf8());
            m_logFile->flush();
        }
    }
    if (m_logFile && m_logFile->isOpen()) {
        QString line = QString("[%1] %2 %3\n")
            .arg(tag, QDateTime::currentDateTime().toString("HH:mm:ss.zzz"), msg);
        m_logFile->write(line.toUtf8());
        m_logFile->flush();
    }
}

void MainWindow::onMapLoaded(bool ok) {
    if (ok && webView->url().toString().contains("qtmap.html")) {
        log("地图加载完成");
        mapRetryCount = 0;
        webView->page()->runJavaScript("qtGetPosForUDP()", [this](const QVariant &v) {
            QStringList parts = v.toString().split(",");
            if (parts.size() >= 2) {
                centerLat = parts[0].toDouble();
                centerLon = parts[1].toDouble();
                log(QString("中心位置: %1, %2").arg(centerLat,0,'f',6).arg(centerLon,0,'f',6));
            }
        });
        {
            QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
            int lastMode = s.value("lastMode", 0).toInt();
#ifndef GPS_VERSION_GAODUAN
            if (lastMode < 0 || lastMode > 2) lastMode = 0;
            // If last mode was route (1) but no saved path, fall back to single (0)
            // If there's a saved path, always use custom mode (2)
            QString lastPath = s.value("savedPathFile").toString();
            if (lastMode == 1 && (lastPath.isEmpty() || !QFile::exists(lastPath))) {
                lastMode = 0;
            }
            if (!lastPath.isEmpty() && QFile::exists(lastPath)) {
                lastMode = 2;
            }
#else
            if (lastMode < 0 || lastMode > 1) lastMode = 0;
#endif
            onModeSwitchChanged(lastMode);
        }
#ifndef GPS_VERSION_GAODUAN
        QTimer::singleShot(500, this, [this]() {
            QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
            QString lastPath = s.value("savedPathFile").toString();
            if (!lastPath.isEmpty() && QFile::exists(lastPath)) {
                QFile file(lastPath);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QString content = QString::fromUtf8(file.readAll());
                    file.close();
                    QString jsonStr = QString::fromUtf8(QJsonDocument(QJsonArray{content}).toJson(QJsonDocument::Compact));
                    jsonStr = jsonStr.mid(1, jsonStr.length() - 2);
                    QString js = QString("qtImportPath(%1)").arg(jsonStr);
                    webView->page()->runJavaScript(js, [this](const QVariant &v) {
                        QString result = v.toString();
                        log(result);
                        if (result.startsWith("SPEED:")) {
                            int sep = result.indexOf('|');
                            if (sep > 0) {
                                QString speedStr = result.mid(6, sep - 6);
                                bool ok = false;
                                double speedKmH = speedStr.toDouble(&ok);
                                if (ok && speedKmH >= 0 && speedSlider) {
                                    if (speedSlider) speedSlider->setValue(qBound(0, (int)speedKmH, 300));
                                }
                            }
                        }
                        // Auto-enable cycle mode after path import
                        if (btnCycle) {
                            setCycleOn();
                        }
                    });
                    log("已自动加载上次路径: " + QFileInfo(lastPath).fileName());
                }
            }
        });
#endif
    } else if (mapRetryCount < 5) {
        mapRetryCount++;
        log(QString("地图加载失败，重试%1/5...").arg(mapRetryCount));
        QTimer::singleShot(2000, this, [this]() {
            webView->setUrl(QUrl(QString("http://127.0.0.1:%1/static/qtmap.html").arg(httpPort)));
        });
    } else {
        log("地图加载失败");
        if (!QCoreApplication::arguments().contains("--safe")) {
            QProcess::startDetached(QCoreApplication::applicationFilePath(), QStringList() << "--safe");
            QApplication::quit();
        }
    }
}

void MainWindow::checkForUpdate(bool manual) {
    if (!m_updateMgr) m_updateMgr = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl(QString(UPDATE_BASE_URL) + "/version.json?t=" + QString::number(QDateTime::currentMSecsSinceEpoch())));
    req.setTransferTimeout(15000);
    QNetworkReply *reply = m_updateMgr->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, manual]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (manual) QMessageBox::warning(this, "检查更新", "无法连接更新服务器，请检查网络。");
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            if (manual) QMessageBox::warning(this, "检查更新", "云端版本信息无效。");
            return;
        }
        QJsonObject obj = doc.object();
        m_remoteVersion = obj["version_str"].toString();
        if (m_remoteVersion.isEmpty())
            m_remoteVersion = QString::number(obj["version"].toInt());
        m_updateNote = obj["note"].toString();
        m_updateFiles = obj["files"].toArray();
        m_updateRemove.clear();
        QJsonArray ra = obj["remove"].toArray();
        for (int i = 0; i < ra.size(); i++) {
            QString p = ra[i].toString();
            if (!p.isEmpty()) m_updateRemove << p;
        }
        if (m_remoteVersion.isEmpty() || m_updateFiles.isEmpty()) {
            if (manual) QMessageBox::warning(this, "检查更新", "云端版本信息无效。");
            return;
        }
        QString localVer;
        QFile vf(appDir + "/version.txt");
        if (vf.open(QIODevice::ReadOnly)) {
            localVer = QString::fromUtf8(vf.readAll().trimmed());
            vf.close();
        }
        if (compareVersion(m_remoteVersion, localVer) <= 0) {
            if (manual) QMessageBox::information(this, "检查更新",
                QString("本地版本 v%1\n云端版本 v%2\n已是最新版本。").arg(localVer).arg(m_remoteVersion));
            return;
        }
        bool allMatch = true;
        for (int i = 0; i < m_updateFiles.size(); i++) {
            QJsonObject f = m_updateFiles[i].toObject();
            QString path = f["path"].toString();
            QString expect = f["md5"].toString();
            if (path.isEmpty() || expect.isEmpty() || expect == "missing") continue;
            QFile local(appDir + "/" + path);
            if (!local.open(QIODevice::ReadOnly)) { allMatch = false; break; }
            QByteArray data = local.readAll();
            local.close();
            if (QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex() != expect) { allMatch = false; break; }
        }
        if (allMatch) {
            for (const QString &rp : m_updateRemove) {
                if (QFile::exists(appDir + "/" + rp)) { allMatch = false; break; }
            }
        }
        if (allMatch) {
            QFile vf(appDir + "/version.txt");
            if (vf.open(QIODevice::WriteOnly)) {
                vf.write(m_remoteVersion.toUtf8());
                vf.close();
            }
            if (manual) QMessageBox::information(this, "检查更新",
                QString("本地版本 v%1\n云端版本 v%2\n本地文件已是最新，版本号已同步。").arg(localVer).arg(m_remoteVersion));
            return;
        }
        QString notice = QString("检测到新版本 v%1（当前 v%2）").arg(m_remoteVersion).arg(localVer);
        if (!m_updateNote.isEmpty()) notice += "\n本次更新：" + m_updateNote;
        notice += "\n当前版本已满足使用需求，不建议更新。\n若确实需要更新请点击[是]，否则点击[否]保持当前版本。";
        int ret = QMessageBox::question(this, "发现新版本", notice,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
        m_updateTmpDir = appDir + "/update_tmp";
        QDir d(m_updateTmpDir);
        if (d.exists()) d.removeRecursively();
        d.mkpath(m_updateTmpDir);
        m_updateIdx = 0;
        if (!m_updateProgress) m_updateProgress = new QProgressDialog(this);
        m_updateProgress->setWindowTitle("正在更新");
        m_updateProgress->setRange(0, m_updateFiles.size());
        m_updateProgress->setValue(0);
        m_updateProgress->setLabelText(QString("正在下载更新文件... 0/%1").arg(m_updateFiles.size()));
        m_updateProgress->setCancelButton(nullptr);
        m_updateProgress->setWindowModality(Qt::WindowModal);
        m_updateProgress->setMinimumDuration(0);
        m_updateProgress->show();
        downloadNextUpdateFile();
    });
}

void MainWindow::downloadNextUpdateFile() {
    if (m_updateIdx >= m_updateFiles.size()) { applyUpdateAndRestart(); return; }
    QJsonObject f = m_updateFiles[m_updateIdx].toObject();
    QString path = f["path"].toString();
    if (path.isEmpty()) { m_updateIdx++; m_updateRetry = 0; downloadNextUpdateFile(); return; }
    QString expect = f["md5"].toString();
    if (!expect.isEmpty() && expect != "missing") {
        QFile local(appDir + "/" + path);
        if (local.open(QIODevice::ReadOnly)) {
            QByteArray localData = local.readAll();
            local.close();
            if (QCryptographicHash::hash(localData, QCryptographicHash::Md5).toHex() == expect) {
                m_updateIdx++;
                m_updateRetry = 0;
                downloadNextUpdateFile();
                return;
            }
        }
    }
    if (m_updateProgress) {
        m_updateProgress->setLabelText(QString("正在下载更新文件 %1/%2: %3").arg(m_updateIdx + 1).arg(m_updateFiles.size()).arg(path));
        m_updateProgress->setValue(m_updateIdx);
    }
    QString dst = m_updateTmpDir + "/" + path;
    QFileInfo fi(dst);
    QDir().mkpath(fi.absolutePath());
    QNetworkRequest req(QUrl(QString(UPDATE_BASE_URL) + "/" + path + "?t=" + QString::number(QDateTime::currentMSecsSinceEpoch())));
    req.setTransferTimeout(60000);
    QNetworkReply *reply = m_updateMgr->get(req);
    connect(reply, &QNetworkReply::downloadProgress, this, [this, reply, path](qint64 received, qint64 total) {
        if (m_updateProgress && total > 0)
            m_updateProgress->setLabelText(QString("正在下载 %1  %2%").arg(path).arg((int)(received * 100 / total)));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, f, path, dst]() {
        reply->deleteLater();
        bool ok = reply->error() == QNetworkReply::NoError;
        if (ok) {
            QByteArray data = reply->readAll();
            QFile out(dst);
            if (out.open(QIODevice::WriteOnly)) {
                out.write(data);
                out.close();
            } else ok = false;
            QString expect2 = f["md5"].toString();
            if (ok && !expect2.isEmpty() && expect2 != "missing") {
                if (QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex() != expect2) ok = false;
            }
        }
        if (ok) {
            m_updateIdx++;
            m_updateRetry = 0;
            downloadNextUpdateFile();
        } else if (m_updateRetry < 3) {
            m_updateRetry++;
            downloadNextUpdateFile();
        } else {
            if (m_updateProgress) m_updateProgress->close();
            QMessageBox::warning(this, "更新失败", "文件下载失败: " + path + "\n已重试 3 次仍失败，请稍后重试或联系开发者。");
        }
    });
}

void MainWindow::applyUpdateAndRestart() {
    if (m_updateProgress) {
        m_updateProgress->setValue(m_updateFiles.size());
        m_updateProgress->setLabelText(QString("正在下载更新文件... %1/%1").arg(m_updateFiles.size()));
        m_updateProgress->close();
    }
    if (!QFile::exists(appDir + "/updater.exe")) {
        if (!m_updateProgress) m_updateProgress = new QProgressDialog(this);
        m_updateProgress->setRange(0, 0);
        m_updateProgress->setValue(0);
        m_updateProgress->setLabelText("正在下载更新程序 updater.exe...");
        m_updateProgress->setCancelButton(nullptr);
        m_updateProgress->setWindowModality(Qt::WindowModal);
        m_updateProgress->setMinimumDuration(0);
        m_updateProgress->show();
        QNetworkRequest req(QUrl(QString(UPDATE_BASE_URL) + "/updater.exe?t=" + QString::number(QDateTime::currentMSecsSinceEpoch())));
        req.setTransferTimeout(30000);
        QNetworkReply *reply = m_updateMgr->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            bool ok = reply->error() == QNetworkReply::NoError;
            if (ok) {
                QByteArray data = reply->readAll();
                QFile out(appDir + "/updater.exe");
                if (out.open(QIODevice::WriteOnly)) {
                    out.write(data);
                    out.close();
                } else ok = false;
            }
            if (m_updateProgress) m_updateProgress->close();
            if (ok) confirmAndApply();
            else QMessageBox::warning(this, "更新失败", "无法获取更新程序 updater.exe\n请重新安装软件或联系开发者。");
        });
        return;
    }
    confirmAndApply();
}

void MainWindow::confirmAndApply() {
    int ret = QMessageBox::question(this, "更新完成",
        QString("更新文件已下载完成，是否立即重启软件生效？\n重启后软件将自动重新打开。"),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;
    QString pending = m_updateTmpDir + "/pending_update.txt";
    QFile pf(pending);
    if (!pf.open(QIODevice::WriteOnly)) return;
    QByteArray content;
    for (int i = 0; i < m_updateFiles.size(); i++) {
        QString path = m_updateFiles[i].toObject()["path"].toString();
        if (path.isEmpty()) continue;
        if (!QFile::exists(m_updateTmpDir + "/" + path)) continue;
        content += (m_updateTmpDir + "/" + path + "|" + appDir + "/" + path + "\n").toUtf8();
    }
    for (const QString &rp : m_updateRemove) {
        if (rp.isEmpty()) continue;
        content += ("DEL|" + appDir + "/" + rp + "\n").toUtf8();
    }
    content += QString("VERSION|%1\n").arg(m_remoteVersion).toUtf8();
    content += ("LAUNCH|" + appDir + "/gps-ui-qt.exe\n").toUtf8();
    pf.write(content);
    pf.close();
    if (gpsProc) {
        gpsProc->terminate();
        gpsProc->waitForFinished(3000);
    }
    QStringList args;
    args << pending << QString::number((qint64)QCoreApplication::applicationPid());
    if (!QProcess::startDetached(appDir + "/updater.exe", args)) {
        QMessageBox::warning(this, "更新失败", "无法启动更新程序，请重新打开软件后重试。");
        return;
    }
    QApplication::quit();
}

void MainWindow::onMapTypeChanged(const QString &type) {
    if (!webView || !webView->page()) return;
    QString js = QString("qtMapType('%1')").arg(type);
    webView->page()->runJavaScript(js);
    log(QString("地图类型切换: %1").arg(type));
}


void MainWindow::onLoadTxtPath() {
    QString filePath = QFileDialog::getOpenFileName(this, "选择TXT路径文件", QString(), "文本文件 (*.txt *.nmea);;所有文件(*)");
    if (filePath.isEmpty()) return;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        log("无法打开文件: " + filePath);
        return;
    }
    QByteArray raw = file.readAll();
    file.close();
    QString jsonStr = QString::fromUtf8(QJsonDocument(QJsonArray{QString::fromUtf8(raw)}).toJson(QJsonDocument::Compact));
    jsonStr = jsonStr.mid(1, jsonStr.length() - 2);
    QString js = QString("qtLoadTxt(%1)").arg(jsonStr);
    log("TXT 文件已加载(" + QString::number(raw.size()) + " 字节)");
    QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
    s.setValue("savedPathFile", filePath);
    s.sync();
    webView->page()->runJavaScript(js, [this](const QVariant &v) {
        log(v.toString());
        setCycleOn();
    });
}

void MainWindow::onExportPath() {
    webView->page()->runJavaScript("qtExportPath()", [this](const QVariant &v) {
        QString json = v.toString();
        if (json.isEmpty()) {
            log("当前没有路径数据可导出");
            return;
        }
        QString filePath = QFileDialog::getSaveFileName(this, "导出路径文件", "gps_path.json", "JSON文件 (*.json)");
        if (filePath.isEmpty()) return;
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(json.toUtf8());
            file.close();
            log("路径已导出 " + filePath);
        } else {
            log("导出失败: " + file.errorString());
        }
    });
}

void MainWindow::onImportPath() {
    QString filePath = QFileDialog::getOpenFileName(this, "选择JSON路径文件", QString(), "JSON文件 (*.json);;所有文件(*)");
    if (filePath.isEmpty()) return;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        log("无法打开文件: " + filePath);
        return;
    }
    QString content = QString::fromUtf8(file.readAll());
    file.close();
    QString jsonStr = QString::fromUtf8(QJsonDocument(QJsonArray{content}).toJson(QJsonDocument::Compact));
    jsonStr = jsonStr.mid(1, jsonStr.length() - 2);
    QString js = QString("qtImportPath(%1)").arg(jsonStr);
    QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
    s.setValue("savedPathFile", filePath);
    s.sync();
    webView->page()->runJavaScript(js, [this](const QVariant &v) {
        QString result = v.toString();
        log(result);
        if (result.startsWith("SPEED:")) {
            int sep = result.indexOf('|');
            if (sep > 0) {
                QString speedStr = result.mid(6, sep - 6);
                bool ok = false;
                double speedKmH = speedStr.toDouble(&ok);
                if (ok && speedKmH >= 0 && speedSlider) {
                    if (speedSlider) speedSlider->setValue(qBound(0, (int)speedKmH, 300));
                }
            }
        }
        setCycleOn();
    });
}

void MainWindow::onRouteExportPath() {
    webView->page()->runJavaScript("qtExportRoute()", [this](const QVariant &v) {
        QString json = v.toString();
        if (json.isEmpty()) {
            log("当前没有路线数据可导出");
            return;
        }
        QString filePath = QFileDialog::getSaveFileName(this, "导出路线文件", "gps_route.json", "JSON文件 (*.json)");
        if (filePath.isEmpty()) return;
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(json.toUtf8());
            file.close();
            log("路线已导出 " + filePath);
        } else {
            log("导出失败: " + file.errorString());
        }
    });
}

void MainWindow::onRouteImportPath() {
    QString filePath = QFileDialog::getOpenFileName(this, "选择路线JSON文件", QString(), "JSON文件 (*.json);;所有文件(*)");
    if (filePath.isEmpty()) return;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        log("无法打开文件: " + filePath);
        return;
    }
    QString content = QString::fromUtf8(file.readAll());
    file.close();
    QString jsonStr = QString::fromUtf8(QJsonDocument(QJsonArray{content}).toJson(QJsonDocument::Compact));
    jsonStr = jsonStr.mid(1, jsonStr.length() - 2);
    QString js = QString("qtImportRoute(%1)").arg(jsonStr);
    webView->page()->runJavaScript(js, [this](const QVariant &v) {
        QString result = v.toString();
        log(result);
        if (result.startsWith("SPEED:")) {
            int sep = result.indexOf('|');
            if (sep > 0) {
                QString speedStr = result.mid(6, sep - 6);
                bool ok = false;
                double speedKmH = speedStr.toDouble(&ok);
                if (ok && speedKmH >= 0 && speedSlider) {
                    if (speedSlider) speedSlider->setValue(qBound(0, (int)speedKmH, 300));
                }
            }
        }
        setCycleOn();
    });
}

void MainWindow::setTracking(bool on) {
    m_tracking = on;
    if (btnLockTrack) {
        if (on) {
            btnLockTrack->setText("📍 追踪中");
            btnLockTrack->setStyleSheet(
                "QPushButton{background:#005fb8;border:1px solid #005fb8;border-radius:4px;color:#fff;font-size:11px}"
                "QPushButton:hover{background:#004c8c}");
        } else {
            btnLockTrack->setText("📍 跟踪坐标");
            btnLockTrack->setStyleSheet(
                "QPushButton{background:#ffffff;border:1px solid #d0d5dd;border-radius:4px;color:#616161;font-size:11px}"
                "QPushButton:hover{background:#f0f4f8;color:#1a1a1a}");
        }
    }
    if (webView && webView->page())
        webView->page()->runJavaScript(QString("qtSetTracking(%1)").arg(on ? "true" : "false"));
    if (trackingTimer) {
        if (on) {
            trackingTimer->start(1000);
            log("追踪已开启");
        } else {
            trackingTimer->stop();
            log("追踪已关闭");
        }
    }
}

void MainWindow::onModeSwitchChanged(int val) {
    m_mode = val;
    syncModeButtons();
    if (modeStack) modeStack->setCurrentIndex(val);

    if (webView && webView->page()) {
        webView->page()->runJavaScript("qtClearPath()");
        webView->page()->runJavaScript("qtStopPath()");
        webView->page()->runJavaScript("qtCancelPreview()");
        webView->page()->runJavaScript("g_jitterEnabled=false");
    }

    m_routePending = false;

    if (val == 0) {
        modeLabel->setText("● 单点模式");
        modeLabel->setStyleSheet("font-size:11px;color:#005fb8;background:transparent");
        if (webView && webView->page())
            webView->page()->runJavaScript("qtResetMode('single')");
    } else if (val == 1) {
        modeLabel->setText("● 公路导航模式");
        modeLabel->setStyleSheet("font-size:11px;color:#22c55e;background:transparent");
        if (webView && webView->page()) {
            webView->page()->runJavaScript("qtResetMode('route')");
            webView->page()->runJavaScript("qtEnableRouteClick()");
        }
    }
#ifndef GPS_VERSION_GAODUAN
    else {
        modeLabel->setText("● 自定义路径模式");
        modeLabel->setStyleSheet("font-size:11px;color:#f59e0b;background:transparent");
        if (webView && webView->page()) {
            webView->page()->runJavaScript("qtResetMode('custom')");
            webView->page()->runJavaScript("qtEnablePathMode()");
        }
    }
#endif

    if (jitterEnabled) {
        jitterEnabled = false;
        if (btnJitter) {
            btnJitter->setText("▶ 抖动");
            btnJitter->setStyleSheet(
                "QPushButton{background:#ffffff;border:1px solid #d0d5dd;border-radius:4px;color:#616161;font-size:11px;font-weight:bold;padding:0 8px}"
                "QPushButton:hover{background:#f0f4f8;color:#1a1a1a}");
        }
    }
    if (trackJitterEnabled) {
        trackJitterEnabled = false;
        if (webView && webView->page()) webView->page()->runJavaScript("qtSetJitterEnabled(false)");
    }
    if (jitterTimer) jitterTimer->stop();

    QString modeName;
    if (m_mode == 0) modeName = "单点模式";
    else if (m_mode == 1) modeName = "公路导航模式";
#ifndef GPS_VERSION_GAODUAN
    else modeName = "自定义路径模式";
#else
    else modeName = "单点模式";
#endif
    log(QString("已切换为%1").arg(modeName));
}

void MainWindow::syncModeButtons() {
    if (btnMode1) btnMode1->setChecked(m_mode == 0);
    if (btnMode2) btnMode2->setChecked(m_mode == 1);
#ifndef GPS_VERSION_GAODUAN
    if (btnMode3) btnMode3->setChecked(m_mode == 2);
#endif
}

void MainWindow::onConfirmRoute() {
    QString js = QString("qtPlanRouteToClick(%1,%2)").arg(m_routeTargetLat,0,'f',7).arg(m_routeTargetLon,0,'f',7);
    if (webView && webView->page()) webView->page()->runJavaScript(js);
    log(QString("规划路线到: %1, %2").arg(m_routeTargetLat,0,'f',6).arg(m_routeTargetLon,0,'f',6));
}

void MainWindow::onCancelRoute() {
    m_routePending = false;
    log("已取消路线规划");
}

void MainWindow::onConfirmSingle() {
    if (webView && webView->page()) webView->page()->runJavaScript("qtConfirmSinglePos()");
    log("单点位置已确认");
}

void MainWindow::onCancelSingle() {
    if (webView && webView->page()) webView->page()->runJavaScript("qtCancelPreview()");
    log("已取消位置选择");
}

void MainWindow::onRandomRangeChanged(int val) {
    double range = val;
    if (randRangeLabel) randRangeLabel->setText(val > 0 ? QString("±%1 km/h").arg(range, 0, 'f', 0) : "关");
    if (!webView || !webView->page()) return;
    QString js = QString("qtSetRandomRange(%1)").arg(range, 0, 'f', 0);
    webView->page()->runJavaScript(js);
}

void MainWindow::onUsrpIq(const QVector<float> &samples) {
    if (satStatus) {
        satStatus->setConnected(true);
        float avg = 0;
        int n = samples.size();
        if (n > 0) {
            for (int i = 0; i < n; i++) avg += fabsf(samples[i]);
            float level = (avg / n) * 3.0f;
            if (level > 1.0f) level = 1.0f;
            satStatus->setSignalLevel(level);
        }
    }
}

void MainWindow::onLogMsg(const QString &msg) {
    log(msg, "USRP");
    usrpStatus->setText("● " + msg);
}

void MainWindow::onGpssimConnected() {
    if (satStatus) {
        for (int i = 1; i <= 10; i++) satStatus->setSatellitePRN(i, true);
    }
    gpssimConnected = true;
    log("GPS 信号已连接");
    QString devName = usrpThread ? QString::fromLatin1(usrpThread->deviceName()) : "USRP";
    usrpStatus->setText("● " + devName + " ← GPS 已连接");
}

void MainWindow::onGpssimDisconnected() {
    if (satStatus) {
        for (int i = 1; i <= 32; i++) satStatus->setSatellitePRN(i, false);
    }
    gpssimConnected = false;
    if (satStatus) satStatus->setConnected(false);
    usrpStatus->setText("●等待连接...");
}

void MainWindow::pollStatus() {
    bool gpsRunning = gpsProc && gpsProc->state() == QProcess::Running;
    btnGpsToggle->setChecked(gpsRunning);
    if (gpsRunning) {
        btnGpsToggle->setText("●停止 GPS");
    } else {
        btnGpsToggle->setText("●发射 GPS");
    }
    if (satStatus) satStatus->setGpsRunning(gpsRunning);
}

void MainWindow::onClearPath() {
    if (!webView || !webView->page()) return;
    webView->page()->runJavaScript("qtClearPath()");
    QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
    s.remove("savedPathFile");
    s.sync();
    log("路径已清除");
}

void MainWindow::updateBeijingTime() {
    QDateTime utc = QDateTime::currentDateTimeUtc();
    QDateTime bj = utc.addSecs(8 * 3600);
    if (beijingTimeLabel)
        beijingTimeLabel->setText(bj.toString("yyyy-MM-dd HH:mm:ss"));
    if (m_barTime)
        m_barTime->setText(bj.toString("yyyy-MM-dd HH:mm:ss"));
}

void MainWindow::setCycleOn() {
    if (!webView || !webView->page()) return;
    webView->page()->runJavaScript("if(cycleMode) qtToggleCycle(); qtToggleCycle(); cycleMode;", [this](const QVariant &v) {
        bool on = v.toBool();
        btnCycle->setText(on ? "关闭循环" : "开启循环");
        btnCycle->setStyleSheet(on
            ? "QPushButton{background:#ffffff;border:1px solid #66bb6a;border-radius:4px;padding:2px 8px;color:#fff;font-size:11px}"
                "QPushButton:hover{background:#66bb6a}"
            : "QPushButton{background:#ffffff;border:1px solid #d0d5dd;border-radius:4px;padding:2px 8px;color:#1a1a1a;font-size:11px}"
                "QPushButton:hover{background:#f0f4f8}");
        log(on ? "循环:开" : "循环:关");
    });
}

void MainWindow::onToggleCycle() {
    if (!webView || !webView->page()) return;
    webView->page()->runJavaScript("qtToggleCycle()", [this](const QVariant &v) {
        bool on = v.toBool();
        btnCycle->setText(on ? "关闭循环" : "开启循环");
        btnCycle->setStyleSheet(on
            ? "QPushButton{background:#ffffff;border:1px solid #66bb6a;border-radius:4px;padding:2px 8px;color:#fff;font-size:11px}"
                "QPushButton:hover{background:#66bb6a}"
            : "QPushButton{background:#ffffff;border:1px solid #d0d5dd;border-radius:4px;padding:2px 8px;color:#1a1a1a;font-size:11px}"
                "QPushButton:hover{background:#f0f4f8}");
        log(on ? "循环:开" : "循环:关");
    });
}

void MainWindow::onHeightSliderChanged(int val) {
    if (heightLabel) heightLabel->setText(QString("%1米").arg(val));
    if (!webView || !webView->page()) return;
    QString js = QString("qtSetHeight(%1)").arg(val);
    webView->page()->runJavaScript(js);
}

void MainWindow::onSpeedSliderChanged(int val) {
    double speedKmH = val;
    if (speedLabel) speedLabel->setText(QString("%1 km/h").arg(speedKmH, 0, 'f', 1));
    m_ts = speedKmH; /* 同步到 C++ 位置引擎 */
    if (!webView || !webView->page()) return;
    QString js = QString("qtSetSpeed(%1)").arg(speedKmH, 0, 'f', 1);
    webView->page()->runJavaScript(js);
}

void MainWindow::onStartGps() {
    if (gpsProc) {
        if (gpsProc->state() != QProcess::NotRunning) return;
        gpsProc->deleteLater();
        gpsProc = nullptr;
    }
#ifdef GPS_VERSION_GAODUAN
    // 高端版：自动检测最新星历
    QStringList filters = {"brdc*", "*.14n", "*.15n", "*.16n", "*.17n", "*.18n", "*.19n", "*.20n", "*.21n", "*.22n", "*.23n", "*.24n", "*.25n", "*.26n", "*.27n"};
    QStringList files = QDir(xingliDir).entryList(filters, QDir::Files, QDir::Name);
    files << QDir(appDir + "/手动星历").entryList(filters, QDir::Files, QDir::Name);
    files.removeDuplicates();
    files.sort();
    QString brdcFileName;
    for (const auto &f : files) { if (!f.isEmpty()) { brdcFileName = f; break; } }
    if (brdcFileName.isEmpty()) {
        log("无星历文件，请先手动放入 brdc* 文件");
        return;
    }
    QString brdcPath;
    if (QFile::exists(xingliDir + "/" + brdcFileName))
        brdcPath = xingliDir + "/" + brdcFileName;
    else
        brdcPath = appDir + "/手动星历/" + brdcFileName;
#else
    QString brdcFileName = brdcCombo->currentText();
    if (brdcFileName.isEmpty() || brdcFileName == "无星历") {
        log("无星历文件，请先放入 brdc* 文件");
        return;
    }
    QString brdcPath;
    if (QFile::exists(xingliDir + "/" + brdcFileName)) {
        brdcPath = brdcFileName;
    } else if (QFile::exists(appDir + "/手动星历/" + brdcFileName)) {
        brdcPath = appDir + "/手动星历/" + brdcFileName;
    } else {
        log("未找到星历文件: " + brdcFileName);
        return;
    }
#endif
    QString exe = appDir + "/gps-core.exe";
    if (!QFileInfo::exists(exe)) { log("未找到 " + exe); return; }
    gpsProc = new QProcess(this);
    gpsProc->setProcessChannelMode(QProcess::SeparateChannels);
    gpsProc->setWorkingDirectory(xingliDir);
    connect(gpsProc, &QProcess::readyReadStandardError, this, [this]() {
        QByteArray err = gpsProc->readAllStandardError();
        QString text = QString::fromUtf8(err);
        // Satellite data lines (first batch) -> "发射中"
        if (text.contains("[GPS]") && !text.contains("into run") && !gpsEmitted) {
            if (gpsStatus) gpsStatus->setText(QString::fromUtf8("\u53d1\u5c04\u4e2d"));
            if (gpsStatus) gpsStatus->setStyleSheet(
                "QLabel{background:#f59e0b;border:1px solid #d97706;border-radius:8px;"
                "font-size:16px;font-weight:bold;color:#ffffff;"
                "padding:12px 20px;}"
            );
        }
        // "Time into run = X.X" -> "已发射" when X >= 4.0
        if (text.contains("into run")) {
            int eqIdx = text.indexOf("=");
            if (eqIdx >= 0) {
                QString numStr = text.mid(eqIdx + 1).trimmed();
                bool ok = false;
                double sec = numStr.toDouble(&ok);
                if (ok && sec >= 2.0 && statusLight) {
                    statusLight->setStyleSheet("background:#22c55e;border:1px solid #16a34a;border-radius:9px;min-width:18px;min-height:18px;max-width:18px;max-height:18px");
                    if (m_statusText) m_statusText->setText("发射中");
                }
                if (ok && sec >= 4.0 && !gpsEmitted) {
                    gpsEmitted = true;
                    if (gpsStatus) gpsStatus->setText(QString::fromUtf8("\u6b63\u5728\u53d1\u5c04\u4e2d"));
                    if (gpsStatus) gpsStatus->setStyleSheet(
                        "QLabel{background:#22c55e;border:1px solid #16a34a;border-radius:8px;"
                        "font-size:16px;font-weight:bold;color:#ffffff;"
                        "padding:20px 30px;}"
                    );
                    log(QString::fromUtf8("GPS \u5df2\u53d1\u5c04"));
                }
            }
        }
        log(text.trimmed(), "GPS");
    });
    QStringList args;
    args << "-e" << brdcPath << "-s" << "2560000" << "-n" << "1999" << "-d" << "36000" << "-w" << "5678";
#ifdef GPS_VERSION_GAODUAN
    args << "-b" << "8";
#else
    if (m_bitDepth8)
        args << "-b" << "8";
    if (realtimeCheck && realtimeCheck->isChecked())
        args << "-T" << "now";
#endif
    if (usrpThread && !usrpThread->isRunning()) {
#ifdef GPS_VERSION_GAODUAN
        usrpThread->setDeviceName("pluto");
#else
        usrpThread->setDeviceName(deviceCombo->currentData().toString());
#endif
        qputenv("UHD_IMAGES_DIR", (appDir + "/uhd").toUtf8());
        usrpThread->start();
    }
    log("启动 gpssim: " + exe + " " + args.join(" "));
    gpsProc->start(exe, args);
    /* 提升 gps-core 进程优先级：防止软件最小化或后台运行时被 Windows 节流 */
    if (gpsProc->waitForStarted(3000)) {
#ifdef Q_OS_WIN
        HANDLE hProc = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_INFORMATION, FALSE, gpsProc->processId());
        if (hProc) {
            SetPriorityClass(hProc, ABOVE_NORMAL_PRIORITY_CLASS);
            CloseHandle(hProc);
            log("[GPS] gps-core 优先级已提升为 ABOVE_NORMAL_PRIORITY");
        }
        /* 防止系统进入睡眠状态，确保 gps-core 持续运行 */
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED);
        /* 提升主进程优先级，避免被节流 */
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#endif
    }
    connect(gpsProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus status) {
        if (g_closing || userStoppedGps) {
            userStoppedGps = false;
            m_gpsRestartCount = 0;
            return;
        }
        if (status == QProcess::NormalExit && exitCode == 0) {
            m_gpsRestartCount = 0;
            if (gpsStatus) gpsStatus->setText("GPS 已完成");
            if (gpsStatus) gpsStatus->setStyleSheet(
                "QLabel{background:#ffffff;border:1px solid #d0d5dd;border-radius:8px;"
                "font-size:16px;font-weight:bold;color:#22c55e;"
                "padding:20px 30px;}"
            );
            log("gpssim 正常结束");
            return;
        }
        if (gpsStatus) gpsStatus->setText("GPS 已停止");
        if (gpsStatus) gpsStatus->setStyleSheet(
            "QLabel{background:#ffffff;border:1px solid #d0d5dd;border-radius:8px;"
            "font-size:16px;font-weight:bold;color:#ef4444;"
            "padding:20px 30px;}"
        );
        if (satStatus) satStatus->setGpsRunning(false);
        m_gpsRestartCount++;
        if (m_gpsRestartCount > 5) {
            m_gpsRestartCount = 0;
            log("gpssim 连续异常退出，已停止自动重启，请检查硬件后手动启动");
            return;
        }
        log(QString("gpssim 异常退出 (code=%1)，正在重启...").arg(exitCode));
        if (!m_gpsRestartTimer) {
            m_gpsRestartTimer = new QTimer(this);
            m_gpsRestartTimer->setSingleShot(true);
            connect(m_gpsRestartTimer, &QTimer::timeout, this, &MainWindow::onStartGps);
        }
        m_gpsRestartTimer->start(2000);
    });
    if (gpsStatus) gpsStatus->setText("GPS 启动中...");
    if (gpsStatus) gpsStatus->setStyleSheet(
        "QLabel{background:#ffffff;border:1px solid #d0d5dd;border-radius:8px;"
        "font-size:16px;font-weight:bold;color:#005fb8;"
        "padding:20px 30px;}"
    );
    if (satStatus) satStatus->setGpsRunning(true);
}

void MainWindow::onStopGps() {
    userStoppedGps = true;
    gpsEmitted = false;  // 可重复发射
    if (m_gpsRestartTimer) m_gpsRestartTimer->stop();
    if (gpsProc) {
        gpsProc->kill();
        gpsProc->waitForFinished(3000);
        gpsProc->deleteLater();
        gpsProc = nullptr;
    }
    if (usrpThread) {
        usrpThread->stopUsrp();
        usrpThread->wait(3000);
    }
#ifdef Q_OS_WIN
    /* 还原主进程优先级为正常，释放 SetThreadExecutionState 防止系统睡眠 */
    SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
    SetThreadExecutionState(ES_CONTINUOUS);
#endif
    if (gpsStatus) gpsStatus->setText("● GPS 已停止");
    if (gpsStatus) gpsStatus->setStyleSheet(
        "QLabel{background:#ffffff;border:1px solid #d0d5dd;border-radius:8px;"
        "font-size:16px;font-weight:bold;color:#616171;padding:2px 6px}"
    );
    if (satStatus) satStatus->setGpsRunning(false);
    if (statusLight) statusLight->setStyleSheet("background:#ef4444;border:1px solid #dc2626;border-radius:9px;min-width:18px;min-height:18px;max-width:18px;max-height:18px");
    if (m_statusText) m_statusText->setText("待发射");
}

void MainWindow::sendUdpPos(double lat, double lon, double hgt) {
    if (!gpsProc || gpsProc->state() != QProcess::Running) return;
    QUdpSocket udp;
    double data[3] = {lat, lon, hgt};
    udp.writeDatagram((const char*)data, sizeof(data), QHostAddress::LocalHost, 5678);
}

void MainWindow::advancePathCpp() {
    m_pi++;
    if (m_pi >= m_pathLen) {
        if (m_cyc) {
            m_pi = 0;
        } else {
            m_pf = false;
            return;
        }
    }
    if (m_cyc && m_pi >= m_pathLen - 1) {
        m_pi = 0;
    }
    m_tx = m_ppx[m_pi]; m_ty = m_ppy[m_pi]; m_tz = m_ppz[m_pi];
    double dx = m_tx - m_cx, dy = m_ty - m_cy, dz = m_tz - m_cz;
    double dist = sqrt(dx*dx + dy*dy + dz*dz);
    if (dist > 0.001) {
        double inv = 1.0 / dist;
        m_vx = dx * inv; m_vy = dy * inv; m_vz = dz * inv;
    } else {
        m_vx = 0; m_vy = 0; m_vz = 0;
    }
}

void MainWindow::startPathCpp() {
    if (m_pathLen < 2) return;
    m_pi = 0;
    m_cx = m_ppx[0]; m_cy = m_ppy[0]; m_cz = m_ppz[0];
    if (m_pathLen > 1) {
        m_tx = m_ppx[1]; m_ty = m_ppy[1]; m_tz = m_ppz[1];
        double dx = m_tx - m_cx, dy = m_ty - m_cy, dz = m_tz - m_cz;
        double dist = sqrt(dx*dx + dy*dy + dz*dz);
        if (dist > 0.001) {
            double inv = 1.0 / dist;
            m_vx = dx * inv; m_vy = dy * inv; m_vz = dz * inv;
        }
    }
    m_pf = true;
    m_pp = false;
}

void MainWindow::refreshBrdc() {
#ifndef GPS_VERSION_GAODUAN
    brdcCombo->clear();
#endif
    QStringList filters = {"brdc*", "*.14n", "*.15n", "*.16n", "*.17n", "*.18n", "*.19n", "*.20n", "*.21n", "*.22n", "*.23n", "*.24n", "*.25n", "*.26n", "*.27n"};
    QStringList files;
    files << QDir(xingliDir).entryList(filters, QDir::Files, QDir::Name);
    files << QDir(appDir + "/手动星历").entryList(filters, QDir::Files, QDir::Name);
    files.removeDuplicates();
    files.sort();
#ifndef GPS_VERSION_GAODUAN
    if (files.isEmpty()) brdcCombo->addItem("无星历");
    else for (const auto &f : files) brdcCombo->addItem(f);
    if (brdcCombo->count() > 0 && brdcCombo->currentText() == "无星历")
        brdcCombo->setCurrentIndex(0);
#endif
}

static QByteArray decompressZ(const QByteArray &in) {
    if (in.size() < 4) return in;
    const unsigned char *p = (const unsigned char *)in.constData();
    if (p[0] != 0x1F || p[1] != 0x9D) return in;
    int maxbits = p[2] | (p[3] << 8);
    if (maxbits < 9 || maxbits > 16) return in;
    int pos = 4, bitpos = 0;
    auto readBits = [&](int n) -> int {
        int v = 0;
        for (int i = 0; i < n; i++) {
            v <<= 1;
            int byteIdx = pos + bitpos / 8;
            int bitIdx = 7 - (bitpos % 8);
            if (byteIdx >= in.size()) return -1;
            v |= (p[byteIdx] >> bitIdx) & 1;
            bitpos++;
        }
        return v;
    };
    int nbits = 9;
    int clear = 256, eoi = 257;
    int maxcode = (1 << nbits) - 1;
    QByteArray out;
    QMap<int, QByteArray> table;
    for (int i = 0; i < 256; i++) table[i] = QByteArray(1, (char)i);
    table[clear] = QByteArray();
    table[eoi] = QByteArray();
    int nextCode = 258;
    int oldCode = -1;
    while (true) {
        int code = readBits(nbits);
        if (code < 0) break;
        if (code == eoi) break;
        if (code == clear) {
            nbits = 9; maxcode = (1 << nbits) - 1;
            for (int i = 258; i < nextCode; i++) table.remove(i);
            nextCode = 258;
            oldCode = -1;
            continue;
        }
        QByteArray entry;
        if (table.contains(code)) {
            entry = table[code];
            if (oldCode >= 0 && nextCode <= maxcode) {
                QByteArray newEntry = table[oldCode] + QByteArray(1, entry[0]);
                table[nextCode++] = newEntry;
            }
        } else if (code == nextCode && oldCode >= 0) {
            entry = table[oldCode] + QByteArray(1, table[oldCode][0]);
            if (nextCode <= maxcode) table[nextCode++] = entry;
        } else break;
        out.append(entry);
        oldCode = code;
        if (nextCode > maxcode && nbits < maxbits) {
            nbits++;
            maxcode = (1 << nbits) - 1;
        }
    }
    return out;
}

void MainWindow::onDownloadBrdc() {
    QString dateStr = QDate::currentDate().toString("yyyyMMdd");
    QString fileName = QString("brdc%1.14n").arg(dateStr);

    auto saveFile = [this](const QByteArray &raw, const QString &source, const QString &fileName) -> bool {
        QByteArray data = decompressZ(raw);
        if (data.size() < 30000) {
            log(QString("%1 星历不完整 (%2 KB)").arg(source).arg(data.size() / 1024));
            return false;
        }
        QString filePath = xingliDir + "/" + fileName;
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            log("保存失败: " + file.errorString());
            return false;
        }
        file.write(data); file.close();
        log(QString("星历已保存(%1) %2 (%3 字节)").arg(source, fileName).arg(data.size()));
        QDir dir(xingliDir);
        QStringList allBrdc = dir.entryList({"brdc*", "*.14n", "*.15n", "*.16n", "*.17n", "*.18n", "*.19n", "*.20n", "*.21n", "*.22n", "*.23n", "*.24n", "*.25n", "*.26n", "*.27n"}, QDir::Files, QDir::Name);
        const int MAX_BRDC = 10;
        while (allBrdc.size() > MAX_BRDC) {
            QString oldest = allBrdc.takeFirst();
            if (oldest != fileName) QFile::remove(dir.filePath(oldest));
        }
        refreshBrdc();
        log("星历已保存，正在解析可见卫星...");
        QTimer::singleShot(500, this, &MainWindow::updatePRNsFromBrdc);
#ifndef GPS_VERSION_GAODUAN
        int idx = brdcCombo->findText(fileName);
        if (idx >= 0) brdcCombo->setCurrentIndex(idx);
#endif
        return true;
    };

    QDate today = QDate::currentDate();
    int doy = today.dayOfYear();
    QString doyStr = QString("%1").arg(doy, 3, 10, QChar('0'));
    QString yyStr = today.toString("yy");
    QString ucsdUrl = QString("ftp://lox.ucsd.edu/rinex/%1/%2/auto%3.%4n.Z")
        .arg(today.year()).arg(doyStr).arg(doyStr).arg(yyStr);

    auto tryPtr = std::make_shared<int>(0);
    QStringList fallbackUrls = {
        "http://api.starrtk.com/mini/Ysb/DownloadXingLi",
        ucsdUrl
    };
    QStringList fallbackLabels = {"starrtk", "UCSD-SIO"};

    auto tryNext = std::make_shared<std::function<void()>>();
    *tryNext = [this, fileName, saveFile, fallbackUrls, fallbackLabels, tryPtr, tryNext]() {
        if (*tryPtr >= fallbackUrls.size()) {
            log("所有星历源均下载失败, 请手动放入星历文件");
            return;
        }
        QString url = fallbackUrls[*tryPtr];
        QString label = fallbackLabels[*tryPtr];
        (*tryPtr)++;
        log(QString("正在从 %1 下载星历...").arg(label));
        QUrl qurl(url);
        QNetworkRequest req(qurl);
        req.setRawHeader("User-Agent", "Mozilla/5.0");
        QNetworkReply *r = netMgr->get(req);
        connect(r, &QNetworkReply::finished, this, [this, r, fileName, saveFile, label, tryPtr, tryNext]() {
            r->deleteLater();
            QByteArray data = r->readAll();
            if (r->error() != QNetworkReply::NoError) {
                log(QString("%1 下载失败, 尝试备用源...").arg(label));
                (*tryNext)();
                return;
            }
            saveFile(data, label, fileName);
        });
    };
    (*tryNext)();
}

void MainWindow::updatePRNsFromBrdc() {
    QString brdcFile = brdcCombo ? brdcCombo->currentText() : QString();
    if (brdcFile.isEmpty() || brdcFile == "无星历") return;
    QString filePath;
    if (QFile::exists(xingliDir + "/" + brdcFile)) {
        filePath = xingliDir + "/" + brdcFile;
    } else if (QFile::exists(appDir + "/手动星历/" + brdcFile)) {
        filePath = appDir + "/手动星历/" + brdcFile;
    } else {
        return;
    }
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    if (satStatus) {
        for (int i = 1; i <= 32; i++) satStatus->setSatellitePRN(i, false);
        while (!f.atEnd()) {
            QByteArray line = f.readLine().trimmed();
            if (line.isEmpty()) continue;
            bool ok = false;
            int prn = line.left(2).trimmed().toInt(&ok);
            if (ok && prn >= 1 && prn <= 32) {
                satStatus->setSatellitePRN(prn, true);
                for (int skip = 0; skip < 7 && !f.atEnd(); skip++) f.readLine();
            }
        }
        this->log(QString("星历卫星已更新"));
    }
    f.close();
}

/* ===== API Key Management ===== */
QString MainWindow::getApiKeysPath() {
    return QCoreApplication::applicationDirPath() + "/api_keys.json";
}
void MainWindow::loadApiKeys() {
    m_apiKeys.clear();
    m_apiKeyIndex = -1;
    m_apiKeysTianditu.clear();
    m_apiKeyIndexTianditu = -1;
    QFile f(getApiKeysPath());
    if (!f.open(QIODevice::ReadOnly)) {
        m_apiKeys << "3ecf56d2c71663821d44b9019ce4a9b7";
        m_apiKeysTianditu << "c8668a8aaf9e07cf8c21433a9d13ce0a";
        m_apiKeyIndex = 0;
        m_apiKeyIndexTianditu = 0;
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) {
        m_apiKeys = kBuiltinAmapKeys;
        m_apiKeyIndex = 0;
        m_apiKeysTianditu << "c8668a8aaf9e07cf8c21433a9d13ce0a";
        m_apiKeyIndexTianditu = 0;
        return;
    }
    QJsonObject obj = doc.object();
    QJsonArray arr = obj["keys"].toArray();
    for (auto v : arr) m_apiKeys << v.toString();
    m_apiKeyIndex = obj["idx"].toInt(-1);
    if (m_apiKeys.isEmpty()) { m_apiKeys = kBuiltinAmapKeys; m_apiKeyIndex = 0; }
    /* 迁移旧版 key */
    for (int i = 0; i < m_apiKeys.size(); i++) {
        if (m_apiKeys[i] == "R3qCldcRWt4CIncuv1W5myHaolSLBKFH" || m_apiKeys[i] == "iSt3RxwUToDWHfU66Kru3UxKYt1bgZU2") {
            m_apiKeys[i] = kBuiltinAmapKeys[0];
        }
    }
    QJsonArray tdArr = obj["keys_tianditu"].toArray();
    for (auto v : tdArr) m_apiKeysTianditu << v.toString();
    m_apiKeyIndexTianditu = obj["idx_tianditu"].toInt(-1);
    if (m_apiKeysTianditu.isEmpty()) { m_apiKeysTianditu << "c8668a8aaf9e07cf8c21433a9d13ce0a"; m_apiKeyIndexTianditu = 0; }
}
void MainWindow::saveApiKeys() {
    QJsonArray arr;
    for (auto &k : m_apiKeys) arr.append(k);
    QJsonArray tdArr;
    for (auto &k : m_apiKeysTianditu) tdArr.append(k);
    QJsonObject obj;
    obj["keys"] = arr; obj["idx"] = m_apiKeyIndex;
    obj["keys_tianditu"] = tdArr; obj["idx_tianditu"] = m_apiKeyIndexTianditu;
    QFile f(getApiKeysPath());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        f.close();
    }
}
QString MainWindow::getNextApiKey() {
    if (m_apiKeys.isEmpty()) { m_apiKeys << "3ecf56d2c71663821d44b9019ce4a9b7"; m_apiKeyIndex = 0; }
    if (m_apiKeyIndex < 0 || m_apiKeyIndex >= m_apiKeys.size()) m_apiKeyIndex = 0;
    QString key = m_apiKeys[m_apiKeyIndex];
    m_apiKeyIndex = (m_apiKeyIndex + 1) % m_apiKeys.size();
    return key;
}
void MainWindow::apiKeyAdd(const QString &key) {
    if (key.trimmed().isEmpty()) return;
    m_apiKeys.removeAll(key.trimmed());
    m_apiKeys.append(key.trimmed());
    saveApiKeys();
}
void MainWindow::apiKeyRemove(int index) {
    if (index < 0 || index >= m_apiKeys.size()) return;
    QString deleted = m_apiKeys[index];
    m_apiKeys.removeAt(index);
    if (m_apiKeyIndex >= m_apiKeys.size()) m_apiKeyIndex = m_apiKeys.size() - 1;
    if (m_apiKeyIndex < 0) { m_apiKeys << "3ecf56d2c71663821d44b9019ce4a9b7"; m_apiKeyIndex = 0; }
    saveApiKeys();
    /* If the deleted key is the current session key, switch to next available */
    if (deleted == m_sessionApiKey && !m_apiKeys.isEmpty()) {
        m_sessionApiKey = m_apiKeys[m_apiKeyIndex];
        /* Advance rotation so next startup doesn't repeat same key */
        m_apiKeyIndex = (m_apiKeyIndex + 1) % m_apiKeys.size();
        saveApiKeys();
    }
}
void MainWindow::showApiKeyDialog() {
    loadApiKeys();
    bool applied = false;
    QStringList *keyList = (m_mapSource == "tianditu") ? &m_apiKeysTianditu : &m_apiKeys;
    int *keyIdx = (m_mapSource == "tianditu") ? &m_apiKeyIndexTianditu : &m_apiKeyIndex;
    QString sourceName = (m_mapSource == "tianditu") ? "天地图" : "高德地图";
    QString registerUrl = (m_mapSource == "tianditu") ? "http://lbs.tianditu.gov.cn/home.html" : "https://lbs.amap.com/";
    QString registerHint = (m_mapSource == "tianditu")
        ? "1. 打开天地图开放平台 (lbs.tianditu.gov.cn)\n2. 注册/登录账号\n3. 创建应用 → 选择\"服务端\" → 获取密钥(tk)\n4. 将密钥粘贴到下方输入框"
        : "1. 打开高德地图开放平台 (lbs.amap.com)\n2. 注册/登录账号\n3. 创建应用 → 获取 Key\n4. 将 Key 粘贴到下方输入框";

    QDialog dlg(this);
    dlg.setWindowTitle("API密钥管理 - " + sourceName);
    dlg.resize(520, 480);
    auto *vl = new QVBoxLayout(&dlg);

    /* 注册引导区域 */
    auto *regLabel = new QLabel(registerHint);
    regLabel->setStyleSheet("QLabel{background:#f0f4f8;border:1px solid #d0d5dd;border-radius:6px;padding:8px 12px;font-size:11px;color:#333;line-height:1.6}");
    regLabel->setWordWrap(true);
    vl->addWidget(regLabel);

    /* 高德安全码输入（仅高德地图） */
    if (m_mapSource != "tianditu") {
        auto *scLabel = new QLabel("安全 JS 代码（选填，高德新版 Key 需要）:");
        scLabel->setStyleSheet("font-size:11px;color:#666;padding:4px 0 0 0");
        vl->addWidget(scLabel);
        auto *scEdit = new QLineEdit(m_amapSecurityCode);
        scEdit->setPlaceholderText("从高德控制台 -> 应用管理 -> 添加/查看 Key 获取");
        scEdit->setStyleSheet("QLineEdit{border:1px solid #d0d5dd;border-radius:4px;padding:6px 8px;font-size:12px}");
        vl->addWidget(scEdit);
        QObject::connect(scEdit, &QLineEdit::textChanged, [this](const QString &text) {
            m_amapSecurityCode = text.trimmed();
            QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);
            s.setValue("amapSecurityCode", m_amapSecurityCode);
        });
    }

    /* 注册按钮 */
    auto *btnOpenBrowser = new QPushButton("🌐 打开注册页面");
    btnOpenBrowser->setStyleSheet("QPushButton{background:#005fb8;color:#fff;border:none;border-radius:4px;padding:6px 16px;font-size:12px;font-weight:bold}QPushButton:hover{background:#004a8a}");
    connect(btnOpenBrowser, &QPushButton::clicked, [registerUrl]() {
        QDesktopServices::openUrl(QUrl(registerUrl));
    });
    vl->addWidget(btnOpenBrowser);

    /* === 密钥列表（放在输入框之前，供 doSaveKey 使用） === */
    auto *listLabel = new QLabel("已导入的密钥:");
    listLabel->setStyleSheet("font-size:11px;color:#666;padding:6px 0 2px 0");
    vl->addWidget(listLabel);
    auto *list = new QListWidget;
    for (auto &k : *keyList) list->addItem(k);
    vl->addWidget(list);

    /* === 内联密钥输入行 === */
    auto *keyInputRow = new QWidget;
    keyInputRow->setStyleSheet("background:transparent");
    QHBoxLayout *keyInputL = new QHBoxLayout(keyInputRow);
    keyInputL->setContentsMargins(0, 4, 0, 0);
    keyInputL->setSpacing(4);
    auto *keyInput = new QLineEdit;
    keyInput->setPlaceholderText("粘贴API密钥到此处，然后点击保存...");
    keyInput->setStyleSheet("QLineEdit{border:1px solid #d0d5dd;border-radius:4px;padding:6px 8px;font-size:12px}");
    keyInputL->addWidget(keyInput, 1);
    auto *btnSaveKey = new QPushButton("保存密钥");
    btnSaveKey->setFixedHeight(32);
    btnSaveKey->setStyleSheet("QPushButton{background:#22c55e;color:#fff;border:none;border-radius:4px;padding:4px 14px;font-size:12px;font-weight:bold}QPushButton:hover{background:#16a34a}QPushButton:disabled{background:#d0d5dd;color:#999}");
    btnSaveKey->setEnabled(false);
    keyInputL->addWidget(btnSaveKey);
    vl->addWidget(keyInputRow);

    /* 输入框内容变化时启用/禁用保存按钮 */
    connect(keyInput, &QLineEdit::textChanged, [btnSaveKey, keyInput]() {
        btnSaveKey->setEnabled(!keyInput->text().trimmed().isEmpty());
    });

    /* 保存密钥：添加到列表、持久化、刷新列表、清空输入框、标记已应用 */
    auto doSaveKey = [this, list, keyList, keyIdx, keyInput, btnSaveKey, &applied]() {
        QString key = keyInput->text().trimmed();
        if (key.isEmpty()) return;
        keyList->removeAll(key);
        keyList->append(key);
        *keyIdx = keyList->size() - 1;
        m_sessionApiKey = key;
        saveApiKeys();
        list->clear(); for (auto &k : *keyList) list->addItem(k);
        keyInput->clear();
        btnSaveKey->setEnabled(false);
        applied = true;
    };
    connect(btnSaveKey, &QPushButton::clicked, this, [doSaveKey]() { doSaveKey(); });
    connect(keyInput, &QLineEdit::returnPressed, this, [doSaveKey]() { doSaveKey(); });

    /* 底部按钮行 */
    auto *hl = new QHBoxLayout;
    auto *btnDel = new QPushButton("删除选中");
    btnDel->setStyleSheet("QPushButton{background:#ef4444;color:#fff;border:none;border-radius:4px;padding:4px 12px;font-size:11px}QPushButton:hover{background:#dc2626}");
    auto *btnApply = new QPushButton("✅ 应用");
    btnApply->setStyleSheet("QPushButton{background:#005fb8;color:#fff;border:none;border-radius:4px;padding:4px 16px;font-size:12px;font-weight:bold}QPushButton:hover{background:#004a8a}");
    auto *btnCancel = new QPushButton("取消");
    btnCancel->setStyleSheet("QPushButton{background:#ffffff;border:1px solid #d0d5dd;border-radius:4px;padding:4px 12px;font-size:11px;color:#616161}QPushButton:hover{background:#f0f4f8}");
    hl->addWidget(btnDel);
    hl->addStretch();
    hl->addWidget(btnApply);
    hl->addWidget(btnCancel);
    vl->addLayout(hl);

    connect(btnDel, &QPushButton::clicked, [this, list, keyList, keyIdx, &applied]() {
        int row = list->currentRow(); if (row < 0) return;
        if (row >= keyList->size()) return;
        QString deleted = (*keyList)[row];
        bool wasActive = (deleted == m_sessionApiKey);
        keyList->removeAt(row);
        if (*keyIdx >= keyList->size()) *keyIdx = keyList->size() - 1;
        if (*keyIdx < 0) {
            if (m_mapSource == "tianditu") { keyList->append("c8668a8aaf9e07cf8c21433a9d13ce0a"); *keyIdx = 0; }
            else { keyList->append("3ecf56d2c71663821d44b9019ce4a9b7"); *keyIdx = 0; }
        }
        if (wasActive) {
            if (m_mapSource == "tianditu") m_sessionApiKey = m_apiKeysTianditu.value(*keyIdx, "c8668a8aaf9e07cf8c21433a9d13ce0a");
            else m_sessionApiKey = m_apiKeys.value(*keyIdx, "3ecf56d2c71663821d44b9019ce4a9b7");
        }
        saveApiKeys();
        list->clear(); for (auto &k : *keyList) list->addItem(k);
        if (wasActive) applied = true;
    });

    connect(btnApply, &QPushButton::clicked, [this, &dlg, &applied]() {
        dlg.accept();
        if (applied && webView && webView->page()) {
            m_mapReady = false;
            webView->page()->runJavaScript("qtReloadMap()");
        }
    });

    connect(btnCancel, &QPushButton::clicked, [&dlg]() {
        dlg.reject();
    });

    dlg.exec();
}
