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
    bool enable = false;
    bool disable = false;
    bool status = false;
    string socketPath;
    int connectOrExit();
};

#endif // CLIENT_H
