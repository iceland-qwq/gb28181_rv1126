//
// Created by liketao on 25-9-10.
//

#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H
#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"

// 配置结构体
typedef struct {
    char sip_user[64];
    char username[64];
    char device_id[64];
    char device_name[64];
    char password[64];
    int local_port;
    int gb28181_tcp_port;
    int server_port;
    int expires;
    int keepalive_interval;
    char channel_id[64];
    char channel_id_[64];
    char to[128];
    char server_ip[64];
    char net_name[16];
    char record_file[256];
    char record_path[256];
} Config;

// 全局配置变量
Config g_config;

// 从文件读取JSON内容
char* read_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error opening file: %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* content = (char*)malloc(length + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }

    fread(content, 1, length, file);
    content[length] = '\0';

    fclose(file);
    return content;
}

// 解析JSON配置
int parse_config(const char* json_str) {
    cJSON* root = cJSON_Parse(json_str);
    if (!root) {
        fprintf(stderr, "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        return -1;
    }

    // 解析字符串字段
    cJSON* item;
    if ((item = cJSON_GetObjectItem(root, "sip_user")) && cJSON_IsString(item))
        strncpy(g_config.sip_user, item->valuestring, sizeof(g_config.sip_user));

    if ((item = cJSON_GetObjectItem(root, "username")) && cJSON_IsString(item))
        strncpy(g_config.username, item->valuestring, sizeof(g_config.username));

    if ((item = cJSON_GetObjectItem(root, "device_id")) && cJSON_IsString(item))
        strncpy(g_config.device_id, item->valuestring, sizeof(g_config.device_id));

    if ((item = cJSON_GetObjectItem(root, "device_name")) && cJSON_IsString(item))
        strncpy(g_config.device_name, item->valuestring, sizeof(g_config.device_name));

    if ((item = cJSON_GetObjectItem(root, "password")) && cJSON_IsString(item))
        strncpy(g_config.password, item->valuestring, sizeof(g_config.password));


    if ((item = cJSON_GetObjectItem(root, "channel_id")) && cJSON_IsString(item))
        strncpy(g_config.channel_id, item->valuestring, sizeof(g_config.channel_id));

    if ((item = cJSON_GetObjectItem(root, "channel_id_")) && cJSON_IsString(item))
        strncpy(g_config.channel_id_, item->valuestring, sizeof(g_config.channel_id_));

    if ((item = cJSON_GetObjectItem(root, "to")) && cJSON_IsString(item))
        strncpy(g_config.to, item->valuestring, sizeof(g_config.to));

    if ((item = cJSON_GetObjectItem(root, "server_ip")) && cJSON_IsString(item))
        strncpy(g_config.server_ip, item->valuestring, sizeof(g_config.server_ip));

    if ((item = cJSON_GetObjectItem(root, "net_name")) && cJSON_IsString(item))
        strncpy(g_config.net_name, item->valuestring, sizeof(g_config.net_name));

    if ((item = cJSON_GetObjectItem(root, "record_file")) && cJSON_IsString(item))
        strncpy(g_config.record_file, item->valuestring, sizeof(g_config.record_file));

    if ((item = cJSON_GetObjectItem(root, "record_path")) && cJSON_IsString(item))
        strncpy(g_config.record_path, item->valuestring, sizeof(g_config.record_path));

    // 解析数值字段
    if ((item = cJSON_GetObjectItem(root, "local_port")) && cJSON_IsNumber(item))
        g_config.local_port = item->valueint;

    if ((item = cJSON_GetObjectItem(root, "gb28181_tcp_port")) && cJSON_IsNumber(item))
        g_config.gb28181_tcp_port = item->valueint;


    if ((item = cJSON_GetObjectItem(root, "server_port")) && cJSON_IsNumber(item))
        g_config.server_port = item->valueint;

    if ((item = cJSON_GetObjectItem(root, "expires")) && cJSON_IsNumber(item))
        g_config.expires = item->valueint;

    if ((item = cJSON_GetObjectItem(root, "keepalive_interval")) && cJSON_IsNumber(item))
        g_config.keepalive_interval = item->valueint;

    cJSON_Delete(root);
    return 0;
}

// 初始化配置
int init_config(const char* config_file) {
    char* json_str = read_file(config_file);
    if (!json_str) {
        return -1;
    }

    int result = parse_config(json_str);
    free(json_str);
    return result;
}

// 打印配置（用于调试）
void print_config() {
    printf("SIP_USER: %s\n", g_config.sip_user);
    printf("USERNAME: %s\n", g_config.username);
    printf("DEVICE_ID: %s\n", g_config.device_id);
    printf("DEVICE_NAME: %s\n", g_config.device_name);
    printf("PASSWORD: %s\n", g_config.password);
    printf("LOCAL_PORT: %d\n", g_config.local_port);
    printf("GB28181_TCP_PORT: %d\n", g_config.gb28181_tcp_port);
    printf("SERVER_PORT: %d\n", g_config.server_port);
    printf("EXPIRES: %d\n", g_config.expires);
    printf("KEEPALIVE_INTERVAL: %d\n", g_config.keepalive_interval);
    printf("CHANNEL_ID: %s\n", g_config.channel_id);
    printf("CHANNEL_ID_: %s\n", g_config.channel_id_);
    printf("TO: %s\n", g_config.to);
    printf("SERVER_IP: %s\n", g_config.server_ip);
    printf("NET_NAME: %s\n", g_config.net_name);
    printf("RECORD_FILE: %s\n", g_config.record_file);
    printf("RECORD_PATH: %s\n", g_config.record_path);
}
#endif //CONFIG_LOADER_H
