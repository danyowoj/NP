#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <fstream>
#include <sstream>
#include <cmath>

#define BUFFER_SIZE 1024

std::ofstream logFile;
pthread_mutex_t fileMutex;

void *handleClient(void *arg)
{
    int clientSocket = *(int *)arg;
    char buffer[BUFFER_SIZE];
    int bytesRead;

    while ((bytesRead = recv(clientSocket, buffer, BUFFER_SIZE, 0)) > 0)
    {
        buffer[bytesRead] = '\0';
        std::string message(buffer);

        // Блокируем мьютекс для записи в файл
        pthread_mutex_lock(&fileMutex);
        logFile << "Received from client: " << message << std::endl;
        pthread_mutex_unlock(&fileMutex);

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
        send(clientSocket, response.c_str(), response.length(), 0);
    }

    close(clientSocket);
    delete (int *)arg;
    return nullptr;
}

int main(int argc, char *argv[])
{
    int serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    // Открываем файл для логирования
    logFile.open("server_log.txt", std::ios::app);
    if (!logFile.is_open())
    {
        std::cerr << "Failed to open log file" << std::endl;
        return 1;
    }

    // Инициализация мьютекса
    pthread_mutex_init(&fileMutex, nullptr);

    // Создание TCP сокета
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket creation failed");
        return 1;
    }

    // Настройка адреса сервера
    memset(&serverAddr, 0, sizeof(serverAddr));
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
        if ((clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen)) < 0)
        {
            perror("accept failed");
            continue;
        }

        std::cout << "New client connected" << std::endl;

        // Создаем новый поток для обработки клиента
        pthread_t thread;
        int *newSocket = new int;
        *newSocket = clientSocket;
        if (pthread_create(&thread, nullptr, handleClient, newSocket) < 0)
        {
            perror("could not create thread");
            close(clientSocket);
            delete newSocket;
        }

        // Отсоединяем поток, чтобы он мог работать независимо
        pthread_detach(thread);
    }

    // Закрываем файл и освобождаем мьютекс
    logFile.close();
    pthread_mutex_destroy(&fileMutex);
    close(serverSocket);
    return 0;
}
