#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define closesocket close
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
typedef int SOCKET;
#endif

namespace
{
	void PrintUsage()
	{
		std::fprintf(stderr, "Usage: omp_query_probe <host> <port> [opcode]\n");
		std::fprintf(stderr, "  opcode: i (server info, default), o, c, r, p\n");
	}

	bool ResolveIPv4(const char* host, const char* port, sockaddr_in* output)
	{
		addrinfo hints;
		addrinfo* result = 0;
		std::memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;

		const int rc = getaddrinfo(host, port, &hints, &result);
		if (rc != 0)
		{
			std::fprintf(stderr, "getaddrinfo(%s,%s) failed: %s\n", host, port, gai_strerror(rc));
			return false;
		}

		bool ok = false;
		for (addrinfo* it = result; it; it = it->ai_next)
		{
			if (it->ai_family == AF_INET && it->ai_addrlen >= (socklen_t)sizeof(sockaddr_in))
			{
				std::memcpy(output, it->ai_addr, sizeof(sockaddr_in));
				ok = true;
				break;
			}
		}

		freeaddrinfo(result);
		return ok;
	}

	unsigned short ParsePort(const char* value)
	{
		char* end = 0;
		unsigned long parsed = std::strtoul(value, &end, 10);
		if (end == value || *end != '\0' || parsed > 65535)
			return 0;
		return (unsigned short)parsed;
	}
}

int main(int argc, char** argv)
{
#ifdef _WIN32
	WSADATA winsockInfo;
	if (WSAStartup(MAKEWORD(2, 2), &winsockInfo) != 0)
		return 1;
#endif

	if (argc < 3 || argc > 4)
	{
		PrintUsage();
		return 1;
	}

	const char* host = argv[1];
	const char* portString = argv[2];
	const char opcode = argc == 4 ? argv[3][0] : 'i';
	const unsigned short port = ParsePort(portString);
	if (port == 0)
	{
		std::fprintf(stderr, "invalid port: %s\n", portString);
		return 1;
	}

	sockaddr_in target;
	if (!ResolveIPv4(host, portString, &target))
		return 1;

	unsigned char request[15];
	std::memcpy(request, "SAMP", 4);
	std::memcpy(request + 4, &target.sin_addr.s_addr, 4);
	request[8] = (unsigned char)(port & 0xFF);
	request[9] = (unsigned char)((port >> 8) & 0xFF);
	request[10] = (unsigned char)opcode;
	int requestLength = 11;

	if (opcode == 'p')
	{
		request[11] = 0x78;
		request[12] = 0x56;
		request[13] = 0x34;
		request[14] = 0x12;
		requestLength = 15;
	}

	SOCKET socketFd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socketFd == INVALID_SOCKET)
	{
#ifdef _WIN32
		std::fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
#else
		std::fprintf(stderr, "socket failed: errno=%d (%s)\n", errno, std::strerror(errno));
#endif
		return 1;
	}

#ifdef _WIN32
	DWORD timeout = 2000;
	setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
	timeval timeout;
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;
	setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif

	if (sendto(socketFd, reinterpret_cast<const char*>(request), requestLength, 0, reinterpret_cast<const sockaddr*>(&target), sizeof(target)) == SOCKET_ERROR)
	{
#ifdef _WIN32
		std::fprintf(stderr, "sendto failed: %d\n", WSAGetLastError());
#else
		std::fprintf(stderr, "sendto failed: errno=%d (%s)\n", errno, std::strerror(errno));
#endif
		closesocket(socketFd);
		return 1;
	}

	unsigned char response[2048];
	const int received = recvfrom(socketFd, reinterpret_cast<char*>(response), sizeof(response), 0, 0, 0);
	if (received == SOCKET_ERROR)
	{
#ifdef _WIN32
		std::fprintf(stderr, "recvfrom failed: %d\n", WSAGetLastError());
#else
		std::fprintf(stderr, "recvfrom failed: errno=%d (%s)\n", errno, std::strerror(errno));
#endif
		closesocket(socketFd);
		return 1;
	}

	std::printf("received %d bytes for opcode %c\n", received, opcode);
	if (received >= 11)
	{
		std::printf("magic=%.4s opcode=%c\n", response, response[10]);
	}
	for (int i = 0; i < received; ++i)
	{
		std::printf("%02x", response[i]);
		if (i + 1 < received)
			std::printf(" ");
	}
	std::printf("\n");

	closesocket(socketFd);
#ifdef _WIN32
	WSACleanup();
#endif
	return 0;
}
