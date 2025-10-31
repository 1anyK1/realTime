/*
 * Менеджер ресурсов (Linux версия, скелет для учебного задания)
 *
 * В ОСРВ роль менеджера ресурсов выполняет resmgr с функциями connect/I/O.
 * На Linux аналогичное поведение можно смоделировать сервером на UNIX
 * domain sockets: accept() соответствует open(), recv() — read(), send() — write().
 *
 * Этот скелет поднимает сервер по пути сокета и обслуживает клиентов в отдельных
 * потоках. По умолчанию реализовано простое эхо (возврат присланных данных).
 * СТУДЕНТУ: расширьте протокол, добавьте состояния, буфер устройства, обработку
 * команд, права доступа и т.д.
 */
#define _POSIX_C_SOURCE 199309L
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
#define DEVICE_BUFFER_SIZE 256 // Размер буфера нашего симулированного "устройства"

// --- Глобальные переменные для буфера устройства и синхронизации ---
static char device_buffer[DEVICE_BUFFER_SIZE];
static pthread_mutex_t device_mutex = PTHREAD_MUTEX_INITIALIZER;
// -------------------------------------------------------------------

static const char *progname = "example";
static int optv = 0;
static int listen_fd = -1;

// --- Структура для контекста операции клиента (OCB) ---
typedef struct {
    int fd;
    size_t offset; // Смещение для симулированного "позиции файла"
} client_context_t;
// -----------------------------------------------------

static void options(int argc, char *argv[]);
static void install_signals(void);
static void on_signal(int signo);
static void *client_thread(void *arg);
static void initialize_device_buffer(void);

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("%s: starting...\n", progname);
    options(argc, argv);
    install_signals();
    initialize_device_buffer(); // Инициализируем буфер

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
    printf("Подключитесь клиентом (например: `nc -U %s`).\n", EXAMPLE_SOCK_PATH);
    printf("Протокол: 1 байт команды ('r' для чтения, 'w' для записи), затем данные для 'w', или ожидание данных для 'r'.\n");

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

        // Выделяем память для контекста клиента
        client_context_t *ctx = (client_context_t *)malloc(sizeof(client_context_t));
        if (ctx == NULL) {
            perror("malloc");
            close(client_fd);
            continue;
        }
        ctx->fd = client_fd;
        ctx->offset = 0; // Начинаем с нулевого смещения

        pthread_t th;
        // Запускаем поток для клиента; поток сам закроет fd и освободит ctx
        if (pthread_create(&th, NULL, client_thread, (void *)ctx) != 0) {
            perror("pthread_create");
            free(ctx);
            close(client_fd);
            continue;
        }
        pthread_detach(th);
    }

    if (listen_fd != -1) close(listen_fd);
    unlink(EXAMPLE_SOCK_PATH);
    return EXIT_SUCCESS;
}

// Инициализация буфера устройства для демонстрации
static void initialize_device_buffer(void) {
    memset(device_buffer, 0, DEVICE_BUFFER_SIZE);
    for (int i = 0; i < DEVICE_BUFFER_SIZE; i++) {
        device_buffer[i] = (char)('A' + (i % 26)); // Заполняем алфавитом
    }
    printf("%s: буфер устройства инициализирован (%d байт).\n", progname, DEVICE_BUFFER_SIZE);
}

// Обработчик клиента: теперь с логикой io_read/io_write для устройства
static void *client_thread(void *arg)
{
    client_context_t *ctx = (client_context_t *)arg;
    int fd = ctx->fd;
    char buf[1024];

    for (;;) {
        char command;
        ssize_t n_cmd = recv(fd, &command, 1, 0); // Читаем один байт команды
        if (n_cmd == 0) {
            if (optv) printf("%s: клиент закрыл соединение (fd=%d)\n", progname, fd);
            break;
        }
        if (n_cmd < 0) {
            if (errno == EINTR) continue;
            perror("recv command");
            break;
        }
        
        if (optv) {
            printf("%s: Получена команда: '%c' (fd=%d)\n", progname, command);
        }

        // --- Реализация io_read: команда 'r' (read) ---
        if (command == 'r') {
            // Читаем запрошенный размер данных (до 1024 байт, но не больше, чем до конца буфера)
            size_t bytes_to_read = sizeof(buf);
            if (ctx->offset + bytes_to_read > DEVICE_BUFFER_SIZE) {
                bytes_to_read = DEVICE_BUFFER_SIZE - ctx->offset;
            }

            if (bytes_to_read == 0) {
                // Достигнут конец буфера
                if (optv) printf("%s: io_read — достигнут конец буфера (fd=%d)\n", progname, fd);
                send(fd, NULL, 0, 0); // Отправляем 0 байт, симулируя EOF
            } else {
                pthread_mutex_lock(&device_mutex);
                memcpy(buf, device_buffer + ctx->offset, bytes_to_read);
                pthread_mutex_unlock(&device_mutex);

                ssize_t sent = 0;
                while (sent < bytes_to_read) {
                    ssize_t m = send(fd, buf + sent, bytes_to_read - sent, 0);
                    if (m < 0) {
                        if (errno == EINTR) continue;
                        perror("send read data");
                        goto cleanup;
                    }
                    sent += m;
                }
                
                ctx->offset += bytes_to_read; // Обновляем смещение
                if (optv) {
                    printf("%s: io_read — %zd байт отправлено, новое смещение: %zu (fd=%d)\n", progname, sent, ctx->offset, fd);
                }
            }
        } 
        // --- Реализация io_write: команда 'w' (write) ---
        else if (command == 'w') {
            // Читаем данные, которые клиент хочет записать
            ssize_t n = recv(fd, buf, sizeof(buf), 0);
            if (n == 0) {
                if (optv) printf("%s: клиент закрыл соединение во время записи (fd=%d)\n", progname, fd);
                break;
            }
            if (n < 0) {
                if (errno == EINTR) continue;
                perror("recv write data");
                break;
            }

            // Записываем данные в буфер устройства
            size_t bytes_to_write = (size_t)n;
            if (ctx->offset + bytes_to_write > DEVICE_BUFFER_SIZE) {
                bytes_to_write = DEVICE_BUFFER_SIZE - ctx->offset; // Обрезаем по размеру буфера
            }

            if (bytes_to_write > 0) {
                pthread_mutex_lock(&device_mutex);
                memcpy(device_buffer + ctx->offset, buf, bytes_to_write);
                pthread_mutex_unlock(&device_mutex);

                ctx->offset += bytes_to_write; // Обновляем смещение
            }

            // Отправляем обратно количество записанных байт
            char response_buf[32];
            int len = snprintf(response_buf, sizeof(response_buf), "OK:%zu", bytes_to_write);
            send(fd, response_buf, (size_t)len, 0);

            if (optv) {
                printf("%s: io_write — %zu байт записано, новое смещение: %zu (fd=%d)\n", progname, bytes_to_write, ctx->offset, fd);
            }
        }
        // --- Неизвестная команда ---
        else {
            if (optv) printf("%s: Неизвестная команда '%c' (fd=%d)\n", progname, command);
            send(fd, "ERROR: Unknown command", 22, 0);
        }
    }

cleanup:
    // io_close: закрываем дескриптор и освобождаем контекст
    close(fd);
    free(ctx);
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