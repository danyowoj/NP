#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

int main()
{
    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Создание UDP сокета с использованием IPv4
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Настройка адреса сервера
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = 0; // 0 означает, что система сама выберет свободный порт

    // Привязка сокета к адресу сервера
    if (bind(server_fd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Получение номера порта, который выбрала система
    socklen_t len = sizeof(server_addr);
    getsockname(server_fd, (struct sockaddr *)&server_addr, &len);
    std::cout << "Server is running on port: " << ntohs(server_addr.sin_port) << std::endl;

    while (true)
    {
        // Получение данных от клиента
        int n = recvfrom(server_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);
        buffer[n] = '\0';

        // Вывод информации о клиенте и полученных данных
        std::cout << "Received from client " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << " - " << buffer << std::endl;

        // Преобразование данных (удвоение числа)
        int i = atoi(buffer);
        int transformed = i * 2;
        std::string response = std::to_string(transformed);

        // Отправка преобразованных данных обратно клиенту
        sendto(server_fd, response.c_str(), response.length(), 0, (const struct sockaddr *)&client_addr, client_len);
    }

    close(server_fd);
    return 0;
}
