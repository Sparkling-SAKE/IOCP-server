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

    if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(_listenSocket), _iocp, 0, 0) == nullptr)
        return false;

    _running = true;

    PostInitialAccepts(ACCEPT_COUNT);

    // 워커 스레드 시작
    for (int i = 0; i < workerCount; ++i)
    {
        _workers.emplace_back([this] { workerLoop(); });
    }

    // 로직 스레드
    _logicThread = std::jthread([this] { logicLoop(); });

    return true;
}

void IocpServer::Stop()
{
    if (!_running.exchange(false))
        return;

    _jobQueue.Stop();

    for (size_t i = 0; i < _workers.size(); ++i)
    {
        PostQueuedCompletionStatus(_iocp, 0, 0, nullptr);
    }

    for (auto& th : _workers)
    {
        if (th.joinable())
            th.join();
    }
    _workers.clear();

    if (_logicThread.joinable())
        _logicThread.join();

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

void IocpServer::EnqueuePacket(std::shared_ptr<Session> session, const PacketHeader& header, std::string_view body)
{
    PacketJob job{};
    job.session = std::move(session);
    job.header = header;
    job.body.assign(body.begin(), body.end());

    _jobQueue.Push(std::move(job));
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
        std::cout << "packet data : " << packet.data() + 4 << std::endl;
        std::cout << "packet size : " << packet.size() << std::endl;
    }
}

bool IocpServer::InitListenSocket(const char* ip, uint16_t port)
{
    _listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (_listenSocket == INVALID_SOCKET)
        return false;

    if (!SocketUtils::InitAcceptEx(_listenSocket))
    {
        std::cout << "InitAcceptEx failed\n";
        return false;
    }

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

void IocpServer::PostAccept()
{
    auto* ctx = new OverlappedContext{};
    ctx->Reset();
    ctx->op = IoOperation::Accept;

    ctx->acceptSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
        nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (ctx->acceptSocket == INVALID_SOCKET)
    {
        std::cout << "PostAccept: WSASocket failed: "
            << WSAGetLastError() << "\n";
        delete ctx;
        return;
    }

    DWORD bytesReceived = 0;

    BOOL ok = SocketUtils::AcceptEx(
        _listenSocket,
        ctx->acceptSocket,
        ctx->addrBuffer.data(),
        0,  // 초기에 payload는 안 받는다 (순수 accept 전용)
        sizeof(sockaddr_in) + 16,
        sizeof(sockaddr_in) + 16,
        &bytesReceived,
        &ctx->overlapped);

    if (!ok)
    {
        int err = WSAGetLastError();
        if (err != WSA_IO_PENDING)
        {
            std::cout << "PostAccept: AcceptEx failed: " << err << "\n";
            closesocket(ctx->acceptSocket);
            delete ctx;
            return;
        }
    }
}

void IocpServer::PostInitialAccepts(int count)
{
    for (int i = 0; i < count; ++i)
        PostAccept();
}

void IocpServer::HandleAcceptCompleted(OverlappedContext* ctx)
{
    // 새 소켓에 대해 SO_UPDATE_ACCEPT_CONTEXT 설정
    if (setsockopt(ctx->acceptSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, reinterpret_cast<const char*>(&_listenSocket), sizeof(_listenSocket)) == SOCKET_ERROR)
    {
        std::cout << "HandleAcceptCompleted: SO_UPDATE_ACCEPT_CONTEXT failed: "
            << WSAGetLastError() << "\n";
        closesocket(ctx->acceptSocket);
        delete ctx;

        // 다음 Accept를 다시 걸어준다
        PostAccept();
        return;
    }

    // Session 생성
    auto session = std::make_shared<Session>(ctx->acceptSocket, _iocp, this);

    if (session->Initialize() == false)
    {
        std::cout << "Session Initialize Fail!!" << std::endl;
        closesocket(ctx->acceptSocket);
        delete ctx;

        // 다음 Accept를 다시 걸어준다
        PostAccept();
        return;
    }

    {
        std::scoped_lock lock(_sessionMutex);
        _sessions.insert(session);
        std::cout << "Session added. count=" << _sessions.size() << "\n";
    }

    // 소켓을 IOCP에 등록 (key = Session*)
    HANDLE h = CreateIoCompletionPort(
        reinterpret_cast<HANDLE>(ctx->acceptSocket),
        _iocp,
        reinterpret_cast<ULONG_PTR>(session.get()),
        0);

    if (h == nullptr)
    {
        std::cout << "CreateIoCompletionPort for client failed: "
            << GetLastError() << "\n";
        session->Close();
        OnSessionDisconnected(session);
        delete ctx;
        PostAccept();
        return;
    }

    // 첫 Recv
    if (!session->PostRecv())
    {
        session->Close();
        OnSessionDisconnected(session);
        delete ctx;
        PostAccept();
        return;
    }

    // Accept 컨텍스트는 여기서 사용 끝
    delete ctx;

    // 다음 클라를 위해 또 AcceptEx를 걸어둔다
    PostAccept();
}

void IocpServer::logicLoop()
{
    // thread_local int gLogicThreadId = 0;

    PacketJob job{};
    while (_running)
    {
        if (!_jobQueue.Pop(job))
            break; // Stop() 호출 후 큐 비면 빠져나옴

        // 패킷 디스패치
        const auto pid = static_cast<PacketIds>(job.header.id);

        switch (pid)
        {
        case PacketIds::C2S_ECHO:
        {
            std::string packet = BuildPacket(PacketIds::S2C_ECHO, job.body);
            job.session->PostSend(packet.data(), static_cast<int>(packet.size()));
        }
        break;

        case PacketIds::C2S_CHAT:
        {
            // 여기서 BroadcastChat 호출 (이제 로직 스레드에서만 돌게 됨)
            std::cout << "BroadCast!!!!" << std::endl;
            BroadcastChat(job.session, job.body);
        }
        break;

        default:
        {
            std::cout << std::format("Unknown PacketId in logicLoop: {}\n",
                job.header.id);
        }
        break;
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

        if (!ok || ov == nullptr)
        {
            // 에러 or 종료. 진짜 서버라면 여기서 로깅/정리 필요.
             if (!_running)
                break;
             std::cout << "GQCS error: " << GetLastError() << "\n";
             continue;
        }

        auto* ctx = reinterpret_cast<OverlappedContext*>(ov);

        switch (ctx->op)
        {
        case IoOperation::Accept:
        {
            HandleAcceptCompleted(ctx);
        }
        break;
        case IoOperation::Recv:
        {
            auto* session = reinterpret_cast<Session*>(key);
            session->OnRecvCompleted(bytes);
        }
        break;
        case IoOperation::Send:
        {
            auto* session = reinterpret_cast<Session*>(key);
            session->OnSendCompleted(bytes);
        }
        break;
        }

    }
}
