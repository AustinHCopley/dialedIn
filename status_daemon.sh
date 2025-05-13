#!/usr/bin/env bash

echo "DialedIn Daemon Status"
echo "======================"

# service status
echo -e "\nService Status:"
systemctl --user status dialedIn.service --no-pager

# recent logs
echo -e "\nRecent Logs:"
journalctl --user -u dialedIn.service -n 20 --no-pager

echo -e "\nTo view live logs, run: journalctl --user -u dialedIn.service -f"