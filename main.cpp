#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>

using namespace std;

volatile bool active = true;
volatile bool goneActive = false;
volatile bool goneInactive = false;

void sigHandler(int sig)
{
    if(sig == SIGUSR1) {
        active = !active;

        if(active) {
            goneActive = true;
            goneInactive = false;
        } else {
            goneInactive = true;
            goneActive = false;
        }
    }
}

void enableALS(bool enable) {
    if(enable) {
        system("echo '\\_SB.PCI0.LPCB.EC0.TALS 0x1' > sudo tee /proc/acpi/call");
    } else {
        system("echo '\\_SB.PCI0.LPCB.EC0.TALS 0x0' > sudo tee /proc/acpi/call");
    }
}

void showNotification(bool enabled) {
    if(enabled) {
        system("notify-send 'Ambient light sensor enabled'");
    } else {
        system("notify-send 'Ambient light sensor disabled'");
    }
}

void setScreenBacklight(float percent) {
    int xblvalue = (int)(percent * 100);
    char cmd[100];
    snprintf(cmd, 100, "xbacklight -set %d", xblvalue);
    system(cmd);
}

void setKeyboardBacklight(float percent) {
    int value = 0;

    if(percent <= 0.25) value = 0;
    else if(percent <= 0.50) value = 1;
    else if(percent <= 0.75) value = 2;
    else if(percent <= 1.00) value = 3;

    char cmd[150];
    snprintf(cmd, 150, "echo %d | sudo tee /sys/class/leds/asus::kbd_backlight/brightness", value);
    system(cmd);
}

float getAmbientLightPercent() {
    int fd = open("/sys/bus/acpi/devices/ACPI0008:00/ali", O_RDONLY);
    if(fd == -1) {
        fprintf(stderr, "Error opening ali");
        return -1;
    }
    char strals[100];
    int count = read(fd, strals, 100);
    printf("%s", strals);
    strals[count] = '\0';
    close(fd);

    // 0x32 (min illuminance), 0xC8, 0x190, 0x258, 0x320 (max illuminance).
    int als = atoi(strals);

    int percent = 0;

    switch(als) {
    case 0x32:
        percent = 0.10;
        break;
    case 0xC8:
        percent = 0.25;
        break;
    case 0x190:
        percent = 0.50;
        break;
    case 0x258:
        percent = 0.75;
        break;
    case 0x320:
        percent = 1.00;
        break;
    }

    return percent;
}

int main()
{
    struct sigaction sa;
    sa.sa_handler = sigHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    system("notify-send 'Luminosità automatica'");

    while(1) {

        while(!active) {
            if(goneInactive) {
                enableALS(false);
                showNotification(false);
                goneInactive = false;
            }

            if(!goneInactive && !goneActive)
                sleep(60*60*2);

            if(goneActive) {
                enableALS(true);
                showNotification(false);
                goneActive = false;
            }
        }

        float als = getAmbientLightPercent();

        if(als <= 0.10) {
            setScreenBacklight(0.40);
            setKeyboardBacklight(1.0);
        } else if(als <= 0.25) {
            setScreenBacklight(0.60);
            setKeyboardBacklight(0);
        } else if(als <= 0.50) {
            setScreenBacklight(0.75);
            setKeyboardBacklight(0);
        } else if(als <= 0.75) {
            setScreenBacklight(0.90);
            setKeyboardBacklight(0);
        } else if(als <= 1.00) {
            setScreenBacklight(1.00);
            setKeyboardBacklight(0);
        }

        sleep(5);
    }

    return 0;
}
