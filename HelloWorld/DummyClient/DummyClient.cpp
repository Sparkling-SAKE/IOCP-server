#include "pch.h"
#include "PacketUtils.h"
#include <thread>
#include <chrono>

int main()
{
    std::this_thread::sleep_for(std::chrono::seconds(1));

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

    std::string msg = "Hello from client!\n";
    std::string packet = BuildPacket(PacketIds::C2S_CHAT, msg);

    send(s, packet.data(), static_cast<int>(packet.size()), 0);

    char buf[1024]{};
    int recvBytes = recv(s, buf, sizeof(buf), 0);

    if (recvBytes >= static_cast<int>(sizeof(PacketHeader)))
    {
        PacketHeader header{};
        std::memcpy(&header, buf, sizeof(header));

        std::string_view body(buf + sizeof(header),
            recvBytes - static_cast<int>(sizeof(header)));

        std::cout << std::format("recv PacketId={}, body={}\n",
            header.id,
            std::string(body));
    }

    closesocket(s);
    WSACleanup();
}
