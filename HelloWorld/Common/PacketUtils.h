#pragma once
#include <string>
#include <string_view>
#include <cstring> // std::memcpy
#include "Types.h"

inline std::string BuildPacket(PacketIds id, std::string_view body)
{
    PacketHeader header{};
    header.length = static_cast<PacketLen>(sizeof(PacketHeader) + body.size());
    header.id = static_cast<PacketId>(id);

    std::string packet;
    packet.resize(header.length);

    std::memcpy(packet.data(), &header, sizeof(header));
    if (!body.empty())
    {
        std::memcpy(packet.data() + sizeof(header), body.data(), body.size());
    }

    return packet;
}
