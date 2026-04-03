#include <QCoreApplication>
#include <QLocalSocket>
#include <QTimer>
#include <QSocketNotifier>
#include <QDataStream>
#include <QFile>
#include <QDir>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include "dial_common.h"

#define BUTTON_DOWN 0x01
#define ROTATE_CW 0x01
#define ROTATE_CCW 0xFF

class DialDaemon : public QObject {
    Q_OBJECT
    
public:
    DialDaemon(QObject* parent = nullptr) : QObject(parent), dial_fd(-1) {
        if (!openDialDevice()) {
            qWarning("Failed to open dial device");
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
            return;
        }
        
        if (!connectToGUI()) {
            qWarning("Failed to connect to GUI");
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
            return;
        }
        
        // Create socket notifier for the device
        dial_notifier = std::make_unique<QSocketNotifier>(dial_fd, QSocketNotifier::Read);
        connect(dial_notifier.get(), &QSocketNotifier::activated, this, &DialDaemon::handleDialEvent);
    }
    
    ~DialDaemon() {
        closeDialDevice();
    }
    
private slots:
    void handleDialEvent() {
        unsigned char buf[4];
        ssize_t ret = read(dial_fd, buf, sizeof(buf));
        
        if (ret < 0) {
            qWarning("Error reading from dial: %s", strerror(errno));
            return;
        }
        
        if (ret >= 4) {
            unsigned char report_id = buf[0];
            unsigned char button = buf[1];
            unsigned char rotation_hb = buf[2];
            unsigned char rotation_lb = buf[3];
            
            DialMessage msg;
            
            // Handle button press
            if (button == BUTTON_DOWN) {
                msg.type = DialMessageType::BUTTON_PRESS;
                sendMessage(msg);
            }
            
            // Handle rotation
            if (rotation_hb == ROTATE_CW && rotation_lb == 0x00) {
                msg.type = DialMessageType::ROTATION_CW;
                sendMessage(msg);
            } else if (rotation_hb == ROTATE_CCW && rotation_lb == ROTATE_CCW) {
                msg.type = DialMessageType::ROTATION_CCW;
                sendMessage(msg);
            }
        }
    }
    
private:
    int dial_fd;
    std::unique_ptr<QSocketNotifier> dial_notifier;
    std::unique_ptr<QLocalSocket> socket;
    QString dial_device_path;

    QString findDialDevice() {
        // Search for ASUS dial device in /dev/hidraw*
        QDir devDir("/dev");
        QStringList hidrawDevices = devDir.entryList(QStringList() << "hidraw*", QDir::System);

        for (const QString& device : hidrawDevices) {
            QString sysPath = QString("/sys/class/hidraw/%1/device/uevent").arg(device);
            QFile ueventFile(sysPath);

            if (ueventFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString content = QString::fromUtf8(ueventFile.readAll());
                ueventFile.close();

                // Look for ASUS vendor ID (0B05) or ASUS in the name
                if (content.contains("0B05", Qt::CaseInsensitive) ||
                    content.contains("ASUS", Qt::CaseInsensitive)) {
                    QString devicePath = QString("/dev/%1").arg(device);
                    qInfo("Found ASUS dial device: %s", devicePath.toStdString().c_str());
                    return devicePath;
                }
            }
        }

        qWarning("Could not find ASUS dial device, falling back to /dev/hidraw1");
        return "/dev/hidraw1";
    }

    bool openDialDevice() {
        dial_device_path = findDialDevice();
        dial_fd = open(dial_device_path.toStdString().c_str(), O_RDONLY);
        if (dial_fd < 0) {
            qWarning("Failed to open %s: %s", dial_device_path.toStdString().c_str(), strerror(errno));
            return false;
        }
        qInfo("Successfully opened dial device: %s", dial_device_path.toStdString().c_str());
        return true;
    }
    
    void closeDialDevice() {
        if (dial_notifier) {
            dial_notifier->setEnabled(false);
            dial_notifier.reset();
        }
        if (dial_fd >= 0) {
            ::close(dial_fd);
            dial_fd = -1;
        }
    }
    
    bool connectToGUI() {
        socket = std::make_unique<QLocalSocket>();
        socket->connectToServer(DIAL_SOCKET_PATH);
        
        if (!socket->waitForConnected(1000)) {
            qWarning("Failed to connect to GUI: %s", socket->errorString().toStdString().c_str());
            return false;
        }
        
        return true;
    }
    
    void sendMessage(const DialMessage& msg) {
        if (!socket || socket->state() != QLocalSocket::ConnectedState) return;
        
        QDataStream out(socket.get());
        out.setVersion(QDataStream::Qt_5_0);
        out << static_cast<int>(msg.type) << msg.data;
    }
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    DialDaemon daemon;
    return app.exec();
}

#include "dial_daemon.moc" 