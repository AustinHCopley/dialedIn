#!/bin/bash

SERVICES="dialedIn-daemon.service dialedIn-gui.service"

if [ "$1" == "--uninstall" ]; then
    echo "Uninstalling DialedIn..."

    # stop and disable services
    systemctl --user stop $SERVICES 2>/dev/null || true
    systemctl --user disable $SERVICES 2>/dev/null || true
    rm -f ~/.config/systemd/user/dialedIn-daemon.service
    rm -f ~/.config/systemd/user/dialedIn-gui.service
    systemctl --user daemon-reload

    # remove binaries
    sudo rm -f /usr/local/bin/dial_daemon /usr/local/bin/dial_gui

    # remove udev rules
    echo "Removing udev rules..."
    sudo rm -f /etc/udev/rules.d/99-dialedIn.rules
    sudo rm -f /etc/udev/rules.d/99-asus-dial.rules
    sudo rm -f /etc/udev/rules.d/80-uinput.rules
    sudo udevadm control --reload-rules
    sudo udevadm trigger

    echo "Uninstallation complete!"
else
    echo "Stopping DialedIn..."
    systemctl --user stop $SERVICES 2>/dev/null || true
    echo "Stopped. To start again: systemctl --user start $SERVICES"
    echo "To uninstall completely: $0 --uninstall"
fi
