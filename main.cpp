#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <pthread.h>
#include "comsock.h"

using namespace std;

void *IPCHandler(void *arg);
void *clientHandler(void *arg);

volatile bool active = true;
volatile bool goneActive = false;
volatile bool goneInactive = false;

#define SOCKET_PATH "/var/run/als-controller.socket"

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
    int fd = open("/sys/bus/acpi/devices/ACPI0008:00/ali", O_RDONLY);
    if(fd == -1) {
        fprintf(stderr, "Error opening /proc/acpi/call");
    }

    char *buf;
    if(enable) {
        buf = "\\_SB.PCI0.LPCB.EC0.TALS 0x1";
    } else {
        buf = "\\_SB.PCI0.LPCB.EC0.TALS 0x0";
    }

    write(fd, buf, strlen(buf) + 1);

    close(fd);

    printf("ALS enabled = %d\n", enable);
}

void showNotification(bool enabled) {
    if(enabled) {
        system("notify-send 'Ambient light sensor enabled' -i images/active.svg");
    } else {
        system("notify-send 'Ambient light sensor disabled' -i images/inactive.svg");
    }
}

void setScreenBacklight(int percent) {
    char cmd[100];
    snprintf(cmd, 100, "xbacklight -set %d", percent);
    system(cmd);
}

void setKeyboardBacklight(int percent) {
    int value = 0;

    if(percent <= 25) value = 0;
    else if(percent <= 50) value = 1;
    else if(percent <= 75) value = 2;
    else if(percent <= 100) value = 3;

    char cmd[150];
    snprintf(cmd, 150, "echo %d | tee /sys/class/leds/asus::kbd_backlight/brightness", value);
    system(cmd);
}

int getAmbientLightPercent() {
    int fd = open("/sys/bus/acpi/devices/ACPI0008:00/ali", O_RDONLY);
    if(fd == -1) {
        fprintf(stderr, "Error opening ali");
        return -1;
    }
    char strals[100];
    int count = read(fd, strals, 100);
    strals[count] = '\0';
    close(fd);

    // 0x32 (min illuminance), 0xC8, 0x190, 0x258, 0x320 (max illuminance).
    int als = atoi(strals);
    printf("\"%s\"\n", strals);
    printf("%d\n", als);

    float percent = 0;

    switch(als) {
    case 0x32:
        percent = 10;
        break;
    case 0xC8:
        percent = 25;
        break;
    case 0x190:
        percent = 50;
        break;
    case 0x258:
        percent = 75;
        break;
    case 0x320:
        percent = 100;
        break;
    }

    return percent;
}

int main(int argc, char *argv[])
{
    /*struct sigaction sa;
    sa.sa_handler = sigHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);*/

    if(argc == 2) {
        int g_serverFd = openConnection(SOCKET_PATH, NTRIAL, NSEC);
        if(g_serverFd == -1) {
          perror("Connessione al server");
          exit(EXIT_FAILURE);
        }

        int sent;
        message_t msg;

        msg.type = MSG_ENABLE;
        msg.buffer = NULL;
        msg.length = 0;

        sent = sendMessage(g_serverFd, &msg);
        if(sent == -1) {
          perror("Registrazione");
          //return FALSE;
        } else {
          // ok
        }

        closeConnection(g_serverFd);

        return 0;
    } else if(argc == 3) {
        int g_serverFd = openConnection(SOCKET_PATH, NTRIAL, NSEC);
        if(g_serverFd == -1) {
          perror("Connessione al server");
          exit(EXIT_FAILURE);
        }

        int sent;
        message_t msg;

        msg.type = MSG_DISABLE;
        msg.buffer = NULL;
        msg.length = 0;

        sent = sendMessage(g_serverFd, &msg);
        if(sent == -1) {
          perror("Registrazione");
          //return FALSE;
        } else {
          // ok
        }

        closeConnection(g_serverFd);

        return 0;
    }

    enableALS(true);

    pthread_t thread_id;
    int err = pthread_create(&thread_id, NULL, IPCHandler, NULL);
    if(err != 0) {
        perror("Creating thread");
        exit(EXIT_FAILURE);
    }

    showNotification(true);

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
                showNotification(true);
                goneActive = false;
            }
        }

        float als = getAmbientLightPercent();
        printf("%f\%\n", als);
        if(als <= 10) {
            setScreenBacklight(40);
            setKeyboardBacklight(100);
        } else if(als <= 25) {
            setScreenBacklight(60);
            setKeyboardBacklight(0);
        } else if(als <= 50) {
            setScreenBacklight(75);
            setKeyboardBacklight(0);
        } else if(als <= 75) {
            setScreenBacklight(90);
            setKeyboardBacklight(0);
        } else if(als <= 100) {
            setScreenBacklight(100);
            setKeyboardBacklight(0);
        }

        sleep(3);
    }

    return 0;
}

void *IPCHandler(void *arg)
{
    int g_socket = createServerChannel(SOCKET_PATH);
    if(g_socket == -1) {
      perror("Creating socket");
      exit(EXIT_FAILURE);
    }

    while(1)
    {
        int client = acceptConnection(g_socket);
        if(client == -1) {
            perror("Incoming connection");
        } else {
            pthread_t thread_id;
            int err = pthread_create(&thread_id, NULL, clientHandler, (void *)(size_t)client);
            if(err != 0) {
                perror("Creating thread");
                exit(EXIT_FAILURE);
            }
        }
    }
}

void *clientHandler(void *arg)
{
    int client = (int)(size_t)arg;
    message_t msg;

    if(receiveMessage(client, &msg) == -1)
        return NULL;

    if(msg.type == MSG_ENABLE) {
        enableALS(true);
    } else if(msg.type == MSG_DISABLE) {
        enableALS(false);
    }

    return NULL;
}
