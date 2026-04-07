#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd ..

echo "Installing binaries..."
sudo install -m 755 build/dial_daemon /usr/local/bin/
sudo install -m 755 build/dial_gui /usr/local/bin/

echo "Restarting services..."
systemctl --user restart dialedIn-gui.service
sleep 1
systemctl --user restart dialedIn-daemon.service

echo "Done. Service status:"
systemctl --user is-active dialedIn-gui.service dialedIn-daemon.service
