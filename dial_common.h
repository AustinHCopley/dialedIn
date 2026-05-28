#pragma once

#include <QString>

// Message types for communication
enum class DialMessageType {
    BUTTON_PRESS,
    ROTATION_CW,
    ROTATION_CCW
};

// Message structure
struct DialMessage {
    DialMessageType type;
    QString data;
};

// Unix domain socket path used by both the daemon and the GUI.
#define DIAL_SOCKET_PATH "/tmp/dial_socket"

// Mode commands and icons are defined in the config file, not here.
// See config/default.toml and dial_config.h.
