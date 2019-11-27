#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>

#include <sys/stat.h>
#include <sys/types.h>

#ifdef __MINGW32__
	#include <winsock2.h>
	#include <windows.h>
#else /* not a minGW compiler */
	#include <linux/limits.h>
	#include <sys/socket.h>
	#include <sys/sendfile.h>
	#include <netinet/in.h>
#endif /* __MINGW32__ */


#define DEFAULT_PORT 8899

int main(void)
{
	/**
	 * @var socket 		The server socket
	 * @var desc		File descriptor for socket
	 * @var fd			File descriptor for file to send
	 * @struct addr		Socket parameters for bind
	 * @struct addr1	Socket parameters for accept
	 * @var addrlen		Argument to accept
	 * @struct stat_buf	Argument to fstat
	 * @var offset		File offset
	 * @var filename	Filename to send
	 * @var rc			Holds return code of system calls
	 */
	int 				port = DEFAULT_PORT;
	int 				sock;
	int 				desc;
	int 				fd;
	struct sockaddr_in 	addr;
	struct sockaddr_in 	addr1;
	int    				addrlen;
	struct stat 		stat_buf;
	off_t 				offset = 0;
	char 				filename[PATH_MAX];
	int 				rc;
	
    printf("%d\n", sizeof(stat_buf));
	
	/* create Internet domain socket */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		fprintf(stderr, "Unable to create socket: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	/* fill in socket structure */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);
	
	/* bind socket to the port */
	rc =  bind(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (rc == -1) {
		fprintf(stderr, "unable to bind to socket: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	/* listen for clients on the socket */
	rc = listen(sock, 1);
	if (rc == -1) {
		fprintf(stderr, "listen failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	while (1) {
		
		/* wait for a client to connect */
		desc = accept(sock, (struct sockaddr *)  &addr1, &addrlen);
		if (desc == -1) {
			fprintf(stderr, "accept failed: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		
		/* get the file name from the client */
		rc = recv(desc, filename, sizeof(filename), 0);
		if (rc == -1) {
			fprintf(stderr, "recv failed: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		
		/* null terminate and strip any \r and \n from filename */
		filename[rc] = '\0';
		if (filename[strlen(filename)-1] == '\n')
			filename[strlen(filename)-1] = '\0';
		if (filename[strlen(filename)-1] == '\r')
			filename[strlen(filename)-1] = '\0';
		
		/* exit server if filename is "quit" */
		if (strcmp(filename, "quit") == 0) {
			fprintf(stderr, "quit command received, shutting down server\n");
			break;
		}
		
		fprintf(stderr, "received request to send file %s\n", filename);
		
		/* open the file to be sent */
		fd = open(filename, O_RDONLY);
		if (fd == -1) {
			fprintf(stderr, "unable to open '%s': %s\n", filename, strerror(errno));
			exit(EXIT_FAILURE);
		}
		
		/* get the size of the file to be sent */
		fstat(fd, &stat_buf);
		
		/* Send the file size */
		char send_size[16];
		sprintf(send_size, "%"PRIu64"", stat_buf.st_size);
		rc = send(desc, send_size, 16, 0);
		if (rc == -1) {
			fprintf(stderr, "Failed to send the file size\n");
			exit(EXIT_FAILURE);
		}
		
		fprintf(stderr, "File size to be sent [%"PRIu64"]\n", stat_buf.st_size);
		
		/* copy file using sendfile */
		offset = 0;
		int64_t sent = 0;
		while (sent < stat_buf.st_size && (rc = sendfile (desc, fd, &offset, stat_buf.st_size))) {
			if (rc == -1) {
				fprintf(stderr, "error from sendfile: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			else sent += rc;
		}
		
		
		if (sent != stat_buf.st_size) {
			fprintf(stderr, "incomplete transfer from sendfile: %d of %d bytes\n",
					rc,
		   (int)stat_buf.st_size);
			exit(EXIT_FAILURE);
		}
		
		/* close descriptor for file that was sent */
		close(fd);
		
		/* close socket descriptor */
		close(desc);
		
		puts("Ended");
	}
	
	/* close socket */
	close(sock);
	exit(EXIT_SUCCESS);
}