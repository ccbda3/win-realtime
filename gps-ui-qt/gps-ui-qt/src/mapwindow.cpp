#include "mapwindow.h"
#include <QVBoxLayout>
#include <QUrl>
#include <QTimer>

MapWindow::MapWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("GPS 地图 - 百度地图");
    resize(800, 650);

    QWidget *central = new QWidget;
    setCentralWidget(central);
    QVBoxLayout *mainL = new QVBoxLayout(central);
    mainL->setContentsMargins(0, 0, 0, 0);
    mainL->setSpacing(0);

    // WebEngine map
    webView = new QWebEngineView;
    webView->setUrl(QUrl("about:blank"));
    mainL->addWidget(webView, 1);
    connect(webView, &QWebEngineView::loadFinished, this, &MapWindow::onLoadFinished);

    statusLabel = new QLabel("加载地图...");
    statusLabel->setStyleSheet("font-size:10px;color:#8b949e;padding:2px 6px;background:#0d1117");
    statusLabel->setFixedHeight(18);
    mainL->addWidget(statusLabel);

    // Load after delay
    QTimer::singleShot(1500, this, [this]() {
        webView->setUrl(QUrl("http://127.0.0.1:8080/"));
    });
}

void MapWindow::onMapUrl() {
    webView->setUrl(QUrl("http://127.0.0.1:8080/"));
}

void MapWindow::onLoadFinished(bool ok) {
    if (ok) {
        statusLabel->setText("地图已加载");
        retryCount = 0;
    } else if (retryCount < 5) {
        retryCount++;
        statusLabel->setText(QString("加载失败，重试 %1/5...").arg(retryCount));
        QTimer::singleShot(2000, this, [this]() {
            webView->setUrl(QUrl("http://127.0.0.1:8080/"));
        });
    } else {
        statusLabel->setText("地图加载失败");
    }
}
