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
#define SERVERB_PORT "22245"
#define SERVERC_PORT "23245"

#define LOCALHOST "127.0.0.1"
#define MAX_QUEUE 10
#define MAX_DATA_SIZE 20

void sendServerA(char* function, char* input);
int createUDPListener(int *port);
void createUDPSender(int *sockfd, struct addrinfo **addr, char *port);
int sendUDP(int sockfd, struct addrinfo *addr, char payload[]);

int main(void) {
	int status, tcp_sockfd;
	struct addrinfo hints, *servinfo;

	// ===== Listen for client =====

	// lines 36-64 were mostly reused from Beej's guide
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
		// structure of accept code was reused from Beej's guide

		struct sockaddr_storage client_addr;
		socklen_t addrlen = sizeof client_addr;

		// Wait for incoming connection
		int child_sockfd = accept(tcp_sockfd, (struct sockaddr*)&client_addr, &addrlen);
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
			int udp_sockfd = createUDPListener(&udp_port);

			// create UDP ports to send data to backservers
			int sockfd_a, sockfd_b, sockfd_c;
			struct addrinfo *addr_a, *addr_b, *addr_c;
			createUDPSender(&sockfd_a, &addr_a, SERVERA_PORT);
			createUDPSender(&sockfd_b, &addr_b, SERVERB_PORT);
			createUDPSender(&sockfd_c, &addr_c, SERVERC_PORT);
			int port_a = htons(((struct sockaddr_in*)addr_a->ai_addr)->sin_port);
			int port_b = htons(((struct sockaddr_in*)addr_b->ai_addr)->sin_port);
			int port_c = htons(((struct sockaddr_in*)addr_c->ai_addr)->sin_port);


			// send data to backservers
			char *x = input;
			sendUDP(sockfd_a, addr_a, x);
			printf("The AWS sent <%s> to Backend-Server A\n", x);
			sendUDP(sockfd_b, addr_b, x);
			printf("The AWS sent <%s> to Backend-Server B\n", x);
			sendUDP(sockfd_c, addr_c, x);
			printf("The AWS sent <%s> to Backend-Server C\n", x);

			char *x_sq = NULL;
			char *x_cb = NULL; 
			char *x_fourth = NULL; 
			char *x_fifth = NULL; 
			char *x_sixth = NULL;

			// Listen for data from backservers
			while (x_sq == NULL || x_cb == NULL || x_fourth == NULL || x_fifth == NULL || x_sixth == NULL) {
				struct sockaddr from_addr;
				socklen_t from_addrlen = sizeof from_addr;
				char buf[MAX_DATA_SIZE];
				if (recvfrom(udp_sockfd, buf, MAX_DATA_SIZE-1, 0, &from_addr, &from_addrlen) == -1) {
					perror("error receiving from backserver");
				}

				// parse data to figure out who sent it
				char *saveptr2;
				char str2[strlen(buf)];
				strcpy(str2, buf);
				strtok_r(str2, " ", &saveptr2);
				char *server_name = str2;
				char *num = saveptr2;

				// figure out what power each number represents
				int incoming_port;
				if (strcmp(server_name, "A") == 0) {
					if (x_sq == NULL) {
						x_sq = malloc(strlen(num) + 1);
						strcpy(x_sq, num);
						sendUDP(sockfd_a, addr_a, x_sq);
						printf("The AWS sent <%s> to Backend-Server A\n", x_sq);
						sendUDP(sockfd_b, addr_b, x_sq);
						printf("The AWS sent <%s> to Backend-Server B\n", x_sq);
					} else {
						x_fourth = malloc(strlen(num) + 1);
						strcpy(x_fourth, num);
					}

					incoming_port = port_a;
				} else if (strcmp(server_name, "B") == 0) {
					if (x_cb == NULL) {
						x_cb = malloc(strlen(num) + 1);
						strcpy(x_cb, num);
					} else {
						x_sixth = malloc(strlen(num) + 1);
						strcpy(x_sixth, num);
					}

					incoming_port = port_b;
				} else if (strcmp(server_name, "C") == 0) {
					if (x_fifth == NULL) {
						x_fifth = malloc(strlen(num) + 1);
						strcpy(x_fifth, num);
					}

					incoming_port = port_c;
				} else {
					incoming_port = 0;
				}

				printf("The AWS received <%s> Backend-Server %s using UDP over port <%d>\n",
					num,
					server_name,
					incoming_port
				);
			}

			printf("Values of powers received by AWS: <%s, %s, %s, %s, %s, %s>\n",
				x, x_sq, x_cb, x_fourth, x_fifth, x_sixth	
			);

			// convert values to floats for calculations
			float f_x = atof(x);
			float f_x_sq = atof(x_sq);
			float f_x_cb = atof(x_cb);
			float f_x_fourth = atof(x_fourth);
			float f_x_fifth = atof(x_fifth);
			float f_x_sixth = atof(x_sixth);
			float result;

			// Do Taylor Series calculations
			if (strcmp(function, "DIV") == 0) {
				result = 1 + f_x + f_x_sq + f_x_cb + f_x_fourth + f_x_fifth + f_x_sixth;
			} else if (strcmp(function, "LOG") == 0) {
				result = -1*f_x - f_x_sq/2 - f_x_cb/3 - f_x_fourth/4 - f_x_fifth/5 - f_x_sixth/6;
			}
			char sresult[20];
			snprintf(sresult, sizeof sresult, "%f", result);

			printf("AWS calculated %s on <%s>: <%s>\n", function, x, sresult);

			// send result to client
			if (send(child_sockfd, sresult, strlen(sresult), 0) == -1) {
				perror("unable to send result to client");
				exit(1);
			}
			printf("The AWS sent <%s> to client.\n", sresult);

			// exit child
			exit(1);
		}

		close(child_sockfd);
	}

	close(tcp_sockfd);
	return 0;
}

// Almost whole function reused from Beej's
void createUDPSender(int *sockfd, struct addrinfo **addr, char *port) {
	// ====== Send to backserver A
	int status;
	int sockfd_a;
	struct addrinfo hints_a, *servinfo_a;

	memset(&hints_a, 0, sizeof hints_a);
	hints_a.ai_family = AF_INET;
	hints_a.ai_socktype = SOCK_DGRAM;

	if ((status = getaddrinfo(LOCALHOST, port, &hints_a, &servinfo_a)) != 0) {
		perror("getaddrinfo server A");
		exit(1);
	}

	for (*addr = servinfo_a; *addr != NULL; *addr = (*addr)->ai_next) {
		sockfd_a = socket((*addr)->ai_family, (*addr)->ai_socktype, (*addr)->ai_protocol);
		if (sockfd_a == -1) {
			continue;
		}

		break;
	}

	if (*addr == NULL) {
		perror("Failed to create socket for A");
		exit(1);
	}

	*sockfd = sockfd_a;
}

int sendUDP(int sockfd, struct addrinfo *addr, char payload[]) {
	int numbytes;

	if ((numbytes = sendto(sockfd, payload, strlen(payload), 0, addr->ai_addr, addr->ai_addrlen)) == -1) {
		perror("sendUDP");
	}
	return numbytes;
}

// Reused code from Beej's
int createUDPListener(int *port) {
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