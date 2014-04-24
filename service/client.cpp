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

#include <string>
#include <stdio.h>
#include <stdlib.h>
#include "client.h"
#include "comsock.h"

using namespace std;

Client::Client(int argc, char *argv[], string socketPath)
{
    this->socketPath = socketPath;

    enable = false;
    disable = false;
    status = false;

    if(argc >= 2) {
        string arg1(argv[1]);
        if(arg1 == "-e") {
            enable = true;
        } else if(arg1 == "-d") {
            disable = true;
        } else if(arg1 == "-s") {
            status = true;
        }
    }
}

void Client::Run()
{
    if(enable) {
        int g_serverFd = connectOrExit();

        int sent;
        message_t msg;

        msg.type = MSG_ENABLE;
        msg.buffer = NULL;
        msg.length = 0;

        sent = sendMessage(g_serverFd, &msg);
        if(sent == -1) {
          perror("Error");
          closeConnection(g_serverFd);
          exit(EXIT_FAILURE);
        }

        closeConnection(g_serverFd);

    } else if(disable) {
        int g_serverFd = connectOrExit();

        int sent;
        message_t msg;

        msg.type = MSG_DISABLE;
        msg.buffer = NULL;
        msg.length = 0;

        sent = sendMessage(g_serverFd, &msg);
        if(sent == -1) {
          perror("Error");
          closeConnection(g_serverFd);
          exit(EXIT_FAILURE);
        }

        closeConnection(g_serverFd);

    } else if(status) {
        int g_serverFd = connectOrExit();

        int sent;
        message_t msg;

        msg.type = MSG_STATUS;
        msg.buffer = NULL;
        msg.length = 0;

        sent = sendMessage(g_serverFd, &msg);
        if(sent == -1) {
          perror("Error");
          closeConnection(g_serverFd);
          exit(EXIT_FAILURE);
        }

        if(receiveMessage(g_serverFd, &msg) == -1) {
            perror("Error");
            closeConnection(g_serverFd);
            exit(EXIT_FAILURE);
        }

        if(msg.type == MSG_ENABLED)
            printf("1\n");
        else if(msg.type == MSG_DISABLED)
            printf("0\n");
        else {
            perror("Error");
            closeConnection(g_serverFd);
            exit(EXIT_FAILURE);
        }

        closeConnection(g_serverFd);
    }
}

int Client::connectOrExit() {
    int g_serverFd = openConnection((char*)this->socketPath.c_str(), NTRIAL, NSEC);
    if(g_serverFd == -1) {
      perror("No connection to the server.");
      exit(EXIT_FAILURE);
    }
    return g_serverFd;
}
