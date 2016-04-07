#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")

SOCKET ListenSocket = INVALID_SOCKET;
SOCKET ClientSocket = INVALID_SOCKET;

const UINT8 requestRefPoint[] = { 0x5f, 0x05, 0x05, 0x5a };
const UINT8 reportPosi[] = { 0x5F, 0x01, 0x00, 0x26, 0x03, 0xC8, 0x00, 0x26, 0x03, 0xC8, 0xE3, 0x5A };
const UINT8 reportResult[] = {0x5F, 0x04, 0x01, 0x05, 0x5A};

void print_usage(char* name)
{
	printf("usage: %s ip port cmd\n", name);
	printf("\tcmd:\n");
	printf("\t\t1: request reference point;\n");
	printf("\t\t2: report position;\n");
	printf("\t\t3: report Result.\n");
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
		}
		break;
	}
	freeaddrinfo(result);

	if (ClientSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		return 1;
	}

	// Send an initial buffer
	sendType = atoi(argv[3]);
	switch (sendType)
	{
	case 1:
		sendbuf = requestRefPoint;
		sendlen = 4;
		break;
	case 2:
		sendbuf = reportPosi;
		sendlen = 12;
		break;
	case 3:
		sendbuf = reportResult;
		sendlen = 5;
		break;
	default:
		break;
	}
	if (!sendbuf) {
		print_usage(argv[0]);
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}
	iResult = send(ClientSocket, sendbuf, sendlen, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}

	printf("Bytes Sent: %ld\n", iResult);

	closesocket(ClientSocket);
	WSACleanup();

	return 0;
}
