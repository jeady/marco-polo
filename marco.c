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

void disarm_timerfd(int fd) {
  struct itimerspec tv = {0};
  timerfd_settime(fd, 0, &tv, NULL);
}

void set_timerfd(int fd, int timeout_ms) {
  struct itimerspec tv = {0};
  int s = timeout_ms / 1000;
  int ms = timeout_ms % 1000;
  disarm_timerfd(fd);

  tv.it_value.tv_sec = s;
  tv.it_value.tv_nsec = ms * 1000000;
  timerfd_settime(fd, 0, &tv, NULL);
}

int add_timer_to_epoll(int epfd) {
  struct epoll_event event;
  int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
  event.events = EPOLLIN;
  event.data.fd = fd;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event)) {
    fprintf(
        stderr,
        "Could not add timerfd to epoll: %s\n",
        strerror(errno));
    exit(1);
  }

  return fd;
}

void verify_timerfd_fire(struct epoll_event e) {
  uint64_t expirations;
  int r = read(e.data.fd, &expirations, sizeof(expirations));

  if (r != sizeof(expirations)) {
    fprintf(stderr, "Unexpected number of bytes from timerfd: %d.\n", r);
    exit(1);
  } else if (expirations != 1) {
    fprintf(stderr, "Timerfd expired too many times: %ld\n", expirations);
    exit(1);
  }
}

unsigned char transmit_marco(
    int tty_fd,
    struct timeval* transmit_time,
    int timerfd) {
  unsigned char marco_str[] = "S__E";
  unsigned char a = (unsigned char)rand();
  unsigned char b = (unsigned char)rand();
  unsigned char sum = a + b;

  marco_str[1] = a;
  marco_str[2] = b;

  write(tty_fd, marco_str, 4);
  if (transmit_time != NULL)
    gettimeofday(transmit_time, NULL);
  set_timerfd(timerfd, gResponseTimeoutMS);

  debug("Transmitted %d + %d = %d.\n", a, b, sum);

  return marco_str[1] + marco_str[2];
}

void schedule_transmit(int fd) {
  set_timerfd(fd, gTransmitDelayMS);
}

void print_stats(int success, int dropped, int corrupt, float latency) {
  int sent = success + dropped + corrupt;

  printf(
      "%.2f%% success rate (%d / %d) "
      "%.2f%% drop rate (%d / %d) "
      "%.2f%% corrupt rate (%d / %d) "
      "avg. latency %.2fms\n",
      (float)success / (float)sent * 100.,
      success, sent,
      (float)dropped / (float)sent * 100.,
      dropped, sent,
      (float)corrupt / (float)sent * 100.,
      corrupt, sent,
      latency / (float)success);
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
  unsigned success = 0;
  unsigned dropped = 0;
  unsigned corrupt = 0;
  struct timeval transmit_time;
  double total_latency = 0;
  int timeout_timer_fd;
  int transmit_timer_fd;
  int idle_timer_fd;
  struct epoll_event timer_event;
  int opt;
  char* device_name;

  if (argc < 2) {
    printf("Usage: %s [-v] /dev/serial-device\n", argv[0]);
    return 0;
  }

  while ((opt = getopt(argc, argv, "vd:")) != -1) {
    switch (opt) {
    case 'v':
      gDebug = 1;
      break;
    case 'd':
      device_name = malloc(strlen(optarg) + 1);
      strncpy(device_name, optarg, strlen(optarg) + 1);
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

  tty_fd = open(device_name, O_RDWR | O_NONBLOCK);
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

  timeout_timer_fd = add_timer_to_epoll(epfd);
  transmit_timer_fd = add_timer_to_epoll(epfd);
  idle_timer_fd = add_timer_to_epoll(epfd);

  schedule_transmit(transmit_timer_fd);
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
          disarm_timerfd(timeout_timer_fd);
          success++;

          timersub(&receive_time, &transmit_time, &elapsed_time);
          elapsed = (float)elapsed_time.tv_sec * 1000. +
                    (float)elapsed_time.tv_usec / 1000.;
          total_latency += elapsed;

          debug("Success! %.4fms elapsed.\n", elapsed);
          print_stats(success, dropped, corrupt, total_latency);
        } else {
          disarm_timerfd(timeout_timer_fd);
          corrupt++;

          debug("Corrupt.\n");
          print_stats(success, dropped, corrupt, total_latency);
        }
        schedule_transmit(transmit_timer_fd);
      }
    } else if (event.events & EPOLLIN && event.data.fd == timeout_timer_fd) {
      verify_timerfd_fire(event);
      dropped++;

      debug("Timeout.\n");
      print_stats(success, dropped, corrupt, total_latency);
      schedule_transmit(transmit_timer_fd);
    } else if (event.events & EPOLLIN && event.data.fd == transmit_timer_fd) {
      verify_timerfd_fire(event);
      marco_sum = transmit_marco(tty_fd, &transmit_time, timeout_timer_fd);
    } else if (event.events & EPOLLIN && event.data.fd == idle_timer_fd) {
      verify_timerfd_fire(event);
    } else {
      fprintf(stderr, "Unknown epoll FD: %d\n", event.data.fd);
    }

    if (event.events & EPOLLHUP || event.events & EPOLLERR) {
      fprintf(stderr, "Shutting down.\n");
      break;
    }
  }

  close(tty_fd);
  return 0;
}
