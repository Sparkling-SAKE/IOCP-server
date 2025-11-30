#pragma once
#include "JobQueue.h"
#include "OverlappedContext.h"

class Session;

class IocpServer
{
public:
    IocpServer();
    ~IocpServer();

    bool Start(const char* ip, uint16_t port, int workerCount);
    void Stop();

    void OnSessionDisconnected(std::shared_ptr<Session> session);
    void EnqueuePacket(std::shared_ptr<Session> session, const PacketHeader& header, std::string_view body);
    void BroadcastChat(std::shared_ptr<Session> from, std::string_view msg);

private:
    bool InitListenSocket(const char* ip, uint16_t port);
    void PostAccept();
    void PostInitialAccepts(int count);
    void HandleAcceptCompleted(OverlappedContext* ctx);

    void workerLoop();

    void logicLoop();

private:
    SOCKET _listenSocket = INVALID_SOCKET;
    HANDLE _iocp = nullptr;

    std::atomic<bool> _running = false;

    std::vector<std::jthread> _workers{};

    std::mutex _sessionMutex{};
    std::unordered_set<std::shared_ptr<Session>> _sessions{};

    JobQueue _jobQueue{};
    std::jthread _logicThread{};

    static constexpr int32_t ACCEPT_COUNT = 8;
};
