#pragma once
#include "OverlappedContext.h"

class Session
{
public:
    explicit Session(SOCKET sock, HANDLE iocp);
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

private:
    SOCKET _sock = INVALID_SOCKET;
    HANDLE _iocp = nullptr;

    OverlappedContext _recvCtx{};
    OverlappedContext _sendCtx{};

    std::array<char, 4096> _recvBuffer{};
    std::string _recvAccum{};
};
