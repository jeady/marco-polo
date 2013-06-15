#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

unsigned char transmit_marco(int tty_fd, struct timeval* transmit_time) {
  unsigned char marco_str[] = "S__E";
  unsigned char a = (unsigned char)rand();
  unsigned char b = (unsigned char)rand();
  unsigned char sum = a + b;

  marco_str[1] = a;
  marco_str[2] = b;

  write(tty_fd, marco_str, 4);
  if (transmit_time != NULL)
    gettimeofday(transmit_time, NULL);

  printf("Transmitted %d + %d = %d.\n", a, b, sum);

  return marco_str[1] + marco_str[2];
}

int main(int argc,char** argv)
{
  struct termios tio;
  int tty_fd;
  unsigned char read_buffer[3] = {0};
  int epfd;
  struct epoll_event tio_event;
  struct epoll_event event;
  unsigned char marco_sum;
  unsigned sent = 1;
  unsigned success = 0;
  struct timeval transmit_time;
  double total_latency = 0;

  srand(time(NULL));

  memset(&tio,0,sizeof(tio));
  tio.c_iflag = 0;
  tio.c_oflag = 0;
  tio.c_cflag = CS8 | CREAD | CLOCAL;
  tio.c_lflag = 0;
  tio.c_cc[VMIN] = 1;
  tio.c_cc[VTIME] = 5;

  tty_fd = open(argv[1], O_RDWR | O_NONBLOCK);
  cfsetospeed(&tio, B1200);
  cfsetispeed(&tio, B1200);
  tcsetattr(tty_fd, TCSANOW, &tio);

  epfd = epoll_create(1);
  tio_event.events = EPOLLIN | EPOLLHUP | EPOLLERR;
  tio_event.data.fd = tty_fd;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, tty_fd, &tio_event)) {
    fprintf(stderr, "Could not add tio to epoll: %s\n", strerror(errno));
    return 1;
  }

  marco_sum = transmit_marco(tty_fd, &transmit_time);
  while (epoll_wait(epfd, &event, 1, -1) == 1) {
    if (event.events & EPOLLIN && event.data.fd == tty_fd) {
      struct timeval receive_time;
      gettimeofday(&receive_time, NULL);

      memmove(read_buffer, read_buffer + 1, sizeof(read_buffer) - 1);
      read(tty_fd, read_buffer + sizeof(read_buffer) - 1, 1);

      if (read_buffer[0] == 's' && read_buffer[2] == 'e') {
        if (read_buffer[1] == marco_sum) {
          struct timeval elapsed_time;
          float elapsed;
          timersub(&receive_time, &transmit_time, &elapsed_time);
          elapsed = (float)elapsed_time.tv_sec * 1000. +
                    (float)elapsed_time.tv_usec / 1000.;

          total_latency += elapsed;
          success++;
          printf(
              "Success! "
              "%.2f%% success rate (%d / %d), "
              "avg. latency %.2fms\n",
              (float)success / (float)sent * 100.,
              success,
              sent,
              total_latency / (float)success);

        } else {
          printf(
              "Incorrect sum. "
              "%.2f%% success rate (%d / %d), "
              "avg. latency %.2fms\n",
              (float)success / (float)sent * 100.,
              success,
              sent,
              total_latency / (float)success);
        }
        sleep(1);
        marco_sum = transmit_marco(tty_fd, &transmit_time);
        sent++;
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
