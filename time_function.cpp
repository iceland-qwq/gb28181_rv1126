//
// Created by liketao on 25-8-24.
//

#include "time_function.h"
/**
 * 将时间戳转换为 ISO 8601 格式字符串 "YYYY-MM-DDTHH:MM:SS"
 * @param timestamp: 输入的时间戳 (time_t)
 * @param buffer: 用户提供的缓冲区，至少 20 字节
 * @return: 返回指向 buffer 的指针，失败返回 NULL
 */
char* format_timestamp_iso8601(time_t timestamp, char* buffer) {
    if (buffer == NULL) {
        return NULL;
    }

    struct tm *timeinfo = localtime(&timestamp);
    if (timeinfo == NULL) {
        return NULL;
    }

    // 格式化为 "YYYY-MM-DDTHH:MM:SS"
    if (strftime(buffer, 20, "%Y-%m-%dT%H:%M:%S", timeinfo) == 0) {
        // 如果输出被截断（理论上不会，因为正好19字符）
        buffer[0] = '\0';
        return NULL;
    }

    return buffer;
}

std::string add_seconds_to_timestamp(const std::string& ts_str, int seconds_offset) {
    struct tm tm_time = {0};
    int year, month, day, hour, minute, second;

    // 解析输入字符串
    if (sscanf(ts_str.c_str(), "%4d%2d%2d%2d%2d%2d",
               &year, &month, &day, &hour, &minute, &second) != 6) {
        return ""; // 解析失败
               }

    tm_time.tm_year = year - 1900;
    tm_time.tm_mon  = month - 1;
    tm_time.tm_mday = day;
    tm_time.tm_hour = hour;
    tm_time.tm_min  = minute;
    tm_time.tm_sec  = second;
    tm_time.tm_isdst = -1; // 自动处理夏令时

    // 转为 time_t，加偏移量
    time_t unix_time = mktime(&tm_time);
    unix_time += seconds_offset; // 加上偏移（正数为之后，负数为之前）

    // 转回 tm 结构
    struct tm* new_tm = localtime(&unix_time);

    // 格式化为 YYYYMMDDhhmmss 字符串
    char buffer[15];
    strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", new_tm);

    return std::string(buffer);
}

