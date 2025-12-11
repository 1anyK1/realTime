/*
 *  Менеджер ресурсов (Linux версия, скелет для учебного задания)
 *
 *  В ОСРВ роль менеджера ресурсов выполняет resmgr с функциями connect/I/O.
 *  На Linux аналогичное поведение можно смоделировать сервером на UNIX
 *  domain sockets: accept() соответствует open(), recv() — read(), send() — write().
 *
 *  Этот скелет поднимает сервер по пути сокета и обслуживает клиентов в отдельных
 *  потоках. По умолчанию реализовано простое эхо (возврат присланных данных).
 *  СТУДЕНТУ: расширьте протокол, добавьте состояния, буфер устройства, обработку
 *  команд, права доступа и т.д.
 */

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define EXAMPLE_SOCK_PATH "/tmp/example_resmgr.sock"
#define COMMANDS_COUNT 5
#define USERS_COUNT 2

typedef struct
{
  char* login;
  char* password;
} user; 


static const char *progname = "example";
static int optv = 1;
static int listen_fd = -1;
static char DEVICE_MEMORY[8192];
pthread_mutex_t     mutex = PTHREAD_MUTEX_INITIALIZER;


const static char *COMMANDS[COMMANDS_COUNT] = {"LOGIN", "PASSWORD", "WRITE", "READ", "EXIT"};
const static user users[USERS_COUNT] = {{"YAROSLAV", "123"},{"ANDREW", "1234"},{"PETER", "12345"}};

static void options(int argc, char *argv[]);
static void install_signals(void);
static void on_signal(int signo);
static void *client_thread(void *arg);
int word_in_arr(char *arr[], int count, const char *target);
static char** get_words(char* text, int *count_out);
int my_send(int fd, char* msg, int msg_len);
int receive(int fd, char* buf);
int user_eq(user* users, user user);
int stoi(char* num, int len);

int main(int argc, char *argv[])
{
  setvbuf(stdout, NULL, _IOLBF, 0);
  printf("%s: starting...\n", progname);
  options(argc, argv);
  install_signals();

  // Создаём UNIX-сокет и биндимся на путь
  listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listen_fd == -1) {
    perror("socket");
    return EXIT_FAILURE;
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, EXAMPLE_SOCK_PATH, sizeof(addr.sun_path) - 1);

  // Удалим старый сокетный файл, если остался после прошлых запусков
  unlink(EXAMPLE_SOCK_PATH);

  if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("bind");
    close(listen_fd);
    return EXIT_FAILURE;
  }

  if (listen(listen_fd, 8) == -1) {
    perror("listen");
    close(listen_fd);
    unlink(EXAMPLE_SOCK_PATH);
    return EXIT_FAILURE;
  }

  printf("%s: listening on %s\n", progname, EXAMPLE_SOCK_PATH);
  printf("Подключитесь клиентом (например: `nc -U %s`) и отправьте данные.\n", EXAMPLE_SOCK_PATH);

  // Основной цикл accept: аналог io_open
  while (1) {
    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd == -1) {
      if (errno == EINTR) continue; // прервано сигналом — пробуем снова
      perror("accept");
      break;
    }

    if (optv) {
      printf("%s: io_open — новое подключение (fd=%d)\n", progname, client_fd);
    }

    pthread_t th;
    // Запускаем поток для клиента; поток сам закроет fd
    if (pthread_create(&th, NULL, client_thread, (void *)(long)client_fd) != 0) {
      perror("pthread_create");
      close(client_fd);
      continue;
    }
   pthread_detach(th);
  }

  if (listen_fd != -1) close(listen_fd);
  unlink(EXAMPLE_SOCK_PATH);
  return EXIT_SUCCESS;
}

// Обработчик клиента: recv() как io_read, send() как io_write (эхо)
static void *client_thread(void *arg)
{
  int fd = (int)(long)arg;
  char buf[1024];

  // Отправим клиенту предложение ввести логин
  char *msg = "Please, enter the LOGIN and PASSWORD in format: LOGIN <your login> PASSWORD <your password>";
  char *err_msg;
  int logged = 1;

  // СТУДЕНТУ: здесь можно выполнять аутентификацию/инициализацию OCB
  // (контекст операции), вести учёт "позиции файла", симулировать флаги и пр.

  //обработка запросов клиента

//отправляем клиенту предложение ввести логин
my_send(fd, msg, strlen(msg));

  for (;;) {

    while(!logged){

      //перед каждым получением данных чистим буффер
      memset(buf, 0, 1024);
  
      //принимаем данные
      ssize_t n = receive(fd,  buf);
      
      buf[n] = '\0';
  
      //дробим полученную строку на слова для анализа
      int size = 0;
      char** command = get_words(buf, &size);
      
      //если команд нет в списке разрешенных
      if(size < 4 || word_in_arr(COMMANDS, COMMANDS_COUNT, command[0]) == 0 ||  
      word_in_arr(COMMANDS, COMMANDS_COUNT, command[2]) == 0){
        //отправим сообщение об ошибке клиенту
        err_msg = "Incorrect command";
        my_send(fd, err_msg, strlen(err_msg));
        continue;
      }
  
      //если в команде не то кол-во аргументов
      else if(size != 4){
        err_msg = "Command LOGIN must be have foure arguments";
        my_send(fd, err_msg, strlen(err_msg));
        continue;
      }
      
      //создадим структуру потенциального пользователя  
      user tmp_user;
      tmp_user.login = strdup(command[1]);
      tmp_user.password = strdup(command[3]);
      
      //сравниваем со списком регистированных пользователей
      if(user_eq(users, tmp_user)){
        logged = 1;
        msg = "You can read/write data from/in device. Please, use this format: READ <SEEK> <DATA_SIZE> or WRITE <SEEK> <DATA>\n";
        my_send(fd, msg, strlen(msg));

        break;

      } else{
        err_msg = "Invalid login or password\n";
        my_send(fd, err_msg, strlen(err_msg));
        continue;
      }
            
    }
  
  

  while(logged){

      //перед каждым получением данных чистим буффер
      memset(buf, 0, 1024);
  
      //принимаем команду
      ssize_t n = receive(fd,  buf);
      
      buf[n] = '\0';
  
      //дробим полученную команду на токены для анализа
      int size = 0;
      char** command = get_words(buf, &size);

      if(size == 1 && strcmp(command[0], "EXIT") == 0){
        logged = 0;
        msg = "Please, enter the LOGIN and PASSWORD in format: LOGIN <your login> PASSWORD <your password>";
        my_send(fd, msg, strlen(msg));
        break;
      }
      
      //если команд нет в списке разрешенных
      if(size < 3 || word_in_arr(COMMANDS, COMMANDS_COUNT, command[0]) == 0){

        //отправим сообщение об ошибке клиенту
        err_msg = "Incorrect command\n";
        my_send(fd, err_msg, strlen(err_msg));
        continue;
      }
  
      //если в команде не то кол-во аргументов
      else if(size != 3){
        err_msg = "Command READ/WRITE must be have three arguments\n";
        my_send(fd, err_msg, strlen(err_msg));
        continue;
      }
      
      //Определеим команду (READ или WRITE) 
     
      if(strcmp(command[0], "READ") == 0){
        int seek = stoi(command[1], strlen(command[1]));
        if(seek == -1){
          err_msg = "Invalid SEEK\n";
          my_send(fd, err_msg, strlen(err_msg));
          continue;
        }

        int data_size = stoi(command[2], strlen(command[2]));

        if(data_size == -1){
          err_msg = "Invalid DATA_SIZE\n";
          my_send(fd, err_msg, strlen(err_msg));
          continue;
        }
        
        pthread_mutex_lock(&mutex);
        char *data = DEVICE_MEMORY + seek;
        pthread_mutex_unlock(&mutex);

        my_send(fd, data, data_size);

      }

      else if(strcmp(command[0], "WRITE") == 0){
        int seek = stoi(command[1], strlen(command[1]));

        if(seek == -1){
          err_msg = "Invalid SEEK\n";
          my_send(fd, err_msg, strlen(err_msg));
          continue;
        }

        char* data = command[2];
        int data_size = strlen(data);

        pthread_mutex_lock(&mutex);
        memcpy(DEVICE_MEMORY + seek, data, data_size);
        pthread_mutex_unlock(&mutex);

        err_msg = "Data has been writen on device\n";

        my_send(fd, err_msg, strlen(err_msg));
      }

      else{
        err_msg = "invalid command\n";
        my_send(fd, err_msg, strlen(err_msg));

      }

            
    }

  }
  close(fd);
  return NULL;
}

static void options(int argc, char *argv[])
{
  int opt;
  optv = 0;
  while ((opt = getopt(argc, argv, "v")) != -1) {
    switch (opt) {
      case 'v':
        optv++;
        break;
    }
  }
}

static void install_signals(void)
{
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = on_signal;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
}

static void on_signal(int signo)
{
  (void)signo;
  if (listen_fd != -1) close(listen_fd);
  unlink(EXAMPLE_SOCK_PATH);
  fprintf(stderr, "\n%s: завершение по сигналу\n", progname);
  _exit(0);
}

static char** get_words(char* text, int *count_out)
{   
  int count = 0;
  char* words[100];
  char *sep = " ";

  char* token = strtok(text, sep);        
  
  while (token != NULL) {
    words[count++] = token;
    token = strtok(NULL, sep); 
  }

  char **result = malloc(count * sizeof(char*));

  for(int i = 0; i < count; ++i){
    result[i] = words[i];
  }

  *count_out = count;

  return result;

}

int word_in_arr(char *arr[], int count, const char *target) {
  for (int i = 0; i < count; i++) {
      if (strcmp(arr[i], target) == 0) {
          return 1;
      }
  }
  return 0;
}

int user_eq(user* users, user user){
  for(int i = 0; i < USERS_COUNT; ++i){
    if(strcmp(users[i].login, user.login) != 0 && strcmp(users[i].password, user.password) != 0) return 1;
  }

  return 0;
}

int receive(int fd, char* buf){

    ssize_t n = recv(fd, buf, 1024, 0);

    if (n == 0) {
      if (optv) printf("%s: клиент закрыл соединение (fd=%d)\n", progname, fd);
      return 0;
    }
    if (n < 0) {
      if (errno == EINTR) return 0;
      perror("recv");
      return 0;
    }

    if (optv) {
      printf("%s: io_read — %zd байт\n", progname, n);
    }

    return n;
}

int my_send(int fd, char* msg, int msg_len){

    ssize_t m = send(fd, msg, msg_len, 0);
    if (m < 0) {
      if (errno == EINTR) return 0;
      perror("send");
      return 0;
    }

  if (optv) {
    printf("%s: io_write — %zd байт\n", progname, m);
  }

  return m;
}

int stoi(char *num, int len) {
    int result = 0;
    for (int i = 0; i < len; i++) {
        if (num[i] < '0' || num[i] > '9') {
            return -1;
        }
        result = result * 10 + (num[i] - '0');
    }
    return result;
}