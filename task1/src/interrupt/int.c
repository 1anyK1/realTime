#define _POSIX_C_SOURCE 199309L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

static volatile sig_atomic_t counter = 0;
static volatile sig_atomic_t mess = 0;

static void on_alarm(int signo) {
  (void)signo;
  if (++counter == 100) {
    counter = 0;
    mess++;
    write(STDOUT_FILENO, "100 events\n", 11);
  }
}

int main(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = on_alarm;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGALRM, &sa, NULL);

  struct itimerval itv;
  // период 10 мс (для демонстрации). При необходимости подберите значения.
  itv.it_interval.tv_sec = 0;
  itv.it_interval.tv_usec = 10000; // 10ms
  itv.it_value = itv.it_interval;  // стартовое значение
  if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
    perror("setitimer");
    return EXIT_FAILURE;
  }

  // Ждём 10 сообщений по 100 событий => 1000 тиков ~ 10 секунд
  while(mess < 10){
    pause;
  }

  // Остановим таймер
  memset(&itv, 0, sizeof(itv));
  setitimer(ITIMER_REAL, &itv, NULL);
  return EXIT_SUCCESS;
}