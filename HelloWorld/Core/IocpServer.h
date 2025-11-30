#pragma once

class Session;

class IocpServer
{
public:
    IocpServer();
    ~IocpServer();

    bool Start(const char* ip, uint16_t port, int workerCount);
    void Stop();

    void OnSessionDisconnected(std::shared_ptr<Session> session);

public:
    void BroadcastChat(std::shared_ptr<Session> from, std::string_view msg);

private:
    bool InitListenSocket(const char* ip, uint16_t port);
    void workerLoop();
    void acceptLoop();

private:
    SOCKET _listenSocket = INVALID_SOCKET;
    HANDLE _iocp = nullptr;

    std::atomic<bool> _running = false;

    std::vector<std::jthread> _workers{};
    std::jthread _acceptThread{};

    std::mutex _sessionMutex{};
    std::unordered_set<std::shared_ptr<Session>> _sessions{};
};
