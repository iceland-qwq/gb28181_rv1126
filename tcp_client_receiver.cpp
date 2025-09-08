// tcp_client_receiver.cpp
#include "tcp_client_receiver.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>

tcp_client_receiver::tcp_client_receiver(message_queue& mq, const std::string& server_ip, int server_port)
    : mq_(mq), server_ip_(server_ip), server_port_(server_port),
      is_running_(false) {}

tcp_client_receiver::~tcp_client_receiver() {
    stop();
}

void tcp_client_receiver::start() {
    is_running_ = true;
    receive_thread_ = std::thread(&tcp_client_receiver::connect_and_receive, this);
}

void tcp_client_receiver::stop() {
    is_running_ = false;
    if (client_fd_ >= 0) {
        close(client_fd_);
    }
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
}

bool tcp_client_receiver::send_json(const cJSON* jsonRoot, bool isLora) {
    std::lock_guard<std::mutex> lock(send_mutex_);  // 线程安全

    if (client_fd_ < 0) {
        std::cerr << "[TcpReceiver] Not connected to server" << std::endl;
        return false;
    }

    // 1. 生成 JSON 字符串
    char* jsonStr = cJSON_PrintUnformatted(jsonRoot);
    if (!jsonStr) {
        std::cerr << "[TcpReceiver] Failed to print JSON" << std::endl;
        return false;
    }

    // 2. 构建帧
    int frameLen;
    unsigned char* frame = buildFrame(
        isLora ? 0xAB : 0xAA,  // 帧头选择
        jsonStr,
        strlen(jsonStr),
        frameLen
    );

    // 3. 发送帧数据
    int sentBytes = send(client_fd_, frame, frameLen, 0);
    delete[] frame;
    cJSON_free(jsonStr);

    if (sentBytes != frameLen) {
        std::cerr << "[TcpReceiver] Send incomplete: " << sentBytes << "/" << frameLen << " bytes sent" << std::endl;
        return false;
    }

    std::cout << "[TcpReceiver] JSON sent successfully" << std::endl;
    return true;
}

unsigned char* tcp_client_receiver::buildFrame(
    unsigned char frameHeader,
    const void* content,
    int contentLength,
    int& frameLength
) {
    // 帧结构：帧头(1B) + 长度(2B, 网络字节序) + 内容 + 校验和(1B)
    frameLength = 1 + 2 + contentLength + 1;
    unsigned char* frame = new unsigned char[frameLength];

    // 1. 填充帧头
    frame[0] = frameHeader;

    // 2. 填充内容长度（网络字节序）
    unsigned short lenNet = htons(contentLength);
    memcpy(frame + 1, &lenNet, 2);

    // 3. 填充JSON内容
    memcpy(frame + 1 + 2, content, contentLength);

    // 4. 填充校验和（固定0x00）
    frame[frameLength - 1] = 0x00;

    return frame;
}

bool tcp_client_receiver::do_connect() {
    if (client_fd_ >= 0) close(client_fd_);

    client_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd_ < 0) return false;

    int opt = 1;
    setsockopt(client_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port_);
    if (inet_pton(AF_INET, server_ip_.c_str(), &serv_addr.sin_addr) <= 0)
        return false;

    if (connect(client_fd_, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(client_fd_);
        client_fd_ = -1;
        return false;
    }
    std::cout << "[TcpReceiver] connected to " << server_ip_ << ":" << server_port_ << std::endl;
    return true;
}

void tcp_client_receiver::receive_loop() {
    uint8_t buffer[512];
    while (is_running_) {
        ssize_t n = recv(client_fd_, buffer, sizeof(buffer), 0);
        if (n > 0) {
            Message msg;
            msg.type = 0;
            msg.size = n;
            memcpy(msg.data, buffer, n);
            mq_.push(msg);
        } else if (n == 0) {
            std::cout << "[TcpReceiver] server closed, need reconnect\n";
            break;
        } else {
            if (errno == EINTR) continue;
            std::cerr << "[TcpReceiver] recv error: " << strerror(errno) << std::endl;
            break;
        }
    }
}

void tcp_client_receiver::connect_and_receive() {
    while (is_running_) {
        if (!do_connect()) {
            std::cerr << "[TcpReceiver] connect failed, retry in 1s\n";
            sleep(1);
            continue;
        }

        receive_loop();

        if (client_fd_ >= 0) {
            close(client_fd_);
            client_fd_ = -1;
        }
        if (!is_running_) break;
        std::cout << "[TcpReceiver] reconnect in 1s\n";
        sleep(1);
    }
}