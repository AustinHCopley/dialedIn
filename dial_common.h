#pragma once

#include <QString>

// Message types for communication
enum class DialMessageType {
    BUTTON_PRESS,
    ROTATION_CW,
    ROTATION_CCW,
    SELECT_OPTION,
    SET_CURRENT_OPTION
};

// Message structure
struct DialMessage {
    DialMessageType type;
    QString data;
};

// Unix domain socket path
#define DIAL_SOCKET_PATH "/tmp/dial_socket"

// Commands for different options
namespace DialCommands {
    const QString VOLUME_UP = "pactl set-sink-volume @DEFAULT_SINK@ +1%";
    const QString VOLUME_DOWN = "pactl set-sink-volume @DEFAULT_SINK@ -1%";
    const QString BRIGHTNESS_UP = "brightnessctl set +1%";
    const QString BRIGHTNESS_DOWN = "brightnessctl set 1%-";
    const QString MEDIA_NEXT = "playerctl next";
    const QString MEDIA_PREV = "playerctl previous";
    const QString WINDOW_MAX = "wmctrl -a :ACTIVE: -e 0,0,0,1920,1080";
    const QString WINDOW_HALF = "wmctrl -a :ACTIVE: -e 0,0,0,960,1080";
    // Scrolling - using ydotool key simulation for Wayland
    // Note: ydotool requires ydotoold daemon running (see setup_ydotool.sh)
    // Key codes: 103 = Up arrow, 108 = Down arrow
    // For page-based scrolling, use: 104 = Page Up, 109 = Page Down
    const QString SCROLL_UP = "ydotool key 103:1 103:0";      // Up arrow (line-by-line)
    const QString SCROLL_DOWN = "ydotool key 108:1 108:0";    // Down arrow (line-by-line)
} 