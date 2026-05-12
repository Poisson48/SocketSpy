#!/bin/bash
# setup_vcan.sh — configure virtual CAN interfaces vcan0 and vcan1
set -e

sudo modprobe vcan
sudo ip link add dev vcan0 type vcan 2>/dev/null || true
sudo ip link add dev vcan1 type vcan 2>/dev/null || true
sudo ip link set up vcan0
sudo ip link set up vcan1
echo "vcan0 vcan1 ready"
