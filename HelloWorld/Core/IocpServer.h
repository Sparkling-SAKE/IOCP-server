#pragma once

class IocpServer
{
public:
    IocpServer();
    ~IocpServer();

    bool Start(const char* ip, uint16_t port, int workerCount);
    void Stop();

private:
    bool InitListenSocket(const char* ip, uint16_t port);
    void workerLoop();
    void acceptLoop();

private:
    SOCKET _listenSocket = INVALID_SOCKET;
    HANDLE _iocp = nullptr;

    std::atomic<bool> _running = false;

    std::vector<std::jthread> _workers = {};
    std::jthread _acceptThread = {};
};
