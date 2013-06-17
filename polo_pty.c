#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

int main(int argc,char** argv)
{
  struct termios tio;
  int tty_fd;
  int tty_child_fd;
  char tty_name[64] = {0};
  unsigned char read_buffer[4] = {0};
  int epfd;
  struct epoll_event tio_event;
  struct epoll_event event;
  unsigned char polo_str[] = "s_e";
  float success_rate = 50.;  // From 0 to 100.

  srand(time(NULL));

  memset(&tio,0,sizeof(tio));
  tio.c_iflag = 0;
  tio.c_oflag = 0;
  tio.c_cflag = CS8 | CREAD | CLOCAL;
  tio.c_lflag = 0;
  tio.c_cc[VMIN] = 1;
  tio.c_cc[VTIME] = 5;
  cfsetospeed(&tio, B1200);
  cfsetispeed(&tio, B1200);

  openpty(&tty_fd, &tty_child_fd, tty_name, &tio, NULL);
  tcsetattr(tty_fd, TCSANOW, &tio);

  printf("PTY: %s\n", tty_name);

  epfd = epoll_create(1);
  tio_event.events = EPOLLIN | EPOLLHUP | EPOLLERR;
  tio_event.data.fd = tty_fd;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, tty_fd, &tio_event)) {
    fprintf(stderr, "Could not add tio to epoll: %s\n", strerror(errno));
    return 1;
  }

  while (epoll_wait(epfd, &event, 1, -1) == 1) {
    if (event.events & EPOLLIN && event.data.fd == tty_fd) {
      memmove(read_buffer, read_buffer + 1, sizeof(read_buffer) - 1);
      read(tty_fd, read_buffer + sizeof(read_buffer) - 1, 1);
      if (read_buffer[0] == 'S' && read_buffer[3] == 'E') {
        unsigned char a = read_buffer[1];
        unsigned char b = read_buffer[2];
        unsigned char sum = a + b;
        useconds_t delay = (float)(rand() % 100);  // ms
        usleep(delay * 1000);
        printf("Received %d + %d = %d.\n", a, b, sum);

        if ((float)(rand() % 100) <= success_rate) {
          // Success
          polo_str[1] = sum;
          write(tty_fd, polo_str, 3);
          printf("Delayed %dms\n", delay);
        } else {
          // Failure, pick whether to transmit a bad response or no response.
          if (rand() % 2) {
            // Respond with invalid bits
            printf("Sending back corrupted data.\n");
            polo_str[1] = sum + 1;
            write(tty_fd, polo_str, 3);
          } else {
            // No response.
            printf("Not responding.\n");
          }
        }
      }
    } else {
      fprintf(stderr, "Unknown epoll FD: %d\n", event.data.fd);
    }

    if (event.events & EPOLLHUP || event.events & EPOLLERR) {
      fprintf(stderr, "Shutting down.\n");
      break;
    }
  }

  sleep(1);
  close(tty_fd);
  return 0;
}
