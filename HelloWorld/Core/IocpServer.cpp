#include "pch.h"
#include "IocpServer.h"
#include "Session.h"
#include "SocketUtils.h"
#include "PacketUtils.h"

IocpServer::IocpServer()
{
}

IocpServer::~IocpServer()
{
    Stop();
}

bool IocpServer::Start(const char* ip, uint16_t port, int workerCount)
{
    if (!SocketUtils::InitWinsock())
        return false;

    if (!InitListenSocket(ip, port))
        return false;

    _iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (_iocp == nullptr)
        return false;

    _running = true;

    // 워커 스레드 시작
    for (int i = 0; i < workerCount; ++i)
    {
        _workers.emplace_back([this] { workerLoop(); });
    }

    // accept 스레드 시작 (blocking accept)
    _acceptThread = std::jthread([this] { acceptLoop(); });

    return true;
}

void IocpServer::Stop()
{
    if (!_running.exchange(false))
        return;

    // 워커들 깨우기 위해 dummy completion post 해도 됨 (여기선 생략 가능)

    for (auto& th : _workers)
    {
        if (th.joinable())
            th.join();
    }
    _workers.clear();

    if (_acceptThread.joinable())
        _acceptThread.join();

    if (_listenSocket != INVALID_SOCKET)
    {
        closesocket(_listenSocket);
        _listenSocket = INVALID_SOCKET;
    }

    if (_iocp)
    {
        CloseHandle(_iocp);
        _iocp = nullptr;
    }

    SocketUtils::CleanupWinsock();
}

void IocpServer::OnSessionDisconnected(std::shared_ptr<Session> session)
{
    std::scoped_lock lock(_sessionMutex);
    auto it = _sessions.find(session);
    if (it != _sessions.end())
    {
        _sessions.erase(it);
        std::cout << std::format("Session removed. current session count = {}\n", _sessions.size());
    }
}

void IocpServer::BroadcastChat(std::shared_ptr<Session> from, std::string_view msg)
{
    std::string packet = BuildPacket(PacketIds::S2C_CHAT, msg);

    std::scoped_lock lock(_sessionMutex);

    for (auto& session : _sessions)
    {
        // 필요하면 보내는 본인은 스킵할 수도 있음
        // if (session == from) continue;

        session->PostSend(packet.data(), static_cast<int>(packet.size()));
    }
}

bool IocpServer::InitListenSocket(const char* ip, uint16_t port)
{
    _listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (_listenSocket == INVALID_SOCKET)
        return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (bind(_listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
        return false;

    if (listen(_listenSocket, SOMAXCONN) != 0)
        return false;

    return true;
}

void IocpServer::acceptLoop()
{
    while (_running)
    {
        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);

        SOCKET clientSock = accept(_listenSocket, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
        if (clientSock == INVALID_SOCKET)
        {
            int err = WSAGetLastError();
            if (!_running)
                break;
            std::cout << std::format("accept FAILED: {}\n", err);
            continue;
        }

        std::cout << std::format("accept OK: new client\n");

        // shared_ptr 세션 생성
        auto session = std::make_shared<Session>(clientSock, _iocp, this);

        // 먼저 컨테이너에 등록
        {
            std::scoped_lock lock(_sessionMutex);
            _sessions.insert(session);
            std::cout << std::format("Session added. current session count = {}\n", _sessions.size());
        }

        if (!session->Initialize())
        {
            std::cout << std::format("Session Initialize FAILED\n");
            session->Close();
            OnSessionDisconnected(session);
            continue;
        }

        if (!session->PostRecv())
        {
            std::cout << std::format("Session PostRecv FAILED\n");
            session->Close();
            OnSessionDisconnected(session);
            continue;
        }
    }
}

void IocpServer::workerLoop()
{
    while (_running)
    {
        DWORD bytes = 0;
        ULONG_PTR key = 0;
        OVERLAPPED* ov = nullptr;

        BOOL ok = GetQueuedCompletionStatus(
            _iocp,
            &bytes,
            &key,
            &ov,
            INFINITE);

        if (!_running)
            break;

        if (!ok || ov == nullptr || key == 0)
        {
            // 에러 or 종료. 진짜 서버라면 여기서 로깅/정리 필요.
            continue;
        }

        auto* session = reinterpret_cast<Session*>(key);
        auto* ctx = reinterpret_cast<OverlappedContext*>(ov);

        switch (ctx->op)
        {
        case IoOperation::Recv:
            session->OnRecvCompleted(bytes);
            break;
        case IoOperation::Send:
            session->OnSendCompleted(bytes);
            break;
        case IoOperation::Accept:
            // 지금은 안 씀 (나중에 AcceptEx에서 사용)
            break;
        }
    }
}
