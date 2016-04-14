

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdint.h>

#include <process.h>

#pragma comment(lib, "Ws2_32.lib")

SOCKET ListenSocket = INVALID_SOCKET;
SOCKET ClientSocket = INVALID_SOCKET;

const uint8_t requestRefPoint[] = { 0x5f, 0x05, 0x05, 0x5a };
const uint8_t reportPosi[] = { 0x5F, 0x01, 0x00, 0x26, 0x03, 0xC8, 0x00, 0x26, 0x03, 0xC8, 0xE3, 0x5A };
const uint8_t responseStartCruise[] = { 0x5F, 0x02, 0x00, 0x26, 0x03, 0xC8, 0x00, 0x26, 0x03, 0xC8, 0xE4, 0x5A };
const uint8_t responseStopCruise[] = { 0x5F, 0x03, 0x01, 0x04, 0x5A };
const uint8_t reportResult[] = { 0x5F, 0x04, 0x01, 0x05, 0x5A };

volatile int quit = 0;

void print_hex(uint8_t* data, int len, const char* type)
{
	const uint8_t hex[] = "0123456789ABCDEF";
	uint8_t* p = data;
	char* buf = malloc(len * 2 + 1);
	char* pbuf = buf;
	int i = 0;

	for (i = 0; i < len; i++, p++) {
		*buf++ = hex[*p >> 4];
		*buf++ = hex[*p & 0x0F];
	}
	*buf = '\0';
	printf("%s %d data: 0x%s\n", type, len, pbuf);
	free(pbuf);
}

void print_usage(char* name)
{
	printf("usage: %s clientIP clientPort serverListenPort\n", name);
}

void do_response(uint8_t* req, int len)
{
	uint8_t type = req[1];
	int result;
	switch (type)
	{
	case 0x02:
		result = send(ClientSocket, responseStartCruise, sizeof(responseStartCruise), 0);
		print_hex(responseStartCruise, result, "response");
		break;
	case 0x03:
		result = send(ClientSocket, responseStopCruise, sizeof(responseStopCruise), 0);
		print_hex(responseStopCruise, result, "response");
		break;
	default:
		break;
	}
}

int start_server(void* port)
{
	struct addrinfo *result = NULL,
					hints;
	int iResult;
	struct sockaddr_in SenderAddr;
	int SenderAddrSize = sizeof(SenderAddr);
	char RecvBuf[1024];
	int BufLen = 1024;

	struct sockaddr_in* addr;


	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_PASSIVE; // fix: for wild card ip address.

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, port, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		return 1;
	}

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		return 1;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		return 1;
	}

	addr = (struct sockaddr_in*)result->ai_addr;
	printf("local udp server listening on: %s:%u\n", inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
	freeaddrinfo(result);

	while (!quit && (iResult = recvfrom(ListenSocket,
		RecvBuf, BufLen, 0, (SOCKADDR *)& SenderAddr, &SenderAddrSize)) != SOCKET_ERROR) {

		print_hex(RecvBuf, iResult, "received");
		do_response(RecvBuf, iResult);
	}

	closesocket(ListenSocket);
	return 0;
}

int main(int argc, char **argv)
{
	WSADATA wsaData;
	int iResult;
	struct addrinfo *result = NULL,
					*ptr = NULL,
					hints;
	const char* sendbuf = NULL;
	int sendlen = 0;
	int sendType = -1;

	if (argc < 4) {
		print_usage(argv[0]);
		return 1;
	}


	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;

	// Resolve the server address and port
	iResult = getaddrinfo(argv[1], argv[2], &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		ClientSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (ClientSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return 1;
		}

		// Connect to server.
		iResult = connect(ClientSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(ClientSocket);
			ClientSocket = INVALID_SOCKET;
			continue;
		} else {
			struct sockaddr_in* addr = (struct sockaddr_in*)ptr->ai_addr;
			printf("success connect to %s:%u\n", inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
		}
		break;
	}
	freeaddrinfo(result);

	if (ClientSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		return 1;
	}

	_beginthread(start_server, 0, argv[3]);
	// Send an initial buffer
	do {
		char input;
		sendbuf = NULL;
		sendlen = 0;
		printf("\ttype cmd:\n");
		printf("\t\t1: request reference point;\n");
		printf("\t\t2: report position;\n");
		printf("\t\t3: report Result.\n");
		printf("\t\t0: quit.\n");
		input = _getche();
		printf("\n");
		switch (input)
		{
		case '1':
			sendbuf = requestRefPoint;
			sendlen = 4;
			break;
		case '2':
			sendbuf = reportPosi;
			sendlen = 12;
			break;
		case '3':
			sendbuf = reportResult;
			sendlen = 5;
			break;
		case '0':
			quit = 1;
			break;
		default:
			printf("\nwrong cmd received!please try again.\n");
			continue;
			break;
		}
		if (quit) {
			break;
		}
		iResult = send(ClientSocket, sendbuf, sendlen, 0);
		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 1;
		}

		print_hex(sendbuf, iResult, "sent");
	} while (1);

	closesocket(ClientSocket);
	WSACleanup();

	return 0;
}
