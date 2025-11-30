#pragma once

enum class IoOperation
{
    Recv,
    Send,
    Accept,
};

struct OverlappedContext
{
    OVERLAPPED overlapped{};
    IoOperation op = IoOperation::Recv;
    class Session* owner = nullptr;

    WSABUF wsaBuf{};

    // AcceptEx
    SOCKET acceptSocket = INVALID_SOCKET;
    std::array<char, 64> addrBuffer{}; // (로컬+리모트 주소 정보 넣을 버퍼)

    void Reset()
    {
        std::memset(&overlapped, 0, sizeof(overlapped));
        wsaBuf = {};
        op = IoOperation::Recv;
        acceptSocket = INVALID_SOCKET;
        addrBuffer.fill(0);
    }
};
