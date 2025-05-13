#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>

#define HIDRAW_DEVICE "/dev/hidraw1"  // "/dev/hidraw0"
#define BUFFER_SIZE 256
#define BUTTON_DOWN  0x01
#define BUTTON_UP    0x00
#define ROTATE_CW  0x01
#define ROTATE_CCW 0xff

typedef struct {
    unsigned char report_id;
    unsigned char button;
    unsigned char rotation_hb;
    unsigned char rotation_lb;
} DialPacket;

int main() {
    int fd;
    int i, ret;
    DialPacket packet;
    unsigned char buf[BUFFER_SIZE];
    struct hidraw_devinfo info;
    
    // open HID device
    fd = open(HIDRAW_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("Unable to open device");
        return 1;
    }
    
    // get device info
    ret = ioctl(fd, HIDIOCGRAWINFO, &info);
    if (ret < 0) {
        perror("HIDIOCGRAWINFO");
    } else {
        printf("Raw Info:\n");
        printf("\tVendor: 0x%04hx\n", info.vendor);
        printf("\tProduct: 0x%04hx\n", info.product);
    }
    
    printf("Listening for dial events. Press Ctrl+C to exit.\n");
    
    // main loop
    while (1) {
        ret = read(fd, buf, BUFFER_SIZE);
        if (ret < 0) {
            perror("read");
            break;
        }
        
        printf("Raw data (%d bytes):", ret);
        for (i = 0; i < ret; i++) {
            printf(" %02x", buf[i]);
        }
        printf("\n");
        
        // check if enough bytes for DialPacket
        if (ret >= sizeof(DialPacket)) {
            memcpy(&packet, buf, sizeof(DialPacket));
            
            if (packet.button == BUTTON_DOWN) {
                printf("Button pressed\n");
            } else if (packet.button == BUTTON_UP) {
                printf("Button released\n");
            }
            
            if (packet.rotation_hb == ROTATE_CW && packet.rotation_lb == 0x00) {
                printf("Rotation: Clockwise\n");
            } else if (packet.rotation_hb == ROTATE_CCW && packet.rotation_lb == ROTATE_CCW) {
                printf("Rotation: Counter-clockwise\n");
            }
        }
    }
    
    close(fd);
    return 0;
}
