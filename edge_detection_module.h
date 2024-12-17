// edge_detection_ioctl.h
#ifndef EDGE_DETECTION_IOCTL_H
#define EDGE_DETECTION_IOCTL_H

#include <linux/ioctl.h>

#define EDGE_IOC_MAGIC 'e'
#define IOCTL_ENABLE_EDGE_DETECTION _IOW(EDGE_IOC_MAGIC, 1, int)

#endif