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
#include <syslog.h>
#include <errno.h>
#include "comsock.h"
#include "client.h"
#include <errno.h>
#include <err.h>
#include <bsd/libutil.h>

using namespace std;

void logServerExit(int __status, int __pri, const char *fmt);
void startDaemon();
void enableALS(bool enable);
void *IPCHandler(void *arg);
void *clientHandler(void *arg);
int getLidStatus();

volatile bool active = false;

int g_socket = -1;

const string SOCKET_PATH = "/var/run/als-controller.socket";
char* C_SOCKET_PATH = (char*)SOCKET_PATH.c_str();

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t start = PTHREAD_COND_INITIALIZER;

/** Signal mask */
static sigset_t g_sigset;


void *sigManager(void *arg) {
  int signum;

  while(1) {
    sigwait(&g_sigset, &signum);

    if(signum == SIGINT || signum == SIGTERM) {
        logServerExit(EXIT_SUCCESS, LOG_INFO, "Terminated.");
    }
  }

  return NULL;
}

void logServerExit(int __status, int __pri, const char *fmt) {
    closeServerChannel(C_SOCKET_PATH, g_socket);
    enableALS(false);
    syslog(__pri, "%s", fmt);
    if(__status != EXIT_SUCCESS)
        syslog(LOG_INFO, "Terminated.");
    closelog();
    exit(__status);
}

void writeAttribute(string path, string data) {
    int fd = open(path.c_str(), O_WRONLY);
    if(fd == -1) {
        string msg = "Error opening " + path;
        logServerExit(EXIT_FAILURE, LOG_CRIT, msg.c_str());
    }
    if(write(fd, data.c_str(), data.length() + 1) == -1) {
        string msg = "Error writing to " + path;
        logServerExit(EXIT_FAILURE, LOG_CRIT, msg.c_str());
    }

    close(fd);
}

void enableALS(bool enable) {
    if(enable) {
        writeAttribute("/sys/bus/acpi/devices/ACPI0008:00/enable", "1");
    } else {
        writeAttribute("/sys/bus/acpi/devices/ACPI0008:00/enable", "0");
    }

    if (enable)
        syslog(LOG_INFO, "ALS enabled");
    else
        syslog(LOG_INFO, "ALS disabled");
}

void setScreenBacklight(int percent) {
    int ret = 0;
    char cmd[100];
    snprintf(cmd, 100, "xbacklight -set %d", percent);
    ret = system(cmd);
    if (ret < 0) {
        syslog(LOG_ERR, "Failed to set screen backlight.");
    }
}

void setKeyboardBacklight(int percent) {
    int value = 0;
    int ret = 0;
    if(percent <= 25) value = 0;
    else if(percent <= 50) value = 1;
    else if(percent <= 75) value = 2;
    else if(percent <= 100) value = 3;

    char cmd[150];
    snprintf(cmd, 150, "echo %d | tee /sys/class/leds/asus::kbd_backlight/brightness", value);
    ret = system(cmd);
    if (ret < 0) {
        syslog(LOG_ERR, "Failed to set keyboard backlight.");
    }
}

/**
 * @brief getLidStatus
 * @return 1 if opened, 0 if closed, -1 on error, -2 if unknown
 */
int getLidStatus() {
    int fd = open("/proc/acpi/button/lid/LID/state", O_RDONLY);
    if(fd == -1) {
        syslog(LOG_ERR, "Error opening /proc/acpi/button/lid/LID/state");
        return -1;
    } else {
        char str[100];
        int count = read(fd, str, 100);
        str[count] = '\0';
        close(fd);
        string s = string(str);
        if(s.find("open") != string::npos) {
            return 1;
        } else if(s.find("closed") != string::npos) {
            return 0;
        } else {
            return -2;
        }
    }
}

int getAmbientLightPercent() {
    int fd = open("/sys/bus/acpi/devices/ACPI0008:00/ali", O_RDONLY);
    if(fd == -1) {
        logServerExit(EXIT_FAILURE, LOG_CRIT, "Error opening /sys/bus/acpi/devices/ACPI0008:00/ali");
    }
    char strals[100];
    int count = read(fd, strals, 100);
    strals[count] = '\0';
    close(fd);

    // 0x32 (min illuminance), 0xC8, 0x190, 0x258, 0x320 (max illuminance).
    int als = atoi(strals);
    //printf("\"%s\"\n", strals);
    //printf("Illuminance detected: %d\n", als);

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
    if(argc > 1) {
        Client c = Client(argc, argv, SOCKET_PATH);
        c.Run();
        exit(EXIT_SUCCESS);
    }

    struct pidfh *pfh;
    pid_t otherpid;
    pfh = pidfile_open("/var/run/als-controller.pid", 0600, &otherpid);
    if (pfh == NULL) {
        if (errno == EEXIST) {
                    errx(EXIT_FAILURE, "Daemon already running, pid: %jd.",
                        (intmax_t)otherpid);
        }
        /* If we cannot create pidfile from other reasons, only warn. */
        warn("Cannot open or create pidfile");
    }

    if (daemon(0, 0) == -1) {
        warn("Cannot daemonize");
        pidfile_remove(pfh);
        exit(EXIT_FAILURE);
    }

    pidfile_write(pfh);

    /* Change the file mode mask */
    umask(0);

    /* Open the log file */
    openlog("als-controller", LOG_PID, LOG_DAEMON);

    startDaemon();
    pidfile_remove(pfh);
    return 0;
}

void startDaemon()
{
    syslog(LOG_NOTICE, "Started.");

    /* Signals blocked in all threads.
     * sigManager is the only thread responsible to catch signals */
    sigemptyset(&g_sigset);
    sigaddset(&g_sigset, SIGINT);
    sigaddset(&g_sigset, SIGTERM);
    if(pthread_sigmask(SIG_SETMASK, &g_sigset, NULL) != 0) {
        logServerExit(EXIT_FAILURE, LOG_CRIT, "Sigmask error.");
    }

    pthread_t sigthread;
    if(pthread_create(&sigthread, NULL, sigManager, NULL) != 0) {
        logServerExit(EXIT_FAILURE, LOG_CRIT, "Creating thread.");
    }


    pthread_t thread_id;
    int err = pthread_create(&thread_id, NULL, IPCHandler, NULL);
    if(err != 0) {
        syslog(LOG_CRIT, "Cannot create thread");
        exit(EXIT_FAILURE);
    }

    while(1) {

        pthread_mutex_lock(&mtx);
        while(!active) {
            pthread_cond_wait(&start, &mtx);
        }
        pthread_mutex_unlock(&mtx);

        if(getLidStatus() == 0) {
            setKeyboardBacklight(0);
        } else {

            float als = getAmbientLightPercent();
            //printf("Illuminance percent: %f\n", als);

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
        }

        sleep(3);
    }

    logServerExit(EXIT_SUCCESS, LOG_NOTICE, "Terminated.");
}

void *IPCHandler(void *arg)
{
    unlink(C_SOCKET_PATH);
    g_socket = createServerChannel(C_SOCKET_PATH);
    if(g_socket == -1) {
      logServerExit(EXIT_FAILURE, LOG_CRIT, "Error creating socket");
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
            syslog(LOG_ERR, "Error accepting client connection.");
        } else {
            pthread_t thread_id;
            int err = pthread_create(&thread_id, NULL, clientHandler, (void *)(size_t)client);
            if(err != 0) {
                logServerExit(EXIT_FAILURE, LOG_CRIT, "Error creating client thread.");
            }
        }
    }
}

void *clientHandler(void *arg)
{
    int client = (int)(size_t)arg;
    message_t msg;

    if(receiveMessage(client, &msg) == -1) {
        syslog(LOG_ERR, "Error receiving message from client.");
        return NULL;
    }

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
            syslog(LOG_ERR, "Error sending reply to client.");
            return NULL;
        }
    }

    return NULL;
}
