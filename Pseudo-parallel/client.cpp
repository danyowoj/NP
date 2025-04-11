#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <server_ip> <server_port> <message>" << std::endl;
        return 1;
    }

    int clientSocket;
    struct sockaddr_in serverAddr;
    char buffer[BUFFER_SIZE];

    // Создание TCP сокета
    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket creation failed");
        return 1;
    }

    // Настройка адреса сервера
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &serverAddr.sin_addr);

    // Подключение к серверу
    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("connect failed");
        close(clientSocket);
        return 1;
    }

    std::string message = argv[3]; // Сообщение, которое будет отправляться
    int delay = 1;                 // Задержка по умолчанию

    // Проверяем, является ли сообщение числом
    bool isNumber = true;
    try
    {
        delay = std::stoi(message);
    }
    catch (const std::invalid_argument &)
    {
        // Если сообщение не число, используем задержку по умолчанию
        isNumber = false;
    }

    if (isNumber)
    {
        // Если сообщение — число, отправляем его с задержкой
        while (true)
        {
            std::cout << "Sending to server: " << message << std::endl;

            // Отправка данных на сервер
            send(clientSocket, message.c_str(), message.length(), 0);

            // Получение ответа от сервера
            int n = recv(clientSocket, buffer, BUFFER_SIZE, 0);
            if (n <= 0)
            {
                std::cerr << "Server disconnected" << std::endl;
                break;
            }
            buffer[n] = '\0';
            std::cout << "Received from server: " << buffer << std::endl;

            sleep(delay); // Задержка в зависимости от значения
        }
    }
    else
    {
        // Если сообщение — строка, отправляем её один раз
        std::cout << "Sending to server: " << message << std::endl;
        send(clientSocket, message.c_str(), message.length(), 0);

        // Получение ответа от сервера
        int n = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (n <= 0)
        {
            std::cerr << "Server disconnected" << std::endl;
        }
        else
        {
            buffer[n] = '\0';
            std::cout << "Received from server: " << buffer << std::endl;
        }
    }

    close(clientSocket);
    return 0;
}
