#!/usr/bin/env bash

# exit on error
set -e

echo "Installing DialedIn (ASUS Dial Driver for Linux)..."

# get current user
CURRENT_USER=$(whoami)
USER_HOME=$(eval echo ~$CURRENT_USER)
USER_ID=$(id -u)

# check for required build tools
echo "Checking build tools..."
if ! command -v cmake &> /dev/null || ! command -v g++ &> /dev/null; then
    echo "Installing build tools..."
    if command -v apt-get &> /dev/null; then
        sudo apt-get update
        sudo apt-get install -y build-essential cmake
    elif command -v dnf &> /dev/null; then
        sudo dnf install -y gcc-c++ cmake
    elif command -v pacman &> /dev/null; then
        sudo pacman -S --noconfirm base-devel cmake
    else
        echo "Could not install build tools. Please install them manually."
        exit 1
    fi
fi

# check for Qt5
echo "Checking Qt5..."
if ! pkg-config --exists Qt5Core Qt5Gui Qt5Widgets Qt5Network; then
    echo "Installing Qt5..."
    if command -v apt-get &> /dev/null; then
        sudo apt-get update
        sudo apt-get install -y qtbase5-dev libqt5network5
    elif command -v dnf &> /dev/null; then
        sudo dnf install -y qt5-qtbase-devel
    elif command -v pacman &> /dev/null; then
        sudo pacman -S --noconfirm qt5-base
    else
        echo "Could not install Qt5. Please install it manually."
        exit 1
    fi
fi

# install additional system dependencies
echo "Installing system dependencies..."
if command -v apt-get &> /dev/null; then
    sudo apt-get update
    sudo apt-get install -y brightnessctl playerctl wmctrl
elif command -v dnf &> /dev/null; then
    sudo dnf install -y brightnessctl playerctl wmctrl
elif command -v pacman &> /dev/null; then
    sudo pacman -S --noconfirm brightnessctl playerctl wmctrl
else
    echo "Warning: Could not install system dependencies. Some features may not work."
fi

# stop existing services if running
echo "Stopping existing services..."
systemctl --user stop dialedIn-gui.service 2>/dev/null || true
systemctl --user stop dialedIn-daemon.service 2>/dev/null || true
systemctl --user stop dialedIn.service 2>/dev/null || true
sleep 1

# build the application
echo "Building daemon and GUI..."
mkdir -p build
cd build
cmake ..
make -j$(nproc)
cd ..

# install binaries
echo "Installing binaries..."
sudo install -m 755 build/dial_daemon /usr/local/bin/
sudo install -m 755 build/dial_gui /usr/local/bin/

# create udev rules
echo "Setting up udev rules..."

# ASUS Dial HID device access
sudo tee /etc/udev/rules.d/99-dialedIn.rules > /dev/null << 'EOL'
# Rule for ASUS Dial HID device (I2C-HID, vendor 0B05, product 0220)
# Uses KERNELS to match parent HID device — ATTRS{idVendor} does not exist for I2C-HID
KERNEL=="hidraw*", SUBSYSTEM=="hidraw", KERNELS=="0018:0B05:0220.*", MODE="0660", GROUP="input", TAG+="uaccess"
EOL

# uinput access for ydotool (Wayland scroll emulation)
sudo tee /etc/udev/rules.d/80-uinput.rules > /dev/null << 'EOL'
# Allow input group to access uinput for ydotool
KERNEL=="uinput", GROUP="input", MODE="0660", OPTIONS+="static_node=uinput"
EOL

# clean up old rules file if it exists
sudo rm -f /etc/udev/rules.d/99-asus-dial.rules

# ensure uinput module is loaded now and on boot
sudo modprobe uinput 2>/dev/null || true
echo "uinput" | sudo tee /etc/modules-load.d/uinput.conf > /dev/null

# reload and apply udev rules
echo "Reloading udev rules..."
sudo udevadm control --reload-rules
sudo udevadm trigger

# add user to input group if not already a member
if ! groups $CURRENT_USER | grep -q input; then
    echo "Adding user to input group..."
    sudo usermod -a -G input $CURRENT_USER
    echo "WARNING: You need to log out and log back in for group changes to take effect!"
fi

# create systemd user service directory
mkdir -p ~/.config/systemd/user/

# detect display manager and set XAUTHORITY path
XAUTHORITY_PATH=""
if [ -f "/run/user/$USER_ID/gdm/Xauthority" ]; then
    XAUTHORITY_PATH="/run/user/$USER_ID/gdm/Xauthority"
elif [ -f "/run/user/$USER_ID/lightdm/Xauthority" ]; then
    XAUTHORITY_PATH="/run/user/$USER_ID/lightdm/Xauthority"
elif [ -f "/run/user/$USER_ID/sddm/Xauthority" ]; then
    XAUTHORITY_PATH="/run/user/$USER_ID/sddm/Xauthority"
else
    XAUTHORITY_PATH="$USER_HOME/.Xauthority"
fi

# get current DISPLAY value
CURRENT_DISPLAY=$(echo $DISPLAY)
if [ -z "$CURRENT_DISPLAY" ]; then
    CURRENT_DISPLAY=":0"
fi

# create GUI systemd service
echo "Creating GUI systemd service..."
tee ~/.config/systemd/user/dialedIn-gui.service > /dev/null << EOL
[Unit]
Description=DialedIn GUI for ASUS Dial
After=graphical.target
Before=dialedIn-daemon.service

[Service]
Type=simple
ExecStart=/usr/local/bin/dial_gui
Restart=always
RestartSec=3
Environment=DISPLAY=$CURRENT_DISPLAY
Environment=XAUTHORITY=$XAUTHORITY_PATH
Environment=XDG_RUNTIME_DIR=/run/user/$USER_ID
Environment=DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/$USER_ID/bus

[Install]
WantedBy=default.target
EOL

# create daemon systemd service
echo "Creating daemon systemd service..."
tee ~/.config/systemd/user/dialedIn-daemon.service > /dev/null << EOL
[Unit]
Description=DialedIn Daemon for ASUS Dial
After=graphical.target dialedIn-gui.service
Requires=dialedIn-gui.service

[Service]
Type=simple
ExecStart=/usr/local/bin/dial_daemon
Restart=always
RestartSec=3
Environment=XDG_RUNTIME_DIR=/run/user/$USER_ID

[Install]
WantedBy=default.target
EOL

# reload, enable and start services
echo "Enabling and starting services..."
systemctl --user daemon-reload
systemctl --user enable dialedIn-gui.service
systemctl --user enable dialedIn-daemon.service
systemctl --user start dialedIn-gui.service
sleep 1
systemctl --user start dialedIn-daemon.service

echo ""
echo "========================================="
echo "Installation complete!"
echo "========================================="
echo ""
echo "Usage:"
echo "  - Press the dial button to show the wheel"
echo "  - Rotate to select an option"
echo "  - Press again to execute the selected action"
echo "  - Rotate clockwise/counter-clockwise for different commands"
echo ""
echo "Service status:"
systemctl --user status dialedIn-gui.service --no-pager -l | head -n 5
systemctl --user status dialedIn-daemon.service --no-pager -l | head -n 5
echo ""
echo "Check status: systemctl --user status dialedIn-gui.service dialedIn-daemon.service"
echo "View logs: journalctl --user -u dialedIn-gui.service -u dialedIn-daemon.service -f"
echo ""
if ! groups $CURRENT_USER | grep -q input; then
    echo "IMPORTANT: Log out and log back in for input group membership to take effect!"
fi
