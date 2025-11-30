#include "pch.h"
#include "SocketUtils.h"

LPFN_ACCEPTEX SocketUtils::AcceptEx = nullptr;
LPFN_GETACCEPTEXSOCKADDRS SocketUtils::GetAcceptExSockaddrs = nullptr;

bool SocketUtils::InitWinsock()
{
	WSADATA wsaData{};
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	return (result == 0);
}

void SocketUtils::CleanupWinsock()
{
	WSACleanup();
}

bool SocketUtils::InitAcceptEx(SOCKET listenSocket)
{
    DWORD bytes = 0;

    // AcceptEx
    GUID guidAcceptEx = WSAID_ACCEPTEX;
    if (WSAIoctl(listenSocket,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guidAcceptEx, sizeof(guidAcceptEx),
        &AcceptEx, sizeof(AcceptEx),
        &bytes, nullptr, nullptr) == SOCKET_ERROR)
    {
        std::cout << "InitAcceptEx: WSAIoctl(AcceptEx) failed: "
            << WSAGetLastError() << "\n";
        return false;
    }

    // GetAcceptExSockaddrs (필수는 아니지만, 있으면 편함)
    GUID guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
    if (WSAIoctl(listenSocket,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guidGetAcceptExSockaddrs, sizeof(guidGetAcceptExSockaddrs),
        &GetAcceptExSockaddrs, sizeof(GetAcceptExSockaddrs),
        &bytes, nullptr, nullptr) == SOCKET_ERROR)
    {
        std::cout << "InitAcceptEx: WSAIoctl(GetAcceptExSockaddrs) failed: "
            << WSAGetLastError() << "\n";
        return false;
    }

    return true;
}

bool SocketUtils::SetNoDelay(SOCKET socket, bool enable)
{
	return SetSockOpt(socket, SOL_SOCKET, TCP_NODELAY, enable);
}

bool SocketUtils::SetLinger(SOCKET socket, bool enable, uint16_t seconds)
{
	LINGER option{};
	option.l_onoff = enable ? 1 : 0;
	option.l_linger = seconds;
	return SetSockOpt(socket, SOL_SOCKET, SO_LINGER, option);
}
