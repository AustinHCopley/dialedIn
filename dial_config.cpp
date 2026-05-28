#include "dial_config.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <cstdlib>

#include <toml++/toml.hpp>

namespace {

// Parse "#RRGGBB" or "#RRGGBBAA" into a QColor, keeping the fallback on failure.
QColor parseColor(const std::string& value, const QColor& fallback) {
    QString s = QString::fromStdString(value).trimmed();
    if (!s.startsWith('#')) s.prepend('#');
    QColor c(s);  // QColor handles #RRGGBB and #AARRGGBB
    if (s.length() == 9) {
        // Qt reads #AARRGGBB; our convention is #RRGGBBAA, so reorder.
        bool ok = false;
        uint rgba = s.mid(1).toUInt(&ok, 16);
        if (ok) {
            c = QColor((rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF,
                       (rgba >> 8) & 0xFF, rgba & 0xFF);
        }
    }
    return c.isValid() ? c : fallback;
}

QString xdgRuntimeDir() {
    if (const char* env = std::getenv("XDG_RUNTIME_DIR")) {
        if (env[0] != '\0') return QString::fromLocal8Bit(env);
    }
    return QString();
}

// Built-in modes, used when the config defines no [[modes]] array.
std::vector<ModeConfig> defaultModes() {
    // Wheel icons are emoji; status icons are Nerd Font glyphs (nf-md-*).
    return {
        {"Volume",     "\U0001F50A", QString::fromUtf8("\U000F057E"),
         "pactl set-sink-volume @DEFAULT_SINK@ +1%",
         "pactl set-sink-volume @DEFAULT_SINK@ -1%"},
        {"Brightness", "☀️", QString::fromUtf8("\U000F00E0"),
         "brightnessctl set +1%",
         "brightnessctl set 1%-"},
        {"Media",      "\U0001F3B5", QString::fromUtf8("\U000F040A"),
         "playerctl next",
         "playerctl previous"},
        {"Scroll",     "\U0001F5B1️", QString::fromUtf8("\U000F037D"),
         "ydotool key 108:1 108:0",
         "ydotool key 103:1 103:0"},
        {"Windows",    "\U0001FA9F", QString::fromUtf8("\U000F05AF"),
         "wmctrl -a :ACTIVE: -e 0,0,0,1920,1080",
         "wmctrl -a :ACTIVE: -e 0,0,0,960,1080"},
    };
}

QString userConfigPath() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return QDir(base).filePath("dialedIn/config.toml");
}

void applyTable(const toml::table& tbl, DialConfig& cfg) {
    if (auto wheel = tbl["wheel"].as_table()) {
        auto& s = cfg.style;
        s.size             = (*wheel)["size"].value_or(s.size);
        s.option_radius    = (*wheel)["option_radius"].value_or(s.option_radius);
        s.steps_per_option = (*wheel)["steps_per_option"].value_or(s.steps_per_option);
        s.font_family      = QString::fromStdString(
            (*wheel)["font"].value_or(s.font_family.toStdString()));
        s.icon_point_size  = (*wheel)["icon_size"].value_or(s.icon_point_size);
        s.label_point_size = (*wheel)["label_size"].value_or(s.label_point_size);
    }

    if (auto colors = tbl["colors"].as_table()) {
        auto& s = cfg.style;
        auto col = [&](const char* key, QColor& dst) {
            if (auto v = (*colors)[key].value<std::string>())
                dst = parseColor(*v, dst);
        };
        col("background",        s.background);
        col("background_border", s.background_border);
        col("selection",         s.selection);
        col("accent",            s.accent);
        col("selected_border",   s.selected_border);
        col("option",            s.option);
        col("option_border",     s.option_border);
        col("text",              s.text);
    }

    if (auto status = tbl["status"].as_table()) {
        if (auto v = (*status)["file"].value<std::string>())
            cfg.status_file = QString::fromStdString(*v);
    }

    if (auto modes = tbl["modes"].as_array()) {
        std::vector<ModeConfig> parsed;
        for (const auto& node : *modes) {
            const toml::table* m = node.as_table();
            if (!m) continue;
            ModeConfig mode;
            mode.name        = QString::fromStdString((*m)["name"].value_or(std::string{}));
            mode.icon        = QString::fromStdString((*m)["icon"].value_or(std::string{}));
            mode.status_icon = QString::fromStdString((*m)["status_icon"].value_or(std::string{}));
            mode.cw_cmd      = QString::fromStdString((*m)["cw"].value_or(std::string{}));
            mode.ccw_cmd     = QString::fromStdString((*m)["ccw"].value_or(std::string{}));
            if (!mode.name.isEmpty()) parsed.push_back(std::move(mode));
        }
        if (!parsed.empty()) cfg.modes = std::move(parsed);
    }
}

}  // namespace

QString defaultStatusFile() {
    QString runtime = xdgRuntimeDir();
    if (!runtime.isEmpty())
        return QDir(runtime).filePath("dialedIn/status.json");
    return QStringLiteral("/tmp/dial_status");
}

DialConfig loadConfig() {
    DialConfig cfg;
    cfg.modes = defaultModes();
    cfg.status_file = defaultStatusFile();

    // Prefer the user's config; otherwise the installed system default.
    QString path = userConfigPath();
    if (!QFile::exists(path)) {
#ifdef DIALEDIN_DEFAULT_CONFIG
        QString sys = QStringLiteral(DIALEDIN_DEFAULT_CONFIG);
        path = QFile::exists(sys) ? sys : QString();
#else
        path = QString();
#endif
    }
    if (path.isEmpty()) return cfg;

    try {
        toml::table tbl = toml::parse_file(path.toStdString());
        applyTable(tbl, cfg);
    } catch (const toml::parse_error& err) {
        qWarning("dialedIn: failed to parse config %s: %s",
                 path.toStdString().c_str(), err.description().data());
    }
    return cfg;
}
