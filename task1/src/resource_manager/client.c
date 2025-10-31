#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <ctype.h>

#define EXAMPLE_SOCK_PATH "/tmp/example_resmgr.sock"
#define MAX_RECV_SIZE 1024

// Функция для чтения ответа от сервера
ssize_t read_response(int fd, char *buf, size_t max_len) {
    ssize_t total_n = 0;
    ssize_t n;
    
    // В цикле читаем все присланные данные, пока сервер не закроет соединение или не отправит 0 байт
    // Для данного сервера, он отправляет данные одним блоком (для 'r') или короткий ответ (для 'w')
    // Но для надежности лучше использовать цикл, хотя для простоты оставим один recv
    
    n = recv(fd, buf, max_len - 1, 0);
    if (n < 0) {
        perror("recv");
        return -1;
    }
    total_n = n;
    buf[total_n] = '\0';
    return total_n;
}

int main(int argc, char *argv[])
{
    // usage: ./client <command> [argument]
    if (argc < 2 || (strcmp(argv[1], "w") == 0 && argc != 3)) {
        fprintf(stderr, "Использование:\n");
        fprintf(stderr, "  Запись: %s w <данные>\n", argv[0]);
        fprintf(stderr, "  Чтение: %s r\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char command = tolower(argv[1][0]);
    const char *data_to_send = (argc == 3) ? argv[2] : NULL;
    
    if (command != 'r' && command != 'w') {
        fprintf(stderr, "Ошибка: Неизвестная команда '%c'. Используйте 'r' или 'w'.\n", command);
        return EXIT_FAILURE;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, EXAMPLE_SOCK_PATH, sizeof(addr.sun_path) - 1);

    // 1. Соединение (io_open)
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("Соединение установлено. Отправка команды '%c'.\n", command);

    // 2. Отправка команды (1 байт)
    if (send(fd, &command, 1, 0) != 1) {
        perror("send command");
        close(fd);
        return EXIT_FAILURE;
    }

    char buf[MAX_RECV_SIZE];
    ssize_t n;

    if (command == 'w') {
        // 3a. Для 'w': Отправка данных для записи
        size_t len = strlen(data_to_send);
        if (send(fd, data_to_send, len, 0) != (ssize_t)len) {
            perror("send data");
            close(fd);
            return EXIT_FAILURE;
        }
        printf("Отправлено %zu байт данных для записи.\n", len);

        // 4a. Получение ответа от io_write
        n = read_response(fd, buf, MAX_RECV_SIZE);
        if (n < 0) return EXIT_FAILURE;
        
        printf("Сервер ответил: %s\n", buf);

    } else if (command == 'r') {
        // 3b. Для 'r': Ожидание данных от io_read
        printf("Ожидание данных от сервера...\n");
        
        n = read_response(fd, buf, MAX_RECV_SIZE);
        if (n < 0) return EXIT_FAILURE;
        
        printf("Получено %zd байт данных:\n", n);
        // Выводим данные в сыром виде, так как они могут быть нестроковыми
        for (int i = 0; i < n; i++) {
            printf("%c", isprint(buf[i]) ? buf[i] : '.');
        }
        printf("\n");
        printf("(Буфер устройства содержит данные начиная со смещения 0)\n");
    }

    // 5. Закрытие соединения (io_close)
    close(fd);
    return EXIT_SUCCESS;
}