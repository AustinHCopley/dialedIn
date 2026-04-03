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
#include <QTextStream>
#include <vector>
#include <string>
#include <map>
#include <cmath>

// Constants for dial events
constexpr unsigned char BUTTON_DOWN = 0x01;
constexpr unsigned char BUTTON_UP = 0x00;
constexpr unsigned char ROTATE_CW = 0x01;
constexpr unsigned char ROTATE_CCW = 0xff;
constexpr const char* HIDRAW_DEVICE = "/dev/hidraw1";

DialWheel::DialWheel(QWidget* parent)
    : QWidget(parent),
      m_is_visible(false),
      current_mode(SELECTION_MODE),
      active_mode_index(0),
      last_rotation_direction("cw") {
    // Window flags — BypassWindowManagerHint breaks Wayland surface damage tracking
    // (causes only top-left quadrant to repaint). Use Hyprland window rules for
    // decoration removal instead (see hyprland_rules.conf).
    setWindowFlags(Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint |
                   Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    // Initialize options
    options = {
        DialOption("Volume", "🔊",
                  DialCommands::VOLUME_UP,
                  DialCommands::VOLUME_DOWN),
        DialOption("Brightness", "☀️",
                  DialCommands::BRIGHTNESS_UP,
                  DialCommands::BRIGHTNESS_DOWN),
        DialOption("Media", "🎵",
                  DialCommands::MEDIA_NEXT,
                  DialCommands::MEDIA_PREV),
        DialOption("Scroll", "🖱️",
                  DialCommands::SCROLL_DOWN,
                  DialCommands::SCROLL_UP),
        DialOption("Windows", "🪟",
                  DialCommands::WINDOW_MAX,
                  DialCommands::WINDOW_HALF)
    };

    current_index = 0;
    rotation = 0;
    setFixedSize(300, 300);

    // Center on screen
    if (QScreen* screen = QApplication::primaryScreen()) {
        QRect screenGeometry = screen->geometry();
        move((screenGeometry.width() - width()) / 2,
             (screenGeometry.height() - height()) / 2);
    }
}

void DialWheel::setCurrentOption(int index) {
    if (index >= 0 && index < static_cast<int>(options.size())) {
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

    QPointF center(width() / 2.0, height() / 2.0);
    double radius = std::min(width(), height()) / 2.0 - 30;

    // Draw background circle
    painter.setPen(QPen(QColor(40, 40, 40, 200), 2));
    painter.setBrush(QBrush(QColor(30, 30, 30, 200)));
    painter.drawEllipse(10, 10, width() - 20, height() - 20);

    // Draw fixed selection area at the top
    // Qt angles: 0° = 3 o'clock, 90° = 12 o'clock (top), 180° = 9 o'clock, 270° = 6 o'clock
    const double selectionAngle = 90.0; // Top position (12 o'clock in Qt coordinates)
    const double selectionArc = 360.0 / options.size() * 0.9; // Cover most of option width

    // Draw selection indicator (wedge at top) - extend to edge of background circle
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(QColor(80, 120, 200, 120))); // Blue highlight

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
    for (size_t i = 0; i < options.size(); ++i) {
        // Qt coordinate system: 0° = 3 o'clock, goes counter-clockwise
        // Start at 90° (top) and distribute options evenly
        double angle = 90.0 - (i * 360.0 / options.size() + rotation);
        double rad_angle = angle * M_PI / 180.0;

        // Calculate position using standard circular positioning
        double x = center.x() + radius * 0.7 * std::cos(rad_angle);
        double y = center.y() - radius * 0.7 * std::sin(rad_angle); // Negative because Qt Y goes down

        // Highlight the selected option (the one in the selection area)
        if (static_cast<int>(i) == current_index) {
            painter.setPen(QPen(QColor(255, 255, 255), 3));
            painter.setBrush(QBrush(QColor(80, 120, 200, 230))); // Bright blue
        } else {
            painter.setPen(QPen(QColor(180, 180, 180), 1));
            painter.setBrush(QBrush(QColor(50, 50, 50, 200)));
        }

        QRectF option_rect(x - 30, y - 30, 60, 60);
        painter.drawEllipse(option_rect);

        // Draw icon and text
        painter.setPen(QColor(255, 255, 255));
        painter.setFont(QFont("Arial", 16));
        painter.drawText(QRectF(x - 30, y - 30, 60, 35), Qt::AlignCenter,
                        QString::fromStdString(options[i].icon));

        painter.setFont(QFont("Arial", 9));
        painter.drawText(QRectF(x - 30, y, 60, 25), Qt::AlignCenter,
                        QString::fromStdString(options[i].name));
    }
}

void DialWheel::rotate(const QString& direction) {
    last_rotation_direction = direction;

    if (current_mode == SELECTION_MODE) {
        // In selection mode: rotate the wheel, selection updates based on position
        const double degreesPerOption = 360.0 / options.size();
        const double degreesPerStep = degreesPerOption / STEPS_PER_OPTION;

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
                            ? options[active_mode_index].cw_cmd
                            : options[active_mode_index].ccw_cmd;
        QStringList parts = QProcess::splitCommand(cmd);
        QProcess::startDetached(parts.takeFirst(), parts);
        qInfo("Active mode [%s]: executed %s command",
              options[active_mode_index].name.c_str(),
              direction.toStdString().c_str());
    }
}

int DialWheel::calculateSelectedIndex() const {
    // Selection area is at the top (90 degrees in Qt coordinates)
    // Calculate which option is closest to the top position
    const double degreesPerOption = 360.0 / options.size();
    const double selectionAngle = 90.0; // Top of circle (Qt coordinates)

    // Normalize rotation to 0-360
    double normalizedRotation = std::fmod(rotation, 360.0);
    if (normalizedRotation < 0) normalizedRotation += 360.0;

    // Calculate which option index should be at the top
    // Options rotate, so we need to find which one is closest to selectionAngle
    int selectedIndex = 0;
    double minDiff = 360.0;

    for (size_t i = 0; i < options.size(); ++i) {
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
    qInfo("Activated mode: %s", options[active_mode_index].name.c_str());

    // Write current mode to file for waybar integration
    updateWaybarStatus();
}

void DialWheel::updateWaybarStatus() {
    // Write current mode info to /tmp/dial_status for waybar to read
    QFile statusFile("/tmp/dial_status");
    if (statusFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&statusFile);
        QString mode_name = QString::fromStdString(options[active_mode_index].name);
        QString mode_icon = QString::fromStdString(options[active_mode_index].icon);

        // Write JSON format for waybar custom module
        out << "{\n";
        out << "  \"text\": \"" << mode_icon << " " << mode_name << "\",\n";
        out << "  \"tooltip\": \"Dial Mode: " << mode_name << "\",\n";
        out << "  \"class\": \"dial-mode-" << mode_name.toLower() << "\"\n";
        out << "}\n";
        statusFile.close();
    }
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