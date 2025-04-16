// chat_server.cpp
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

struct ClientInfo
{
    std::string username;
    sockaddr_in addr;
};

int main()
{
    // Создаем UDP-сокет
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket error");
        exit(1);
    }

    // Настраиваем адрес сервера: порт 0 для автоматического назначения свободного порта
    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = 0; // ОС выберет свободный порт

    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("bind error");
        exit(1);
    }

    // Получаем назначенный порт и выводим его
    socklen_t addrlen = sizeof(servaddr);
    if (getsockname(sockfd, (struct sockaddr *)&servaddr, &addrlen) < 0)
    {
        perror("getsockname error");
        exit(1);
    }
    int assignedPort = ntohs(servaddr.sin_port);
    std::cout << "Сервер запущен на порту: " << assignedPort << std::endl;

    // "Жёстко" заданный список пользователей (логин:пароль)
    std::map<std::string, std::string> validUsers = {
        {"user1", "password1"},
        {"user2", "password2"}};

    // Список подключенных клиентов (после авторизации)
    std::vector<ClientInfo> clients;
    char buffer[1024];

    while (true)
    {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&clientAddr, &clientLen);
        if (n < 0)
        {
            perror("recvfrom error");
            continue;
        }
        buffer[n] = '\0';
        std::string message(buffer);
        std::cout << "Получено: " << message << std::endl;

        // Обработка команды авторизации: "AUTH username password"
        if (message.substr(0, 4) == "AUTH")
        {
            size_t pos1 = message.find(' ');
            size_t pos2 = message.find(' ', pos1 + 1);
            if (pos1 == std::string::npos || pos2 == std::string::npos)
                continue;
            std::string username = message.substr(pos1 + 1, pos2 - pos1 - 1);
            std::string password = message.substr(pos2 + 1);
            std::string reply;
            if (validUsers.count(username) && validUsers[username] == password)
            {
                // Клиент прошёл авторизацию – если его еще нет в списке, добавляем его
                bool exists = false;
                for (auto it = clients.begin(); it != clients.end(); ++it)
                {
                    if (it->username == username)
                    {
                        // Обновляем адрес (на случай изменения)
                        it->addr = clientAddr;
                        exists = true;
                        break;
                    }
                }
                if (!exists)
                {
                    ClientInfo ci;
                    ci.username = username;
                    ci.addr = clientAddr;
                    clients.push_back(ci);
                }
                reply = "AUTH_OK";
            }
            else
            {
                reply = "AUTH_FAIL";
            }
            sendto(sockfd, reply.c_str(), reply.size(), 0, (struct sockaddr *)&clientAddr, clientLen);
        }
        // Обработка команды отключения: "QUIT username"
        else if (message.substr(0, 4) == "QUIT")
        {
            size_t pos = message.find(' ');
            if (pos != std::string::npos)
            {
                std::string username = message.substr(pos + 1);
                // Удаляем клиента из списка
                for (auto it = clients.begin(); it != clients.end(); ++it)
                {
                    if (it->username == username)
                    {
                        std::cout << "Пользователь " << username << " отключился." << std::endl;
                        clients.erase(it);
                        break;
                    }
                }
            }
        }
        // Публичное сообщение: формат "MSG username: сообщение"
        else if (message.substr(0, 3) == "MSG")
        {
            // Отсылаем всем клиентам, кроме отправителя
            for (auto &c : clients)
            {
                if (c.addr.sin_addr.s_addr == clientAddr.sin_addr.s_addr &&
                    c.addr.sin_port == clientAddr.sin_port)
                    continue; // не отправляем обратно отправителю
                sendto(sockfd, message.c_str(), message.size(), 0,
                       (struct sockaddr *)&c.addr, sizeof(c.addr));
            }
        }
        // Приватное сообщение: формат "PRIVATE target_username sender_username сообщение"
        else if (message.substr(0, 7) == "PRIVATE")
        {
            size_t pos1 = message.find(' ');
            if (pos1 == std::string::npos)
                continue;
            size_t pos2 = message.find(' ', pos1 + 1);
            if (pos2 == std::string::npos)
                continue;
            std::string target = message.substr(pos1 + 1, pos2 - pos1 - 1);
            size_t pos3 = message.find(' ', pos2 + 1);
            if (pos3 == std::string::npos)
                continue;
            std::string sender = message.substr(pos2 + 1, pos3 - pos2 - 1);
            std::string msgText = message.substr(pos3 + 1);

            bool found = false;
            for (auto &c : clients)
            {
                if (c.username == target)
                {
                    std::string forward = "PRIVATE from " + sender + ": " + msgText;
                    sendto(sockfd, forward.c_str(), forward.size(), 0,
                           (struct sockaddr *)&c.addr, sizeof(c.addr));
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                std::string errorMsg = "User " + target + " not found.";
                sendto(sockfd, errorMsg.c_str(), errorMsg.size(), 0, (struct sockaddr *)&clientAddr, clientLen);
            }
        }
        // Запрос на передачу файла: формат "FILE_REQ target_username sender_username filename filesize"
        else if (message.substr(0, 8) == "FILE_REQ")
        {
            std::istringstream iss(message);
            std::string command, target, sender, filename, filesize;
            iss >> command >> target >> sender >> filename >> filesize;
            bool found = false;
            for (auto &c : clients)
            {
                if (c.username == target)
                {
                    std::string forward = "FILE_REQ " + sender + " " + filename + " " + filesize;
                    sendto(sockfd, forward.c_str(), forward.size(), 0,
                           (struct sockaddr *)&c.addr, sizeof(c.addr));
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                std::string errorMsg = "User " + target + " not found for file transfer.";
                sendto(sockfd, errorMsg.c_str(), errorMsg.size(), 0, (struct sockaddr *)&clientAddr, clientLen);
            }
        }
        // Передача информации о TCP-порту для передачи файла: формат "FILE_PORT target_username sender_username ip port"
        else if (message.substr(0, 9) == "FILE_PORT")
        {
            std::istringstream iss(message);
            std::string command, target, sender, ip, port;
            iss >> command >> target >> sender >> ip >> port;
            bool found = false;
            for (auto &c : clients)
            {
                if (c.username == target)
                {
                    std::string forward = "FILE_PORT " + sender + " " + ip + " " + port;
                    sendto(sockfd, forward.c_str(), forward.size(), 0,
                           (struct sockaddr *)&c.addr, sizeof(c.addr));
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                std::string errorMsg = "User " + target + " not found for file transfer.";
                sendto(sockfd, errorMsg.c_str(), errorMsg.size(), 0, (struct sockaddr *)&clientAddr, clientLen);
            }
        }
    }

    close(sockfd);
    return 0;
}
