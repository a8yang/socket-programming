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

#define SERVERA_PORT "21245"
#define LOCALHOST "127.0.0.1"
#define MAXBUFLEN 20

int main(void) {
	int status, sockfd;
	struct addrinfo hints, *servinfo;
	struct sockaddr_storage sender_addr;
	char buf[MAXBUFLEN];

	// Listen for UDP requests

	// Fill in hints
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if ((status = getaddrinfo(LOCALHOST, SERVERA_PORT, &hints, &servinfo)) != 0) {
		perror("Error getaddrinfo: ");
		exit(1);
	}

	// iterate servinfo until we find a valid sockaddr and bind it
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
		perror("Error: failed to bind");
		exit(1);
	}

	printf("The Server A is up and running using UDP on port %d\n", 
		htons(((struct sockaddr_in*)addr->ai_addr)->sin_port));

	freeaddrinfo(servinfo);

	socklen_t addrlen = sizeof sender_addr;
	int numbytes;
	if ((numbytes = recvfrom(
		sockfd, 
		buf, 
		MAXBUFLEN-1, 
		0, 
		(struct sockaddr*)&sender_addr, &addrlen)
		) == -1) {

		perror("Error recvfrom: ");
		close(sockfd);
		exit(1);
	}

	printf("listener: packet is %d bytes long\n", numbytes);
	buf[numbytes] = '\0';
	printf("packet contains: %s\n", buf);

	close(sockfd);
	return 0;
}