#ifndef __IMGUI_SOCKET_H__
#define __IMGUI_SOCKET_H__

#include <iostream>
#include <sstream>
#include <string>
#include <stdlib.h>

#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")
#define _WINSOCK_DEPRECATED_NO_WARNINGS
typedef int socklen_t;
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <exception>
#endif

namespace ImSocket
{
    typedef int Socket;
    typedef std::string Ip;
    typedef unsigned int Port;
    typedef std::string Data;

    typedef struct
    {
        Ip ip;
        Port port;
    } Address;

    typedef struct
    {
        Address address;
        Data data;
    } Datagram;

    class Exception
    {
    private:
        std::string _message;
    public:
        Exception(std::string error) { this->_message = error; }
        virtual const char* what() { return this->_message.c_str(); }
    };

    class UDP
    {
    private:
        
        Socket _socket_id;
        bool _binded;
        bool _close{false};
        
    public:
        
        UDP(void);
        ~UDP(void);
        void close(void);
        void bind(Port port);
        void send(Ip ip, Port port, Data data);
        Datagram receive();
    };
} // namespace ImSocket
#endif /* __IMGUI_SOCKET_H__ */