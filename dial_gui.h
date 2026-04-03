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

class DialOption {
public:
    DialOption(const std::string& name, const std::string& icon,
               const QString& cw_cmd, const QString& ccw_cmd)
        : name(name), icon(icon), cw_cmd(cw_cmd), ccw_cmd(ccw_cmd) {}
    std::string name;
    std::string icon;
    QString cw_cmd;
    QString ccw_cmd;
};

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
    std::vector<DialOption> options;
    int current_index;
    int active_mode_index;     // Which mode is currently active (when GUI hidden)
    double rotation;
    bool m_is_visible;
    Mode current_mode;
    QString last_rotation_direction;  // Track last rotation: "cw" or "ccw"
    static const int WHEEL_RADIUS = 100;
    static const int OPTION_RADIUS = 30;
    static const int FADE_DURATION = 1000;
    static constexpr int STEPS_PER_OPTION = 4;  // Number of rotations to move to next option

    int calculateSelectedIndex() const;  // Calculate which option is in selection area
    void updateWaybarStatus();  // Write current mode to /tmp/dial_status for waybar
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