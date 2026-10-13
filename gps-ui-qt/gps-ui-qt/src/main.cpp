#include <QApplication>
#include <stdio.h>
#include <string.h>
#ifdef Q_OS_WIN
#include <winsock2.h>
#include <windows.h>
#endif
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QTimer>
#include <QCryptographicHash>
#include "rsa_verify.h"
#include <QProcess>
#include <QThread>
#include <QSysInfo>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QWebEngineProfile>
#include <QOpenGLContext>
#include "mainwindow.h"

static int g_gpsRunning = 0;
void setGpsRunning(int v) { g_gpsRunning = v; }
int getGpsRunning() { return g_gpsRunning; }

QString g_mid, g_mac, g_cpu;
QString g_deviceType = "pluto";
void setDeviceType(const QString &v) { g_deviceType = v; }
QString getDeviceType() { return g_deviceType; }

static bool g_cpuRender = false;
void setCpuRender(bool v) { g_cpuRender = v; }
bool isCpuRender() { return g_cpuRender; }

static int g_authPermanent = 0;
static double g_authRemaining = 0;
static QString g_storedSig;
void setAuthPermanent(int v) { g_authPermanent = v; }
int getAuthPermanent() { return g_authPermanent; }
void setAuthRemaining(double v) { g_authRemaining = v; }
double getAuthRemaining() { return g_authRemaining; }
void setStoredSig(const QString &v) { g_storedSig = v; }
QString getStoredSig() { return g_storedSig; }

static QString getDataDir() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/PosNavTech";
}

static QString getMachineId() {
    QString parts;
#ifdef Q_OS_WIN
    DWORD ser = 0;
    if (GetVolumeInformationW(L"C:\\", NULL, 0, &ser, NULL, NULL, NULL, 0))
        parts += QString::number(ser, 16);
    wchar_t buf[256] = {0};
    DWORD sz = 256;
    if (GetComputerNameW(buf, &sz))
        parts += QString::fromWCharArray(buf);
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t guid[64] = {0};
        DWORD guidSz = sizeof(guid);
        if (RegQueryValueExW(hKey, L"MachineGuid", NULL, NULL, (LPBYTE)guid, &guidSz) == ERROR_SUCCESS)
            parts += QString::fromWCharArray(guid);
        RegCloseKey(hKey);
    }
#endif
    QByteArray hash = QCryptographicHash::hash(parts.toUtf8(), QCryptographicHash::Sha256);
    return hash.toHex().left(32);
}

struct LicResult {
    bool valid = false;
    QString type;
    qint64 timestamp = 0;
    QString sig;
    QString error;
};

static QString formatMachineId(const QString &raw) {
    QString fmt;
    for (int i = 0; i < raw.size(); i++) {
        if (i > 0 && i % 4 == 0) fmt += '-';
        fmt += raw[i];
    }
    return fmt;
}

static QStringList loadUsedSigs(const QString &dataDir) {
    QStringList list;
    QFile f(dataDir + "/used_sigs.txt");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!f.atEnd()) {
            QString line = QString::fromUtf8(f.readLine()).trimmed();
            if (!line.isEmpty()) list << line;
        }
        f.close();
    }
    return list;
}

static void addUsedSig(const QString &dataDir, const QString &sig) {
    QFile f(dataDir + "/used_sigs.txt");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        f.write(sig.toUtf8() + "\n");
        f.close();
    }
}

static LicResult validateLicense(const QString &filePath) {
    LicResult r;
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) { r.error = "无法打开授权文件"; return r; }
    QStringList lines = QString::fromUtf8(f.readAll()).split('\n');
    f.close();
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty()) lines.removeLast();

    if (lines.size() < 4) {
        r.error = "授权文件格式无效";
        return r;
    }

    QString licMachine = lines[0].trimmed();
    QString licType = lines[1].trimmed();
    QString licTime = lines[2].trimmed();
    QString licSigHex = lines[3].trimmed();

    if (licType != "24" && licType != "48" && licType != "perm") {
        r.error = "授权类型无效: " + licType;
        return r;
    }

    if (licMachine != g_mid) {
        r.error = QString("授权文件绑定机器码不匹配\n本机: %1\n授权: %2")
            .arg(formatMachineId(g_mid), formatMachineId(licMachine));
        return r;
    }

    // RSA-2048 PKCS#1 v1.5 SHA-256 verification
    QByteArray payload = (licMachine + "|" + licType + "|" + licTime).toUtf8();
    QByteArray sig = QByteArray::fromHex(licSigHex.toLatin1());
    if (sig.size() != 256) {
        r.error = "签名长度无效";
        return r;
    }

    int ok = rsa_verify(NULL, 0, (const uint8_t*)sig.constData(),
                        (const uint8_t*)payload.constData(), payload.size());
    if (!ok) {
        r.error = "签名验证失败 - 授权文件无效";
        return r;
    }

    r.valid = true;
    r.type = (licType == "24") ? "24h" : (licType == "48") ? "48h" : "perm";
    r.timestamp = licTime.toLongLong();
    r.sig = licSigHex;
    return r;
}

int main(int argc, char *argv[]) {
    bool safeMode = false;
    bool forceGpu = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i] && strstr(argv[i], "--safe")) safeMode = true;
        if (argv[i] && strstr(argv[i], "--gpu")) forceGpu = true;
    }
    wchar_t selfPathW[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, selfPathW, MAX_PATH);
    QString selfPath = QString::fromWCharArray(selfPathW);
    QFileInfo selfInfo(selfPath);
    QString crashMarker = selfInfo.absolutePath() + "/.crash_marker";
    QApplication app(argc, argv);
    app.setApplicationName("行迹自录");
    if (!safeMode && QFile::exists(crashMarker)) {
        QSettings s(getDataDir() + "/settings.ini", QSettings::IniFormat);
        if (s.value("renderMode").toString() == "gpu") {
            s.setValue("renderMode", "cpu");
            s.sync();
        }
        QProcess::startDetached(selfPath, QStringList() << "--safe");
        return 0;
    }
    bool disableGpu;
    if (safeMode) disableGpu = true;
    else if (forceGpu) disableGpu = false;
    else {
        QSettings s(getDataDir() + "/settings.ini", QSettings::IniFormat);
        disableGpu = (s.value("renderMode", "cpu").toString() != "gpu");
    }
    if (disableGpu)
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-background-timer-throttling --disable-gpu --disable-gpu-compositing");
    else
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-background-timer-throttling");
    setCpuRender(disableGpu);
    app.setApplicationVersion("1.0.0");
    // Enable persistent disk cache for WebEngine (map tiles, etc.)
    QWebEngineProfile::defaultProfile()->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    QWebEngineProfile::defaultProfile()->setHttpCacheMaximumSize(500 * 1024 * 1024);
    QWebEngineProfile::defaultProfile()->setPersistentStoragePath(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/PosNavTech/webcache");
    QString mid = getMachineId();
    if (mid.isEmpty()) {
        QMessageBox::critical(nullptr, "错误", "无法获取设备指纹，即将退出");
        QTimer::singleShot(1000, &app, &QApplication::quit);
        app.exec();
        return 0;
    }
    g_mid = mid; g_mac = mid.left(16); g_cpu = mid.mid(16, 16);

    // Anti-debug: detect debugger at startup
#ifdef Q_OS_WIN
    if (IsDebuggerPresent()) {
        QMessageBox::critical(nullptr, "错误", "检测到调试器，程序无法运行");
        return 0;
    }
#endif

    QString dataDir = getDataDir();
    QDir().mkpath(dataDir);
    QString licPath = dataDir + "/license.lic";
    QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);

    if (!s.value("agreedTos", false).toBool()) {
        QMessageBox msgBox;
        msgBox.setWindowTitle(QStringLiteral("用户协议"));
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setText(QStringLiteral("请仔细阅读以下用户协议："));
        msgBox.setStyleSheet(
            "QMessageBox{background:#ffffff;color:#1a1a1a;font-size:13px}"
            "QLabel{color:#1a1a1a;font-size:13px;background:transparent}"
            "QPushButton{background:#ffffff;border:1px solid #d0d5dd;border-radius:4px;padding:6px 20px;color:#1a1a1a;font-size:13px;min-width:80px}"
            "QPushButton:hover{background:#f0f4f8}"
            "QPushButton#qt_msgbox_button1{background:#005fb8;border-color:#005fb8;color:#ffffff}"
            "QPushButton#qt_msgbox_button1:hover{background:#004c8c}");
        QString agreementText;
        agreementText += QStringLiteral("欢迎使用行迹自录。\n\n");
        agreementText += QStringLiteral("一、使用范围\n");
        agreementText += QStringLiteral("本产品为GPS实时信号发射/模拟器，仅限技术开发与测试用途。\n");
        agreementText += QStringLiteral("禁止用于网约车、代驾、外卖配送、社交软件位置签到、\n");
        agreementText += QStringLiteral("微信或其他平台的位置篡改等行为。\n\n");
        agreementText += QStringLiteral("二、责任声明\n");
        agreementText += QStringLiteral("您在使用过程中的一切操作均由您本人完成，操作结果亦由\n");
        agreementText += QStringLiteral("您自行承担。因您不当使用或违规使用引发的一切法律责任\n");
        agreementText += QStringLiteral("均由您自行承担，我们概不负责。\n\n");
        agreementText += QStringLiteral("三、其他\n");
        agreementText += QStringLiteral("点击[我已阅读并同意]即表示您接受上述条款。\n");
        agreementText += QStringLiteral("如不同意请点击[拒绝退出]退出程序。");
        msgBox.setInformativeText(agreementText);
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.button(QMessageBox::Yes)->setText(QStringLiteral("我已阅读并同意"));
        msgBox.button(QMessageBox::No)->setText(QStringLiteral("拒绝退出"));
        msgBox.setDefaultButton(QMessageBox::No);
        if (msgBox.exec() != QMessageBox::Yes) {
            QMessageBox::information(nullptr, QStringLiteral("提示"), QStringLiteral("您已拒绝用户协议，程序即将退出。"));
            return 0;
        }
        s.setValue("agreedTos", true);
        s.sync();
    }

    // Auth dialog
    QDialog authDlg;
    authDlg.setWindowTitle("设备授权");
    authDlg.setFixedSize(500, 340);
    authDlg.setStyleSheet("background:#ffffff;color:#1a1a1a;font-size:13px");
    auto *vl = new QVBoxLayout(&authDlg);
    vl->setContentsMargins(20, 16, 20, 16);
    vl->setSpacing(10);

    // Machine code display
    auto *midLabel = new QLabel("本机机器码（请复制后发给管理员生成授权文件）:");
    midLabel->setStyleSheet("color:#616161;font-size:12px;padding:0;background:transparent");
    vl->addWidget(midLabel);

    auto *midEdit = new QLineEdit;
    midEdit->setReadOnly(true);
    midEdit->setText(formatMachineId(g_mid));
    midEdit->setStyleSheet(
        "QLineEdit{background:#f5f5f5;color:#005fb8;border:1px solid #d0d5dd;"
        "border-radius:4px;padding:10px 12px;font-size:16px;font-weight:700;"
        "font-family:'Consolas','Courier New',monospace;"
        "selection-background-color:#005fb8;selection-color:#fff}"
    );
    midEdit->setCursorPosition(0);
    midEdit->selectAll();
    vl->addWidget(midEdit);

    // Status label
    auto *lbl = new QLabel("正在验证设备授权...");
    lbl->setWordWrap(true);
    lbl->setStyleSheet("color:#1a1a1a;padding:4px 0;background:transparent");
    vl->addWidget(lbl);

    // Bottom buttons
    auto *btnImport = new QPushButton("导入授权文件");
    btnImport->setStyleSheet("QPushButton{background:#0f7b0f;color:#fff;border:none;border-radius:4px;padding:8px 20px;font-size:13px}"
        "QPushButton:hover{background:#107c10}");
    auto *btnQuit = new QPushButton("退出");
    btnQuit->setStyleSheet("QPushButton{background:#c42b1c;color:#fff;border:none;border-radius:4px;padding:8px 20px;font-size:13px}"
        "QPushButton:hover{background:#a42617}");
    auto *hl = new QHBoxLayout;
    hl->addStretch(); hl->addWidget(btnImport); hl->addWidget(btnQuit);
    vl->addLayout(hl);

    // Auto-check: look for existing activation in settings.ini FIRST
    QString storedType = s.value("licType").toString();
    qint64 activatedAt = s.value("licActivatedAt", 0).toLongLong();
    QString storedSig = s.value("licSigHash").toString();

    // Machine code check: if settings.ini has a machine record, verify it matches
    QString storedMachine = s.value("licMachine").toString();
    if (!storedMachine.isEmpty() && storedMachine != g_mid) {
        // Copied to a different machine → invalidate activation
        storedType.clear();
        activatedAt = 0;
        storedSig.clear();
    }
    if (!storedType.isEmpty()) {
        QString licTime = s.value("licTime").toString();
        QByteArray sig = QByteArray::fromHex(storedSig.toLatin1());
        QByteArray payload = (g_mid + "|" + storedType + "|" + licTime).toUtf8();
        bool sigOk = !licTime.isEmpty() && sig.size() == 256 &&
            rsa_verify(NULL, 0, (const uint8_t*)sig.constData(),
                       (const uint8_t*)payload.constData(), payload.size());
        if (!sigOk) {
            storedType.clear();
            activatedAt = 0;
            storedSig.clear();
        }
    }

    if (!storedType.isEmpty() && activatedAt > 0) {
        // Already activated
        midEdit->setVisible(false);
        midLabel->setVisible(false);
        if (storedType == "perm") {
            setAuthPermanent(1);
            lbl->setText("永久授权有效，正在启动...");
            QTimer::singleShot(500, &authDlg, &QDialog::accept);
        } else {
            qint64 hours = (storedType == "24h") ? 24 : 48;
            qint64 elapsed = (QDateTime::currentSecsSinceEpoch() - activatedAt) / 3600;
            if (elapsed >= hours) {
                lbl->setText(QString("授权已过期 (%1小时)\n\n请将本机机器码发给管理员\n生成授权文件后点击下方「导入授权文件」").arg(hours));
                midEdit->setVisible(true);
                midLabel->setVisible(true);
            } else {
                double remain = hours - elapsed;
                setAuthRemaining(remain);
                lbl->setText(QString("授权有效，剩余 %1 小时\n正在启动...").arg(remain, 0, 'f', 1));
                QTimer::singleShot(500, &authDlg, &QDialog::accept);
            }
        }
    } else if (QFileInfo::exists(licPath)) {
        // No stored activation but .lic exists → first-time import
        midEdit->setVisible(false);
        midLabel->setVisible(false);
        LicResult lic = validateLicense(licPath);
        if (lic.valid && loadUsedSigs(dataDir).contains(lic.sig)) {
            lic.valid = false;
            lic.error = "此授权文件已被使用过，不可重复激活";
        }
        if (lic.valid) {
            s.setValue("licType", lic.type);
            s.setValue("licActivatedAt", QDateTime::currentSecsSinceEpoch());
            s.setValue("licSigHash", lic.sig);
            s.setValue("licMachine", g_mid);
            s.setValue("licTime", QString::number(lic.timestamp));
            s.sync();
            addUsedSig(dataDir, lic.sig);
            QFile::remove(licPath);
            if (lic.type == "perm") {
                setAuthPermanent(1);
                lbl->setText("永久授权有效，正在启动...");
            } else {
                double hours = (lic.type == "24h") ? 24 : 48;
                setAuthRemaining(hours);
                lbl->setText("授权已激活，正在启动...");
            }
            QTimer::singleShot(500, &authDlg, &QDialog::accept);
        } else {
            lbl->setText("现有授权文件无效: " + lic.error + "\n\n请将本机机器码发给管理员\n生成授权文件后点击下方「导入授权文件」");
            midEdit->setVisible(true);
            midLabel->setVisible(true);
        }
    } else {
        lbl->setText("请将上方机器码发给管理员\n生成授权文件后点击「导入授权文件」完成激活");
    }

    QObject::connect(btnImport, &QPushButton::clicked, &authDlg, [&]() {
        QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        QString path = QFileDialog::getOpenFileName(&authDlg, "选择授权文件", desktop + "/", "授权文件 (*.lic)");
        if (path.isEmpty()) return;
        LicResult nr = validateLicense(path);
        if (!nr.valid) {
            lbl->setText("授权验证失败: " + nr.error + "\n\n请选择正确的授权文件");
            QMessageBox::warning(&authDlg, "授权失败", nr.error);
            return;
        }
        // Check if this exact license sig was already activated (prevents reuse)
        if (loadUsedSigs(dataDir).contains(nr.sig) || (s.contains("licSigHash") && s.value("licSigHash").toString() == nr.sig)) {
            lbl->setText("此授权文件已被使用过，不可重复激活");
            QMessageBox::warning(&authDlg, "授权失败", "此授权文件已被使用过，不可重复激活");
            return;
        }
        // Activate: store to settings.ini (use current time as activation start)
        s.setValue("licType", nr.type);
        s.setValue("licActivatedAt", QDateTime::currentSecsSinceEpoch());
        s.setValue("licSigHash", nr.sig);
        s.setValue("licMachine", g_mid);
        s.setValue("licTime", QString::number(nr.timestamp));
        s.sync();
        addUsedSig(dataDir, nr.sig);
        QFile::remove(licPath);
        if (nr.type == "perm") {
            setAuthPermanent(1);
            lbl->setText("永久授权有效，正在启动...");
        } else {
            double hours = (nr.type == "24h") ? 24 : 48;
            setAuthRemaining(hours);
            lbl->setText("授权已激活，正在启动...");
        }
        QTimer::singleShot(300, &authDlg, &QDialog::accept);
    });
    QObject::connect(btnQuit, &QPushButton::clicked, &app, &QApplication::quit);

    authDlg.exec();
    if (authDlg.result() != QDialog::Accepted) return 0;
    if (!safeMode) {
        QFile mf(crashMarker);
        if (mf.open(QIODevice::WriteOnly)) { mf.write("1"); mf.close(); }
    }
    // Create log file for debugging
    QFile::remove(dataDir + "/debug.log");
    QFile dbgLog(dataDir + "/debug.log");
    dbgLog.open(QIODevice::Append | QIODevice::Text);
    QTextStream d(&dbgLog);
    d << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << " Starting MainWindow...\n";
    dbgLog.close();

    // Portable directory for crash logging
    QString portDir = QCoreApplication::applicationDirPath();
    QFile::remove(portDir + "/crash.log");
    auto crashLog = [&](const QString &msg) {
        QFile f(portDir + "/crash.log");
        f.open(QIODevice::Append | QIODevice::Text);
        QTextStream s(&f);
        s << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << " " << msg << "\n";
        f.close();
    };

    crashLog("Creating MainWindow...");
    MainWindow *w = new MainWindow();
    crashLog("MainWindow created, showing...");
    w->move((QApplication::primaryScreen()->size().width() - w->width()) / 2,
            (QApplication::primaryScreen()->size().height() - w->height()) / 2);
    w->show();
    w->raise();
    w->activateWindow();
    int ret = app.exec();
    QFile::remove(crashMarker);
    delete w;
    return ret;
}

// Called from mainwindow.cpp to re-activate with a new license file
QString reActivateLicense(const QString &filePath) {
    QString dataDir = getDataDir();
    QString licPath = dataDir + "/license.lic";
    QSettings s(dataDir + "/settings.ini", QSettings::IniFormat);

    LicResult nr = validateLicense(filePath);
    if (!nr.valid)
        return "验证失败: " + nr.error;

    if (loadUsedSigs(dataDir).contains(nr.sig) || (s.contains("licSigHash") && s.value("licSigHash").toString() == nr.sig))
        return "该授权文件已被使用过，不可重复激活";

    s.setValue("licType", nr.type);
    s.setValue("licActivatedAt", QDateTime::currentSecsSinceEpoch());
    s.setValue("licSigHash", nr.sig);
    s.setValue("licMachine", g_mid);
    s.setValue("licTime", QString::number(nr.timestamp));
    s.sync();
    addUsedSig(dataDir, nr.sig);
    QFile::remove(licPath);

    if (nr.type == "perm") {
        setAuthPermanent(1);
        return "永久授权已激活";
    } else {
        double hours = (nr.type == "24h") ? 24 : 48;
        setAuthRemaining(hours);
        return QString("授权已激活，剩余 %1 小时").arg(hours, 0, 'f', 0);
    }
}
