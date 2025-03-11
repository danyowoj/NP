#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define BUFFER_SIZE 1024 // Размер буфера для приема и отправки данных

// Функция для обработки зомби-процессов
void handle_zombie(int sig)
{
    pid_t pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
    { // Обрабатываем завершенные процессы
        std::cout << "Зомби-процесс с PID " << pid << " завершен." << std::endl;
    }
}

// Функция для поиска свободного порта
int find_free_port()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0); // Создаем TCP сокет
    if (sock < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));           // Обнуляем структуру адреса
    addr.sin_family = AF_INET;                // Используем IPv4
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // Принимаем соединения на все интерфейсы
    addr.sin_port = 0;                        // 0 означает, что система сама выберет свободный порт

    // Привязываем сокет к адресу
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Получаем информацию о порте, который система выбрала
    socklen_t len = sizeof(addr);
    if (getsockname(sock, (struct sockaddr *)&addr, &len) < 0)
    {
        perror("getsockname failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    int port = ntohs(addr.sin_port); // Преобразуем порт из сетевого формата в host-формат
    close(sock);                     // Закрываем сокет
    return port;                     // Возвращаем номер порта
}

// Функция для обработки клиента
void handle_client(int client_fd)
{
    char buffer[BUFFER_SIZE]; // Буфер для приема данных
    while (true)
    {
        // Принимаем данные от клиента
        int n = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (n <= 0)
        { // Если соединение закрыто или произошла ошибка
            break;
        }
        buffer[n] = '\0'; // Добавляем завершающий нулевой символ
        std::cout << "Received from client: " << buffer << std::endl;

        // Формируем ответ клиенту
        std::string response = "Server received: " + std::string(buffer);
        send(client_fd, response.c_str(), response.length(), 0); // Отправляем ответ
    }
    close(client_fd); // Закрываем сокет клиента

    // Искусственная задержка для создания зомби-процесса
    std::cout << "Дочерний процесс " << getpid() << " завершается через 5 секунд..." << std::endl;
    sleep(5); // Задержка перед завершением
    exit(0);  // Завершаем дочерний процесс
}

int main()
{
    int server_fd, client_fd;                    // Дескрипторы сокетов сервера и клиента
    struct sockaddr_in server_addr, client_addr; // Структуры для хранения адресов
    socklen_t client_len = sizeof(client_addr);  // Длина адреса клиента

    // Находим свободный порт
    int port = find_free_port();
    std::cout << "Using port: " << port << std::endl;

    // Создание TCP сокета
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Настройка адреса сервера
    memset(&server_addr, 0, sizeof(server_addr));    // Обнуляем структуру
    server_addr.sin_family = AF_INET;                // Используем IPv4
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Принимаем соединения на все интерфейсы
    server_addr.sin_port = htons(port);              // Указываем порт

    // Привязка сокета к адресу
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Ожидание соединений (максимум 5 в очереди)
    if (listen(server_fd, 5) < 0)
    {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Установка обработчика сигнала для завершения зомби-процессов
    signal(SIGCHLD, handle_zombie);

    // Основной цикл сервера
    while (true)
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
        { // Ошибка при создании процесса
            perror("fork failed");
            close(client_fd);
        }
        else if (pid == 0)
        {                             // Дочерний процесс
            close(server_fd);         // Закрываем сокет сервера в дочернем процессе
            handle_client(client_fd); // Обрабатываем клиента
        }
        else
        {                     // Родительский процесс
            close(client_fd); // Закрываем сокет клиента в родительском процессе
        }
    }

    close(server_fd); // Закрываем сокет сервера (эта строка никогда не выполнится в данном примере)
    return 0;
}
