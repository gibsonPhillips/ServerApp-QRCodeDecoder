#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>

#define TRUE 1
#define FALSE 0

void logger(char *x, char addr[])
{
	time_t t = time(NULL);

	struct tm tm = *localtime(&t);

	printf("Log Time: %s", ctime(&t));
	printf("Log Address: %s\n", addr);
	printf("Log Message: %s\n", x);
}

int main(int argc, char *argv[])
{
	const char *port = argv[1]; /* available port */
	int rateLimit = atoi(argv[2]);
	int currentRequests;
	int currentSeconds;
	int lastSeconds;
	int currentMin;
	int lastMin;
	int maxUsers = atoi(argv[3]);
	int timeout = atoi(argv[4]);

	const char *welcome_msg = "Type 'close' to disconnect\n";
	const int hostname_size = 32;
	char hostname[hostname_size];
	const int backlog = 10;					 /* also max connections */
	char connection[backlog][hostname_size]; /* storage for IPv4 connections */
	socklen_t address_len = sizeof(struct sockaddr);
	struct addrinfo hints, *server;
	struct sockaddr address;
	int r, max_connect, fd, done;
	fd_set main_fd, read_fd;
	int serverfd, clientfd;

	time_t t = time(NULL);
	printf("%s", ctime(&t));

	struct tm tm = *localtime(&t);
	printf("Seconds: %02d\n", tm.tm_sec);

	if (argc < 5)
	{
		fprintf(stderr, "Format: listening port, rate limit, max users, timeout\n");
		exit(1);
	}

	/* configure the server */
	memset(&hints, 0, sizeof(struct addrinfo)); /* use memset_s() */
	hints.ai_family = AF_INET;					/* IPv4 */
	hints.ai_socktype = SOCK_STREAM;			/* TCP */
	hints.ai_flags = AI_PASSIVE;				/* accept any */
	r = getaddrinfo(0, port, &hints, &server);
	if (r != 0)
	{
		perror("failed");
		exit(1);
	}

	/* create a socket */
	serverfd = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
	if (serverfd == -1)
	{
		perror("failed");
		exit(1);
	}

	/* bind to a port */
	r = bind(serverfd, server->ai_addr, server->ai_addrlen);
	if (r == -1)
	{
		perror("failed");
		exit(1);
	}

	/* listen for a connection*/
	puts("TCP Server is listening...");
	r = listen(serverfd, backlog);
	if (r == -1)
	{
		perror("failed");
		exit(1);
	}

	/* deal with multiple connections */
	max_connect = backlog;		/* maximum connections */
	FD_ZERO(&main_fd);			/* initialize file descriptor set */
	FD_SET(serverfd, &main_fd); /* set the server's file descriptor */
	/* endless loop to process the connections */
	done = FALSE;
	while (!done)
	{
		/* backup the main set into a read set for processing */
		read_fd = main_fd;

		/* scan the connections for any activity */
		r = select(max_connect + 1, &read_fd, NULL, NULL, 0);
		if (r == -1)
		{
			perror("failed");
			exit(1);
		}

		/* process any connections */
		for (fd = 1; fd <= max_connect; fd++)
		{
			/* filter only active or new clients */
			if (FD_ISSET(fd, &read_fd)) /* returns true for any fd waiting */
			{
				/* filter out the server, which indicates a new connection */
				if (fd == serverfd)
				{
					/* add the new client */
					clientfd = accept(serverfd, &address, &address_len);
					if (clientfd == -1)
					{
						perror("failed");
						exit(1);
					}
					/* connection accepted, get name */
					r = getnameinfo(&address, address_len, hostname, hostname_size, 0, 0, NI_NUMERICHOST);
					/* update array */
					strcpy(connection[clientfd], hostname);
					printf("New connection from %s\n", connection[clientfd]);

					/* add new client socket to the master list */
					FD_SET(clientfd, &main_fd);

					/* respond to the connection */
					unsigned char binaryBuffer[BUFSIZ];
					strcpy(binaryBuffer, "Hello to ");
					strcat(binaryBuffer, connection[clientfd]);
					strcat(binaryBuffer, "!\n");
					strcat(binaryBuffer, welcome_msg);
					uint32_t code = 0;
					send(clientfd, &code, sizeof(uint32_t), 0);
					uint32_t urlLength = 0;
					send(clientfd, &urlLength, sizeof(uint32_t), 0);
					send(clientfd, binaryBuffer, BUFSIZ, 0);
					printf("%s", binaryBuffer);

					// set up rate limit and log new client
					t = time(NULL);
					tm = *localtime(&t);
					currentRequests = 0;
					lastSeconds = tm.tm_sec;
					lastMin = tm.tm_min;
					logger("Client connected", connection[clientfd]);
					printf("%d", clientfd);

				} /* end if, add new client */
				/* the current fd has incoming info - deal with it */
				else
				{
					// recieve the length of the binaryBuffer
					uint32_t length;
					recv(fd, &length, sizeof(uint32_t), 0);
					unsigned char binaryBuffer[length];

					// recieve the contents of the binaryBuffer one byte at a time
					uint32_t recvd_bytes = 0;
					unsigned char char_buffer = 0;
					while (recvd_bytes < length)
					{
						recv(fd, &char_buffer, 1, 0);
						binaryBuffer[recvd_bytes] = char_buffer;
						recvd_bytes++;
					}

					// if the binaryBuffer is not close, move on to processing URL
					if ((strncmp(binaryBuffer, "close", 5)) != 0)
					{
						// set up for the rate limit check
						t = time(NULL);
						tm = *localtime(&t);
						currentSeconds = tm.tm_sec;
						currentMin = tm.tm_min;

						if ((currentSeconds > lastSeconds) && (currentMin == lastMin))
						{
							currentRequests++;
						}
						else
						{
							currentRequests = 1;
						}

						// if rate limit is exceeded, send to client
						if (currentRequests > rateLimit)
						{
							uint32_t code = 3;
							send(fd, &code, sizeof(uint32_t), 0);
							uint32_t urlLength = 33;
							send(fd, &urlLength, sizeof(uint32_t), 0);
							char urlBuffer[33];
							strcpy(urlBuffer, "Too many requests. Please wait.\n");
							send(fd, urlBuffer, 33, 0);
							logger("Rate limit exceeded", connection[clientfd]);
						}
						// if rate limit is not exceeded
						else
						{
							printf("Current Request: %d\n", currentRequests);

							lastMin = currentMin;
							lastSeconds = currentSeconds;

							// open file and read the binaryBuffer into it
							FILE *fp = fopen("qr.png", "wb");
							fwrite(binaryBuffer, length, 1, fp);
							fclose(fp);

							// run Java command on file
							system("java -cp javase.jar:core.jar com.google.zxing.client.j2se.CommandLineRunner qr.png > QR_results.txt");

							// file with URL in it
							FILE *fp2;
							fp2 = fopen("QR_results.txt", "r");

							// determine length of url
							fseek(fp2, 0, SEEK_END);
							uint32_t urlLength = ftell(fp2);
							rewind(fp2);

							// read the URL file into the urlBuffer
							char urlBuffer[urlLength];
							fread(urlBuffer, urlLength, 1, fp2);
							fclose(fp2);

							// if file is not found
							// 44 is the length of the message sent when the image isn't found
							uint32_t code;
							if (urlLength == 44 || urlLength == 0)
							{
								code = 1;
								urlLength = 16;
								strcpy(urlBuffer, "Invalid QR code\n");
								logger("Invalid QR code", connection[clientfd]);

								// send code, urlLength and url contents
								send(fd, &code, sizeof(uint32_t), 0);
								send(fd, &urlLength, sizeof(uint32_t), 0);
								send(fd, urlBuffer, urlLength, 0);
							}
							else
							{
								code = 0;
								printf("%d", clientfd);
								logger("Valid QR sent", connection[clientfd]);

								// send code, urlLength and url contents
								send(fd, &code, sizeof(uint32_t), 0);
								send(fd, &urlLength, sizeof(uint32_t), 0);
								send(fd, urlBuffer, urlLength, 0);
							}
						}
					}
					else
					{
						/* clear the fd and close the connection */
						FD_CLR(fd, &main_fd); /* reset in the list */
						close(fd);			  /* disconnect */
						/* update the screen */
						printf("%s closed\n", connection[fd]);
						logger("Client disconnected", connection[clientfd]);
					}

				} /* end else to send/recv from client(s) */
			}	  /* end if */
		}		  /* end for loop through connections */
	}			  /* end while */

	puts("TCP Server shutting down");
	close(serverfd);
	freeaddrinfo(server);
	return (0);
}
