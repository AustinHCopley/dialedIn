
# Linux Driver for ASUS Dial

A Linux driver daemon for the ASUS Dial found on ProArt Studiobook laptops. This tool enables the functionality of the physical dial in Linux environments, allowing mapping dial rotations and button presses to custom commands.

Features:
- Detect dial rotation (clockwise and counter-clockwise) and button presses
- Configurable actions for each input type
- Runs as a background service that starts at boot
- Lightweight implementation with minimal dependencies

Has been tested on ProArt Studiobook H7600ZW_H7600ZW using:
- Ubuntu 22.04.5 LTS x86_64
- Ubuntu 20.04.5 LTS x86_64
- Arch Linux 6.14.4

paths:
Service: 
[/etc/systemd/system/dialedIn.service](/etc/systemd/system/dialedIn.service)

Config: 
[/etc/dialedIn.conf](/etc/dialedIn.conf)