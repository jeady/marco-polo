#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

int gResponseTimeoutMS = 300;
int gTransmitDelayMS = 1000;
bool gDebug = false;

void debug(const char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  if (gDebug)
    vprintf(fmt, va);
  va_end(va);
}

unsigned char transmit_marco(
    int tty_fd,
    struct timeval* transmit_time,
    int timerfd) {
  unsigned char marco_str[] = "S__E";
  unsigned char a = (unsigned char)rand();
  unsigned char b = (unsigned char)rand();
  unsigned char sum = a + b;
  struct itimerspec tv = {0};

  tv.it_value.tv_nsec = gResponseTimeoutMS * 1000000;

  marco_str[1] = a;
  marco_str[2] = b;

  write(tty_fd, marco_str, 4);
  if (transmit_time != NULL)
    gettimeofday(transmit_time, NULL);
  timerfd_settime(timerfd, 0, &tv, NULL);

  debug("Transmitted %d + %d = %d.\n", a, b, sum);

  return marco_str[1] + marco_str[2];
}

void disarm_timerfd(int fd) {
  struct itimerspec tv = {0};
  timerfd_settime(fd, 0, &tv, NULL);
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
  int timer_fd;
  struct epoll_event timer_event;
  int opt;

  if (argc < 2) {
    printf("Usage: %s [-v] /dev/serial-device\n", argv[0]);
    return 0;
  }

  while ((opt = getopt(argc, argv, "nt:")) != -1) {
    switch (opt) {
    case 'v':
      gDebug = 1;
      break;
    default:
      fprintf(stderr, "Usage: %s [-v] /dev/serial-device\n", argv[0]);
      return 1;
    }
  }

  srand(time(NULL));

  memset(&tio,0,sizeof(tio));
  tio.c_iflag = 0;
  tio.c_oflag = 0;
  tio.c_cflag = CS8 | CREAD | CLOCAL;
  tio.c_lflag = 0;
  tio.c_cc[VMIN] = 1;
  tio.c_cc[VTIME] = 5;

  tty_fd = open(argv[1], O_RDWR | O_NONBLOCK);
  if (tty_fd == -1) {
    fprintf(stderr, "Could not open serial device %s\n", argv[1]);
  }

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

  timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
  timer_event.events = EPOLLIN;
  timer_event.data.fd = timer_fd;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, timer_fd, &timer_event)) {
    fprintf(stderr, "Could not add timer_fd to epoll: %s\n", strerror(errno));
    return 1;
  }

  marco_sum = transmit_marco(tty_fd, &transmit_time, timer_fd);
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
        usleep(gTransmitDelayMS * 1000);
        marco_sum = transmit_marco(tty_fd, &transmit_time, timer_fd);
        sent++;
      }
    } else if (event.events & EPOLLIN && event.data.fd == timer_fd) {
      uint64_t expirations;
      int r = read(timer_fd, &expirations, sizeof(expirations));
      if (r != sizeof(expirations)) {
        fprintf(stderr, "Unexpected number of bytes from timerfd: %d.\n", r);
        return 1;
      } else if (expirations != 1) {
        fprintf(stderr, "Timerfd expired too many times: %ld\n", expirations);
        return 1;
      }

      printf("Timeout, resending. "
             "%.2f%% success rate (%d / %d), "
             "avg. latency %.2fms\n",
             (float)success / (float)sent * 100.,
             success,
             sent,
             total_latency / (float)success);
      usleep(gTransmitDelayMS * 1000);
      marco_sum = transmit_marco(tty_fd, &transmit_time, timer_fd);
      sent++;
    } else {
      fprintf(stderr, "Unknown epoll FD: %d\n", event.data.fd);
    }

    if (event.events & EPOLLHUP || event.events & EPOLLERR) {
      fprintf(stderr, "Shutting down.\n");
      break;
    }
  }

  usleep(gTransmitDelayMS * 1000);
  close(tty_fd);
  return 0;
}
