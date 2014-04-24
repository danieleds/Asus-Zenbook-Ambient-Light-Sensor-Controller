#ifndef CLIENT_H
#define CLIENT_H

#include <string>
using namespace std;

class Client
{
public:
    Client(int argc, char *argv[], string socketPath);
    void Run();

private:
    bool enable;
    bool disable;
    bool status;
    string socketPath;
    int connectOrExit();
};

#endif // CLIENT_H
