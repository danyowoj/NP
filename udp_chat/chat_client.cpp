// chat_client.cpp
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <cstring>

// Глобальная переменная для имени пользователя
std::string myUsername;

// Глобальная переменная с адресом сервера (используется для отправки сообщений)
sockaddr_in serverAddr;

// Потоковый флаг для завершения работы
std::atomic<bool> running(true);

// Структура для хранения информации о поступившем запросе на передачу файла
struct FileRequest
{
    std::string sender;
    std::string filename;
    std::string filesize;
    bool pending;
};
std::mutex fileReqMutex;
FileRequest pendingFileRequest = {"", "", "", false};

// Функция для прослушивания UDP-сокета для получения входящих сообщений от сервера
void udp_listener(int sockfd)
{
    char buffer[1024];
    while (running)
    {
        sockaddr_in senderAddr;
        socklen_t addrlen = sizeof(senderAddr);
        int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (sockaddr *)&senderAddr, &addrlen);
        if (n <= 0)
            continue;
        buffer[n] = '\0';
        std::string message(buffer);

        // Если сообщение является запросом на передачу файла
        if (message.substr(0, 8) == "FILE_REQ")
        {
            std::istringstream iss(message);
            std::string command, sender, filename, filesize;
            iss >> command >> sender >> filename >> filesize;
            {
                std::lock_guard<std::mutex> lock(fileReqMutex);
                // Если уже есть незавершённый запрос, выводим уведомление и игнорируем новый
                if (pendingFileRequest.pending)
                {
                    std::cout << "\nПолучен новый запрос на файл от " << sender
                              << ", но уже есть ожидающий запрос. Новый запрос игнорируется.\n";
                    continue;
                }
                // Сохраняем данные о новом запросе
                pendingFileRequest.sender = sender;
                pendingFileRequest.filename = filename;
                pendingFileRequest.filesize = filesize;
                pendingFileRequest.pending = true;
            }
            std::cout << "\n[Получено] Запрос на передачу файла от " << sender
                      << " для файла " << filename
                      << " (" << filesize << " байт).\n"
                      << "Чтобы принять, введите команду \"/accept\", чтобы отклонить – \"/decline\".\n";
        }
        else
        {
            // Вывод остальных сообщений
            std::cout << "\n[Получено] " << message << std::endl;
        }
    }
}

// Функция для отправки файла по TCP (отправитель)
void send_file_tcp(const std::string &ip, int port, const std::string &filepath)
{
    int tcpSock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSock < 0)
    {
        perror("Ошибка TCP-сокета");
        return;
    }
    sockaddr_in targetAddr;
    memset(&targetAddr, 0, sizeof(targetAddr));
    targetAddr.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &targetAddr.sin_addr);
    targetAddr.sin_port = htons(port);

    if (connect(tcpSock, (struct sockaddr *)&targetAddr, sizeof(targetAddr)) < 0)
    {
        perror("Ошибка соединения");
        close(tcpSock);
        return;
    }

    std::ifstream inFile(filepath, std::ios::binary);
    if (!inFile)
    {
        std::cerr << "Не удалось открыть файл: " << filepath << std::endl;
        close(tcpSock);
        return;
    }

    char buffer[1024];
    while (inFile.read(buffer, sizeof(buffer)) || inFile.gcount() > 0)
    {
        send(tcpSock, buffer, inFile.gcount(), 0);
    }
    std::cout << "Файл успешно отправлен." << std::endl;
    inFile.close();
    close(tcpSock);
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        std::cout << "Использование: " << argv[0] << " <server_ip> <server_port> <username>" << std::endl;
        return 1;
    }
    // Сохраняем имя пользователя
    myUsername = argv[3];

    std::string serverIP = argv[1];
    int serverPort = std::stoi(argv[2]);
    std::string username = argv[3];

    std::string password;
    std::cout << "Введите пароль: ";
    std::cin >> password;
    std::cin.ignore();

    // Настраиваем UDP-сокет клиента
    int udpSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSock < 0)
    {
        perror("Ошибка UDP-сокета");
        return 1;
    }
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);
    serverAddr.sin_port = htons(serverPort);

    // Отправка сообщения авторизации: "AUTH username password"
    std::string authMsg = "AUTH " + username + " " + password;
    sendto(udpSock, authMsg.c_str(), authMsg.size(), 0,
           (struct sockaddr *)&serverAddr, sizeof(serverAddr));

    char buffer[1024];
    sockaddr_in fromAddr;
    socklen_t fromLen = sizeof(fromAddr);
    int n = recvfrom(udpSock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&fromAddr, &fromLen);
    if (n < 0)
    {
        perror("recvfrom error");
        return 1;
    }
    buffer[n] = '\0';
    std::string reply(buffer);
    if (reply != "AUTH_OK")
    {
        std::cout << "Ошибка авторизации: " << reply << std::endl;
        close(udpSock);
        return 1;
    }
    std::cout << "Авторизация успешна!" << std::endl;

    // Запускаем поток для прослушивания входящих UDP-сообщений
    std::thread listener(udp_listener, udpSock);

    std::cout << "\nДоступные команды:\n"
              << "  обычное сообщение – просто введите текст\n"
              << "  /private <username> <сообщение> – приватное сообщение\n"
              << "  /file <username> <путь_к_файлу> – передача файла\n"
              << "  /accept – принять входящий запрос на файл\n"
              << "  /decline – отклонить входящий запрос на файл\n"
              << "  /quit – выход\n"
              << std::endl;

    // Главный цикл отправки сообщений и обработки команд
    while (true)
    {
        std::string input;
        std::getline(std::cin, input);
        if (input.empty())
            continue;

        // Обработка команды для принятия файла
        if (input == "/accept")
        {
            // Проверяем, есть ли ожидающий запрос
            FileRequest req;
            {
                std::lock_guard<std::mutex> lock(fileReqMutex);
                if (!pendingFileRequest.pending)
                {
                    std::cout << "Нет ожидающих запросов на передачу файла." << std::endl;
                    continue;
                }
                req = pendingFileRequest;
                pendingFileRequest.pending = false;
            }
            // Открываем TCP-сокет для приёма файла на свободном порту
            int tcpSock = socket(AF_INET, SOCK_STREAM, 0);
            if (tcpSock < 0)
            {
                perror("Ошибка TCP-сокета");
                continue;
            }
            sockaddr_in tcpAddr;
            memset(&tcpAddr, 0, sizeof(tcpAddr));
            tcpAddr.sin_family = AF_INET;
            tcpAddr.sin_addr.s_addr = INADDR_ANY;
            tcpAddr.sin_port = 0; // ОС выберет свободный порт

            if (bind(tcpSock, (struct sockaddr *)&tcpAddr, sizeof(tcpAddr)) < 0)
            {
                perror("bind TCP error");
                close(tcpSock);
                continue;
            }
            socklen_t tcpLen = sizeof(tcpAddr);
            if (getsockname(tcpSock, (struct sockaddr *)&tcpAddr, &tcpLen) < 0)
            {
                perror("getsockname TCP error");
                close(tcpSock);
                continue;
            }
            int tcpPort = ntohs(tcpAddr.sin_port);
            listen(tcpSock, 1);

            // Отправляем через сервер сообщение с информацией о TCP-порту:
            // Формат: "FILE_PORT <target_username> <receiver_username> <ip> <port>"
            std::string localIP = "127.0.0.1"; // для упрощения, предполагается, что клиенты на одной машине
            std::string filePortMsg = "FILE_PORT " + req.sender + " " + myUsername + " " + localIP + " " + std::to_string(tcpPort);
            sendto(udpSock, filePortMsg.c_str(), filePortMsg.size(), 0,
                   (struct sockaddr *)&serverAddr, sizeof(serverAddr));
            std::cout << "Отправлено сообщение FILE_PORT: " << filePortMsg << std::endl;
            std::cout << "Ожидаем входящее TCP-соединение для передачи файла..." << std::endl;
            int connSock = accept(tcpSock, NULL, NULL);
            if (connSock < 0)
            {
                perror("accept error");
                close(tcpSock);
                continue;
            }
            std::ofstream outFile("received_" + req.filename, std::ios::binary);
            if (!outFile)
            {
                std::cerr << "Не удалось открыть файл для записи." << std::endl;
                close(connSock);
                close(tcpSock);
                continue;
            }
            char bufferTCP[1024];
            int bytesRead;
            while ((bytesRead = recv(connSock, bufferTCP, sizeof(bufferTCP), 0)) > 0)
            {
                outFile.write(bufferTCP, bytesRead);
            }
            std::cout << "Файл получен и сохранен как received_" << req.filename << std::endl;
            outFile.close();
            close(connSock);
            close(tcpSock);
            continue;
        }

        // Обработка команды для отклонения запроса на файл
        if (input == "/decline")
        {
            std::lock_guard<std::mutex> lock(fileReqMutex);
            if (pendingFileRequest.pending)
            {
                pendingFileRequest.pending = false;
                std::cout << "Запрос передачи файла отклонён." << std::endl;
            }
            else
            {
                std::cout << "Нет ожидающих запросов на передачу файла." << std::endl;
            }
            continue;
        }

        // Команда выхода
        if (input == "/quit")
        {
            running = false;
            break;
        }
        // Приватное сообщение
        if (input.substr(0, 8) == "/private")
        {
            // Формат: /private target_username сообщение...
            std::istringstream iss(input);
            std::string command, target;
            iss >> command >> target;
            std::string msgText;
            std::getline(iss, msgText);
            std::string outMsg = "PRIVATE " + target + " " + username + " " + msgText;
            sendto(udpSock, outMsg.c_str(), outMsg.size(), 0,
                   (struct sockaddr *)&serverAddr, sizeof(serverAddr));
        }
        // Передача файла (инициатор)
        else if (input.substr(0, 5) == "/file")
        {
            // Формат: /file target_username путь_к_файлу
            std::istringstream iss(input);
            std::string command, target, filepath;
            iss >> command >> target >> filepath;
            // Определяем размер файла
            std::ifstream inFile(filepath, std::ios::binary | std::ios::ate);
            if (!inFile)
            {
                std::cerr << "Не удалось открыть файл: " << filepath << std::endl;
                continue;
            }
            std::streamsize filesize = inFile.tellg();
            inFile.close();
            // Извлекаем имя файла (без пути)
            size_t pos = filepath.find_last_of("/\\");
            std::string filename = (pos == std::string::npos) ? filepath : filepath.substr(pos + 1);
            // Формируем сообщение запроса передачи файла:
            // "FILE_REQ target_username username filename filesize"
            std::string fileReq = "FILE_REQ " + target + " " + username + " " + filename + " " + std::to_string(filesize);
            sendto(udpSock, fileReq.c_str(), fileReq.size(), 0,
                   (struct sockaddr *)&serverAddr, sizeof(serverAddr));
            std::cout << "Запрос передачи файла отправлен пользователю " << target << std::endl;

            // Ожидаем ответ с информацией о TCP-порту (FILE_PORT)
            std::cout << "Ожидаем информацию о TCP-порту для передачи файла..." << std::endl;
            bool receivedPort = false;
            std::string peerIP;
            int peerPort = 0;
            fd_set readfds;
            struct timeval tv;
            tv.tv_sec = 10; // таймаут 10 секунд
            tv.tv_usec = 0;
            FD_ZERO(&readfds);
            FD_SET(udpSock, &readfds);
            int ret = select(udpSock + 1, &readfds, NULL, NULL, &tv);
            if (ret > 0 && FD_ISSET(udpSock, &readfds))
            {
                int n2 = recvfrom(udpSock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&fromAddr, &fromLen);
                if (n2 > 0)
                {
                    buffer[n2] = '\0';
                    std::string resp(buffer);
                    // Если получено сообщение FILE_PORT, формат: "FILE_PORT sender_ip tcpPort"
                    if (resp.substr(0, 9) == "FILE_PORT")
                    {
                        std::istringstream iss2(resp);
                        std::string cmd, sender;
                        iss2 >> cmd >> sender >> peerIP >> peerPort;
                        // Проверяем, что отправитель совпадает с целевым пользователем
                        if (sender == target)
                        {
                            receivedPort = true;
                        }
                    }
                }
            }
            if (receivedPort)
            {
                std::cout << "Подключаемся к " << peerIP << ":" << peerPort << " для передачи файла..." << std::endl;
                send_file_tcp(peerIP, peerPort, filepath);
            }
            else
            {
                std::cout << "Не получена информация о порте для передачи файла. Передача прервана." << std::endl;
            }
        }
        // Публичное сообщение
        else
        {
            std::string outMsg = "MSG " + username + ": " + input;
            sendto(udpSock, outMsg.c_str(), outMsg.size(), 0,
                   (struct sockaddr *)&serverAddr, sizeof(serverAddr));
        }
    }

    listener.join();
    close(udpSock);
    return 0;
}
