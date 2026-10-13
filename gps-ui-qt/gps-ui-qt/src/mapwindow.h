#ifndef MAPWINDOW_H
#define MAPWINDOW_H

#include <QMainWindow>
#include <QWebEngineView>
#include <QLabel>
#include <QTimer>

class MapWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MapWindow(QWidget *parent = nullptr);

private slots:
    void onMapUrl();
    void onLoadFinished(bool ok);

private:
    QWebEngineView *webView;
    QLabel *statusLabel;
    int retryCount = 0;
};

#endif
