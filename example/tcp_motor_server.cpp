#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>

#include <wiringPi.h>
#include <softPwm.h>

static constexpr int BUFSIZE = 512;

// WiringPi pin numbers (NOT BCM)
int PWMA = 1;
int AIN2 = 2;
int AIN1 = 3;

int PWMB = 4;
int BIN2 = 5;
int BIN1 = 6;

static void t_up(unsigned int speed, unsigned int t_time)
{
    digitalWrite(AIN2, 0);
    digitalWrite(AIN1, 1);
    softPwmWrite(PWMA, static_cast<int>(speed));

    digitalWrite(BIN2, 0);
    digitalWrite(BIN1, 1);
    softPwmWrite(PWMB, static_cast<int>(speed));

    delay(static_cast<unsigned int>(t_time));
}

static void t_stop(unsigned int t_time)
{
    digitalWrite(AIN2, 0);
    digitalWrite(AIN1, 0);
    softPwmWrite(PWMA, 0);

    digitalWrite(BIN2, 0);
    digitalWrite(BIN1, 0);
    softPwmWrite(PWMB, 0);

    delay(static_cast<unsigned int>(t_time));
}

static void t_down(unsigned int speed, unsigned int t_time)
{
    digitalWrite(AIN2, 1);
    digitalWrite(AIN1, 0);
    softPwmWrite(PWMA, static_cast<int>(speed));

    digitalWrite(BIN2, 1);
    digitalWrite(BIN1, 0);
    softPwmWrite(PWMB, static_cast<int>(speed));

    delay(static_cast<unsigned int>(t_time));
}

static void t_left(unsigned int speed, unsigned int t_time)
{
    digitalWrite(AIN2, 1);
    digitalWrite(AIN1, 0);
    softPwmWrite(PWMA, static_cast<int>(speed));

    digitalWrite(BIN2, 0);
    digitalWrite(BIN1, 1);
    softPwmWrite(PWMB, static_cast<int>(speed));

    delay(static_cast<unsigned int>(t_time));
}

static void t_right(unsigned int speed, unsigned int t_time)
{
    digitalWrite(AIN2, 0);
    digitalWrite(AIN1, 1);
    softPwmWrite(PWMA, static_cast<int>(speed));

    digitalWrite(BIN2, 1);
    digitalWrite(BIN1, 0);
    softPwmWrite(PWMB, static_cast<int>(speed));

    delay(static_cast<unsigned int>(t_time));
}

struct Client
{
    int fd{-1};
    sockaddr_in addr{};
};

int main(int argc, char* argv[])
{
    // ---- RPi GPIO init ----
    if (wiringPiSetup() != 0)
    {
        std::perror("wiringPiSetup failed");
        return 1;
    }

    pinMode(1, OUTPUT); // PWMA
    pinMode(2, OUTPUT); // AIN2
    pinMode(3, OUTPUT); // AIN1

    pinMode(4, OUTPUT); // PWMB
    pinMode(5, OUTPUT); // BIN2
    pinMode(6, OUTPUT); // BIN1

    softPwmCreate(PWMA, 0, 100);
    softPwmCreate(PWMB, 0, 100);

    // ---- args ----
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    int portnumber = 0;
    try
    {
        portnumber = std::stoi(argv[1]);
    }
    catch (...)
    {
        std::cerr << "Invalid port.\n";
        return 1;
    }

    if (portnumber <= 0 || portnumber > 65535)
    {
        std::cerr << "Port out of range.\n";
        return 1;
    }

    // ---- socket server ----
    int listenfd = ::socket(PF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
    {
        std::perror("socket");
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(static_cast<uint16_t>(portnumber));

    if (::bind(listenfd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == -1)
    {
        std::perror("bind");
        ::close(listenfd);
        return 1;
    }

    if (::listen(listenfd, 128) == -1)
    {
        std::perror("listen");
        ::close(listenfd);
        return 1;
    }

    std::array<Client, FD_SETSIZE> client{};
    for (auto& c : client) c.fd = -1;

    fd_set allset;
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);

    int maxfd = listenfd;
    int maxi  = -1;

    std::cout << "waiting for the client's request...\n";

    while (true)
    {
        fd_set rset = allset;

        timeval tv{};
        tv.tv_sec  = 0;
        tv.tv_usec = 1; // 1 microsecond, same as original code

        int ret = ::select(maxfd + 1, &rset, nullptr, nullptr, &tv);

        if (ret == 0) continue;
        if (ret < 0)
        {
            std::perror("select");
            break;
        }

        // new connection
        if (FD_ISSET(listenfd, &rset))
        {
            sockaddr_in client_addr{};
            socklen_t len = sizeof(client_addr);
            int connectfd = ::accept(listenfd, reinterpret_cast<sockaddr*>(&client_addr), &len);
            if (connectfd == -1)
            {
                std::perror("accept");
                continue;
            }

            int i = 0;
            for (; i < FD_SETSIZE; i++)
            {
                if (client[i].fd < 0)
                {
                    client[i].fd = connectfd;
                    client[i].addr = client_addr;
                    std::cout << "You got a connection from "
                              << inet_ntoa(client[i].addr.sin_addr) << "\n";
                    break;
                }
            }

            if (i == FD_SETSIZE)
            {
                std::cerr << "Too many connections\n";
                ::close(connectfd);
            }
            else
            {
                FD_SET(connectfd, &allset);
                if (connectfd > maxfd) maxfd = connectfd;
                if (i > maxi) maxi = i;
            }

            continue; // go next loop
        }

        // handle existing clients
        for (int i = 0; i <= maxi; i++)
        {
            int sockfd = client[i].fd;
            if (sockfd < 0) continue;
            if (!FD_ISSET(sockfd, &rset)) continue;

            std::array<char, BUFSIZE + 1> buf{};
            std::memset(buf.data(), 0, buf.size());

            int z = static_cast<int>(::read(sockfd, buf.data(), BUFSIZE));
            if (z > 0)
            {
                buf[static_cast<size_t>(z)] = '\0';
                std::cout << "num = " << z << " received data:" << buf.data() << "\n";

                if (z == 3)
                {
                    if (buf[0] == 'O' && buf[1] == 'N')
                    {
                        switch (buf[2])
                        {
                            case 'A': t_up(50, 0);    std::cout << "forward\n"; break;
                            case 'B': t_down(50, 0);  std::cout << "back\n";    break;
                            case 'C': t_left(50, 0);  std::cout << "left\n";    break;
                            case 'D': t_right(50, 0); std::cout << "right\n";   break;
                            case 'E': t_stop(0);      std::cout << "stop\n";    break;
                            default:  t_stop(0);      std::cout << "stop\n";    break;
                        }
                    }
                    else
                    {
                        t_stop(0);
                    }
                }
                else if (z == 6)
                {
                    // Keep original byte-protocol logic
                    if (static_cast<unsigned char>(buf[2]) == 0x00)
                    {
                        switch (static_cast<unsigned char>(buf[3]))
                        {
                            case 0x01: t_up(50, 0);    std::cout << "forward\n"; break;
                            case 0x02: t_down(50, 0);  std::cout << "back\n";    break;
                            case 0x03: t_left(50, 0);  std::cout << "left\n";    break;
                            case 0x04: t_right(50, 0); std::cout << "right\n";   break;
                            case 0x00: t_stop(0);      std::cout << "stop\n";    break;
                            default: break;
                        }
                    }
                    else
                    {
                        t_stop(0);
                    }
                }
                else
                {
                    t_stop(0);
                }
            }
            else
            {
                std::cout << "disconnected by client!\n";
                ::close(sockfd);
                FD_CLR(sockfd, &allset);
                client[i].fd = -1;
            }
        }
    }

    ::close(listenfd);
    return 0;
}