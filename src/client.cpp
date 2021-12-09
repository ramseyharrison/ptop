#include <iostream>
#include <string>
#include "socket.h"

#include "windows_socket.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")
#pragma comment(lib, "wininet.lib")


int create_client(int argc, char** argv)
{
    cout << "Starting Client (:" << endl;

#ifdef WIN32
    windows_internet garbo(MAKEWORD(2,2));
#endif

    cout << "Insert Address: ";
    string address{};
    cin >> address; //places the data from cin into address
    cout << endl; //prints the current data in the terminal buffer (address) and then carriage returns

    try
    {
        auto send_socket = Sockets::CreateSenderSocket(address);

        string input;
        do
        {
            cout << "Send a message (enter \"disconnect\" to stop): ";
            getline(cin, input);
            cout << endl;

            send_socket->send_data(vector<char>(input.begin(), input.end()));

            if (input == "disconnect")
                break;

        } while (true);
    }

    catch (exception& e)
    {
        cerr << "Exception Caught: \"" << e.what() << '\"' << endl;
        return -1;
    }
    return 0;
}