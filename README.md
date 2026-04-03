# DialedIn - ASUS Dial Driver for Linux

A Linux driver and GUI for the ASUS Dial, providing control of system functions through a radial wheel interface. Runs as a background service via systemd.

## Features

- **Volume Control**: Adjust system volume with dial rotation
- **Brightness Control**: Change screen brightness
- **Media Playback**: Skip tracks forward/backward
- **Scroll**: Scroll pages/documents (via ydotool)
- **Window Management**: Maximize/split windows
- **Automatic Device Detection**: Finds your ASUS Dial automatically
- **Radial Wheel UI**: Semi-transparent overlay with smooth rotation
- **Systemd Integration**: Runs automatically in the background

## How It Works

The driver uses a two-mode system:

**Selection Mode** (GUI visible):
1. Press the dial button to show the wheel overlay
2. Rotate to select a mode (Volume, Brightness, Media, Scroll, Windows)
3. Press the button again to activate that mode (wheel hides)

**Active Mode** (GUI hidden):
4. Rotate clockwise/counter-clockwise to adjust the selected function
5. Press the button to return to selection mode

| Mode | CW Action | CCW Action |
|------|-----------|------------|
| Volume | Increase +1% | Decrease -1% |
| Brightness | Increase +1% | Decrease -1% |
| Media | Next track | Previous track |
| Scroll | Scroll up | Scroll down |
| Windows | Maximize | Half-size |

## Requirements

- Qt5 (Core, Gui, Widgets, Network)
- CMake 3.16+
- C++17 compatible compiler
- Linux with HID support
- brightnessctl, playerctl, wmctrl
- ydotool + ydotoold (for scroll mode on Wayland)

## Installation

```bash
./install.sh
```

This will install dependencies, build the project, set up udev rules, and create/enable systemd user services. If you're added to the `input` group during installation, log out and back in for it to take effect.

### Scroll Mode Setup

Scroll mode uses ydotool, which requires its daemon:

```bash
sudo pacman -S ydotool        # or your distro's package manager
sudo systemctl enable --now ydotool
```

### Hyprland Users

Copy the rules from `hyprland_rules.conf` into your `~/.config/hypr/hyprland.conf` to remove window decorations from the dial overlay, then reload with `hyprctl reload`.

## Architecture

The driver has two components communicating via Unix domain socket (`/tmp/dial_socket`):

- **dial_daemon**: Reads HID events from the ASUS Dial device
- **dial_gui**: Displays the wheel interface and executes system commands

The GUI must start before the daemon (the install script handles this via systemd service ordering).

## Customization

Edit commands in `dial_common.h` and rebuild:

```cpp
namespace DialCommands {
    const QString VOLUME_UP = "pactl set-sink-volume @DEFAULT_SINK@ +1%";
    const QString VOLUME_DOWN = "pactl set-sink-volume @DEFAULT_SINK@ -1%";
    // ...
}
```

Rotation sensitivity can be changed in `dial_gui.h`:

```cpp
static constexpr int STEPS_PER_OPTION = 4;  // rotations to move between options
```

For X11 users, change scroll commands in `dial_common.h` to use xdotool:

```cpp
const QString SCROLL_UP = "xdotool click 4";
const QString SCROLL_DOWN = "xdotool click 5";
```

Rebuild after changes: `cd build && cmake .. && make`

## Troubleshooting

```bash
# Check service status
systemctl --user status dialedIn-gui.service dialedIn-daemon.service

# View logs
journalctl --user -u dialedIn-gui.service -u dialedIn-daemon.service -f

# Check if dial is detected
ls -la /dev/hidraw*
cat /sys/class/hidraw/hidraw*/device/uevent | grep -i asus

# Restart services
systemctl --user restart dialedIn-gui.service dialedIn-daemon.service
```

**Device not detected**: Check that it's plugged in/enabled in BIOS. Look for vendor ID `0B05` (ASUS) in `/sys/class/hidraw/hidraw*/device/uevent`.

**Permission errors**: Ensure you're in the `input` group (`groups | grep input`). Check udev rules at `/etc/udev/rules.d/99-dialedIn.rules`. Reload with `sudo udevadm control --reload-rules && sudo udevadm trigger`.

**Qt platform plugin error**: Install `qt5-wayland` (`sudo pacman -S qt5-wayland`), or force X11 with `export QT_QPA_PLATFORM=xcb`.

## Uninstallation

```bash
./stop_daemon.sh --uninstall
```

## License

MIT License
