#!/bin/bash
echo "Resetting Bluetooth stack..."
sudo systemctl restart bluetooth
sleep 2
sudo hciconfig hci0 down
sleep 1
sudo hciconfig hci0 up
sleep 2
sudo btmgmt power off
sleep 1
sudo btmgmt power on
echo "Bluetooth reset complete."
