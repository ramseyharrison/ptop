#include <iostream>

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <mswsock.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")
#pragma comment(lib, "wininet.lib")

#include "windows_socket.h"
#endif // WIN32

#include <string>
#include <iostream>
#include <stdio.h>
#include <thread>
#include <future>
#include <chrono>
#include <shared_mutex>
#include <mutex>
#include <queue>
#include <string.h>

#include "server.h"
#include "client.h"
#include "socket.h"
#include "ip.h"
#include "message.h"


int main(int argc, char** argv) {

#ifdef WIN32
    // windows_internet uses RAII to ensure WSAStartup and WSACleanup get called in the proper order
    windows_internet wsa_wrapper(MAKEWORD(2, 2));
#endif

    try
    {
        if (argc > 1 && !strcmp(argv[1], "server"))
        {
            std::string port = Sockets::DefaultPort;
            if (argc > 2)
                port = argv[2];
            server_loop(port);
        }
        else
        {
            std::string ip = "localhost";
            if (argc > 1)
                ip = argv[1];
            std::string port = Sockets::DefaultPort;
            if (argc > 2)
                port = argv[2];
            client_loop(ip, port);


            //auto receiver = create_server();
            //auto sender = create_client();

            //while (true) {

            //    std::string message{};
            //    std::cin >> message;

            //    if (message == "") {
            //        continue;
            //    }

            //    //TransmitFile(socket, file, 0, 0, NULL, NULL, TF_WRITE_BEHIND); //file should be opened with FILE_FLAG_SEQUENTIAL_SCAN option

            //    int last_error = WSAGetLastError();

            //    if (last_error != 0) {

            //    }
            //}
        }
    }
    catch (const std::exception& e)
    {
        std::cout << "Caught std::exception: " << e.what() << std::endl;
        return -1;
    }
}

