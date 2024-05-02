#include "ConnectionSocket.hpp"

ConnectionSocket::ConnectionSocket()
: Socket()
{
}

ConnectionSocket::ConnectionSocket(int sockfd)
	: Socket()
{
	m_sockfd = sockfd;
}

ConnectionSocket::~ConnectionSocket()
{
}

ConnectionSocket::ConnectionSocket(ConnectionSocket&& other)
: Socket(std::move(other))
{
}

ConnectionSocket& ConnectionSocket::operator=(ConnectionSocket&& other)
{
	Socket::operator=(std::move(other));
	return *this;
}

size_t ConnectionSocket::send(const char *data, size_t size)
{
	return ::send(m_sockfd, data, size, MSG_DONTWAIT);
}

size_t ConnectionSocket::recv(char *data, size_t size)
{
	return ::recv(m_sockfd, data, size, MSG_DONTWAIT);
}
