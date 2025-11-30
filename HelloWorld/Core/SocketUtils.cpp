#include "pch.h"
#include "SocketUtils.h"

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
