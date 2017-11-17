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
#include <math.h>

#define SERVERC_PORT "23245"
#define AWS_PORT "24245"
#define LOCALHOST "127.0.0.1"
#define MAXBUFLEN 20

// Most of the code except for the calculation part was reused from Beej's
int main(void) {
	int status, sockfd;
	struct addrinfo hints, *servinfo;

	// Listen for UDP requests

	// Fill in hints
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if ((status = getaddrinfo(LOCALHOST, SERVERC_PORT, &hints, &servinfo)) != 0) {
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

	printf("The Server C is up and running using UDP on port %d\n", 
		htons(((struct sockaddr_in*)addr->ai_addr)->sin_port));

	freeaddrinfo(servinfo);

	// create socket to send data to AWS
	int aws_sockfd;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if (getaddrinfo(LOCALHOST, AWS_PORT, &hints, &servinfo) != 0) {
		perror("Error creating AWS port");
		exit(1);
	}

	struct addrinfo *addr_aws;
	for (addr_aws = servinfo; addr_aws != NULL; addr_aws = addr_aws->ai_next) {
		aws_sockfd = socket(addr_aws->ai_family, addr_aws->ai_socktype, 0);
		if (aws_sockfd == -1) {
			continue;
		}
		break;
	}

	if (addr_aws == NULL) {
		perror("Error when creating AWS socket");
		exit(1);
	}

	while (1) {
		struct sockaddr_storage sender_addr;
		char buf[MAXBUFLEN];
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

		buf[numbytes] = '\0';
		printf("The Server C received input %s\n", buf);

		// calculate 5th power of input
		float x = atof(buf);
		float x_fifth = pow(x, 5.0);

		// convert to string
		char x_fif_string[20];
		snprintf(x_fif_string, sizeof x_fif_string, "%s%f", "C ", x_fifth);
		printf("%s", x_fif_string);
		printf("The Server C calculated 5th power: %s\n", x_fif_string);

		// Send fifth power result to aws
		if (sendto(
			aws_sockfd, 
			x_fif_string, 
			strlen(x_fif_string), 
			0, 
			addr_aws->ai_addr, 
			addr_aws->ai_addrlen
		) == -1) {
			perror("Error when sending fifth power of number");
		}

		printf("The Server C finished sending the output to AWS\n");
	}

	close(sockfd);
	return 0;
}