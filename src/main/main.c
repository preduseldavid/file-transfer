/**
 * @file main.c
 * @brief The main file(and function) of project
 */

#define DEFAULT_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef UNIX
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif /* UNIX */

#include "config.h"
#include "data_types.h"
#include "error.h"
#include "receive.h"
#include "send.h"
#include "tcpip_server.h"
#include "user_thread.h"

/* INTERNAL FUNCTIONS */
static void callback_on_accept(SOCKET *sock, struct sockaddr_in *client_addr);

/* GLOBAL VARIABLES */
TC_t TC;

int main(int argc, char **argv) {
  signal(SIGPIPE, SIG_IGN);

  int ret;

  TC.lock = 0;

  if (argc == 2) {
    /* create the server part on a different thread */
    tcpip_server_t *server = tcpip_server_create(PORT);
    if (!server) {
      exit(EXIT_FAILURE);
    }

    ret = tcpip_server_setstate(server, &callback_on_accept, LISTEN);
    if (ret != 0) {
      exit(EXIT_FAILURE);
    }
  }

  /* use this thread to handle user's input data, as the client side */
  user_thread_arg_t *arg =
      (user_thread_arg_t *)malloc(sizeof(user_thread_arg_t));
  arg->TC = &TC;
  user_thread(arg);

  fprintf(stdout, "Closing...\n");

  exit(EXIT_SUCCESS);
}

static void callback_on_accept(SOCKET *sock, struct sockaddr_in *client_addr) {
  SOCKET sock_desc = *sock;
  struct sockaddr_in client_info = *client_addr;
  char *client_ip = inet_ntoa(client_info.sin_addr);
  net_packet_t *packet = NULL;

  /* release the arguments */
  free(sock);
  free(client_addr);

  /* Ask for permission to accept a transfer
   * Lock the TC lock and the user thread will unlock it after will get an
   * answer
   */
  while (__sync_lock_test_and_set(&TC.lock, 1)) {
    usleep(100);
  }
  fprintf(stdout, "\nNew connection from %s... Do you accept it? [Y/N]\n",
          client_ip);
  fflush(stdout);
  /* wait for answer */
  while (__sync_lock_test_and_set(&TC.lock, 1)) {
    usleep(100);
  }

  if (TC.action & ALLOW_ACTION) {
    packet = recv_packet(sock_desc, 0);
    if (packet) {
      if (packet->flags.val & START_TRANSFER) {
        if (packet->flags.val & SEND_OPERATION) {
          __recv(sock_desc, packet->flags.val, RECEIVING_PATH);
        } else if (packet->flags.val & RECEIVE_OPERATION) {
          __send(sock_desc, packet->data);
        }
      }
      destroy_packet(packet);
    } else {
      fprintf(stdout, "Connection lost...");
    }
  }
  /* release the TC lock */
  __sync_lock_release(&TC.lock);

  close(sock_desc);
}
