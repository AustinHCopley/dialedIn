#include "dial_gui.h"
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QTimer>
#include <QProcess>
#include <QScreen>
#include <QFont>
#include <QColor>
#include <QPen>
#include <QBrush>
#include <QDataStream>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QJsonObject>
#include <QJsonDocument>
#include <vector>
#include <string>
#include <cmath>

DialWheel::DialWheel(QWidget* parent)
    : QWidget(parent),
      config(loadConfig()),
      current_index(0),
      active_mode_index(0),
      rotation(0),
      m_is_visible(false),
      current_mode(SELECTION_MODE),
      last_rotation_direction("cw") {
    // Window flags — BypassWindowManagerHint breaks Wayland surface damage tracking
    // (causes only top-left quadrant to repaint). Use Hyprland window rules for
    // decoration removal instead (see hyprland_rules.conf).
    setWindowFlags(Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint |
                   Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    // Modes and styling come from the config (see loadConfig()).
    setFixedSize(config.style.size, config.style.size);

    // Center on screen
    if (QScreen* screen = QApplication::primaryScreen()) {
        QRect screenGeometry = screen->geometry();
        move((screenGeometry.width() - width()) / 2,
             (screenGeometry.height() - height()) / 2);
    }
}

void DialWheel::setCurrentOption(int index) {
    if (index >= 0 && index < static_cast<int>(config.modes.size())) {
        current_index = index;
        update();
    }
}

void DialWheel::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Clear entire surface to transparent — without this, stale pixels persist
    // in regions the compositor doesn't mark as damaged
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    const WheelStyle& style = config.style;

    QPointF center(width() / 2.0, height() / 2.0);
    double radius = std::min(width(), height()) / 2.0 - 30;

    // Draw background circle
    painter.setPen(QPen(style.background_border, 2));
    painter.setBrush(QBrush(style.background));
    painter.drawEllipse(10, 10, width() - 20, height() - 20);

    // Draw fixed selection area at the top
    // Qt angles: 0° = 3 o'clock, 90° = 12 o'clock (top), 180° = 9 o'clock, 270° = 6 o'clock
    const double selectionAngle = 90.0; // Top position (12 o'clock in Qt coordinates)
    const double selectionArc = 360.0 / config.modes.size() * 0.9; // Cover most of option width

    // Draw selection indicator (wedge at top) - extend to edge of background circle
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(style.selection));

    QPainterPath selectionPath;
    selectionPath.moveTo(center);
    // Use full radius to extend to edge
    double arcRadius = std::min(width(), height()) / 2.0 - 12; // Almost to edge
    selectionPath.arcTo(QRectF(center.x() - arcRadius, center.y() - arcRadius,
                                arcRadius * 2, arcRadius * 2),
                        selectionAngle - selectionArc / 2,
                        selectionArc);
    selectionPath.lineTo(center);
    painter.drawPath(selectionPath);

    // Draw rotating options
    for (size_t i = 0; i < config.modes.size(); ++i) {
        // Qt coordinate system: 0° = 3 o'clock, goes counter-clockwise
        // Start at 90° (top) and distribute config.modes evenly
        double angle = 90.0 - (i * 360.0 / config.modes.size() + rotation);
        double rad_angle = angle * M_PI / 180.0;

        // Calculate position using standard circular positioning
        double x = center.x() + radius * 0.7 * std::cos(rad_angle);
        double y = center.y() - radius * 0.7 * std::sin(rad_angle); // Negative because Qt Y goes down

        // Highlight the selected option (the one in the selection area)
        if (static_cast<int>(i) == current_index) {
            painter.setPen(QPen(style.selected_border, 3));
            painter.setBrush(QBrush(style.accent));
        } else {
            painter.setPen(QPen(style.option_border, 1));
            painter.setBrush(QBrush(style.option));
        }

        const double r = style.option_radius;
        QRectF option_rect(x - r, y - r, r * 2, r * 2);
        painter.drawEllipse(option_rect);

        // Draw icon and text
        painter.setPen(style.text);
        painter.setFont(QFont(style.font_family, style.icon_point_size));
        painter.drawText(QRectF(x - r, y - r, r * 2, r + 5), Qt::AlignCenter,
                        config.modes[i].icon);

        painter.setFont(QFont(style.font_family, style.label_point_size));
        painter.drawText(QRectF(x - r, y, r * 2, r - 5), Qt::AlignCenter,
                        config.modes[i].name);
    }
}

void DialWheel::rotate(const QString& direction) {
    last_rotation_direction = direction;

    if (current_mode == SELECTION_MODE) {
        // In selection mode: rotate the wheel, selection updates based on position
        const double degreesPerOption = 360.0 / config.modes.size();
        const double degreesPerStep = degreesPerOption / config.style.steps_per_option;

        if (direction == "cw") {
            rotation = std::fmod(rotation + degreesPerStep, 360);
        } else {
            rotation = std::fmod(rotation - degreesPerStep + 360, 360);
        }

        // Update current_index based on which option is in the selection area
        current_index = calculateSelectedIndex();
        update();
    } else {
        // In active mode: execute the command for the active mode
        const QString& cmd = (direction == "cw")
                            ? config.modes[active_mode_index].cw_cmd
                            : config.modes[active_mode_index].ccw_cmd;
        QStringList parts = QProcess::splitCommand(cmd);
        QProcess::startDetached(parts.takeFirst(), parts);
        qInfo("Active mode [%s]: executed %s command",
              qUtf8Printable(config.modes[active_mode_index].name),
              qUtf8Printable(direction));
    }
}

int DialWheel::calculateSelectedIndex() const {
    // Selection area is at the top (90 degrees in Qt coordinates)
    // Calculate which option is closest to the top position
    const double degreesPerOption = 360.0 / config.modes.size();
    const double selectionAngle = 90.0; // Top of circle (Qt coordinates)

    // Normalize rotation to 0-360
    double normalizedRotation = std::fmod(rotation, 360.0);
    if (normalizedRotation < 0) normalizedRotation += 360.0;

    // Calculate which option index should be at the top
    // Options rotate, so we need to find which one is closest to selectionAngle
    int selectedIndex = 0;
    double minDiff = 360.0;

    for (size_t i = 0; i < config.modes.size(); ++i) {
        // Calculate where this option is positioned (matching the drawing code)
        double optionAngle = std::fmod(90.0 - (i * degreesPerOption + normalizedRotation), 360.0);
        if (optionAngle < 0) optionAngle += 360.0;

        // Calculate angular distance to selection area
        double diff = std::abs(optionAngle - selectionAngle);
        if (diff > 180.0) diff = 360.0 - diff; // Wrap around

        if (diff < minDiff) {
            minDiff = diff;
            selectedIndex = i;
        }
    }

    return selectedIndex;
}

void DialWheel::executeCurrent() {
    // This method is now only used for activating a mode
    // Not for executing commands (that happens in rotate())
    active_mode_index = current_index;
    current_mode = ACTIVE_MODE;
    hide();
    m_is_visible = false;
    qInfo("Activated mode: %s", qUtf8Printable(config.modes[active_mode_index].name));

    // Publish the active mode for any status bar that watches the file.
    writeStatus();
}

void DialWheel::writeStatus() {
    if (config.status_file.isEmpty()) return;

    QDir().mkpath(QFileInfo(config.status_file).absolutePath());
    QFile statusFile(config.status_file);
    if (!statusFile.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    const ModeConfig& mode = config.modes[active_mode_index];

    // Single-line JSON for a status bar (e.g. a Waybar custom module).
    QJsonObject obj;
    obj["text"] = mode.status_icon;
    obj["tooltip"] = "Dial Mode: " + mode.name;
    obj["class"] = "dial-mode-" + mode.name.toLower();

    statusFile.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    statusFile.close();
}

void DialWheel::toggleVisibility() {
    if (current_mode == SELECTION_MODE && m_is_visible) {
        // In selection mode, GUI visible: button press activates the selected mode
        executeCurrent();
    } else {
        // In active mode or GUI hidden: button press shows selection GUI
        current_mode = SELECTION_MODE;
        show();
        raise();
        activateWindow();
        m_is_visible = true;
        qInfo("Entered selection mode");
    }
}

void DialWheel::mousePressEvent(QMouseEvent* event) {
    // No-op for now
}

void DialWheel::mouseReleaseEvent(QMouseEvent* event) {
    // No-op for now
}

void DialWheel::mouseMoveEvent(QMouseEvent* event) {
    // No-op for now
}

DialGUI::DialGUI() {
    wheel = std::make_unique<DialWheel>();
    wheel->hide();
    
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(1, 1);
    
    if (QScreen* screen = QApplication::primaryScreen()) {
        QRect screenGeometry = screen->geometry();
        move(screenGeometry.width() - 1, screenGeometry.height() - 1);
    }
    
    if (!setupSocketServer()) {
        qWarning("Failed to setup socket server");
    }
}

DialGUI::~DialGUI() {
    if (socket) {
        socket->disconnectFromServer();
    }
    if (server) {
        server->close();
    }
    QLocalServer::removeServer(DIAL_SOCKET_PATH);
}

bool DialGUI::setupSocketServer() {
    // Remove any existing socket
    QLocalServer::removeServer(DIAL_SOCKET_PATH);
    
    server = std::make_unique<QLocalServer>();
    if (!server->listen(DIAL_SOCKET_PATH)) {
        qWarning("Failed to start server: %s", server->errorString().toStdString().c_str());
        return false;
    }
    
    connect(server.get(), &QLocalServer::newConnection, this, &DialGUI::handleSocketConnection);
    return true;
}

void DialGUI::handleSocketConnection() {
    if (socket) {
        socket->disconnectFromServer();
    }

    // nextPendingConnection() returns a Qt-managed pointer, use reset() to transfer ownership
    socket.reset(server->nextPendingConnection());
    connect(socket.get(), &QLocalSocket::readyRead, this, &DialGUI::handleSocketData);
    connect(socket.get(), &QLocalSocket::errorOccurred, this, &DialGUI::handleSocketError);
}

void DialGUI::handleSocketData() {
    if (!socket) return;
    
    QDataStream in(socket.get());
    in.setVersion(QDataStream::Qt_5_0);
    
    while (socket->bytesAvailable() > 0) {
        DialMessage msg;
        int type;
        in >> type >> msg.data;
        msg.type = static_cast<DialMessageType>(type);
        handleMessage(msg);
    }
}

void DialGUI::handleSocketError(QLocalSocket::LocalSocketError error) {
    qWarning("Socket error: %s", socket->errorString().toStdString().c_str());
    socket.reset();
}

void DialGUI::sendMessage(const DialMessage& msg) {
    if (!socket || socket->state() != QLocalSocket::ConnectedState) return;
    
    QDataStream out(socket.get());
    out.setVersion(QDataStream::Qt_5_0);
    out << static_cast<int>(msg.type) << msg.data;
}

void DialGUI::handleMessage(const DialMessage& msg) {
    switch (msg.type) {
        case DialMessageType::BUTTON_PRESS:
            wheel->toggleVisibility();
            break;

        case DialMessageType::ROTATION_CW:
            // Always process rotation, even when hidden (for active mode)
            wheel->rotate("cw");
            break;

        case DialMessageType::ROTATION_CCW:
            // Always process rotation, even when hidden (for active mode)
            wheel->rotate("ccw");
            break;

        case DialMessageType::SELECT_OPTION:
            wheel->executeCurrent();
            break;

        case DialMessageType::SET_CURRENT_OPTION:
            wheel->setCurrentOption(msg.data.toInt());
            break;

        default:
            break;
    }
}

DialWheel* DialGUI::getWheel() {
    return wheel.get();
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    DialGUI gui;
    // Don't show the main window - it's just a socket server container
    // The DialWheel will show/hide itself as needed
    return app.exec();
}