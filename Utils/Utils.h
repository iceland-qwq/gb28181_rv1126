//
// Created by bxc on 2022/12/13.
//

#ifndef BXC_GB28181CLIENT_UTILS_H
#define BXC_GB28181CLIENT_UTILS_H
#include <string>
#include <chrono>
#include <vector>



static std::string getCurFormatTime(const char *format = "%Y-%m-%d %H:%M:%S") { //

    time_t t = time(nullptr);
    char tc[64];
    strftime(tc, sizeof(tc), format, localtime(&t));

    std::string tcs = tc;
    return tcs;
}




#endif // BXC_GB28181CLIENT_UTILS_H
