#include "UDPServer.h"
#include "DisplayProtocol.h"

#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

void executeCommand(const Command& command);

UDPServer::UDPServer()
{
    serverSocket = INVALID_SOCKET;
    running = false;
    winsockStarted = false;
}

UDPServer::~UDPServer()
{
    stop();
}

bool UDPServer::start(unsigned short port)
{
    if (running)
        return true;

    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return false;

    winsockStarted = true;

    serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (serverSocket == INVALID_SOCKET)
    {
        stop();
        return false;
    }

    sockaddr_in address = {};

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (bind(
        serverSocket,
        reinterpret_cast<sockaddr*>(&address),
        sizeof(address)) == SOCKET_ERROR)
    {
        stop();
        return false;
    }

    DWORD timeout = 500;

    setsockopt(
        serverSocket,
        SOL_SOCKET,
        SO_RCVTIMEO,
        reinterpret_cast<const char*>(&timeout),
        sizeof(timeout)
    );

    running = true;

    try
    {
        serverThread = std::thread(
            &UDPServer::receiveLoop, this
        );
    }
    catch (...)
    {
        stop();
        return false;
    }

    std::cout << "UDP server started on port "
        << port << std::endl;

    return true;
}

void UDPServer::stop()
{
    running = false;

    if (serverSocket != INVALID_SOCKET)
    {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }

    if (serverThread.joinable())
        serverThread.join();

    if (winsockStarted)
    {
        WSACleanup();
        winsockStarted = false;
    }
}

void UDPServer::receiveLoop()
{
    SOCKET sock = serverSocket;

    uint8_t buffer[65536];

    while (running)
    {
        sockaddr_in clientAddress = {};
        int clientLength = sizeof(clientAddress);

        int bytesReceived = recvfrom(
            sock,
            reinterpret_cast<char*>(buffer),
            static_cast<int>(sizeof(buffer)),
            0,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &clientLength
        );

        if (bytesReceived == SOCKET_ERROR)
        {
            if (!running)
                break;

            int error = WSAGetLastError();

            if (error == WSAETIMEDOUT ||
                error == WSAEWOULDBLOCK)
                continue;

            std::cout << "UDP receive error: "
                << error << std::endl;

            continue;
        }

        Command command;

        if (parseCommand(
            buffer,
            static_cast<size_t>(bytesReceived),
            command))
        {
            executeCommand(command);
        }
        else
        {
            std::cout << "Invalid command packet"
                << std::endl;
        }
    }
}