#pragma once
#include "Types.h"

inline std::string BuildEchoPacket(std::string_view msg)
{
    PacketHeader header{};
    header.length = static_cast<PacketLen>(sizeof(PacketHeader) + msg.size());
    header.id = 1; // ECHO 라고 치자

    std::string packet;
    packet.resize(header.length);

    std::memcpy(packet.data(), &header, sizeof(header));
    std::memcpy(packet.data() + sizeof(header), msg.data(), msg.size());

    return packet;
}
