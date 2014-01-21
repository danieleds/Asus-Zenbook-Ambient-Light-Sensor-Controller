/*
   Copyright 2013-2014 Daniele Di Sarli

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

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
#include "client.h"

using namespace std;

void *IPCHandler(void *arg);
void *clientHandler(void *arg);

volatile bool active = false;

int g_socket = -1;

const string SOCKET_PATH = "/var/run/als-controller.socket";
char* C_SOCKET_PATH = (char*)SOCKET_PATH.c_str();

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t start = PTHREAD_COND_INITIALIZER;

/*void sigHandler(int sig)
{
    if(sig == SIGUSR1) {
        ..
    }
}*/

void enableALS(bool enable) {
    int fd = open("/sys/bus/acpi/devices/ACPI0008:00/ali", O_RDONLY);
    if(fd == -1) {
        fprintf(stderr, "Error opening /proc/acpi/call");
    }

    string buf;
    if(enable) {
        buf = "\\_SB.PCI0.LPCB.EC0.TALS 0x1";
    } else {
        buf = "\\_SB.PCI0.LPCB.EC0.TALS 0x0";
    }

    write(fd, buf.c_str(), buf.length() + 1);

    close(fd);

    printf("ALS enabled = %d\n", enable);
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
    //printf("\"%s\"\n", strals);
    printf("Illuminance detected: %d\n", als);

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

    if(argc > 1) {
        Client c = Client(argc, argv, SOCKET_PATH);
        c.Run();
        exit(EXIT_SUCCESS);
    }

    pthread_t thread_id;
    int err = pthread_create(&thread_id, NULL, IPCHandler, NULL);
    if(err != 0) {
        perror("Creating thread");
        exit(EXIT_FAILURE);
    }

    while(1) {

        pthread_mutex_lock(&mtx);
        while(!active) {
            pthread_cond_wait(&start, &mtx);
        }
        pthread_mutex_unlock(&mtx);

        float als = getAmbientLightPercent();
        printf("Illuminance percent: %f\n", als);
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
    unlink(C_SOCKET_PATH);
    g_socket = createServerChannel(C_SOCKET_PATH);
    if(g_socket == -1) {
      perror("Creating socket");
      exit(EXIT_FAILURE);
    }

    // Permessi 777 sulla socket
    if(chmod(C_SOCKET_PATH, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) != 0) {
      closeServerChannel(C_SOCKET_PATH, g_socket);
      return NULL;
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
        pthread_mutex_lock(&mtx);
        active = true;
        pthread_mutex_unlock(&mtx);
        pthread_cond_signal(&start);
    } else if(msg.type == MSG_DISABLE) {
        pthread_mutex_lock(&mtx);
        active = false;
        pthread_mutex_unlock(&mtx);
        enableALS(false);
    } else if(msg.type == MSG_STATUS) {
        bool status = false;
        pthread_mutex_lock(&mtx);
        status = active;
        pthread_mutex_unlock(&mtx);

        int sent;
        message_t msg;

        msg.type = status ? MSG_ENABLED : MSG_DISABLED;
        msg.buffer = NULL;
        msg.length = 0;

        sent = sendMessage(client, &msg);
        if(sent == -1) {
          perror("Error sending reply.");
        }
    }

    return NULL;
}
