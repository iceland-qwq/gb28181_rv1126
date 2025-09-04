// tcp_sender.h
#ifndef TCP_SENDER_H
#define TCP_SENDER_H


#include <string>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include "cJSON.h"

class TcpJsonSender {
private:
    int sockfd;                // TCP套接字描述符
    struct sockaddr_in server_addr; // 服务器地址信息
    std::string server_ip;     // 服务器IP
    int server_port;           // 服务器端口

    // 组装完整消息帧（返回缓冲区指针和长度）
    static unsigned char* buildFrame(
        unsigned char frameHeader,  // 帧头 (0xAA/0xAB)
        const void* content,        // JSON内容指针
        int contentLength,          // 内容长度
        int& frameLength            // 返回帧总长度
    );

public:
    // 构造函数：初始化服务器地址和端口
    TcpJsonSender(const std::string& ip, int port);

    // 析构函数：关闭套接字
    ~TcpJsonSender();

    // 连接服务器
    bool connectServer();

    // 关闭连接
    void disconnect();

    // 发送JSON数据（自动组装帧并发送）
    bool sendJson(const cJSON* jsonRoot, bool isLora = false);
};

#endif // TCP_SENDER_H