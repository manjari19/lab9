//client.c:

/*
1. What is the address of the server it is trying to connect to (IP address and port number).
   The client connects to IP address 127.0.0.1 on port 8000.
   This comes from the defines: ADDR "127.0.0.1" and PORT 8000,
   and the code that calls inet_pton() and connect() with those values.

2. Is it UDP or TCP? How do you know?
   It is TCP. In main(), the socket is created with:
       socket(AF_INET, SOCK_STREAM, 0)
   SOCK_STREAM means a TCP stream socket (UDP would use SOCK_DGRAM).

3. The client is going to send some data to the server. Where does it get this data from? How can you tell in the code?
   The data comes from standard input (stdin), i.e., whatever the user types
   in the terminal. In the loop:
       while ((num_read = read(STDIN_FILENO, buf, BUF_SIZE)) > 1) {
   it calls read() on STDIN_FILENO, then writes that data to the socket.

4. How does the client program end? How can you tell that in the code?
   The client keeps reading from stdin and sending data until read() returns
   a value <= 1 (for example, when the user sends EOF with Ctrl-D or just
   presses Enter so only a newline is read). Then the loop ends, it checks
   for read errors, closes the socket with close(sfd), and finally calls
   exit(EXIT_SUCCESS); so the program terminates normally.
*/

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8000
#define BUF_SIZE 64
#define ADDR "127.0.0.1"

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

int main() {
  struct sockaddr_in addr;
  int sfd;
  ssize_t num_read;
  char buf[BUF_SIZE];

  sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    handle_error("socket");
  }

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  if (inet_pton(AF_INET, ADDR, &addr.sin_addr) <= 0) {
    handle_error("inet_pton");
  }

  int res = connect(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
  if (res == -1) {
    handle_error("connect");
  }

  while ((num_read = read(STDIN_FILENO, buf, BUF_SIZE)) > 1) {
    if (write(sfd, buf, num_read) != num_read) {
      handle_error("write");
    }
    printf("Just sent %zd bytes.\n", num_read);
  }

  if (num_read == -1) {
    handle_error("read");
  }

  close(sfd);
  exit(EXIT_SUCCESS);
}


//server.c

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 64
#define PORT 8000
#define LISTEN_BACKLOG 32

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

// Shared counters for: total # messages, and counter of clients (used for
// assigning client IDs)
int total_message_count = 0;
int client_id_counter = 1;

// Mutexs to protect above global state.
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t client_id_mutex = PTHREAD_MUTEX_INITIALIZER;

struct client_info {
  int cfd;
  int client_id;
};

void *handle_client(void *arg) {
  struct client_info *client = (struct client_info *)arg;
  int cfd = client->cfd;
  int client_id = client->client_id;
  ssize_t num_read;
  char buf[BUF_SIZE];

  // Read messages from this client until it closes the connection.
  for (;;) {
    // Leave 1 byte for the null terminator so we can print as a string.
    num_read = read(cfd, buf, BUF_SIZE - 1);
    if (num_read <= 0) {
      // Error or client closed the connection.
      if (num_read == -1) {
        perror("read");
      }
      break;
    }

    // Null-terminate so printf("%s") is safe.
    buf[num_read] = '\0';

    // Increment total_message_count in a thread-safe way.
    pthread_mutex_lock(&count_mutex);
    total_message_count++;
    int current_msg = total_message_count;
    pthread_mutex_unlock(&count_mutex);

    // Print as in the sample output.
    printf("Msg #%4d; Client ID %d: %s\n", current_msg, client_id, buf);
    fflush(stdout);
  }

  printf("Ending thread for client %d\n", client_id);
  fflush(stdout);

  if (close(cfd) == -1) {
    perror("close");
  }

  free(client);

  return NULL;
}

int main() {
  struct sockaddr_in addr;
  int sfd;

  sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    handle_error("socket");
  }

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1) {
    handle_error("bind");
  }

  if (listen(sfd, LISTEN_BACKLOG) == -1) {
    handle_error("listen");
  }

  for (;;) {
    struct sockaddr_in caddr;
    socklen_t caddr_len = sizeof(struct sockaddr_in);

    int cfd = accept(sfd, (struct sockaddr *)&caddr, &caddr_len);
    if (cfd == -1) {
      if (errno == EINTR) {
        continue; // interrupted by signal so try again
      }
      handle_error("accept");
    }

    struct client_info *client = malloc(sizeof(struct client_info));
    if (client == NULL) {
      perror("malloc");
      close(cfd);
      continue;
    }
    client->cfd = cfd;

    pthread_mutex_lock(&client_id_mutex);
    client->client_id = client_id_counter++;
    pthread_mutex_unlock(&client_id_mutex);

    printf("New client created! ID %d on socket FD %d\n",
           client->client_id, client->cfd);
    fflush(stdout);

    pthread_t tid;
    int s = pthread_create(&tid, NULL, handle_client, client);
    if (s != 0) {
      errno = s;
      perror("pthread_create");
      close(cfd);
      free(client);
      continue;
    }

    s = pthread_detach(tid);
    if (s != 0) {
      errno = s;
      perror("pthread_detach");
    } 
  }

  if (close(sfd) == -1) {
    handle_error("close");
  }

  return 0;
}
