//
// Created by liketao on 25-6-29.
//
// Copyright 2020 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <cstring>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>

#include <chrono>

#include "mpp_guard.hpp"
using namespace std::chrono;
#include "tool.h"
#include "shared_queue.h"
#include "venc.h"
#include "tcp_sender.h"
#include "config.h"
#include <thread>

#include "gb28181_functions.h"
#include "record.h"
#include "message_queue.h"
#include "time_function.h"
extern "C"{
#include "common/sample_common.h"
#include "cJSON.h"
}
#include "rkmedia/rkmedia_api.h"
#include "rkmedia/rkmedia_venc.h"
#include "common/sample_common.h"
#define DEVICE_ID "34020000001310008758"
#define DEVICE_NAME      "34020000001320008758"

#define CAP_PIC_PATH "/root/data/monitor/"
#define ALRAM_PATH "/root/data/alarm/"


#include <vector>
#include <memory>
#include <cstring>
#include <mutex>



static char LOCAL_IP[50];

RecordCsv *recorder = new RecordCsv("/root/lkt/record_data/record.csv");
static FILE *h264_file = NULL;
static time_t segment_start_time = 0;  // 当前段起始时间（秒）
static time_t segment_end_time = 0;    // 当前段结束时间（秒）
static pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

#include <signal.h>
bool thread0_start_play_ontime  = false;


// 配置参数
static const int FPS = 30;
static const int DURATION_SEC = 180;  // 3分钟ol RecordCsv::append(const Record& rec)



int black_flag = 0;



extern SharedQueue sharedQueue;
extern SharedQueue sharedQueue_0;
static bool quit = false;
static bool quit_0 = false;
static FILE *g_output_file;
static RK_S32 g_s32FrameCnt = -1;

// thread 0 word type
enum {
    thread0_type_normol = 0,
    thread0_type_cap_pic ,
    thread0_type_cap_video,
    thread0_type_cap_ontime_pic ,

};
static int thread0_video_type = thread0_type_normol;
static int thread0_jpeg_type = thread0_type_normol;
// save alarm video pic structs

// 定义缓冲区大小
const int MAX_FRAMES = 900;
// 每帧数据结构
struct Frame {
    std::unique_ptr<uint8_t[]> data;
    size_t size;
};
std::vector<Frame> frame_buffer(MAX_FRAMES);  // 环形缓冲区
bool buffer_full = false;
int write_index = 0;  // 当前写入位置
std::mutex buffer_mutex;

// 触发时的信息记录
struct TriggerInfo {
    int trigger_index = -1;
    bool buffer_full_at_trigger = false;
    int write_index_at_trigger = 0;
};

TriggerInfo trigger_info;
// 全局变量：是否正在等待保存
std::atomic<bool> waiting_for_post_frames{false};
int required_post_frames = 450;
int collected_post_frames = 0;

// 用于通知保存完成
std::condition_variable post_frame_cv;
std::mutex post_frame_mutex;





/* jepg save header
 *
 */

// JPEG 环形缓冲区
const int MAX_JPEG_FRAMES = 20; // 30fps * 20秒 = 600帧
struct JpegFrame {
    std::unique_ptr<uint8_t[]> data;
    size_t size;
    time_t timestamp; // 可选：用于生成文件名
};
std::vector<JpegFrame> jpeg_frame_buffer(MAX_JPEG_FRAMES);
bool jpeg_buffer_full = false;
int jpeg_write_index = 0;

// 触发时的信息记录
struct JpegTriggerInfo {
    int trigger_index = -1;
    bool buffer_full_at_trigger = false;
    int write_index_at_trigger = 0;
};
JpegTriggerInfo jpeg_trigger_info;

// 全局变量：是否正在等待保存
std::atomic<bool> waiting_for_jpeg_post_frames{false};
int required_jpeg_post_frames = 10; // 10秒
int collected_jpeg_post_frames = 0;

// 用于通知保存完成
std::condition_variable jpeg_post_frame_cv;
std::mutex jpeg_post_frame_mutex;



// timestamp record info
static const char *  cap_ontime_stamp = NULL;
static const char *  cap_alarm_stamp = NULL;

static int global_msgid = 1;


int is_night_time(void) {
    time_t now;
    struct tm *local;

    // 获取当前时间
    time(&now);
    local = localtime(&now);

    int hour = local->tm_hour;  // 0-23

    // 判断是否在 19:00 - 23:59 或 00:00 - 07:00
    if (hour >= 19 || hour < 7) {
        return 0;  // 在晚上七点到早上七点之间
    } else {
        return 1;  // 不在该时间段
    }
}


static void draw_char(RK_U32 *buffer, int x, int y, int width, int height, char c, RK_U32 color) {
    int char_index = -1;
    if (c >= '0' && c <= '9') {
        char_index = c - '0';
    } else if (c == ':') {
        char_index = 10;
    } else if (c == '-') {
        return;  // 假设 font_32x32 支持 '-'，请确认索引
    } else if (c == ' ') {
        return; // 空格不绘制
    } else {
        return; // 不支持的字符
    }

    const unsigned char *bitmap = font_32x32[char_index]; // 指向 128 字节数据
    for (int row = 0; row < 32; row++) {                  // 32 行
        int base_byte = row * 4; // 每行占 4 字节
        for (int col = 0; col < 32; col++) {              // 32 列
            int byte_offset = base_byte + (col / 8);       // 字节在行内的偏移
            int bit = 7 - (col % 8);                       // 位在字节内的位置（从左到右）
            if (bitmap[byte_offset] & (1 << bit)) {        // 该位为 1，则绘制像素
                int px = x + col;
                int py = y + row;
                if (px >= 0 && px < width && py >= 0 && py < height) {
                    buffer[py * width + px] = color;
                }
            }
        }
    }
}

static void draw_string(RK_U32 *buffer, int x, int y, int width, int height,const char *str, RK_U32 color) {
    int current_x = x;
    for (int i = 0; str[i] != '\0'; i++) {
        draw_char(buffer, current_x, y, width, height, str[i], color);
        current_x += 25;  // 每个字符宽 32 像素，紧密排列
        // 可选：加空隙 current_x += 36; // 32 + 4 像素间距
    }
}
// 获取当前时间字符串
static void get_time_string(char *buffer, int buffer_size) {
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", timeinfo);
}

static void set_argb8888_buffer(RK_U32 *buf, RK_U32 size, RK_U32 color) {
    for (RK_U32 i = 0; buf && (i < size); i++)
        *(buf + i) = color;
}




void parse_json_from_string(const std::string& json_str) {
    // 1. 使用 c_str() 转换为 C 字符串
    cJSON *root = cJSON_Parse(json_str.c_str());
    if (root == nullptr) {
        std::cerr << "Error: Failed to parse JSON." << std::endl;
        return;
    }

    // 2. 提取字段示例
    cJSON *name = cJSON_GetObjectItem(root, "name");


    if (cJSON_IsString(name) && name->valuestring != nullptr) {
        std::cout << "Name: " << name->valuestring << std::endl;
    }


    // 3. 释放内存（非常重要！）
    cJSON_Delete(root);
}


cJSON* parse_json_from_frame(uint8_t *data, size_t data_len) {
    // 1. 检查最小数据长度（帧头 + 长度字段 + 校验和）
    if (data_len < 4) {
        printf("Error: Data too short for frame header.\n");
        return NULL;
    }

    // 2. 检查帧头
    uint8_t frame_header = data[0];
    std::cout << "first byte:"<<data[0] << std::endl;
    if (frame_header != 0xAA && frame_header != 0xAB) {
        printf("Error: Invalid frame header (0x%02X).\n", frame_header);
        return NULL;
    }

    // 3. 解析长度字段（网络字节序 -> 本地字节序）
    uint16_t payload_len = (data[1] << 8) | data[2];

    // 4. 计算总帧长度（含内容 + 校验和）
    int total_len = 4 + payload_len;
    if (data_len < (size_t)total_len) {
        printf("Error: Frame incomplete. Expected %d bytes, got %zu.\n", total_len, data_len);
        return NULL;
    }

    // 5. 提取内容部分（注意：可能是二进制数据，需手动加 \0）
    uint8_t *content = data + 3;        // 内容起始地址
    uint8_t checksum = data[total_len - 1]; // 校验和地址（可选验证）

    // 6. 将内容复制到 char[] 并加 \0（假设是 JSON 字符串）
    char json_str[256];  // 假设最大 JSON 长度为 255
    if (payload_len >= sizeof(json_str)) {
        printf("Error: JSON payload too long.\n");
        return NULL;
    }
    memcpy(json_str, content, payload_len);
    json_str[payload_len] = '\0';  // 手动加字符串结束符

    // 7. 使用 cJSON 解析 JSON
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        printf("Error: JSON parse failed at: %s\n", cJSON_GetErrorPtr());
        return NULL;
    }

    // ✅ 成功返回 cJSON 对象
    return root;
}

static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  quit = true;
}

void stop_media_cam(){

    quit = true;

}
int ret ;

void sig_save_video(int sig) {
    printf("\nReceived SIGUSR1! Saving last 3 minutes...\n");

}

void save_around_trigger_video_async() {
    const char * timestamp = cap_alarm_stamp;
    std::unique_lock<std::mutex> cv_lock(post_frame_mutex);
    printf("save method start \n");
    // 等待收集够 900 帧
    post_frame_cv.wait(cv_lock, [] {
        return collected_post_frames >= required_post_frames;
    });
    if (timestamp != NULL) {
        std::string cap_alarm_path = std::string(ALRAM_PATH) + timestamp + "/";

        create_directories(cap_alarm_path);
        // 现在可以安全保存前后帧了
        char filename[300];
        snprintf(filename, sizeof(filename), "%s%s.h264", cap_alarm_path.c_str(),timestamp);
        std::cout << filename << std::endl;
        FILE *fp = fopen(filename, "wb");
        if (!fp) {
            printf("ERROR: failed to open file\n");
            return;
        }

        std::lock_guard<std::mutex> lock(buffer_mutex);

        int trigger_idx = trigger_info.trigger_index;

        // 保存前900帧
        for (int i = 0; i < 450; ++i) {
            int idx = (trigger_idx - 450 + i + MAX_FRAMES) % MAX_FRAMES;
            auto& f = frame_buffer[idx];
            if (f.size > 0) {
                fwrite(f.data.get(), 1, f.size, fp);
            }
        }

        // 保存后900帧
        for (int i = 0; i < 450; ++i) {
            int idx = (trigger_idx + i) % MAX_FRAMES;
            auto& f = frame_buffer[idx];
            if (f.size > 0) {
                fwrite(f.data.get(), 1, f.size, fp);
            }
        }

        fclose(fp);
        printf("✅ Saved 900 frames around trigger\n");

        // 重置状态
        waiting_for_post_frames = false;
        collected_post_frames = 0;

            printf("USB0 IP Address: %s\n", LOCAL_IP);
            TcpJsonSender sender("127.0.0.1", TCP_SENDER_PORT);
            if (!sender.connectServer()) printf("Failed to connect to server.\n");
            cJSON *root = cJSON_CreateObject();

            cJSON_AddNumberToObject(root, "msgid", global_msgid);
            cJSON_AddNumberToObject(root, "msgcode", 7);

            cJSON_AddStringToObject(root, "result", "ok");
            if (sender.sendJson(root)) {
                std::cout << "JSON sent successfully!" << std::endl;
            } else {
                std::cout << "Send failed!" << std::endl;
            }

            // 5. 清理资源
            cJSON_Delete(root);
            sender.disconnect();


    }
    else {
        printf("cap timestamp is NULL\n");
    }
}



void save_around_trigger_jpeg_async() {
    const char * timestamp = cap_alarm_stamp;
    std::unique_lock<std::mutex> cv_lock(jpeg_post_frame_mutex);
    printf("JPEG save method start\n");

    // 等待收集够 300 帧（10秒）
    jpeg_post_frame_cv.wait(cv_lock, [] {
        return collected_jpeg_post_frames >= required_jpeg_post_frames;
    });

    std::lock_guard<std::mutex> lock(buffer_mutex);
    int trigger_idx = jpeg_trigger_info.trigger_index;

    // 构建文件名前缀
    char base_filename[256];
    time_t now = time(nullptr);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    std::string cap_alarm_path = std::string(ALRAM_PATH) + timestamp + "/";

    create_directories(cap_alarm_path);

    // 1. 保存前 10 秒的一张 JPEG
    int prev_idx = (trigger_idx - 10+ MAX_JPEG_FRAMES) % MAX_JPEG_FRAMES; // 300 帧 = 10 秒
    auto& f_prev = jpeg_frame_buffer[prev_idx];
    if (f_prev.size > 0) {
        char filename[300];
        snprintf(filename, sizeof(filename), "%s%s.jpg", cap_alarm_path.c_str(),add_seconds_to_timestamp(timestamp,-10).c_str());
        FILE *fp = fopen(filename, "wb");
        if (fp) {
            fwrite(f_prev.data.get(), 1, f_prev.size, fp);
            fclose(fp);
            printf("Saved: %s\n", filename);
        }
    }

    // 2. 保存当前帧的一张 JPEG
    int curr_idx = trigger_idx % MAX_JPEG_FRAMES;

    auto& f_curr = jpeg_frame_buffer[curr_idx];
    if (f_curr.size > 0) {
        char filename[300];
        snprintf(filename, sizeof(filename), "%s%s.jpg", cap_alarm_path.c_str(),timestamp);
        FILE *fp = fopen(filename, "wb");
        if (fp) {
            fwrite(f_curr.data.get(), 1, f_curr.size, fp);
            fclose(fp);
            printf("Saved: %s\n", filename);
        }
    }

    // 3. 保存后 10 秒的一张 JPEG
    int next_idx = (trigger_idx + 300) % MAX_JPEG_FRAMES;
    auto& f_next = jpeg_frame_buffer[next_idx];
    if (f_next.size > 0) {
        char filename[300];
        snprintf(filename, sizeof(filename), "%s%s.jpg", cap_alarm_path.c_str(),add_seconds_to_timestamp(timestamp,+10).c_str());
        FILE *fp = fopen(filename, "wb");
        if (fp) {
            fwrite(f_next.data.get(), 1, f_next.size, fp);
            fclose(fp);
            printf("Saved: %s\n", filename);
        }
    }

    // 重置状态
    waiting_for_jpeg_post_frames = false;
    collected_jpeg_post_frames = 0;


}
void video_packet_cb_chh0(MEDIA_BUFFER mb) {

    static steady_clock::time_point last = steady_clock::now();
    steady_clock::time_point now_ = steady_clock::now();
    int64_t delta_us = duration_cast<microseconds>(now_ - last).count();
    last = now_;

    /* 打印间隔：可以改成每 N 帧打印一次，减少刷屏 */
    static int cnt = 0;
    if (++cnt % 30 == 0)   // 每 30 帧打印一次
        printf("[ch0] frame=%d delta=%ld us\n", cnt, delta_us);
    if (quit_0)
        return;
    //printf("video_packet_cb_chh0 is running \n");


    switch (thread0_video_type) {
        case thread0_type_normol: {

            break;
        }


        case thread0_type_cap_pic: {
            break;
        }

        case thread0_type_cap_video:{
;
            std::thread cap_video(save_around_trigger_video_async);

            thread0_video_type = thread0_type_normol;

            cap_video.detach();
            break;
        }


    }


    // 获取当前时间
    time_t now = time(NULL);

    // ===== 1. 检查是否需要创建新文件 =====
    bool should_create_new_file = false;

    if (h264_file == NULL) {
        // 首次运行，必须创建
        should_create_new_file = true;
    } else if (now >= segment_end_time) {
        // 当前时间段已结束（10分钟到），切换文件
        should_create_new_file = true;
    }

    if (should_create_new_file) {
        pthread_mutex_lock(&file_mutex);

        // 关闭旧文件
        if (h264_file) {

            fclose(h264_file);
            h264_file = NULL;
        }

        // 计算当前10分钟段的起始时间（对齐到10分钟边界）
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        // 将分钟对齐到 10 的倍数，秒归零
        int aligned_min = (tm_now.tm_min / 10) * 10;
        tm_now.tm_min = aligned_min;
        tm_now.tm_sec = 0;

        // 起始时间
        time_t start = mktime(&tm_now);
        time_t end = start + 600;  // 10分钟 = 600秒

        // 格式化时间字符串
        char start_str[32], end_str[32];
        strftime(start_str, sizeof(start_str), "%Y-%m-%dT%H:%M:%S", &tm_now);
        struct tm tm_end;
        localtime_r(&end, &tm_end);
        strftime(end_str, sizeof(end_str), "%Y-%m-%dT%H:%M:%S", &tm_end);

        // 生成文件名
        char filename[256];
        snprintf(filename, sizeof(filename), "%s%s_%s.h264", RECORD_PATH,start_str, end_str);
        recorder->append({
    DEVICE_ID ,
        DEVICE_NAME,
            start_str,
            end_str,
            100000,
        }
            );
        // 创建文件
        h264_file = fopen(filename, "wb");
        if (h264_file) {
            segment_start_time = start;
            segment_end_time = end;
            printf("New 10min file created: %s\n", filename);
        } else {
            fprintf(stderr, "Failed to open file: %s\n", filename);
        }

        pthread_mutex_unlock(&file_mutex);
    }


    // 检查是否在等待后置帧
    if (waiting_for_post_frames) {
        std::lock_guard<std::mutex> cv_lock(post_frame_mutex);
        collected_post_frames++;
        printf("collected_post_frames: %d\n", collected_post_frames);
        // 如果收集够了，通知保存线程
        if (collected_post_frames >= required_post_frames) {
            post_frame_cv.notify_one();
        }
    }



    // ===== 2. 写入数据 =====
    void *data = RK_MPI_MB_GetPtr(mb);
    size_t size = RK_MPI_MB_GetSize(mb);

    if (!data || size == 0) {
        fprintf(stderr, "Invalid MEDIA_BUFFER data or size is zero.\n");
        return;
    }



    std::lock_guard<std::mutex> lock(buffer_mutex);  //  线程安全

    Frame& current_frame = frame_buffer[write_index];

    // 释放旧数据（如果存在）
    if (current_frame.data) {
        current_frame.data.reset();
    }

    // 分配新内存并拷贝 H.264 数据
    current_frame.data = std::make_unique<uint8_t[]>(size);
    std::memcpy(current_frame.data.get(), data, size);
    current_frame.size = size;

    // 更新缓冲区状态
    write_index = (write_index + 1) % MAX_FRAMES;
    if (write_index == 0) {
        buffer_full = true;  // 缓冲区已满
    }
    pthread_mutex_lock(&file_mutex);
    if (h264_file) {
        if (fwrite(data, 1, size, h264_file) != size) {
            fprintf(stderr, "Failed to write to file.\n");
        }
    } else {
        fprintf(stderr, "No file open, dropping packet.\n");
    }
    int flag = RK_MPI_MB_GetFlag(mb);
    if (thread0_start_play_ontime) {
        bool is_idr = (flag == VENC_NALU_IDRSLICE);
        Buffer buffer(new char[1024 * 1024]);
        if (size < (1024 * 1024)) {
            std::memcpy(buffer.get() + 19, data, size);
            printf("4K packet size = %d\n",size);
            video_packet packet = {0};
            packet.buf = std::move(buffer);
            packet.size = size;
            packet.isIFrame = is_idr;
            sharedQueue_0.enqueue(std::move(packet));
        }
    }




    RK_MPI_MB_TsNodeDump(mb);
    RK_MPI_MB_ReleaseBuffer(mb);

    pthread_mutex_unlock(&file_mutex);
}



void jpeg_packet_cb_chh0(MEDIA_BUFFER mb) {
    if (quit_0)
        return;

    static steady_clock::time_point last = steady_clock::now();
    steady_clock::time_point now_ = steady_clock::now();
    int64_t delta_us = duration_cast<microseconds>(now_ - last).count();
    last = now_;

    static int cnt = 0;
    if (++cnt % 1 == 0)   // 每 1 帧打印一次
        printf("[ch0] frame=%d delta=%ld us\n", cnt, delta_us);


    //printf("jpeg_packet_cb_chh0 is running \n");
    switch (thread0_jpeg_type) {
        case thread0_type_normol: {

            break;
        }


        case thread0_type_cap_pic: {
            thread0_jpeg_type = thread0_type_normol;
            std::thread cap_jpeg(save_around_trigger_jpeg_async);


            cap_jpeg.detach();
            break;
        }

        case thread0_type_cap_video:{


            break;
        }
        case thread0_type_cap_ontime_pic: {
            thread0_jpeg_type = thread0_type_normol;
            auto& f_prev = jpeg_frame_buffer[jpeg_write_index];
            if (f_prev.size > 0) {
                char filename[300];
                if (cap_ontime_stamp!=NULL){
                snprintf(filename, sizeof(filename), "%s%s.jpg",CAP_PIC_PATH,cap_ontime_stamp);
                printf("filname: %s\n", filename);

                    FILE *fp = fopen(filename, "wb");
                if (fp) {
                    fwrite(f_prev.data.get(), 1, f_prev.size, fp);
                    fclose(fp);
                    printf("Saved: %s\n", filename);
                }
                }
            }

            break ;
        }

    }



    void *data = RK_MPI_MB_GetPtr(mb);
    size_t size = RK_MPI_MB_GetSize(mb);

    if (!data || size == 0) {
        fprintf(stderr, "Invalid MEDIA_BUFFER data or size is zero.\n");
        return;
    }
    JpegFrame& current_frame_jepg = jpeg_frame_buffer[jpeg_write_index];

    // 释放旧数据（如果存在）
    if (current_frame_jepg.data) {
        current_frame_jepg.data.reset();
    }

    // 分配新内存并拷贝 JPEG 数据
    current_frame_jepg.data = std::make_unique<uint8_t[]>(size);
    std::memcpy(current_frame_jepg.data.get(), data, size);
    current_frame_jepg.size = size;
    current_frame_jepg.timestamp = time(nullptr); // 可选

    RK_MPI_MB_TsNodeDump(mb);
    RK_MPI_MB_ReleaseBuffer(mb);
    // 更新缓冲区状态
    jpeg_write_index = (jpeg_write_index + 1) % MAX_JPEG_FRAMES;
    if (jpeg_write_index == 0) {
        jpeg_buffer_full = true;
    }

    // 检查是否在等待后置帧
    if (waiting_for_jpeg_post_frames) {
        std::lock_guard<std::mutex> cv_lock(jpeg_post_frame_mutex);
        collected_jpeg_post_frames ++;
        printf("collected_post_frames: %d\n", collected_jpeg_post_frames );
        // 如果收集够了，通知保存线程
        if (collected_jpeg_post_frames  >= required_jpeg_post_frames ) {
            jpeg_post_frame_cv.notify_one();
        }
    }

}
void jpeg_packet_thread(int channel_id) {
    VENC_RECV_PIC_PARAM_S stRecvParam;
    while (!quit_0) {
        stRecvParam.s32RecvPicNum = 1;
        RK_MPI_VENC_StartRecvFrame(channel_id, &stRecvParam);
        sleep(1);
    }

}
void time_watermark_thread(int channel_id) {


    int osd_width = 640;
    int osd_height = 64;
    int wxh_size = osd_width * osd_height;
    BITMAP_S BitMap;

    BitMap.enPixelFormat = PIXEL_FORMAT_ARGB_8888;
    BitMap.u32Width = osd_width;
    BitMap.u32Height = osd_height;
    BitMap.pData = malloc(wxh_size * TEST_ARGB32_PIX_SIZE);
    if (!BitMap.pData) {
        printf("ERROR: no mem left for argb8888(%d)!\n", wxh_size * TEST_ARGB32_PIX_SIZE);
    }


    // 设置OSD区域属性 - 确保位置和尺寸16字节对齐
    OSD_REGION_INFO_S RngInfo;
    RngInfo.enRegionId = REGION_ID_0;
    RngInfo.u32PosX = 16;    // 16 % 16 = 0，符合要求
    RngInfo.u32PosY = 16;    // 16 % 16 = 0，符合要求
    RngInfo.u32Width = osd_width;
    RngInfo.u32Height = osd_height;
    RngInfo.u8Enable = 1;
    RngInfo.u8Inverse = 0;



    // 获取当前时间
    while (true) {
        set_argb8888_buffer((RK_U32 *)BitMap.pData, wxh_size, TEST_ARGB32_TRANS);
        char time_str[32];
        get_time_string(time_str, sizeof(time_str));


        draw_string((RK_U32 *)BitMap.pData, 10,10, osd_width,osd_height, time_str, TEST_ARGB32_RED);

        // 更新OSD
        ret = RK_MPI_VENC_RGN_SetBitMap(channel_id, &RngInfo, &BitMap);
        if (ret) {
            printf("ERROR: set rgn bitmap failed! ret=%d\n", ret);
        }
        sleep(1);
    }
}

void time_watermark_thread_(int channel_id) {


    int osd_width = 640;
    int osd_height = 64;
    int wxh_size = osd_width * osd_height;
    BITMAP_S BitMap;

    BitMap.enPixelFormat = PIXEL_FORMAT_ARGB_8888;
    BitMap.u32Width = osd_width;
    BitMap.u32Height = osd_height;
    BitMap.pData = malloc(wxh_size * TEST_ARGB32_PIX_SIZE);
    if (!BitMap.pData) {
        printf("ERROR: no mem left for argb8888(%d)!\n", wxh_size * TEST_ARGB32_PIX_SIZE);
    }


    // 设置OSD区域属性 - 确保位置和尺寸16字节对齐
    OSD_REGION_INFO_S RngInfo;
    RngInfo.enRegionId = REGION_ID_0;
    RngInfo.u32PosX = 16;    // 16 % 16 = 0，符合要求
    RngInfo.u32PosY = 16;    // 16 % 16 = 0，符合要求
    RngInfo.u32Width = osd_width;
    RngInfo.u32Height = osd_height;
    RngInfo.u8Enable = 1;
    RngInfo.u8Inverse = 0;



    // 获取当前时间
    while (!quit) {
        set_argb8888_buffer((RK_U32 *)BitMap.pData, wxh_size, TEST_ARGB32_TRANS);
        char time_str[32];
        get_time_string(time_str, sizeof(time_str));


        draw_string((RK_U32 *)BitMap.pData, 10,10, osd_width,osd_height, time_str, TEST_ARGB32_RED);

        // 更新OSD
        ret = RK_MPI_VENC_RGN_SetBitMap(channel_id, &RngInfo, &BitMap);
        if (ret) {
            printf("ERROR: set rgn bitmap failed! ret=%d\n", ret);
        }
        sleep(1);
    }
    RngInfo.u8Enable = 0;
    RK_MPI_VENC_RGN_SetBitMap(channel_id, &RngInfo, &BitMap);

    // 释放资源
    if (BitMap.pData)
        free(BitMap.pData);
}



void video_packet_cb(MEDIA_BUFFER mb) {
    static RK_S32 packet_cnt = 0;
    if (quit)
        return;

    // === 1. 获取帧类型 ===
    int flag = RK_MPI_MB_GetFlag(mb);
    bool is_idr = (flag == VENC_NALU_IDRSLICE);
    const char *nalu_type = is_idr ? "IDR Slice" : "P Slice";
    FILE *file;

    char is_black[50];
    /*
    if (!is_night_time()) {
        if (black_flag==0) {
            SAMPLE_COMM_ISP_SET_Saturation(0,0);
            printf("set black sucess\n");
            black_flag = 1;




                TcpJsonSender sender("127.0.0.1", GB28181_TCP_PORT);
                if (!sender.connectServer()) printf("Failed to connect to server.\n");
                cJSON *root = cJSON_CreateObject();

                cJSON_AddNumberToObject(root, "msgid", global_msgid+1);
                cJSON_AddNumberToObject(root, "msgcode", 3);
                cJSON_AddNumberToObject(root, "switch", 1);
                cJSON_AddNumberToObject(root, "lightness", 100);
                cJSON_AddNumberToObject(root, "filter", 1);
                cJSON_AddNumberToObject(root, "lightauto", 1);
                if (sender.sendJson(root)) {
                    std::cout << "JSON sent successfully!" << std::endl;
                } else {
                    std::cout << "Send failed!" << std::endl;
                }

                // 5. 清理资源
                cJSON_Delete(root);
                sender.disconnect();

        }

    }
    else {
        if (black_flag == 1) {
            SAMPLE_COMM_ISP_SET_Saturation(0,128);
            printf("set color sucess\n");
            black_flag = 0;



                TcpJsonSender sender("127.0.0.1", GB28181_TCP_PORT);
                if (!sender.connectServer()) printf("Failed to connect to server.\n");
                cJSON *root = cJSON_CreateObject();

                cJSON_AddNumberToObject(root, "msgid", global_msgid+1);
                cJSON_AddNumberToObject(root, "msgcode", 3);
                cJSON_AddNumberToObject(root, "switch", 1);
                cJSON_AddNumberToObject(root, "lightness", 100);
                cJSON_AddNumberToObject(root, "filter", 0);
                cJSON_AddNumberToObject(root, "lightauto", 1);
                if (sender.sendJson(root)) {
                    std::cout << "JSON sent successfully!" << std::endl;
                } else {
                    std::cout << "Send failed!" << std::endl;
                }

                // 5. 清理资源
                cJSON_Delete(root);
                sender.disconnect();


        }
    }
  */
    // === 2. 获取数据 ===
    void *data = RK_MPI_MB_GetPtr(mb);
    int size = RK_MPI_MB_GetSize(mb);
    int64_t ts_ms = RK_MPI_MB_GetTimestamp(mb) / 1000;  // 毫秒

    printf("Frame %d, size: %d, type: %s\n", packet_cnt, size, nalu_type);

    // === 5. 发送到共享队列（原逻辑）===
    Buffer buffer(new char[1024 * 1024]);
    if (size < (1024 * 1024)) {
        std::memcpy(buffer.get() + 19, data, size);
        video_packet packet = {0};
        packet.buf = std::move(buffer);
        packet.size = size;
        packet.isIFrame = is_idr;
        sharedQueue.enqueue(std::move(packet));
    }

    RK_MPI_MB_TsNodeDump(mb);
    RK_MPI_MB_ReleaseBuffer(mb);

    packet_cnt++;
    if ((g_s32FrameCnt >= 0) && (packet_cnt > g_s32FrameCnt))
        quit = true;
}



void thread_chh0(message_queue& mq,eXosip_t *context_exosip) {

    quit_0 = false;
    //RK_U32 u32Width = 3840;
    VENC_RECV_PIC_PARAM_S stRecvParam;

    //RK_U32 u32Height = 2160;
    RK_U32 u32Width = 1280;
    RK_U32 u32Height = 720;
    const RK_CHAR *pDeviceName = "rkispp_scale1";
    RK_CHAR *pOutPath = NULL;
    const RK_CHAR *pIqfilesPath = "/etc/iqfiles";
    CODEC_TYPE_E enCodecType = RK_CODEC_TYPE_H264;
    const RK_CHAR *pCodecName = "H264";
    RK_S32 s32CamId = 0;
    RK_U32 u32BufCnt = 20;

    MPP_CHN_S stSrcChn;
    stSrcChn.enModId = RK_ID_VI;
    stSrcChn.s32DevId = 0;
    stSrcChn.s32ChnId = 0;

    MPP_CHN_S stDestChn;
    stDestChn.enModId = RK_ID_VENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = 2;

    MPP_CHN_S stEncChn;
    stEncChn.enModId = RK_ID_VENC;
    stEncChn.s32DevId = 0;
    stEncChn.s32ChnId = 0;


#ifdef RKAIQ
    RK_BOOL bMultictx = RK_FALSE;
    RK_U32 u32Fps = 30;
    rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
#endif


    printf("#Device: %s\n", pDeviceName);
    printf("#CodecName:%s\n", pCodecName);
    printf("#Resolution: %dx%d\n", u32Width, u32Height);
    printf("#Frame Count to save: %d\n", g_s32FrameCnt);
    printf("#Output Path: %s\n", pOutPath);
    printf("#CameraIdx: %d\n\n", s32CamId);
#ifdef RKAIQ
    printf("#bMultictx: %d\n\n", bMultictx);
    printf("#Aiq xml dirpath: %s\n\n", pIqfilesPath);
#endif

    if (pIqfilesPath) {
#ifdef RKAIQ


        if (SAMPLE_COMM_ISP_Init(s32CamId, hdr_mode, bMultictx, pIqfilesPath)==-1) {
            while (SAMPLE_COMM_ISP_Init(s32CamId, hdr_mode, bMultictx, pIqfilesPath)==0) {
                sleep(1);
                printf("SAMPLE_COMM_ISP_REINIT\n");
            }
        }


        if ( SAMPLE_COMM_ISP_Run(s32CamId)==-1) {
            while ( SAMPLE_COMM_ISP_Run(s32CamId)==0) {
                sleep(1);
                printf("SAMPLE_COMM_ISP_RERUN\n");
            }
        }

        SAMPLE_COMM_ISP_SetFrameRate(s32CamId, u32Fps);

#endif
    }

    if (pOutPath) {
        g_output_file = fopen(pOutPath, "w");
        if (!g_output_file) {
            printf("ERROR: open file: %s fail, exit\n", pOutPath);

        }
    }

    RK_MPI_SYS_Init();
    VI_CHN_ATTR_S vi_chn_attr;
    vi_chn_attr.pcVideoNode = pDeviceName;
    vi_chn_attr.u32BufCnt = u32BufCnt;
    vi_chn_attr.u32Width = u32Width;
    vi_chn_attr.u32Height = u32Height;
    vi_chn_attr.enPixFmt = IMAGE_TYPE_NV12;
    vi_chn_attr.enBufType = VI_CHN_BUF_TYPE_MMAP;
    vi_chn_attr.enWorkMode = VI_WORK_MODE_NORMAL;
    ret = RK_MPI_VI_SetChnAttr(s32CamId, 0, &vi_chn_attr);
    ret |= RK_MPI_VI_EnableChn(s32CamId, 0);
    if (ret) {
        printf("ERROR: create VI[0] error! ret=%d\n", ret);
    }
 VENC_CHN_ATTR_S venc_chn_attr;
  memset(&venc_chn_attr, 0, sizeof(venc_chn_attr));
  switch (enCodecType) {
  case RK_CODEC_TYPE_H265:
    venc_chn_attr.stVencAttr.enType = RK_CODEC_TYPE_H265;
    venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
    venc_chn_attr.stRcAttr.stH265Cbr.u32Gop = 30;
    venc_chn_attr.stRcAttr.stH265Cbr.u32BitRate = u32Width * u32Height;
    // frame rate: in 30/1, out 30/1.
    venc_chn_attr.stRcAttr.stH265Cbr.fr32DstFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stH265Cbr.fr32DstFrameRateNum = 30;
    venc_chn_attr.stRcAttr.stH265Cbr.u32SrcFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stH265Cbr.u32SrcFrameRateNum = 30;
    break;
  case RK_CODEC_TYPE_MJPEG:
    venc_chn_attr.stVencAttr.enType = RK_CODEC_TYPE_MJPEG;
    venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGCBR;
    venc_chn_attr.stRcAttr.stMjpegCbr.fr32DstFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stMjpegCbr.fr32DstFrameRateNum = 30;
    venc_chn_attr.stRcAttr.stMjpegCbr.u32SrcFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stMjpegCbr.u32SrcFrameRateNum = 30;
    venc_chn_attr.stRcAttr.stMjpegCbr.u32BitRate = u32Width * u32Height * 8;
    break;
  case RK_CODEC_TYPE_H264:
  default:
    venc_chn_attr.stVencAttr.enType = RK_CODEC_TYPE_H264;
    venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_H264VBR;

    venc_chn_attr.stRcAttr.stH264Vbr.u32Gop = 60;
    venc_chn_attr.stRcAttr.stH264Vbr.u32MaxBitRate = u32Width * u32Height *0.5;
    // frame rate: in 30/1, out 30/1.
    venc_chn_attr.stRcAttr.stH264Vbr.fr32DstFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stH264Vbr.fr32DstFrameRateNum = 15;
    venc_chn_attr.stRcAttr.stH264Vbr.u32SrcFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stH264Vbr.u32SrcFrameRateNum = 30;
    break;
  }
  venc_chn_attr.stVencAttr.imageType = IMAGE_TYPE_NV12;
  venc_chn_attr.stVencAttr.u32PicWidth = u32Width;
  venc_chn_attr.stVencAttr.u32PicHeight = u32Height;
  venc_chn_attr.stVencAttr.u32VirWidth = u32Width;
  venc_chn_attr.stVencAttr.u32VirHeight = u32Height;
  venc_chn_attr.stVencAttr.u32Profile = 77;
  ret = RK_MPI_VENC_CreateChn(0, &venc_chn_attr);
  if (ret) {
    printf("ERROR: create VENC[0] error! ret=%d\n", ret);

  }

// set jpeg venc,stRecvParam
    // 设置编码类型为 MJPEG

    VENC_CHN_ATTR_S venc_chn_attr_jpeg;
    venc_chn_attr_jpeg.stVencAttr.enType = RK_CODEC_TYPE_JPEG;



    venc_chn_attr_jpeg.stVencAttr.imageType = IMAGE_TYPE_NV12;
    venc_chn_attr_jpeg.stVencAttr.u32PicWidth = u32Width;
    venc_chn_attr_jpeg.stVencAttr.u32PicHeight = u32Height;
    venc_chn_attr_jpeg.stVencAttr.u32VirWidth = u32Width;
    venc_chn_attr_jpeg.stVencAttr.u32VirHeight = u32Height;



    ret = RK_MPI_VENC_CreateChn(2, &venc_chn_attr_jpeg);
    if (ret) {
        printf("ERROR: create VENC[2] error! ret=%d\n", ret);

    }

    ret = RK_MPI_VENC_RGN_Init(0, NULL);
    if (ret) {
        printf("ERROR: RK_MPI_VENC_RGN_Init failed! ret=%d\n", ret);

    }




  ret = RK_MPI_SYS_RegisterOutCb(&stEncChn, video_packet_cb_chh0);
  if (ret) {
    printf("ERROR: register output callback for VENC[0] error! ret=%d\n", ret);

  }

    ret = RK_MPI_SYS_RegisterOutCb(&stDestChn, jpeg_packet_cb_chh0);
    if (ret) {
        printf("ERROR: register output callback for VENC[2] error! ret=%d\n", ret);

    }


    std::thread jepg_cap_thread(jpeg_packet_thread ,2);
    jepg_cap_thread.detach();
    // water mark thread
    std::thread water_mark_channel2(time_watermark_thread,0);
    water_mark_channel2.detach();


  ret = RK_MPI_SYS_Bind(&stSrcChn, &stEncChn);
  if (ret) {
    printf("ERROR: Bind VI[0] and VENC[0] error! ret=%d\n", ret);

  }


    ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if (ret) {
        printf("ERROR: Bind VI[0] and VENC[2] error! ret=%d\n", ret);

    }

  printf("%s initial finish\n", __func__);

    create_directories(CAP_PIC_PATH);
    create_directories(ALRAM_PATH);


  //signal(SIGINT, sigterm_handler);
    /*
    MppGuard::instance().push([stSrcChn,stDestChn] {
        RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    });
    MppGuard::instance().push([stSrcChn,stEncChn] {
    RK_MPI_SYS_UnBind(&stSrcChn, &stEncChn);
});
    MppGuard::instance().push([]{
        RK_MPI_VENC_DestroyChn(0);
});
    MppGuard::instance().push([]{
    RK_MPI_VENC_DestroyChn(2);
});

    MppGuard::instance().push([]{
    RK_MPI_VI_DisableChn(0, 0);
});
    MppGuard::instance().push([]{
        SAMPLE_COMM_ISP_Stop(0);
});
*/

    while (!quit_0) {

        Message msg = mq.pop();
        std::cout << "Received JSON data: " << msg.data << "\n";

        cJSON *json = parse_json_from_frame(msg.data, msg.size);
        if (json) {
            int msgcode = -1;
            cJSON *temp = cJSON_GetObjectItem(json, "msgcode");
            msgcode = temp ? temp->valueint : -1;

            if (temp != NULL && msgcode != -1  ) {
                std::cout << "msgcode"<< temp->valueint <<std::endl;
                switch (msgcode) {
                    case 6: {

                        int msgid = 0;
                        int captureType = -1;
                        const char *timeStamp_str = NULL;

                        cJSON *item = NULL;

                        item = cJSON_GetObjectItem(json, "msgid");
                        if (cJSON_IsNumber(item)) {
                            msgid = item->valueint;
                        }

                        item = cJSON_GetObjectItem(json, "timeStamp");
                        if (cJSON_IsString(item) && item->valuestring) {
                            timeStamp_str = strdup(item->valuestring); // 注意：这里只是指针，不要 free
                        }

                        item = cJSON_GetObjectItem(json, "captureType");
                        if (cJSON_IsNumber(item)) {
                            captureType = item->valueint;
                        }


                        if (captureType == 0 ) {
                            /* cap ontime pic */

                            thread0_jpeg_type = thread0_type_cap_ontime_pic;
                            cap_ontime_stamp = timeStamp_str;
                        }
                        else if (captureType == 1) {
                            /* cap alarm pic and video  */
                            std::cout<< "cap function start"<<std::endl;
                            thread0_video_type = thread0_type_cap_video;
                            thread0_jpeg_type = thread0_type_cap_pic;
                            cap_alarm_stamp = timeStamp_str;
                            //std::lock_guard<std::mutex> lock(buffer_mutex);
                            trigger_info.trigger_index = write_index;
                            trigger_info.buffer_full_at_trigger = buffer_full;
                            trigger_info.write_index_at_trigger = write_index;
                            waiting_for_post_frames = true;
                            collected_post_frames = 0;


                            jpeg_trigger_info.trigger_index = jpeg_write_index;
                            jpeg_trigger_info.buffer_full_at_trigger = jpeg_buffer_full;
                            jpeg_trigger_info.write_index_at_trigger = jpeg_write_index;
                            waiting_for_jpeg_post_frames = true;
                            collected_jpeg_post_frames = 0;

                        }

                        break;
                    }

                    case 8: {
                        /*  send alarm events */
                        break;
                    }
                }

            }




            cJSON_Delete(json);  // 释放内存
        }

        usleep(500000);
    }



    if (g_output_file)
        fclose(g_output_file);



    printf("%s exit!\n", __func__);
    // unbind first
    ret = RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    if (ret) {
        printf("ERROR: UnBind VI[0] and VENC[0] error! ret=%d\n", ret);

    }
    // destroy venc before vi
    ret = RK_MPI_VENC_DestroyChn(0);
    if (ret) {
        printf("ERROR: Destroy VENC[0] error! ret=%d\n", ret);

    }
    // destroy vi
    ret = RK_MPI_VI_DisableChn(s32CamId, 0);
    if (ret) {
        printf("ERROR: Destroy VI[0] error! ret=%d\n", ret);

    }

}
int get_rv1126_nalu() {
 signal(SIGUSR1, sig_save_video);
 quit = false;
  //RK_U32 u32Width = 3840;
  //RK_U32 u32Height = 2160;
  RK_U32 u32Width = 1920;
  RK_U32 u32Height = 1080;
  const RK_CHAR *pDeviceName = "rkispp_scale0";
  RK_CHAR *pOutPath = NULL;
  const RK_CHAR *pIqfilesPath = "/etc/iqfiles";
  CODEC_TYPE_E enCodecType = RK_CODEC_TYPE_H264;
  const RK_CHAR *pCodecName = "H264";
  RK_S32 s32CamId = 0;
  RK_U32 u32BufCnt = 3;
#ifdef RKAIQ
  RK_BOOL bMultictx = RK_FALSE;
  RK_U32 u32Fps = 30;
  rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
#endif


  printf("#Device: %s\n", pDeviceName);
  printf("#CodecName:%s\n", pCodecName);
  printf("#Resolution: %dx%d\n", u32Width, u32Height);
  printf("#Frame Count to save: %d\n", g_s32FrameCnt);
  printf("#Output Path: %s\n", pOutPath);
  printf("#CameraIdx: %d\n\n", s32CamId);
#ifdef RKAIQ
  printf("#bMultictx: %d\n\n", bMultictx);
  printf("#Aiq xml dirpath: %s\n\n", pIqfilesPath);
#endif

  if (pIqfilesPath) {
#ifdef RKAIQ
    //SAMPLE_COMM_ISP_Init(s32CamId, hdr_mode, bMultictx, pIqfilesPath);
    //SAMPLE_COMM_ISP_Run(s32CamId);
    //SAMPLE_COMM_ISP_SetFrameRate(s32CamId, u32Fps);
#endif
  }

  if (pOutPath) {
    g_output_file = fopen(pOutPath, "w");
    if (!g_output_file) {
      printf("ERROR: open file: %s fail, exit\n", pOutPath);
      return 0;
    }
  }

  RK_MPI_SYS_Init();
  VI_CHN_ATTR_S vi_chn_attr;
  vi_chn_attr.pcVideoNode = pDeviceName;
  vi_chn_attr.u32BufCnt = u32BufCnt;
  vi_chn_attr.u32Width = u32Width;
  vi_chn_attr.u32Height = u32Height;
  vi_chn_attr.enPixFmt = IMAGE_TYPE_NV12;
  vi_chn_attr.enBufType = VI_CHN_BUF_TYPE_MMAP;
  vi_chn_attr.enWorkMode = VI_WORK_MODE_NORMAL;
  ret = RK_MPI_VI_SetChnAttr(s32CamId, 1, &vi_chn_attr);
  ret |= RK_MPI_VI_EnableChn(s32CamId, 1);
  if (ret) {
    printf("ERROR: create VI[0] error! ret=%d\n", ret);
    return 0;
  }

  VENC_CHN_ATTR_S venc_chn_attr;
  memset(&venc_chn_attr, 0, sizeof(venc_chn_attr));
  switch (enCodecType) {
  case RK_CODEC_TYPE_H265:
    venc_chn_attr.stVencAttr.enType = RK_CODEC_TYPE_H265;
    venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
    venc_chn_attr.stRcAttr.stH265Cbr.u32Gop = 30;
    venc_chn_attr.stRcAttr.stH265Cbr.u32BitRate = u32Width * u32Height;
    // frame rate: in 30/1, out 30/1.
    venc_chn_attr.stRcAttr.stH265Cbr.fr32DstFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stH265Cbr.fr32DstFrameRateNum = 30;
    venc_chn_attr.stRcAttr.stH265Cbr.u32SrcFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stH265Cbr.u32SrcFrameRateNum = 30;
    break;
  case RK_CODEC_TYPE_MJPEG:
    venc_chn_attr.stVencAttr.enType = RK_CODEC_TYPE_MJPEG;
    venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGCBR;
    venc_chn_attr.stRcAttr.stMjpegCbr.fr32DstFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stMjpegCbr.fr32DstFrameRateNum = 30;
    venc_chn_attr.stRcAttr.stMjpegCbr.u32SrcFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stMjpegCbr.u32SrcFrameRateNum = 30;
    venc_chn_attr.stRcAttr.stMjpegCbr.u32BitRate = u32Width * u32Height * 8;
    break;
  case RK_CODEC_TYPE_H264:
  default:
    venc_chn_attr.stVencAttr.enType = RK_CODEC_TYPE_H264;
    venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
    venc_chn_attr.stRcAttr.stH264Cbr.u32Gop = 30;
    venc_chn_attr.stRcAttr.stH264Cbr.u32BitRate = u32Width * u32Height;
    // frame rate: in 30/1, out 30/1.
    venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = 30;
    venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = 30;
    break;
  }
  venc_chn_attr.stVencAttr.imageType = IMAGE_TYPE_NV12;
  venc_chn_attr.stVencAttr.u32PicWidth = u32Width;
  venc_chn_attr.stVencAttr.u32PicHeight = u32Height;
  venc_chn_attr.stVencAttr.u32VirWidth = u32Width;
  venc_chn_attr.stVencAttr.u32VirHeight = u32Height;
  venc_chn_attr.stVencAttr.u32Profile = 77;
  ret = RK_MPI_VENC_CreateChn(1, &venc_chn_attr);
  if (ret) {
    printf("ERROR: create VENC[0] error! ret=%d\n", ret);
    return 0;
  }

  MPP_CHN_S stEncChn;
  stEncChn.enModId = RK_ID_VENC;
  stEncChn.s32DevId = 0;
  stEncChn.s32ChnId = 1;
  ret = RK_MPI_SYS_RegisterOutCb(&stEncChn, video_packet_cb);
  if (ret) {
    printf("ERROR: register output callback for VENC[0] error! ret=%d\n", ret);
    return 0;
  }    ret = RK_MPI_VENC_RGN_Init(1, NULL);
    if (ret) {
        printf("ERROR: RK_MPI_VENC_RGN_Init failed! ret=%d\n", ret);

    }


  MPP_CHN_S stSrcChn;
  stSrcChn.enModId = RK_ID_VI;
  stSrcChn.s32DevId = 0;
  stSrcChn.s32ChnId = 1;
  MPP_CHN_S stDestChn;
  stDestChn.enModId = RK_ID_VENC;
  stDestChn.s32DevId = 0;
  stDestChn.s32ChnId = 1;
  ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
  if (ret) {
    printf("ERROR: Bind VI[0] and VENC[0] error! ret=%d\n", ret);
    return 0;
  }

  printf("%s initial finish\n", __func__);
  //signal(SIGINT, sigterm_handler);
    std::thread water_mark_channel1(time_watermark_thread_ , 1);
    water_mark_channel1.detach();
  while (!quit) {
    usleep(500000);
  }
  
  

  if (g_output_file)
    fclose(g_output_file);


  black_flag = 0;
  printf("%s exit!\n", __func__);
  // unbind first
  ret = RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
  if (ret) {
    printf("ERROR: UnBind VI[0] and VENC[0] error! ret=%d\n", ret);
    return 0;
  }
  // destroy venc before vi
  ret = RK_MPI_VENC_DestroyChn(1);
  if (ret) {
    printf("ERROR: Destroy VENC[0] error! ret=%d\n", ret);
    return 0;
  }
  // destroy vi
  ret = RK_MPI_VI_DisableChn(s32CamId, 1);
  if (ret) {
    printf("ERROR: Destroy VI[0] error! ret=%d\n", ret);
    return 0;
  }

  if (pIqfilesPath) {
#ifdef RKAIQ
    //SAMPLE_COMM_ISP_Stop(s32CamId);
#endif
  }
  return 0;
}



