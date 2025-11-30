#include "pch.h"
#include "Session.h"
#include "PacketUtils.h"

Session::Session(SOCKET sock, HANDLE iocp)
    : _sock(sock), _iocp(iocp)
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
    // 패킷 헤더 크기
    constexpr size_t HEADER_SIZE = sizeof(PacketHeader);

    // 누적 버퍼에 최소 헤더만큼은 있어야 뭔가 할 수 있음
    while (true)
    {
        if (_recvAccum.size() < HEADER_SIZE)
            return;

        // 헤더 읽기
        PacketHeader header{};
        std::memcpy(&header, _recvAccum.data(), HEADER_SIZE);

        // 전체 패킷 길이가 누적 버퍼보다 크면 → 아직 덜 온 것
        if (_recvAccum.size() < header.length)
            return;

        // 패킷 전체 범위 [0, header.length)
        std::string packetData = _recvAccum.substr(0, header.length);

        // 누적 버퍼에서 소비
        _recvAccum.erase(0, header.length);

        // 여기서 패킷 처리
        const char* body = packetData.data() + HEADER_SIZE;
        size_t bodySize = header.length - HEADER_SIZE;

        // 일단은 "패킷 단위 에코"로 구현
        PostSend(packetData.data(), static_cast<int>(packetData.size()));

        // 나중에는 header.id 보고 분기해서:
        // switch (header.id) { case C2S_CHAT: ..., case C2S_MOVE: ... }
    }
}
