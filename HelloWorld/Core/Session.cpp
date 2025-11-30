#include "pch.h"
#include "Session.h"
#include "IocpServer.h"

Session::Session(SOCKET sock, HANDLE iocp, IocpServer* owner)
    : _sock(sock)
    , _iocp(iocp)
    , _owner(owner)
{
}

Session::~Session()
{
    Close();
}

bool Session::Initialize()
{
    // IOCP에 세션 소켓 등록
    HANDLE h = CreateIoCompletionPort(
        reinterpret_cast<HANDLE>(_sock),
        _iocp,
        reinterpret_cast<ULONG_PTR>(this),
        0);

    return (h == _iocp);
}

void Session::Close()
{
    if (_sock != INVALID_SOCKET)
    {
        closesocket(_sock);
        _sock = INVALID_SOCKET;
    }
}

bool Session::PostRecv()
{
    _recvCtx.Reset();
    _recvCtx.op = IoOperation::Recv;
    _recvCtx.owner = this;

    WSABUF buf{};
    buf.buf = _recvBuffer.data();
    buf.len = static_cast<ULONG>(_recvBuffer.size());

    DWORD flags = 0;
    DWORD bytes = 0;

    int result = WSARecv(
        _sock,
        &buf,
        1,
        &bytes,
        &flags,
        reinterpret_cast<LPWSAOVERLAPPED>(&_recvCtx),
        nullptr);

    if (result == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        if (err != WSA_IO_PENDING)
        {
            // 진짜 에러
            return false;
        }
    }

    return true;
}

bool Session::PostSend(const char* data, int len)
{
    _sendCtx.Reset();
    _sendCtx.op = IoOperation::Send;
    _sendCtx.owner = this;

    WSABUF buf{};
    buf.buf = const_cast<char*>(data);
    buf.len = len;

    DWORD bytes = 0;

    int result = WSASend(
        _sock,
        &buf,
        1,
        &bytes,
        0,
        reinterpret_cast<LPWSAOVERLAPPED>(&_sendCtx),
        nullptr);

    if (result == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        if (err != WSA_IO_PENDING)
        {
            return false;
        }
    }

    return true;
}

void Session::OnRecvCompleted(DWORD bytesTransferred)
{
    std::cout << std::format("recv {} bytes \n", bytesTransferred);

    if (bytesTransferred == 0)
    {
        // 클라이언트 종료
        std::cout << "Client disconnected!!" << std::endl;
        Close();
        NotifyDisconnected();
        return;
    }

    _recvAccum.append(_recvBuffer.data(), bytesTransferred);

    // 누적된 데이터에서 패킷 단위로 잘라 처리
    ProcessPackets();

    // 그리고 다시 다음 recv 예약
    PostRecv();
}

void Session::OnSendCompleted(DWORD bytesTransferred)
{
    std::cout << std::format("send {} bytes \n", bytesTransferred);
    // 간단한 에코 서버라 여기서 할 일은 없음.
    // 나중에 send 큐 관리/부분 전송 처리 여기 들어갈 예정.
}

void Session::ProcessPackets()
{
    constexpr size_t HEADER_SIZE = sizeof(PacketHeader);

    while (true)
    {
        if (_recvAccum.size() < HEADER_SIZE)
            return;

        PacketHeader header{};
        std::memcpy(&header, _recvAccum.data(), HEADER_SIZE);

        if (_recvAccum.size() < header.length)
            return;

        const size_t packetSize = header.length;
        std::string packetData = _recvAccum.substr(0, packetSize);
        _recvAccum.erase(0, packetSize);

        const char* bodyPtr = packetData.data() + HEADER_SIZE;
        const size_t bodySize = packetSize - HEADER_SIZE;

        std::string_view body(bodyPtr, bodySize);

        HandlePacket(header, body);
    }
}

void Session::HandlePacket(const PacketHeader& header, std::string_view body)
{
    const auto pid = static_cast<PacketIds>(header.id);

    switch (pid)
    {
    case PacketIds::C2S_ECHO:
    {
        std::string packet = BuildPacket(PacketIds::S2C_ECHO, body);
        PostSend(packet.data(), static_cast<int32_t>(packet.size()));
    }
    break;

    case PacketIds::C2S_CHAT:
    {
        // 일단 서버 콘솔에 찍어보기
        std::cout << std::format("[CHAT] {} bytes: {}\n",
            body.size(),
            std::string(body));

        // 나중에: 전체 유저에게 브로드캐스트
        if (_owner)
        {
            auto self = shared_from_this();
            _owner->BroadcastChat(self, body);
        }
    }
    break;

    default:
        std::cout << std::format("Unknown PacketId: {}\n", header.id);
        break;
    }
}

void Session::NotifyDisconnected()
{
    if (_owner)
    {
        _owner->OnSessionDisconnected(shared_from_this());
    }
}
