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

    void Reset()
    {
        std::memset(&overlapped, 0, sizeof(overlapped));
    }
};
