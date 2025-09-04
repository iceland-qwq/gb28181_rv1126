//
// Created by liketao on 25-6-18.
//

#include "sdp_parser.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <iostream>


// 从 SDP 字符串中提取第一个媒体端口号
int get_first_media_port_from_sdp(const char *sdp) {
    if (sdp == NULL) return -1;

    char sdp_copy[4096];  // 避免修改原始数据
    strncpy(sdp_copy, sdp, sizeof(sdp_copy));
    sdp_copy[sizeof(sdp_copy) - 1] = '\0';

    char *line = strtok(sdp_copy, "\n");
    while (line != NULL) {
        // 去除行首空格
        while (*line == ' ' || *line == '\t') line++;

        if (strncmp(line, "m=", 2) == 0) {
            int port = 0;
            sscanf(line, "m=%*s %d", &port);  // 忽略媒体类型，只读取端口
            if (port > 0 && port <= 65535) {
                return port;  // 返回第一个找到的有效端口号
            }
        }

        line = strtok(NULL, "\n");
    }

    return -1;  // 没有找到有效端口
}

char* extract_ssrc_from_sdp(const char* sdp) {
    char* line_start = (char*)sdp;
    char* line_end;
    char* ssrc = NULL;

    // 逐行解析 SDP
    while ((line_start = strchr(line_start, '\n')) != NULL) {
        line_start++;  // 跳过换行符，指向当前行开头

        // 找到下一行的起始位置
        line_end = strchr(line_start, '\n');
        size_t line_len = (line_end) ? (line_end - line_start) : strlen(line_start);

        // 创建临时缓冲区保存当前行
        char line[line_len + 1];
        strncpy(line, line_start, line_len);
        line[line_len] = '\0';

        // 查找以 "y=" 开头的行
        if (strncmp(line, "y=", 2) == 0) {
            ssrc = strdup(line + 2);  // 提取 y= 后面的内容
            break;
        }
    }

    return ssrc;
}

char* extract_u_from_sdp(const char* sdp) {
    char* line_start = (char*)sdp;
    char* line_end;
    char* u = NULL;

    // 逐行解析 SDP
    while ((line_start = strchr(line_start, '\n')) != NULL) {
        line_start++;  // 跳过换行符，指向当前行开头

        // 找到下一行的起始位置
        line_end = strchr(line_start, '\n');
        size_t line_len = (line_end) ? (line_end - line_start) : strlen(line_start);

        // 创建临时缓冲区保存当前行
        char line[line_len + 1];
        strncpy(line, line_start, line_len);
        line[line_len] = '\0';

        // 查找以 "y=" 开头的行
        if (strncmp(line, "u=", 2) == 0) {
            u = strdup(line + 2);  // 提取 y= 后面的内容
            break;
        }
    }

    return u;
}

char* extract_o_from_sdp(const char* sdp) {
    char* line_start = (char*)sdp;
    char* line_end;
    char* u = NULL;

    // 逐行解析 SDP
    while ((line_start = strchr(line_start, '\n')) != NULL) {
        line_start++;  // 跳过换行符，指向当前行开头

        // 找到下一行的起始位置
        line_end = strchr(line_start, '\n');
        size_t line_len = (line_end) ? (line_end - line_start) : strlen(line_start);

        // 创建临时缓冲区保存当前行
        char line[line_len + 1];
        strncpy(line, line_start, line_len);
        line[line_len] = '\0';

        // 查找以 "y=" 开头的行
        if (strncmp(line, "o=", 2) == 0) {
            u = strdup(line + 2);  // 提取 y= 后面的内容
            break;
        }
    }

    return u;
}

/**
 * 从SDP字符串中提取s标签的内容
 * @param sdp: 输入的SDP字符串
 * @return: 返回s标签的内容（需调用者free），若未找到则返回NULL
 */
int extract_Slabel_content(const char* sdp) {
    if (!sdp) return NULL;

    const char* start = sdp;
    const char* end;
    char* line = NULL;
    char* result = NULL;

    while ((end = strchr(start, '\n')) != NULL) {
        // 计算当前行长度（包括换行符）
        int line_len = end - start + 1;

        // 提取当前行（包含换行符）
        line = (char*)malloc(line_len + 1);
        if (!line) return NULL; // 内存分配失败

        memcpy(line, start, line_len);
        line[line_len] = '\0';

        // 检查是否以 "s=" 开头
        if (strncmp(line, "s=", 2) == 0) {
            // 跳过 "s="，提取内容（去掉换行符）
            char* content_start = line + 2;
            char* newline_pos = strchr(content_start, '\n');
            if (newline_pos) {
                *newline_pos = '\0'; // 去掉换行符
            }

            // 分配内存并复制结果
            int content_len = strlen(content_start);
            result = (char*)malloc(content_len + 1);
            if (result) {
                strcpy(result, content_start);
            }

            free(line);
            break;
        }

        free(line);
        start = end + 1;
    }
    if (result=="Play")return Play;
    else if (strstr(result,"Playback"))return Playback;
    else if (strstr(result,"Download"))return Download;
}


void extract_and_format_times(const char* input, char* output1, char* output2) {
    const char* t_line = strstr(input, "t=");
    if (t_line == NULL) {
        printf("未找到 t 行\n");
        return;
    }

    // 跳过 "t=" 部分
    t_line += 2;

    // 提取第一个时间戳
    long start_time = atol(t_line);
    if (start_time <= 0) {
        printf("无效的第一个时间戳\n");
        return;
    }
    format_timestamp_iso8601(start_time, output1);

    // 找到第一个空格，跳过它
    const char* space = strchr(t_line, ' ');
    if (space == NULL) {
        printf("格式错误：缺少第二个时间戳\n");
        return;
    }
    t_line = space + 1;

    // 提取第二个时间戳
    long end_time = atol(t_line);
    if (end_time <= 0) {
        printf("无效的第二个时间戳\n");
        return;
    }
    format_timestamp_iso8601(end_time, output2);
}
