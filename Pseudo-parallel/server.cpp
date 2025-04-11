#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <vector>
#include <algorithm>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 30

int main()
{
    int serverSocket, max_sd, activity;
    struct sockaddr_in serverAddr;
    fd_set readfds;
    std::vector<int> clientSockets(MAX_CLIENTS, 0);

    // Создание TCP сокета
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket creation failed");
        return 1;
    }

    // Настройка адреса сервера
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = 0; // 0 означает, что система сама выберет свободный порт

    // Привязка сокета к адресу
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("bind failed");
        close(serverSocket);
        return 1;
    }

    // Получаем выбранный порт
    socklen_t len = sizeof(serverAddr);
    getsockname(serverSocket, (struct sockaddr *)&serverAddr, &len);
    std::cout << "Server is running on port: " << ntohs(serverAddr.sin_port) << std::endl;

    // Переводим сокет в режим прослушивания
    if (listen(serverSocket, 5) < 0)
    {
        perror("listen failed");
        close(serverSocket);
        return 1;
    }

    std::cout << "Server is listening..." << std::endl;

    while (true)
    {
        // Очищаем набор файловых дескрипторов
        FD_ZERO(&readfds);

        // Добавляем серверный сокет в набор
        FD_SET(serverSocket, &readfds);
        max_sd = serverSocket;

        // Добавляем клиентские сокеты в набор
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clientSockets[i] > 0)
            {
                FD_SET(clientSockets[i], &readfds);
                if (clientSockets[i] > max_sd)
                {
                    max_sd = clientSockets[i];
                }
            }
        }

        // Ожидаем активности на любом из сокетов
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0)
        {
            perror("select error");
            continue;
        }

        // Если активность на серверном сокете -> новое подключение
        if (FD_ISSET(serverSocket, &readfds))
        {
            struct sockaddr_in clientAddr;
            socklen_t clientAddrLen = sizeof(clientAddr);
            int newSocket;

            if ((newSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen)) < 0)
            {
                perror("accept failed");
                continue;
            }

            std::cout << "New client connected: " << inet_ntoa(clientAddr.sin_addr)
                      << ":" << ntohs(clientAddr.sin_port) << std::endl;

            // Добавляем новый сокет в массив клиентских сокетов
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (clientSockets[i] == 0)
                {
                    clientSockets[i] = newSocket;
                    break;
                }
            }
        }

        // Проверяем активность на клиентских сокетах
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            int sd = clientSockets[i];
            if (sd > 0 && FD_ISSET(sd, &readfds))
            {
                char buffer[BUFFER_SIZE];
                int bytesRead = recv(sd, buffer, BUFFER_SIZE, 0);

                if (bytesRead == 0)
                {
                    // Клиент отключился
                    getpeername(sd, (struct sockaddr *)&serverAddr, &len);
                    std::cout << "Client disconnected: " << inet_ntoa(serverAddr.sin_addr)
                              << ":" << ntohs(serverAddr.sin_port) << std::endl;
                    close(sd);
                    clientSockets[i] = 0;
                }
                else
                {
                    // Обрабатываем полученные данные
                    buffer[bytesRead] = '\0';
                    std::string message(buffer);
                    std::cout << "Received from client: " << message << std::endl;

                    // Обработка сообщения
                    std::string response;
                    try
                    {
                        int number = std::stoi(message);
                        int squared = number * number;
                        response = std::to_string(squared);
                    }
                    catch (const std::invalid_argument &)
                    {
                        // Если сообщение не число, просто возвращаем его обратно
                        response = "You sent: " + message;
                    }

                    // Отправляем ответ клиенту
                    send(sd, response.c_str(), response.length(), 0);
                }
            }
        }
    }

    close(serverSocket);
    return 0;
}
