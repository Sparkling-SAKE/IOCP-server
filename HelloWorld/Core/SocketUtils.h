#pragma once

class SocketUtils
{
public:
    static bool InitWinsock();
    static void CleanupWinsock();

    static bool SetNoDelay(SOCKET socket, bool enable);
    static bool SetLinger(SOCKET socket, bool enable, uint16_t seconds);
};

template<typename T>
static inline bool SetSockOpt(SOCKET socket, int32_t level, int32_t optName, T optVal)
{
    return SOCKET_ERROR != ::setsockopt(socket, level, optName, reinterpret_cast<char*>(&optVal), sizeof(T));
}