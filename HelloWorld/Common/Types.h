#pragma once
#include <cstdint>

using PacketLen = std::uint16_t;
using PacketId = std::uint16_t;

struct PacketHeader
{
    PacketLen length; // 헤더 포함 전체 길이
    PacketId  id;     // 나중에 C2S_MOVE, S2C_CHAT 같은 타입 구분용
};
