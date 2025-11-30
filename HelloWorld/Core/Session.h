#pragma once
#include "OverlappedContext.h"
#include "PacketUtils.h"

class IocpServer;

class Session : public std::enable_shared_from_this<Session>
{
public:
    using Ptr = std::shared_ptr<Session>;

    explicit Session(SOCKET sock, HANDLE iocp, IocpServer* owner);
    ~Session();

    bool Initialize();
    void Close();

    bool PostRecv();
    bool PostSend(const char* data, int len);

    void OnRecvCompleted(DWORD bytesTransferred);
    void OnSendCompleted(DWORD bytesTransferred);

    SOCKET GetSocket() const { return _sock; }

private:
    void ProcessPackets();
    void HandlePacket(const PacketHeader& header, std::string_view body);

    void NotifyDisconnected();

private:
    SOCKET _sock = INVALID_SOCKET;
    HANDLE _iocp = nullptr;

    IocpServer* _owner = nullptr;   // 어느 서버 소속인지

    OverlappedContext _recvCtx{};
    OverlappedContext _sendCtx{};

    std::array<char, 4096> _recvBuffer{};
    std::string _recvAccum{};
};
