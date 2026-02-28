// car_server.cpp (C++ version)
//
// Build (Raspberry Pi):
// g++ -std=c++17 -O2 -Wall -Wextra -pedantic car_server.cpp -o car_server -lwiringPi -lpthread

#include "pca9685.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <wiringPi.h>
#include <softPwm.h>

namespace {

constexpr int PIN_BASE = 300;
constexpr int MAX_PWM  = 4096;
constexpr int HERTZ    = 50;
constexpr int BUFSIZE  = 512;

int PWMA = 1;
int AIN2 = 2;
int AIN1 = 3;

int PWMB = 4;
int BIN2 = 5;
int BIN1 = 6;

float lr_detection = 90.0f;
float qh_detection = 0.0f;

// Calculate ticks for PCA9685 PWM
int calcTicks(float impulseMs, int hertz) {
  const float cycleMs = 1000.0f / static_cast<float>(hertz);
  return static_cast<int>(MAX_PWM * impulseMs / cycleMs + 0.5f);
}

void PWM_write(int servonum, float x) {
  float y = x / 90.0f + 0.5f; // map angle -> pulse width ms-ish
  y = std::max(y, 0.5f);
  y = std::min(y, 2.5f);

  const int tick = calcTicks(y, HERTZ);
  pwmWrite(PIN_BASE + servonum, tick);
}

void t_up(unsigned int speed, unsigned int t_time) {
  digitalWrite(AIN2, 0);
  digitalWrite(AIN1, 1);
  softPwmWrite(PWMA, static_cast<int>(speed));

  digitalWrite(BIN2, 0);
  digitalWrite(BIN1, 1);
  softPwmWrite(PWMB, static_cast<int>(speed));

  delay(static_cast<unsigned int>(t_time)); // wiringPi delay is ms
}

void t_stop(unsigned int t_time) {
  digitalWrite(AIN2, 0);
  digitalWrite(AIN1, 0);
  softPwmWrite(PWMA, 0);

  digitalWrite(BIN2, 0);
  digitalWrite(BIN1, 0);
  softPwmWrite(PWMB, 0);

  delay(static_cast<unsigned int>(t_time));
}

void t_down(unsigned int speed, unsigned int t_time) {
  digitalWrite(AIN2, 1);
  digitalWrite(AIN1, 0);
  softPwmWrite(PWMA, static_cast<int>(speed));

  digitalWrite(BIN2, 1);
  digitalWrite(BIN1, 0);
  softPwmWrite(PWMB, static_cast<int>(speed));

  delay(static_cast<unsigned int>(t_time));
}

void t_left(unsigned int speed, unsigned int t_time) {
  digitalWrite(AIN2, 1);
  digitalWrite(AIN1, 0);
  softPwmWrite(PWMA, static_cast<int>(speed));

  digitalWrite(BIN2, 0);
  digitalWrite(BIN1, 1);
  softPwmWrite(PWMB, static_cast<int>(speed));

  delay(static_cast<unsigned int>(t_time));
}

void t_right(unsigned int speed, unsigned int t_time) {
  digitalWrite(AIN2, 0);
  digitalWrite(AIN1, 1);
  softPwmWrite(PWMA, static_cast<int>(speed));

  digitalWrite(BIN2, 1);
  digitalWrite(BIN1, 0);
  softPwmWrite(PWMB, static_cast<int>(speed));

  delay(static_cast<unsigned int>(t_time));
}

struct Client {
  int fd = -1;
  sockaddr_in addr{};
};

void clamp_servo(float& v) {
  if (v <= 0.0f) v = 0.0f;
  if (v >= 180.0f) v = 180.0f;
}

} // namespace

int main(int argc, char* argv[]) {
  // ----- Parse port -----
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <port>\n";
    return 1;
  }

  const int portnumber = std::atoi(argv[1]);
  if (portnumber <= 0) {
    std::cerr << "Invalid port: " << argv[1] << "\n";
    return 1;
  }

  // ----- wiringPi init -----
  if (wiringPiSetup() != 0) {
    std::perror("wiringPiSetup");
    return 1;
  }

  // GPIO output
  pinMode(PWMA, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(AIN1, OUTPUT);

  pinMode(PWMB, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(BIN1, OUTPUT);

  // soft PWM
  if (softPwmCreate(PWMA, 0, 100) != 0) {
    std::perror("softPwmCreate(PWMA)");
    return 1;
  }
  if (softPwmCreate(PWMB, 0, 100) != 0) {
    std::perror("softPwmCreate(PWMB)");
    return 1;
  }

  // PCA9685 setup
  const int pca_fd = pca9685Setup(PIN_BASE, 0x40, HERTZ);
  if (pca_fd < 0) {
    std::cerr << "Error in pca9685Setup\n";
    return 1;
  }
  pca9685PWMReset(pca_fd);

  PWM_write(1, lr_detection);
  PWM_write(2, qh_detection); // init gimbal

  // ----- socket listen -----
  const int listenfd = ::socket(PF_INET, SOCK_STREAM, 0);
  if (listenfd == -1) {
    std::perror("socket");
    return 1;
  }

  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(static_cast<uint16_t>(portnumber));

  if (::bind(listenfd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == -1) {
    std::perror("bind");
    ::close(listenfd);
    return 1;
  }

  if (::listen(listenfd, 128) == -1) {
    std::perror("listen");
    ::close(listenfd);
    return 1;
  }

  std::array<Client, FD_SETSIZE> clients{};
  for (auto& c : clients) c.fd = -1;

  fd_set allset;
  FD_ZERO(&allset);
  FD_SET(listenfd, &allset);

  int maxfd = listenfd;
  int maxi = -1;

  std::cout << "waiting for the client's request...\n";

  char buf[BUFSIZE + 1]{};

  while (true) {
    fd_set rset = allset;

    timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 1;

    const int ret = ::select(maxfd + 1, &rset, nullptr, nullptr, &tv);
    if (ret == 0) continue;
    if (ret < 0) {
      std::perror("select");
      break;
    }

    // new connection
    if (FD_ISSET(listenfd, &rset)) {
      sockaddr_in client_addr{};
      socklen_t len = sizeof(client_addr);

      const int connectfd = ::accept(listenfd, reinterpret_cast<sockaddr*>(&client_addr), &len);
      if (connectfd == -1) {
        std::perror("accept");
        continue;
      }

      int i = 0;
      for (; i < FD_SETSIZE; ++i) {
        if (clients[i].fd < 0) {
          clients[i].fd = connectfd;
          clients[i].addr = client_addr;
          std::cout << "You got a connection from " << inet_ntoa(clients[i].addr.sin_addr) << "\n";
          break;
        }
      }
      if (i == FD_SETSIZE) {
        std::cerr << "Too many connections\n";
        ::close(connectfd);
      } else {
        FD_SET(connectfd, &allset);
        if (connectfd > maxfd) maxfd = connectfd;
        if (i > maxi) maxi = i;
      }
      continue;
    }

    // existing clients
    for (int i = 0; i <= maxi; ++i) {
      const int sockfd = clients[i].fd;
      if (sockfd < 0) continue;
      if (!FD_ISSET(sockfd, &rset)) continue;

      std::memset(buf, 0, sizeof(buf));
      const int z = ::read(sockfd, buf, BUFSIZE);
      if (z <= 0) {
        std::cout << "disconnected by client!\n";
        ::close(sockfd);
        FD_CLR(sockfd, &allset);
        clients[i].fd = -1;
        continue;
      }

      buf[z] = '\0';
      std::printf("num = %d received data:%s\n", z, buf);

      // Keep your original protocol logic
      if (z == 3 || z == 6) {
        if (buf[0] == 'O' && buf[1] == 'N') {
          switch (buf[2]) {
            case 'A': t_up(50, 0);    std::printf("forward\n"); break;
            case 'B': t_down(50, 0);  std::printf("back\n");    break;
            case 'C': t_left(50, 0);  std::printf("left\n");    break;
            case 'D': t_right(50, 0); std::printf("right\n");   break;
            case 'E': t_stop(0);      std::printf("stop\n");    break;

            case 'L': lr_detection += 10.0f; clamp_servo(lr_detection); PWM_write(1, lr_detection); break; // left
            case 'I': lr_detection -= 10.0f; clamp_servo(lr_detection); PWM_write(1, lr_detection); break; // right
            case 'K': qh_detection += 10.0f; clamp_servo(qh_detection); PWM_write(2, qh_detection); break; // up
            case 'J': qh_detection -= 10.0f; clamp_servo(qh_detection); PWM_write(2, qh_detection); break; // down

            default:  t_stop(0);      std::printf("stop\n");    break;
          }
        } else {
          t_stop(0);
        }
      } else if (z == 6) { // unreachable given above, kept for parity with original
        if (static_cast<unsigned char>(buf[2]) == 0x00) {
          switch (static_cast<unsigned char>(buf[3])) {
            case 0x01: t_up(50, 0);    std::printf("forward\n"); break;
            case 0x02: t_down(50, 0);  std::printf("back\n");    break;
            case 0x03: t_left(50, 0);  std::printf("left\n");    break;
            case 0x04: t_right(50, 0); std::printf("right\n");   break;
            case 0x00: t_stop(0);      std::printf("stop\n");    break;
            default: break;
          }
        } else {
          t_stop(0);
        }
      } else {
        t_stop(0);
      }
    }
  }

  ::close(listenfd);
  return 0;
}