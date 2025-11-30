#include "pch.h"
#include "IocpServer.h"

int main()
{
    IocpServer server;
    const int workerCount = static_cast<int>(std::thread::hardware_concurrency()) + 2;

    if (!server.Start("0.0.0.0", 7777, workerCount))
    {
        std::cout << "Server start failed" << std::endl;
        return 1;
    }

    std::cout << std::format("Server started on port {}\n", 7777);

    std::string dummy;
    std::getline(std::cin, dummy);

    server.Stop();
}
