//
// Created by liketao on 25-9-1.
//

#ifndef TOOL_H
#define TOOL_H

#include <cstring>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

bool create_directories(const std::string& path) ;

int get_interface_ip(const char* interface, char* ip_buf, size_t buf_len) ;

#endif //TOOL_H
