#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <linux/limits.h>
#include <inttypes.h>

#define PORT_NUMBER     8899
#define SERVER_ADDRESS  "localhost"
#define FILENAME        "get_MAC.c"

int main(int argc, char **argv)
{
    int client_socket;
    ssize_t len;
    struct sockaddr_in remote_addr;
    char buffer[BUFSIZ];
    uint64_t file_size;
    FILE *received_file;
    uint64_t recv_data = 0;
    
    /* Zeroing remote_addr struct */
    memset(&remote_addr, 0, sizeof(remote_addr));
    
    /* Construct remote_addr struct */
    remote_addr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_ADDRESS, &(remote_addr.sin_addr));
    remote_addr.sin_port = htons(PORT_NUMBER);
    
    /* Create client socket */
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        fprintf(stdout, "Error creating socket --> %s\n", strerror(errno));
        
        exit(EXIT_FAILURE);
    }
    
    /* Connect to the server */
    if (connect(client_socket, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) == -1) {
        fprintf(stdout, "Error on connect --> %s\n", strerror(errno));
        
        exit(EXIT_FAILURE);
    }
    
    char filename[] = FILENAME;
    len = send(client_socket, filename, PATH_MAX, 0);
    if (len == -1) {
        fprintf(stdout, "Failed to send file name\n");
        exit(EXIT_FAILURE);
    }
    
    /* Receiving file size */
    recv(client_socket, buffer, BUFSIZ, 0);
    file_size = strtoull(buffer, NULL, 10);
    //fprintf(stdout, "\nFile size : %d\n", file_size);
    
    received_file = fopen(FILENAME, "w");
    if (received_file == NULL) {
        fprintf(stdout, "Failed to open file foo --> %s\n", strerror(errno));
        
        exit(EXIT_FAILURE);
    }
    
    fprintf(stdout, "File size to be received [%"PRIu64"]\n", file_size);
    
    while (recv_data != file_size && (len = recv(client_socket, buffer, BUFSIZ, 0)) > 0) {
        fwrite(buffer, sizeof(char), len, received_file);
        recv_data += len;
    }
    
    
    fclose(received_file);
    
    close(client_socket);
    
    return 0;
}
