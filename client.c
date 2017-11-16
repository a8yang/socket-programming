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

#define LOCALHOST "127.0.0.1"
#define AWS_PORT "25245"

int main(int argc, char *argv[]) {
	char *function, *input;
	int sockfd, status;
	struct addrinfo hints, *servinfo;

	if (argc < 3) {
		fprintf(stderr, "Please provide a <function> and an <input>\n");
		exit(1);
	}
	function = argv[1];
	input = argv[2];

	// fill in hints
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if ((status = getaddrinfo(LOCALHOST, AWS_PORT, &hints, &servinfo)) != 0) {
		perror("getaddrinfo");
		exit(1);
	}

	// find a valid socket and connect
	struct addrinfo *addr;
	for (addr = servinfo; addr != NULL; addr = addr->ai_next) {
		sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (sockfd == -1) {
			continue;
		}

		if (connect(sockfd, addr->ai_addr, addr->ai_addrlen) == -1) {
			close(sockfd);
			continue;
		}
		break;
	}

	if (addr == NULL) {
		perror("Couldn't create or connect socket");
		exit(1);
	}

	int numbytes;
	char payload[strlen(function) + strlen(input) + 2];
	snprintf(payload, sizeof payload, "%s%s%s", function, " ", input);
	
	if ((numbytes = send(sockfd, payload, strlen(payload), 0)) == -1) {
		perror("Error sending data");
		exit(1);
	}

	printf("The client sent %s and %s to AWS", input, function);

	close(sockfd);
	return 0;

}