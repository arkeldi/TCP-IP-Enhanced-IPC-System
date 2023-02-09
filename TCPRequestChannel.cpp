#include "TCPRequestChannel.h"

#include <arpa/inet.h>
#include <netinet/in.h>

TCPRequestChannel::TCPRequestChannel(const std::string _ip_address, const std::string _port_no)
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd != -1);

    // setup ip information
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(atoi(_port_no.c_str()));

    if (_ip_address == "")
    {
        // server

        // bind to port
        assert(bind(sockfd, (struct sockaddr *)&address, sizeof(address)) >= 0);

        // listen for connections
        assert(listen(sockfd, 128) >= 0);
    }
    else
    {
        // client

        // connect
        assert(connect(sockfd, (struct sockaddr *)&address, sizeof(address)) >= 0);
    }
}

TCPRequestChannel::TCPRequestChannel(int _sockfd)
{
    sockfd = _sockfd;
    assert(sockfd != -1);
}

TCPRequestChannel::~TCPRequestChannel()
{
    close(sockfd);
}

int TCPRequestChannel::accept_conn()
{
    socklen_t s = sizeof(address);
    int newsocket_fd = accept(sockfd, (struct sockaddr *)&address, &s);

    assert(newsocket_fd != -1);
    return newsocket_fd;
}

int TCPRequestChannel::cread(void *msgbuf, int msgsize)
{
    return read(sockfd, msgbuf, msgsize);
}

int TCPRequestChannel::cwrite(void *msgbuf, int msgsize)
{
    return write(sockfd, msgbuf, msgsize);
    
}
