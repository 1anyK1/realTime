#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <time.h>

#define EXAMPLE_SOCK_PATH "/tmp/example_resmgr.sock"
#define WRITE_MSG "WRITE 0 MSG_NUM1"
#define WRITE_MSG_2 "WRITE 0 MSG_NUM2"
#define WRITE_MSG_3 "WRITE 0 MSG_NUM3"

#define READ_MSG "READ 0 10"

int main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(stderr, "usage: %s <message>\n", argv[0]);
    return EXIT_FAILURE;
  }

  setvbuf(stdout, NULL, _IONBF, 0);


  srand(time(NULL));

  const char *messages[] = {WRITE_MSG, WRITE_MSG_2, WRITE_MSG_3};

  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd == -1) {
    perror("socket");
    return EXIT_FAILURE;
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, EXAMPLE_SOCK_PATH, sizeof(addr.sun_path) - 1);

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("connect");
    close(fd);
    return EXIT_FAILURE;
  }

  const char *msg = "Hello";
  size_t len = strlen(msg);

  char recv_buf[8192];
  char send_buf[30];

  //примем сообщение от сервера 
  ssize_t n = recv(fd, recv_buf, sizeof(recv_buf) - 1, 0);
  if (n < 0) {
    perror("recv");
    close(fd);
    return EXIT_FAILURE;
  }
  recv_buf[n] = '\0';
  printf("response: %s\n", recv_buf);

  while (1){
    // printf("Please, enter the command: ");

    // if (fgets(send_buf, sizeof(send_buf), stdin) != NULL) {
    //     send_buf[strcspn(send_buf, "\n")] = '\0';
    // } 
    if(strcmp(argv[1], "-w") == 0){
      usleep(0.5 * pow(10, 6));
      int msg_num = rand() % 3;
      if (send(fd, messages[msg_num] ,strlen(messages[msg_num]), 0) != strlen(messages[msg_num])) {
      perror("send");
      close(fd);
      return EXIT_FAILURE;
    } else{
      printf("Send to server: %s\n", messages[msg_num]);
    }
  }

    if(strcmp(argv[1], "-r") == 0){
      usleep(0.5 * pow(10, 6));
      if (send(fd, READ_MSG ,strlen(READ_MSG), 0) != strlen(READ_MSG)) {
      perror("send");
      close(fd);
      return EXIT_FAILURE;
    }else{
      printf("Send to server: %s\n", READ_MSG);
      ssize_t n = recv(fd, recv_buf, sizeof(recv_buf) - 1, 0);
      if (n < 0) {
        perror("recv");
        close(fd);
        return EXIT_FAILURE;
      }

      recv_buf[n] = '\0';
      
      printf("response: %s\n", recv_buf);
      }
    }
      memset(recv_buf, 0, sizeof(recv_buf));
      fflush(stdout);
}
  close(fd);
  return EXIT_SUCCESS;
}
