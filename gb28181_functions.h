#ifndef GB28181_FUNCTIONS_H
#define GB28181_FUNCTIONS_H

#include <eXosip2/eXosip.h>
#include <string>
#include "config.h"
// 常量定义
#define LOG_FILE "/root/lkt/revice.log"




// 函数声明
class gb28181_functions {
    public:
        gb28181_functions();
        ~gb28181_functions();
        void save_sip_message_to_file(osip_message_t *sip_message, const char *filename);
        int send_200_OK(eXosip_t *ctx, int tid);
        int send_test_warning(eXosip_t *ctx);
        int send_keep_alive(eXosip_t *ctx);
        void sendHeartbeat();
        void heartbeatLoop(int interval, eXosip_t *ctx, char *from, char *proxy, char *contact);
        int send_device_info_response(eXosip_t *ctx, int tid, int SN);
        int send_catalog_response(eXosip_t *ctx, int tid, int SN);
        int send_DeviceStatus_response(eXosip_t *ctx, int tid, int SN);
        int send_record_info_response(eXosip_t *ctx, int tid, int SN);
        int send_ConfigDownload_response(eXosip_t *ctx, int tid, int SN);
        int send_Subscribe_response(eXosip_t *ctx, int tid, int SN);

        // 辅助函数
        unsigned int get_next_sn();
        int get_interface_ip(const char* interface, char* ip_buf, size_t buf_len) ;
        char LOCAL_IP[30];
        char from[50];
};
#endif // GB28181_FUNCTIONS_H