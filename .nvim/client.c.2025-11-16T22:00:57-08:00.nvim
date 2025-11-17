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
