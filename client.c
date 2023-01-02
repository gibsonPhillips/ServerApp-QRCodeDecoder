#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>

int main(int argc, char *argv[])
{
	const char *port = argv[2];
	char *server;
	struct addrinfo hints, *host;
	int r, sockfd;
	char buffer[BUFSIZ];
	char urlBuffer[BUFSIZ];

	if (argc < 3)
	{
		fprintf(stderr, "Format: client hostname, listening port\n");
		exit(1);
	}
	server = argv[1];

	/* obtain and convert server name and port */
	printf("Looking for server on %s...", server);
	memset(&hints, 0, sizeof(hints)); /* use memset_s() */
	hints.ai_family = AF_INET;		  /* IPv4 */
	hints.ai_socktype = SOCK_STREAM;  /* TCP */
	r = getaddrinfo(server, port, &hints, &host);
	if (r != 0)
	{
		perror("failed");
		exit(1);
	}
	puts("found");

	/* create a socket */
	sockfd = socket(host->ai_family, host->ai_socktype, host->ai_protocol);
	if (sockfd == -1)
	{
		perror("failed");
		exit(1);
	}

	/* connect to the socket */
	r = connect(sockfd, host->ai_addr, host->ai_addrlen);
	if (r == -1)
	{
		perror("failed");
		exit(1);
	}

	/* loop to interact with the server */
	while (1)
	{
		// code representing client-server interaction
		uint32_t code;
		recv(sockfd, &code, sizeof(uint32_t), 0);
		printf("Code: %d\n", code);

		// length of URL
		uint32_t urlLength;
		recv(sockfd, &urlLength, sizeof(uint32_t), 0);
		printf("Length: %d\n", urlLength);

		// contents of URL
		r = recv(sockfd, buffer, BUFSIZ, 0);
		if (r < 1)
		{
			puts("Connection closed by peer");
			break;
		}

		buffer[r] = '\0';
		printf("%s", buffer);

		/* local input  */
		printf("PNG File: ");
		scanf("%s", buffer);

		// if client wants to disconnect session
		if (strncmp(buffer, "close", 5) == 0)
		{
			puts("Closing connection");
			uint32_t length = 5;
			unsigned char binaryBuffer[length];
			strcpy(binaryBuffer, "close");
			// printf("%s", binaryBuffer);
			send(sockfd, &length, sizeof(uint32_t), 0);
			send(sockfd, binaryBuffer, length, 0);
			break;
		}

		FILE *fp;
		fp = fopen(buffer, "rb");

		// determine length of binaryBuffer
		fseek(fp, 0, SEEK_END);
		uint32_t length = ftell(fp);
		rewind(fp);

		// read data into the binaryBuffer
		unsigned char binaryBuffer[length];
		fread(binaryBuffer, length, 1, fp);
		fclose(fp);

		// add multipe sends for each thing we need to send
		send(sockfd, &length, sizeof(uint32_t), 0);
		send(sockfd, binaryBuffer, length, 0);
		//}

	} /* end while loop */

	/* all done, clean-up */
	freeaddrinfo(host);
	close(sockfd);
	puts("Disconnected");

	return (0);
}
