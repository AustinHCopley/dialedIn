#!/usr/bin/env bash

# exit on error
set -e

echo "Installing DialedIn daemon..."

# get current user
CURRENT_USER=$(whoami)
USER_HOME=$(eval echo ~$CURRENT_USER)
USER_ID=$(id -u)

# check for required dependencies
echo "Checking dependencies..."
if ! command -v xdotool &> /dev/null; then
    echo "xdotool not found. Installing..."
    if command -v apt-get &> /dev/null; then
        sudo apt-get update
        sudo apt-get install -y xdotool
    elif command -v dnf &> /dev/null; then
        sudo dnf install -y xdotool
    elif command -v pacman &> /dev/null; then
        sudo pacman -S --noconfirm xdotool
    else
        echo "Could not install xdotool. Please install it manually."
        exit 1
    fi
fi

# stop the service if it exists
echo "Checking for existing service..."
if systemctl --user is-active --quiet dialedIn.service 2>/dev/null; then
    echo "Stopping existing service..."
    systemctl --user stop dialedIn.service
    sleep 2
fi

# create udev rule for HID device
echo "Creating udev rule for HID device..."
sudo tee /etc/udev/rules.d/99-dialedIn.rules > /dev/null << EOL
# Rule for ASUS Dial HID device
KERNEL=="hidraw*", SUBSYSTEM=="hidraw", ATTRS{idVendor}=="0b05", ATTRS{idProduct}=="0220", MODE="0660", GROUP="input", TAG+="uaccess"
EOL

# reload udev rules
echo "Reloading udev rules..."
sudo udevadm control --reload-rules
sudo udevadm trigger

# add user to input group if not already a member
if ! groups $CURRENT_USER | grep -q input; then
    echo "Adding user to input group..."
    sudo usermod -a -G input $CURRENT_USER
    echo "Please log out and log back in for group changes to take effect."
fi

# compile daemon
echo "Compiling daemon..."
gcc -o dial-daemon dial-daemon.c

# create systemd user service directory if it doesn't exist
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

# create systemd service file
echo "Creating systemd service..."
tee ~/.config/systemd/user/dialedIn.service > /dev/null << EOL
[Unit]
Description=DialedIn Daemon for ASUS Dial
After=graphical.target

[Service]
Type=simple
ExecStartPre=/bin/bash -c 'source ~/.bashrc'
ExecStart=/usr/local/bin/dial-daemon
Restart=always
RestartSec=3
Environment=DISPLAY=$CURRENT_DISPLAY
Environment=XAUTHORITY=$XAUTHORITY_PATH
Environment=XDG_RUNTIME_DIR=/run/user/$USER_ID
Environment=DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/$USER_ID/bus
Environment=XDG_SESSION_TYPE=x11
Environment=XDG_SESSION_DESKTOP=ubuntu
Environment=XDG_CURRENT_DESKTOP=Unity

[Install]
WantedBy=default.target
EOL

# copy  daemon to /usr/local/bin
echo "Installing daemon binary..."
# remove old binary if it exists
sudo rm -f /usr/local/bin/dial-daemon
# copy new binary
sudo cp dial-daemon /usr/local/bin/
sudo chown root:input /usr/local/bin/dial-daemon
sudo chmod 755 /usr/local/bin/dial-daemon

# create default config if it doesn't exist
if [ ! -f /etc/dialedIn.conf ]; then
    echo "Creating default configuration..."
    sudo tee /etc/dialedIn.conf > /dev/null << EOL
# DialedIn Configuration
# Commands to execute for different dial actions

# Clockwise rotation (default: increase volume)
rotate_cw=xdotool key XF86AudioRaiseVolume

# Counter-clockwise rotation (default: decrease volume)
rotate_ccw=xdotool key XF86AudioLowerVolume

# Button press (default: toggle mute)
press=xdotool key XF86AudioMute
EOL
    sudo chmod 644 /etc/dialedIn.conf
fi

# enable and start service
echo "Enabling and starting service..."
systemctl --user daemon-reload
systemctl --user enable dialedIn.service
systemctl --user start dialedIn.service

echo "Installation complete!"
echo "You can check the status with: systemctl --user status dialedIn.service"
echo "View logs with: journalctl --user -u dialedIn.service -f"
echo ""
echo "NOTE: If you were added to the input group, please log out and log back in for the changes to take effect." 