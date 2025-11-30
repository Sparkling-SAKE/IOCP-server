#pragma once

#include <string>
#include <memory>
#include "Types.h"

class Session;

struct PacketJob
{
    std::shared_ptr<Session> session{};
    PacketHeader header{};
    std::string body{};
};
