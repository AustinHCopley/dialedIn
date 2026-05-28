#pragma once

#include <QMainWindow>
#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QLocalSocket>
#include <QLocalServer>
#include <QSocketNotifier>
#include <memory>
#include <vector>
#include <string>
#include <cmath>
#include "dial_common.h"
#include "dial_config.h"

class DialWheel : public QWidget {
    Q_OBJECT

public:
    enum Mode {
        SELECTION_MODE,  // GUI visible, selecting which mode to use
        ACTIVE_MODE      // GUI hidden, dial controls the selected function
    };

    explicit DialWheel(QWidget *parent = nullptr);
    void toggleVisibility();
    bool isVisible() const { return m_is_visible; }
    void rotate(const QString& direction);
    void executeCurrent();
    void setCurrentOption(int index);
    Mode getCurrentMode() const { return current_mode; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    DialConfig config;         // loaded style + modes (config.modes is the mode list)
    int current_index;
    int active_mode_index;     // Which mode is currently active (when GUI hidden)
    double rotation;
    bool m_is_visible;
    Mode current_mode;
    QString last_rotation_direction;  // Track last rotation: "cw" or "ccw"

    int calculateSelectedIndex() const;  // Calculate which option is in selection area
    void writeStatus();  // Write current mode JSON to the configured status file
};

class DialGUI : public QMainWindow {
    Q_OBJECT

public:
    DialGUI();
    ~DialGUI();
    DialWheel* getWheel();

private:
    std::unique_ptr<DialWheel> wheel;
    std::unique_ptr<QLocalServer> server;
    std::unique_ptr<QLocalSocket> socket;
    bool setupSocketServer();
    void handleMessage(const DialMessage& msg);
    void sendMessage(const DialMessage& msg);

private slots:
    void handleSocketConnection();
    void handleSocketData();
    void handleSocketError(QLocalSocket::LocalSocketError error);
}; 