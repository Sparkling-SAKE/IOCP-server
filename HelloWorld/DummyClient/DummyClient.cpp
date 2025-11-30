#include "pch.h"
#include "PacketUtils.h"
#include <thread>
#include <chrono>

int main()
{
    std::this_thread::sleep_for(std::chrono::seconds(3));
    WSADATA wsaData{};
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
        return 1;

    SOCKET s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, 0);
    if (s == INVALID_SOCKET)
    {
        std::cout << std::format("socket failed: {}\n", WSAGetLastError());
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(7777);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        std::cout << std::format("connect failed: {}\n", WSAGetLastError());
        closesocket(s);
        return 1;
    }

    std::cout << std::format("connected to server\n");

    std::string msg = "hello IOCP\n";
    std::string packet = BuildEchoPacket(msg);

    int sent = send(s, packet.data(), static_cast<int>(packet.size()), 0);
    std::cout << std::format("sent {} bytes\n", sent);

    char buf[1024]{};
    int recvBytes = recv(s, buf, sizeof(buf), 0);
    std::cout << std::format("recv {} bytes\n", recvBytes);

    // 헤더 까보고 body 출력
    if (recvBytes >= sizeof(PacketHeader))
    {
        PacketHeader header{};
        std::memcpy(&header, buf, sizeof(header));
        std::string body(buf + sizeof(header), buf + recvBytes);

        std::cout << std::format("echo header.length={}, id={}\n", header.length, header.id);
        std::cout << std::format("echo body: {}\n", body);
    }

    closesocket(s);
    WSACleanup();
}
