// tcp_sender.cpp
#include "tcp_sender.h"

// 组装完整消息帧（返回缓冲区指针和长度）
unsigned char* TcpJsonSender::buildFrame(
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

    // 4. 填充校验和（暂固定0x00）
    frame[frameLength - 1] = 0x00;

    return frame;
}

// 构造函数：初始化服务器地址和端口
TcpJsonSender::TcpJsonSender(const std::string& ip, int port)
    : server_ip(ip), server_port(port), sockfd(-1) {
    // 创建TCP套接字
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 初始化服务器地址结构
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);
}

// 析构函数：关闭套接字
TcpJsonSender::~TcpJsonSender() {
    if (sockfd >= 0) close(sockfd);
}

// 连接服务器
bool TcpJsonSender::connectServer() {
    if (sockfd < 0) return false;
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connection failed");
        return false;
    }
    return true;
}

// 关闭连接
void TcpJsonSender::disconnect() {
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
}

// 发送JSON数据（自动组装帧并发送）
bool TcpJsonSender::sendJson(const cJSON* jsonRoot, bool isLora) {
    if (sockfd < 0) {
        std::cerr << "Not connected to server" << std::endl;
        return false;
    }

    // 1. 生成JSON字符串（不格式化，节省空间）
    char* jsonStr = cJSON_PrintUnformatted(jsonRoot);
    if (!jsonStr) {
        std::cerr << "Failed to print JSON" << std::endl;
        return false;
    }

    // 2. 计算内容长度（排除NULL终结符）
    int contentLen = strlen(jsonStr);

    // 3. 选择帧头（0xAA/0xAB）
    unsigned char frameHeader = isLora ? 0xAB : 0xAA;

    // 4. 组装帧
    int frameLen;
    unsigned char* frame = buildFrame(frameHeader, jsonStr, contentLen, frameLen);

    // 5. 发送帧数据
    int sentBytes = send(sockfd, frame, frameLen, 0);
    delete[] frame;  // 释放帧缓冲区
    cJSON_free(jsonStr); // 释放cJSON字符串

    // 6. 检查发送结果
    if (sentBytes != frameLen) {
        std::cerr << "Send incomplete: " << sentBytes << "/" << frameLen << " bytes sent" << std::endl;
        return false;
    }
    return true;
}