import fcntl
import struct

DEVICE_FILE = "/dev/edge_detection"
IOCTL_ENABLE_EDGE_DETECTION = 0x40046501  # 對應驅動定義

def enable_edge_detection(enable):
    with open(DEVICE_FILE, 'r+b', buffering=0) as f:
        fcntl.ioctl(f, IOCTL_ENABLE_EDGE_DETECTION, struct.pack('i', enable))
        print("Edge detection set to:", "Enabled" if enable else "Disabled")

# 測試啟用、關閉
enable_edge_detection(1)
enable_edge_detection(0)
