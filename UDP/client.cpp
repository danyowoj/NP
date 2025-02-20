#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <server_ip> <server_port> <value>" << std::endl;
        return 1;
    }

    int client_fd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // Создание UDP сокета с использованием IPv4
    if ((client_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Настройка адреса сервера
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &server_addr.sin_addr);

    int value = atoi(argv[3]); // Значение, которое будет отправляться
    int delay = value;         // Задержка равна значению

    while (true)
    {
        std::string message = std::to_string(value);
        std::cout << "Sending to server: " << message << std::endl;

        // Отправка данных на сервер
        sendto(client_fd, message.c_str(), message.length(), 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));

        // Получение ответа от сервера
        int n = recvfrom(client_fd, buffer, BUFFER_SIZE, 0, NULL, NULL);
        buffer[n] = '\0';
        std::cout << "Received from server: " << buffer << std::endl;

        sleep(delay); // Задержка в зависимости от значения
    }

    close(client_fd);
    return 0;
}
