#include "imgui_socket.h"
#define MAX_BUFFER 1024

namespace ImSocket
{
    struct sockaddr_in* to_sockaddr(Address* a)
    {
        struct sockaddr_in* ret;
        ret=(struct sockaddr_in*) malloc (sizeof(struct sockaddr_in));
        ret->sin_family = AF_INET;
#ifdef _WIN32
        inet_pton(AF_INET, a->ip.c_str(), &ret->sin_addr);
#else
        inet_aton(a->ip.c_str(),&(ret->sin_addr));
#endif
        ret->sin_port=htons(a->port);
            
        return ret;
    }


    Address* from_sockaddr(struct sockaddr_in* address)
    {
        Address* ret;

        ret=(Address*)malloc(sizeof(Address));
        ret->ip = inet_ntoa(address->sin_addr);
        ret->port = ntohs(address->sin_port);

        return ret;
    }


    UDP::UDP(void)
    {
#ifdef _WIN32
        WSADATA wsadata;
        if (WSAStartup(MAKEWORD(1,1), &wsadata) == SOCKET_ERROR)  // Initialize Winsock version 1.1
        {
            this->_socket_id = -1;
            return;
        }
#endif
        this->_socket_id = socket(AF_INET, SOCK_DGRAM, 0);   
        if (this->_socket_id == -1) throw Exception("[Constructor] Cannot create socket");           
        this->_binded = false;
#ifdef _WIN32
        DWORD recvTimeOutMilliSec = 100;
        std::cout << "Set socket receive timeout to " << recvTimeOutMilliSec << "ms." << std::endl;
        if (setsockopt(_socket_id, SOL_SOCKET, SO_RCVTIMEO, (const char*)&recvTimeOutMilliSec, sizeof(recvTimeOutMilliSec)) == SOCKET_ERROR)
        {
            std::cout << "FAILED to invoke setsockopt() to set receive timeout option! WSAGetLastError() returns " << WSAGetLastError() << "." << std::endl;
        }
#else
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        if (setsockopt(this->_socket_id, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            std::cout << "FAILED to invoke setsockopt() to set receive timeout option! WSAGetLastError() returns." << std::endl;
        }
#endif
    }

    UDP::~UDP(void) {}
        
    void UDP::close(void)
    {
        _close = true;
#ifdef _WIN32
        shutdown(this->_socket_id, SD_BOTH);
#else
        shutdown(this->_socket_id, SHUT_RDWR);
#endif
    }
        
    void UDP::bind(Port port)
    {
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr=htonl(INADDR_ANY);
        address.sin_port=htons(port);

        if (this->_binded)
        {
            this->close();
            this->_socket_id = socket(AF_INET, SOCK_DGRAM, 0);
        }
        // ::bind() calls the function bind() from <arpa/inet.h> (outside the namespace)            
        if (::bind(this->_socket_id, (struct sockaddr*)&address, sizeof(struct sockaddr_in)) == -1)
        {
            std::stringstream error;
            error << "[listen_on_port] with [port=" << port << "] Cannot bind socket";
            throw Exception(error.str());
        }

        this->_binded = true;
    }
        
    void UDP::send(Ip ip, Port port, Data data)
    {
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
#ifdef _WIN32
        inet_pton(AF_INET, ip.c_str(), &address.sin_addr);
        if (sendto(this->_socket_id, (char*)data.c_str(), data.length(), 0, (struct sockaddr*)&address, sizeof(struct sockaddr_in)) == -1)
#else
        inet_aton(ip.c_str(), &address.sin_addr);
        if (sendto(this->_socket_id, (void*)data.c_str(), data.length(), 0, (struct sockaddr*)&address, sizeof(struct sockaddr_in)) == -1)
#endif
        {
            std::stringstream error;
            error << "[send] with [ip=" << ip << "] [port=" << port << "] [data=" << data << "] Cannot send";
            throw Exception(error.str());
        }
    }
        
    Datagram UDP::receive()
    {
        int size = sizeof(struct sockaddr_in);
        char *buffer = (char*)malloc(sizeof(char) * MAX_BUFFER);
        struct sockaddr_in address;
        Datagram ret;

        int sockErr;
#ifdef _WIN32
        while (!_close)
        {
            sockErr = recvfrom(this->_socket_id, (char*)buffer, MAX_BUFFER, 0, (struct sockaddr*)&address, (socklen_t*)&size);
            if (sockErr == SOCKET_ERROR)
            {
                const auto lastErr = WSAGetLastError();
                if (lastErr != WSAETIMEDOUT)
                    throw Exception("[receive] Cannot receive");
            }
            else
                break;
        }
#else
        while (!_close)
        {
            if ((sockErr = recvfrom(this->_socket_id, (void*)buffer, MAX_BUFFER, 0, (struct sockaddr*)&address, (socklen_t*)&size)) == -1)
            {
                if (errno == EWOULDBLOCK || errno == EAGAIN || errno == ETIMEDOUT)
                {
                    continue;
                }
                else
                    throw Exception("[receive] Cannot receive");
            }
            else
                break;
        }
#endif

        ret.data = buffer;
        ret.address.ip = inet_ntoa(address.sin_addr);
        ret.address.port = ntohs(address.sin_port);
        
        free(buffer);
        
        return ret;
    }
} // namespace ImSocket
