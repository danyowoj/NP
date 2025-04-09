#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/resource.h> // Для wait3
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 0 // 0 означает, что система сама выберет свободный порт
#define BUFFER_SIZE 1024

// Обработчик сигнала SIGCHLD для завершения зомби-процессов
void reaper(int sig)
{
    int status;
    struct rusage usage; // Структура для получения информации о ресурсах
    pid_t pid;

    // Используем wait3 с флагом WNOHANG, чтобы не блокировать основной процесс
    while ((pid = wait3(&status, WNOHANG, &usage)) > 0)
    {
        if (WIFEXITED(status))
        {
            printf("Дочерний процесс с PID %d завершился с кодом %d.\n", pid, WEXITSTATUS(status));
        }
        else if (WIFSIGNALED(status))
        {
            printf("Дочерний процесс с PID %d был завершен сигналом %d.\n", pid, WTERMSIG(status));
        }
    }
}

// Функция для обработки клиента
void handle_client(int client_fd)
{
    char buffer[BUFFER_SIZE];
    int n;

    while ((n = recv(client_fd, buffer, BUFFER_SIZE - 1, 0)) > 0)
    {
        buffer[n] = '\0'; // Добавляем завершающий нулевой символ
        printf("Received from client: %s\n", buffer);

        // Отправляем ответ клиенту
        char response[BUFFER_SIZE];
        snprintf(response, BUFFER_SIZE, "Server received: %s", buffer);
        send(client_fd, response, strlen(response), 0);
    }

    if (n < 0)
    {
        perror("recv failed");
    }

    close(client_fd);
    printf("Клиент отключен. Дочерний процесс завершается.\n");
    exit(0); // Завершаем дочерний процесс
}

int main()
{
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int port;

    // Установка обработчика сигнала SIGCHLD
    signal(SIGCHLD, reaper);

    // Создание TCP сокета
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Настройка адреса сервера
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT); // 0 означает, что система выберет порт

    // Привязка сокета к адресу
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Получение номера порта, который система выбрала
    socklen_t len = sizeof(server_addr);
    if (getsockname(server_fd, (struct sockaddr *)&server_addr, &len) < 0)
    {
        perror("getsockname failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    port = ntohs(server_addr.sin_port);
    printf("Server is using port: %d\n", port);

    // Ожидание соединений
    if (listen(server_fd, 5) < 0)
    {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", port);

    // Основной цикл сервера
    while (1)
    {
        // Принимаем новое соединение
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) < 0)
        {
            perror("accept failed");
            continue;
        }

        // Создание нового процесса для обработки клиента
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork failed");
            close(client_fd);
        }
        else if (pid == 0)
        {
            // Дочерний процесс
            close(server_fd); // Закрываем сокет сервера в дочернем процессе
            handle_client(client_fd);
        }
        else
        {
            // Родительский процесс
            close(client_fd); // Закрываем сокет клиента в родительском процессе
        }
    }

    close(server_fd);
    return 0;
}
