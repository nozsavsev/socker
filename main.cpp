#pragma comment(lib, "Ws2_32.lib")

#include "socker.h"
#include <windows.h>
#include <chrono>
#include <iostream>
using namespace std::chrono;

using namespace socker;


void progress_callback(size_t current, size_t all)
{
    printf("\rclient >> send %d%%\t", (int)(current * 100LL / all));
}

void client()
{
    WSAData wsData;

    WSAStartup(MAKEWORD(2, 0), &wsData);

    ADDRINFO hints = { 0 }, * result;
    SOCKET client;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    std::string serverIP;
    printf("enter server IPv4 ");
    std::cin >> serverIP;
    getaddrinfo(serverIP.c_str(), "5568", &hints, &result);

    client = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

    while (SOCKET_ERROR == connect(client, result->ai_addr, result->ai_addrlen));

    printf("client >> connected\n");

    smart_buff_t Tx(1024LL * 1024LL * 1024LL);
    Tx.Set_DataSize(1024LL * 1024LL * 1024LL);

    while (1)
    {
        if (Send(client, &Tx, &progress_callback) == SR_OK)
            printf("\rclient >> send OK                  \n");
        else
        {
            printf("\rclient >> send FAIL                \n");
            break;
        }
    }

    closesocket(client);
    printf("client >> disconnected\n");
}

void server_socket_handler(SOCKET client)
{
    printf("server >> connected\n");

    smart_buff_t rx(sizeof(tagPOINT));

    while (1)
    {
        auto begin = high_resolution_clock::now();

        if (Receive(client, &rx, false) == SR_OK)
        {
            auto end = high_resolution_clock::now();
            printf("\rserver >> recv OK %llu\t\t%lluMb/s\n", duration_cast<milliseconds>(end - begin).count(),
                (long long)((long double)rx.Get_DataSize() / (long double)(duration_cast<milliseconds>(end - begin).count()) / (long double)1000));
        }
        else
        {
            printf("\rserver >> recv FAIL                   \n");
            break;
        }
    }

    closesocket(client);

    printf("\rserver >> disconnected\n");
}

void server()
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    ADDRINFO hints;
    ADDRINFO* result;

    ZeroMemory(&hints, sizeof(ADDRINFO));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(nullptr, "5568", &hints, &result);

    SOCKET listen_sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

    bind(listen_sock, result->ai_addr, (int)result->ai_addrlen);


    listen(listen_sock, SOMAXCONN);


    while (true)
    {

        SOCKET client = accept(listen_sock, nullptr, nullptr);

        std::thread(&server_socket_handler, client).detach();
    }

    closesocket(listen_sock);
    WSACleanup();
}

int main()
{
    printf("possible IPv4 addreses\n");
    system("ipconfig | findstr \"IPv4\"");
    printf("enter connection type (server | client | both) ");

    std::string ctype;
    std::cin >> ctype;

    if (ctype == "server")
    {
        std::thread th1(&server);
        th1.join();
    }
    else if (ctype == "client")
    {
        std::thread th1(&client);
        th1.join();
    }

    else if (ctype == "both")
    {
        std::thread th1(&client);
        server();
        th1.join();
    }

    system("pause");

    return 0;
}