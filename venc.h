//
// Created by liketao on 25-6-29.
//

#ifndef VENC_H
#define VENC_H
#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <eXosip2/eXosip.h>
#include "message_queue.h"
extern "C"{
#include "common/sample_common.h"
}
#include "rkmedia/rkmedia_api.h"
#include "rkmedia/rkmedia_venc.h"
// for argb8888
#define TEST_ARGB32_PIX_SIZE 4
#define TEST_ARGB32_GREEN 0xFF00FF00
#define TEST_ARGB32_RED 0xFFFF0000
#define TEST_ARGB32_BLUE 0xFF0000FF
#define TEST_ARGB32_WHITE 0xFFFFFFFF
#define TEST_ARGB32_BLACK 0xFF000000
#define TEST_ARGB32_TRANS 0x00000000
extern bool thread0_start_play_ontime;

void stop_media_cam();
void video_packet_cb(MEDIA_BUFFER mb);

int get_rv1126_nalu();
void thread_chh0(message_queue& mq,eXosip_t *context_exosip);
void jpeg_packet_cb_chh0(MEDIA_BUFFER mb);
#endif //VENC_H
