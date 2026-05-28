#pragma once

#include <QColor>
#include <QString>
#include <vector>

// A single selectable dial mode. Loaded from the [[modes]] array in the config.
struct ModeConfig {
    QString name;            // label shown under the icon on the wheel
    QString icon;            // glyph drawn on the wheel (emoji or Nerd Font)
    QString status_icon;     // glyph written to the status file (e.g. for a bar)
    QString cw_cmd;          // command run on clockwise rotation in active mode
    QString ccw_cmd;         // command run on counter-clockwise rotation
};

// Visual style of the wheel overlay. Colors accept "#RRGGBB" or "#RRGGBBAA".
struct WheelStyle {
    int size = 300;              // overlay width/height in pixels
    int option_radius = 30;      // radius of each option bubble
    int steps_per_option = 4;    // dial steps required to move one option

    QString font_family = "sans-serif";
    int icon_point_size = 16;
    int label_point_size = 9;

    QColor background        {30, 30, 30, 200};   // inner circle fill
    QColor background_border {40, 40, 40, 200};
    QColor selection         {80, 120, 200, 120}; // top selection wedge
    QColor accent            {80, 120, 200, 230};  // selected option fill
    QColor selected_border   {255, 255, 255, 255};
    QColor option            {50, 50, 50, 200};    // unselected option fill
    QColor option_border     {180, 180, 180, 255};
    QColor text              {255, 255, 255, 255};
};

struct DialConfig {
    WheelStyle style;
    std::vector<ModeConfig> modes;
    // Absolute path to write the status JSON. Empty disables status output.
    QString status_file;
};

// Resolve the active config: the first of the user config
// ($XDG_CONFIG_HOME/dialedIn/config.toml) or the installed system default is
// loaded over the built-in defaults. Any missing key keeps its default.
DialConfig loadConfig();

// Returns the default status-file path ($XDG_RUNTIME_DIR/dialedIn/status.json,
// falling back to /tmp/dial_status) used when the config does not set one.
QString defaultStatusFile();
