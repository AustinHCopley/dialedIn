#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <time.h>
#include <errno.h>

#define HIDRAW_DEVICE "/dev/hidraw1"  // "/dev/hidraw0"
#define BUFFER_SIZE 256
#define BUTTON_DOWN  0x01
#define BUTTON_UP    0x00
#define ROTATE_CW    0x01
#define ROTATE_CCW   0xff
#define CONFIG_FILE "/etc/dialedIn.conf"

typedef struct {
    unsigned char report_id;
    unsigned char button;
    unsigned char rotation_hb;
    unsigned char rotation_lb;
} DialPacket;

// termination signal flag
volatile sig_atomic_t terminate = 0;

// default configuration
char cmd_rotate_cw[256] = "xdotool key XF86AudioRaiseVolume";
char cmd_rotate_ccw[256] = "xdotool key XF86AudioLowerVolume";
char cmd_press[256] = "xdotool key XF86AudioMute";

// signal handler
void handle_signal(int signum) {
    syslog(LOG_INFO, "Received signal %d, terminating", signum);
    terminate = 1;
}

// load configuration file
void load_config() {
    FILE *config = fopen(CONFIG_FILE, "r");
    if (!config) {
        syslog(LOG_WARNING, "Could not open config file %s, using defaults", CONFIG_FILE);
        return;
    }

    char line[512];
    char key[64], value[448];

    while (fgets(line, sizeof(line), config)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        if (sscanf(line, "%63[^=]=%447[^\n]", key, value) == 2) {
            if (strcmp(key, "rotate_cw") == 0) {
                strncpy(cmd_rotate_cw, value, sizeof(cmd_rotate_cw) - 1);
            } else if (strcmp(key, "rotate_ccw") == 0) {
                strncpy(cmd_rotate_ccw, value, sizeof(cmd_rotate_ccw) - 1);
            } else if (strcmp(key, "press") == 0) {
                strncpy(cmd_press, value, sizeof(cmd_press) - 1);
            }
        }
    }

    fclose(config);
    syslog(LOG_INFO, "Configuration loaded");
}

// execute a command
void execute_cmd(const char *cmd) {
    syslog(LOG_INFO, "Executing: %s", cmd);

    // create a fork process
    pid_t pid = fork();

    if (pid == 0) { // child process
        // close file descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        // execute command
        system(cmd);
        exit(0);
    }

}

int main(int argc, char *argv[]) {
    int fd;
    int ret;
    DialPacket packet;
    unsigned char buf[BUFFER_SIZE];
    struct hidraw_devinfo info;
    int daemonize = 0;

    // parse command line options
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--daemon") == 0) {
            daemonize = 1;
        }
    }

    // signal handling
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    // initialize syslog
    openlog("dialedIn", LOG_PID, LOG_USER);

    load_config();

    // open HID device
    fd = open(HIDRAW_DEVICE, O_RDWR);
    if (fd < 0) {
        syslog(LOG_ERR, "Unable to open device %s", HIDRAW_DEVICE);
        closelog();
        return 1;
    }

    // get device info
    ret = ioctl(fd, HIDIOCGRAWINFO, &info);
    if (ret < 0) {
        syslog(LOG_WARNING, "Failed to get device info");
    } else {
        syslog(LOG_INFO, "Device: Vendor=0x%04hx Product=0x%04hx",
               info.vendor, info.product);
    }

    if (daemonize) {
        // create a daemon process
        pid_t pid = fork();
        if (pid < 0) {
            syslog(LOG_ERR, "Failed to fork");
            close(fd);
            exit(EXIT_FAILURE);
        }

        
        if (pid > 0) { // parent
            close(fd);
            exit(EXIT_SUCCESS);
        }

        // create new session
        if (setsid() < 0) {
            syslog(LOG_ERR, "Failed to create session");
            close(fd);
            exit(EXIT_FAILURE);
        }

        chdir("/");

        // close file descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        // open null device for stdin/stdout/stderr
        open("/dev/null", O_RDONLY);
        open("/dev/null", O_WRONLY);
        open("/dev/null", O_WRONLY);
    }

    syslog(LOG_INFO, "DialedIn daemon started");

    // main event loop
    while (!terminate) {
        ret = read(fd, buf, BUFFER_SIZE);

        // no data or error
        if (ret <= 0) {
            if (ret < 0) {
                syslog(LOG_ERR, "Failed to read from device: %m");
                sleep(1);
            }
            continue;
        }

        // check if enough bytes for DialPacket
        if (ret >= sizeof(DialPacket)) {
            memcpy(&packet, buf, sizeof(DialPacket));
            // log data
            syslog(LOG_INFO, "Parsed packet: report_id=%02x button=%02x rotation_hb=%02x rotation_lb=%02x",
                   packet.report_id, packet.button, packet.rotation_hb, packet.rotation_lb);

            // button events
            if (packet.button == BUTTON_DOWN) {
                syslog(LOG_INFO, "Button pressed");
                execute_cmd(cmd_press);
            }

            // rotation events
            if (packet.rotation_hb == ROTATE_CW && packet.rotation_lb == 0x00) {
                syslog(LOG_INFO, "Rotation: Clockwise");
                execute_cmd(cmd_rotate_cw);
            } else if (packet.rotation_hb == ROTATE_CCW && packet.rotation_lb == ROTATE_CCW) {
                syslog(LOG_INFO, "Rotation: Counter-clockwise");
                execute_cmd(cmd_rotate_ccw);
            }
        }
    }

    close(fd);
    syslog(LOG_INFO, "DialedIn daemon stopped");
    closelog();
    return 0;
}
