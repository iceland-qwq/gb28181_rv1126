//
// Created by liketao on 25-9-1.
//

#include "tool.h"




// 递归创建目录

bool create_directories(const std::string& path) {
    int len = path.length();
    char temp[1024];

    if (len >= sizeof(temp) - 1) {
        std::cerr << "路径过长: " << path << std::endl;
        return false;
    }

    strcpy(temp, path.c_str());

    // 从第二个字符开始遍历（跳过根或相对路径起始）
    for (char* p = temp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';  // 暂时截断

            // 尝试创建这一级目录
            if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
                std::cerr << "创建目录失败: " << temp << " (" << strerror(errno) << ")" << std::endl;
                *p = '/';  // 恢复
                return false;
            }

            *p = '/';  // 恢复斜杠
        }
    }

    // 创建最终完整路径
    if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
        std::cerr << "创建最终目录失败: " << temp << " (" << strerror(errno) << ")" << std::endl;
        return false;
    }

    return true;
}

int get_interface_ip(const char* interface, char* ip_buf, size_t buf_len) {
    int sock;
    struct ifreq ifr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    if (ioctl(sock, SIOCGIFADDR, &ifr) < 0) {
        close(sock);
        return -1;  // 接口可能未启用或不存在
    }

    const char* ip = inet_ntop(AF_INET, &((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr, ip_buf, buf_len);
    close(sock);

    return ip ? 0 : -1;
}
