#pragma once

#define WIN32_LEAN_AND_MEAN

// Windows
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <Windows.h>

// STL
#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <queue>
#include <chrono>
#include <format>
#include <span>
#include <functional>
#include <algorithm>

// 기타 유틸
#include <cassert>
#include <cstdint>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")
