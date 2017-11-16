#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define AWS_TCP_PORT "25245"
#define AWS_UDP_PORT "24245"
#define SERVERA_PORT "21245"

#define LOCALHOST "127.0.0.1"
#define MAX_QUEUE 10
#define MAX_DATA_SIZE 20

void sendServerA(char* function, char* input);
int createUDP(int *port);

int main(void) {
	int status, tcp_sockfd, child_sockfd;
	struct addrinfo hints, *servinfo;
	struct sockaddr_storage client_addr;

	// ===== Listen for client =====

	// fill in hints
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if ((status = getaddrinfo(LOCALHOST, AWS_TCP_PORT, &hints, &servinfo)) != 0) {
		perror("Error getaddrinfo: ");
		exit(1);
	}

	// iterate servinfo until we find a valid sockaddr and bind it
	struct addrinfo *addr;
	for (addr = servinfo; addr != NULL; addr = addr->ai_next) {
		tcp_sockfd = socket(addr->ai_family, addr->ai_socktype, 0);
		if (tcp_sockfd == -1) {
			continue;
		}

		if (bind(tcp_sockfd, addr->ai_addr, addr->ai_addrlen) == -1) {
			close(tcp_sockfd);
			continue;
		}

		break;
	}

	if (addr == NULL) {
		perror("Error: failed to bind");
		exit(1);
	}

	int tcp_port = htons(((struct sockaddr_in*)addr->ai_addr)->sin_port);

	freeaddrinfo(servinfo);


	// listen to client
	if (listen(tcp_sockfd, MAX_QUEUE) == -1) {
		perror("Error: failed to listen");
		exit(1);
	}

	printf("The AWS is up and running\n");

	while (1) {
		socklen_t addrlen = sizeof client_addr;

		// Wait for incoming connection
		child_sockfd = accept(tcp_sockfd, (struct sockaddr*)&client_addr, &addrlen);
		if (child_sockfd == -1) {
			perror("Error in accept");
			continue;
		}

		// get info about client
		char ip4[INET_ADDRSTRLEN];
		struct sockaddr_in *client_addr_in = (struct sockaddr_in*)&client_addr;
		inet_ntop(AF_INET, &(client_addr_in->sin_addr), ip4, INET_ADDRSTRLEN);
		int client_port = htons(client_addr_in->sin_port);

		if (!fork()) {
			close(tcp_sockfd);
			int numbytes;
			char data[MAX_DATA_SIZE];

			if ((numbytes = recv(child_sockfd, data, MAX_DATA_SIZE-1, 0)) == -1) {
				perror("Error when receiving");
				close(child_sockfd);
				exit(1);
			}
			data[numbytes] = '\0';

			printf("received data: %s\n", data);

			// parse function and input
			char *saveptr;
			char str[sizeof data];
			strcpy(str, data);
			strtok_r(str, " ", &saveptr);
			char *function = str;
			char *input = saveptr;

			printf("The AWS received %s and function=%s from the client using TCP over port %d\n", 
				input,
				function,
				tcp_port);

			// create UDP port that backservers can talk to
			int udp_port;
			int udp_sockfd = createUDP(&udp_port);

			exit(1);
		}

		close(child_sockfd);
	}

	// sendServerA("LOG", "0.8");

	close(tcp_sockfd);
	return 0;
}

void sendServerA(char* function, char* input) {
	// ====== Send to backserver A
	int status;
	int sockfd_a;
	struct addrinfo hints_a, *servinfo_a;

	memset(&hints_a, 0, sizeof hints_a);
	hints_a.ai_family = AF_INET;
	hints_a.ai_socktype = SOCK_DGRAM;

	if ((status = getaddrinfo(LOCALHOST, SERVERA_PORT, &hints_a, &servinfo_a)) != 0) {
		perror("getaddrinfo server A");
		exit(1);
	}

	struct addrinfo *addr_a;
	for (addr_a = servinfo_a; addr_a != NULL; addr_a = addr_a->ai_next) {
		sockfd_a = socket(addr_a->ai_family, addr_a->ai_socktype, addr_a->ai_protocol);
		if (sockfd_a == -1) {
			continue;
		}

		break;
	}

	if (addr_a == NULL) {
		perror("Failed to create socket for A");
		exit(1);
	}

	int numbytes_a;
	char payload[strlen(function) + strlen(input) + 2];
	snprintf(payload, sizeof payload, "%s%s%s", function, " ", input);

	if ((numbytes_a = sendto(sockfd_a, payload, strlen(payload), 0, addr_a->ai_addr, addr_a->ai_addrlen)) == -1) {
		perror("Failed to send to A");
		exit(1);
	}

	// Remove this when running for real
	freeaddrinfo(servinfo_a);
}

int createUDP(int *port) {
	struct addrinfo hints, *servinfo;
	int status, sockfd;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if ((status = getaddrinfo(LOCALHOST, AWS_UDP_PORT, &hints, &servinfo)) != 0) {
		perror("getaddrinfo createUDP");
		return -1;
	}

	struct addrinfo *addr;
	for (addr = servinfo; addr != NULL; addr = addr->ai_next) {
		sockfd = socket(addr->ai_family, addr->ai_socktype, 0);
		if (sockfd == -1) {
			continue;
		}

		if (bind(sockfd, addr->ai_addr, addr->ai_addrlen) == -1) {
			close(sockfd);
			continue;
		}
		break;
	}

	if (addr == NULL) {
		perror("Unable to create UDP socket");
		return -1;
	}

	*port = htons(((struct sockaddr_in*)addr->ai_addr)->sin_port);

	freeaddrinfo(servinfo);
	return sockfd;
}