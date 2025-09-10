//
// Created by liketao on 25-8-29.
//

#include "gb28181_functions.h"
#include <eXosip2/eXosip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <iostream>
#include <chrono>
#include <inttypes.h>
#include <sstream>

#include "record.h"
#include "xml_parser.h"
#include "RtpSendPs.h"
#include "sdp_parser.h"
#include "shared_queue.h"
#include "venc.h"
#include "message_queue.h"
#include "tcp_receiver.h"

// 常量定义



// 全局变量
static unsigned int g_sn_counter = 0;

// 获取递增的SN
unsigned int gb28181_functions::get_next_sn() {
    return ++g_sn_counter;
}

int  gb28181_functions::get_interface_ip(const char* interface, char* ip_buf, size_t buf_len) {
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

gb28181_functions::gb28181_functions() {
    if (get_interface_ip(NET_NAME, LOCAL_IP, sizeof(LOCAL_IP)) == 0) {
        printf("%s IP Address: %s\n",NET_NAME ,LOCAL_IP);
    } else {
        printf("Failed to get IP for %s\n",NET_NAME);
    }

    sprintf(from, "sip:%s@%s:5060", SIP_USER,LOCAL_IP);
}
void gb28181_functions::save_sip_message_to_file(osip_message_t *sip_message, const char *filename) {
    size_t message_length;
    char *sip_message_str = NULL;

    int ret = osip_message_to_str(sip_message, &sip_message_str, &message_length);
    if (ret != OSIP_SUCCESS || sip_message_str == NULL) {
        perror("Failed to convert SIP message to string");
        return;
    }

    FILE *file = fopen(filename, "a");
    if (file == NULL) {
        perror("Failed to open file for writing");
        osip_free(sip_message_str);
        return;
    }

    fprintf(file, "%s\n", sip_message_str);
    fprintf(file, "%s\n", "-------------------------");
    fclose(file);
    osip_free(sip_message_str);
}

int gb28181_functions::send_200_OK(eXosip_t *ctx, int tid) {
    osip_message_t *response = NULL;
    int ret = eXosip_message_build_answer(ctx, tid, 200, &response);
    if (ret != 0) {
        printf("构建设备信息响应消息失败\n");
        return -1;
    }

    osip_message_set_content_length(response, 0);

    ret = eXosip_message_send_answer(ctx, tid, 200, response);
    printf("已发送设备信息响应: \n%s\n", response);

    return ret;
}

int gb28181_functions::send_test_warning(eXosip_t *ctx) {
    osip_message_t *response = NULL;


    int ret = eXosip_message_build_request(ctx, &response, "MESSAGE", to, from, NULL);
    if (ret != 0) {
        printf("构建设备信息响应消息失败\n");
        return -1;
    }

    char keepalive_xml[512];
    snprintf(keepalive_xml, sizeof(keepalive_xml),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<Notify>\n"
        "<CmdType>Alarm</CmdType>\n"
        "<SN>%u</SN>\n"
        "<DeviceID>" SIP_USER "</DeviceID>\n"
        "<AlarmPriority>4</AlarmPriority>\n"
        "<AlarmTime>2025-07-27T10:19:14</AlarmTime>\n"
        "<Longitude>0.000</Longitude>\n"
        "<Latitude>0.000</Latitude>\n"
        "<AlarmMethod>2</AlarmMethod>\n"
        "</Notify>",
        get_next_sn());
    osip_uri_t *new_uri = NULL;
    osip_uri_init(&new_uri);
    int res = osip_uri_parse(new_uri, to);
    if (res != 0) {
        printf("Failed to parse new URI.\n");
        return -1;
    }

    osip_message_set_uri(response, new_uri);
    osip_message_set_content_type(response, "Application/MANSCDP+xml");
    osip_message_set_body(response, keepalive_xml, strlen(keepalive_xml));

    ret = eXosip_message_send_request(ctx, response);
    printf("已发送warning test  : \n%s\n", keepalive_xml);
    return ret;
}

int gb28181_functions::send_keep_alive(eXosip_t *ctx) {
    osip_message_t *response = NULL;
    eXosip_lock(ctx);
    int ret = eXosip_message_build_request(ctx, &response, "MESSAGE", to, from, NULL);
    if (ret != 0) {
        printf("构建设备信息响应消息失败\n");
        return -1;
    }

    char keepalive_xml[512];
    snprintf(keepalive_xml, sizeof(keepalive_xml),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Notify>"
        "<CmdType>Keepalive</CmdType>"
        "<SN>%u</SN>"
        "<DeviceID>" SIP_USER "</DeviceID>"
        "<Status>OK</Status>"
        "</Notify>",
        get_next_sn());
    osip_uri_t *new_uri = NULL;
    osip_uri_init(&new_uri);
    int res = osip_uri_parse(new_uri, to);
    if (res != 0) {
        printf("Failed to parse new URI.\n");
        return -1;
    }
    osip_message_set_uri(response, new_uri);
    osip_message_set_content_type(response, "Application/MANSCDP+xml");
    osip_message_set_body(response, keepalive_xml, strlen(keepalive_xml));

    ret = eXosip_message_send_request(ctx, response);
    eXosip_unlock(ctx);
    //printf("已发送keepalive : \n%s\n", keepalive_xml);
    return ret;
}

void gb28181_functions::sendHeartbeat() {
    std::cout << "Sending heartbeat..." << std::endl;
    // 这里调用你的实际心跳发送函数
}

void gb28181_functions::heartbeatLoop(int interval, eXosip_t *ctx, char *from, char *proxy, char *contact) {
    int time_count = 0;
    while (true) {
        send_keep_alive(ctx);
        if (time_count % 50 == 0) {
            int ret;
            osip_message_t *reg = NULL;
            int reg_id;
            reg_id = eXosip_register_build_initial_register(ctx, from, proxy, contact, 3600, &reg);
            if (reg_id < 0) {
                printf("eXosip_register_build_initial_register failed\n");
                eXosip_quit(ctx);
            }

            ret = eXosip_register_send_register(ctx, reg_id, reg);
            if (ret != 0) {
                printf("eXosip_register_send_register failed\n");
                eXosip_quit(ctx);
            }
            time_count = 0;
        }
        std::this_thread::sleep_for(std::chrono::seconds(interval));
        time_count++;
    }
}

int gb28181_functions::send_device_info_response(eXosip_t *ctx, int tid, int SN) {
    osip_message_t *response = NULL;

    int ret = eXosip_message_build_request(ctx, &response, "MESSAGE", to, from, NULL);

    if (ret != 0) {
        printf("构建设备信息响应消息失败\n");
        return -1;
    }

    char device_info_xml[1024];
    snprintf(device_info_xml, sizeof(device_info_xml),
        "<?xml version=\"1.0\" encoding=\"GB2312\"?>\n"
        "<Response>\n"
        "<CmdType>DeviceInfo</CmdType>\n"
        "<SN>%d</SN>\n"
        "<DeviceID>%s</DeviceID>\n"
        "<Result>OK</Result>\n"
        "<Manufacturer>Manufacturer</Manufacturer>\n"
        "<Model>Model</Model>\n"
        "<Firmware>1.0</Firmware>\n"
        "<Channel>1</Channel>\n"
        "</Response>",
        SN, SIP_USER);

    osip_uri_t *new_uri = NULL;
    osip_uri_init(&new_uri);
    int res = osip_uri_parse(new_uri, to);
    if (res != 0) {
        printf("Failed to parse new URI.\n");
        return -1;
    }
    osip_message_set_uri(response, new_uri);
    osip_message_set_content_type(response, "Application/MANSCDP+xml");
    osip_message_set_body(response, device_info_xml, strlen(device_info_xml));

    ret = eXosip_message_send_request(ctx, response);
   // printf("已发送设备信息响应: \n%s\n", device_info_xml);
    return ret;
}

int gb28181_functions::send_catalog_response(eXosip_t *ctx, int tid, int SN) {
    osip_message_t *response = NULL;
    int ret = eXosip_message_build_request(ctx, &response, "MESSAGE", to, from, NULL);
    if (ret != 0) {
        printf("构建目录响应消息失败\n");
        return -1;
    }
/* 2channel
 *
    *        "<?xml version=\"1.0\" encoding=\"GB2312\"?>\n"
            "<Response>\n"
            "<CmdType>Catalog</CmdType>\n"
            "<SN>%u</SN>\n"
            "<DeviceID>%s</DeviceID>\n"
            "<SumNum>2</SumNum>\n"
            "<DeviceList Num=\"2\">\n"
           "<Item>\n"
            "<DeviceID>%s</DeviceID>\n"
            "<Name>IPC</Name>\n"
            "<Manufacturer>Manufacturer</Manufacturer>\n"
            "<Model>Model</Model>\n"
            "<Owner>Owner</Owner>\n"
            "<CivilCode>3402000000</CivilCode>\n"
            "<Address>Address</Address>\n"
            "<Parental>0</Parental>\n"
            "<ParentID>%s</ParentID>\n"
            "<RegisterWay>1</RegisterWay>\n"
            "<Secrecy>0</Secrecy>\n"
            "<Status>ON</Status>\n"
            "</Item>\n"
            "<Item>\n"
            "<DeviceID>%s</DeviceID>\n"
            "<Name>IPC</Name>\n"
            "<Manufacturer>Manufacturer</Manufacturer>\n"
            "<Model>Model</Model>\n"
            "<Owner>Owner</Owner>\n"
            "<CivilCode>3402000000</CivilCode>\n"
            "<Address>Address</Address>\n"
            "<Parental>0</Parental>\n"
            "<ParentID>%s</ParentID>\n"
            "<RegisterWay>1</RegisterWay>\n"
            "<Secrecy>0</Secrecy>\n"
            "<Status>ON</Status>\n"
            "</Item>\n"
            "</DeviceList>\n"
            "</Response>",
 *
 */
    char catalog_xml[2048];
    snprintf(catalog_xml, sizeof(catalog_xml),
        "<?xml version=\"1.0\" encoding=\"GB2312\"?>\n"
        "<Response>\n"
        "<CmdType>Catalog</CmdType>\n"
        "<SN>%u</SN>\n"
        "<DeviceID>%s</DeviceID>\n"
        "<SumNum>1</SumNum>\n"
        "<DeviceList Num=\"1\">\n"
        "<DeviceID>%s</DeviceID>\n"
        "<Name>IPC</Name>\n"
        "<Manufacturer>Manufacturer</Manufacturer>\n"
        "<Model>Model</Model>\n"
        "<Owner>Owner</Owner>\n"
        "<CivilCode>3402000000</CivilCode>\n"
        "<Address>Address</Address>\n"
        "<Parental>0</Parental>\n"
        "<ParentID>%s</ParentID>\n"
        "<RegisterWay>1</RegisterWay>\n"
        "<Secrecy>0</Secrecy>\n"
        "<Status>ON</Status>\n"
        "</Item>\n"
        "</DeviceList>\n"
        "</Response>",
        SN, SIP_USER,CHANNEL_ID_,SIP_USER);
    osip_uri_t *new_uri = NULL;
    osip_uri_init(&new_uri);
    int res = osip_uri_parse(new_uri, to);
    if (res != 0) {
        printf("Failed to parse new URI.\n");
        return -1;
    }
    osip_message_set_uri(response, new_uri);
    osip_message_set_content_type(response, "Application/MANSCDP+xml");
    osip_message_set_body(response, catalog_xml, strlen(catalog_xml));

    ret = eXosip_message_send_request(ctx, response);
    //printf("已发送目录响应: \n%s\n", catalog_xml);
    return ret;
}

int gb28181_functions::send_DeviceStatus_response(eXosip_t *ctx, int tid, int SN) {
    osip_message_t *response = NULL;
    int ret = eXosip_message_build_request(ctx, &response, "MESSAGE", to, from, NULL);

    char device_status_xml[1024];
    snprintf(device_status_xml, sizeof(device_status_xml),
        "<?xml version=\"1.0\" encoding=\"GB2312\"?>\n"
        "<Response>\n"
        "<CmdType>DeviceStatus</CmdType>\n"
        "<SN>%d</SN>\n"
        "<DeviceID>%s</DeviceID>\n"
        "<Result>OK</Result>\n"
        "<Online>ONLINE</Online>\n"
        "<Status>OK</Status>\n"
        "<Encode>ON</Encode>\n"
        "<Record>OFF</Record>\n"
        "<DeviceTime>%d</DeviceTime>\n"
        "</Response>",
        SN, SIP_USER, (int)time(NULL));
    osip_uri_t *new_uri = NULL;
    osip_uri_init(&new_uri);
    int res = osip_uri_parse(new_uri, to);
    if (res != 0) {
        printf("Failed to parse new URI.\n");
        return -1;
    }
    osip_message_set_uri(response, new_uri);
    osip_message_set_content_type(response, "Application/MANSCDP+xml");
    osip_message_set_body(response, device_status_xml, strlen(device_status_xml));

    ret = eXosip_message_send_request(ctx, response);
    //printf("已发送目录响应: \n%s\n", device_status_xml);
    return ret;
}

int gb28181_functions::send_record_info_response(eXosip_t *ctx, int tid, int SN) {
    osip_message_t *response = NULL;
    int ret = eXosip_message_build_request(ctx, &response, "MESSAGE", to, from, NULL);
    if (ret != 0) {
        printf("eXosip_message_build_request failed: %d\n", ret);
        return ret;
    }

    RecordCsv record_parser(RECORD_FILE);
    std::vector<Record> top10;
    record_parser.loadTopN(top10, 3);

    std::ostringstream oss;
    oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
        << "<Response>\r\n"
        << "  <CmdType>RecordInfo</CmdType>\r\n"
        << "  <SN>" << SN << "</SN>\r\n"
        << "  <DeviceID>" << CHANNEL_ID << "</DeviceID>\r\n"
        << "  <Name>" << SIP_USER << "</Name>\r\n"
        << "  <SumNum>" << top10.size() << "</SumNum>\r\n"
        << "  <RecordList Num=\"" << top10.size() << "\">\r\n";

    for (const auto& top : top10) {
        oss << "    <Item>\r\n"
            << "      <DeviceID>" << top.deviceId << "</DeviceID>\r\n"
            << "      <Name>" << top.name << "</Name>\r\n"
            << "      <Address></Address>\r\n"
            << "      <StartTime>" << top.startTime << "</StartTime>\r\n"
            << "      <EndTime>" << top.endTime << "</EndTime>\r\n"
            << "      <Secrecy>0</Secrecy>\r\n"
            << "      <Type>all</Type>\r\n"
            << "      <FileSize>" << top.fileSize << "</FileSize>\r\n"
            << "    </Item>\r\n";
    }

    oss << "  </RecordList>\r\n"
        << "</Response>\r\n";

    std::string xml_content = oss.str();

    osip_uri_t *new_uri = NULL;
    osip_uri_init(&new_uri);
    int res = osip_uri_parse(new_uri, to);
    if (res != 0) {
        printf("Failed to parse new URI.\n");
        return -1;
    }
    osip_message_set_uri(response, new_uri);
    osip_message_set_content_type(response, "Application/MANSCDP+xml");
    osip_message_set_body(response, xml_content.c_str(), xml_content.size());

    ret = eXosip_message_send_request(ctx, response);
#ifdef DEBUG
    //printf("已发送recordinfo响应: \n%s\n", xml_content.c_str());
#endif

    return ret;
}

int gb28181_functions::send_ConfigDownload_response(eXosip_t *ctx, int tid, int SN) {
    osip_message_t *response = NULL;
    int ret = eXosip_message_build_request(ctx, &response, "MESSAGE", to, from, NULL);

    char config_download_xml[1024];
    snprintf(config_download_xml, sizeof(config_download_xml),
        "<?xml version=\"1.0\" encoding=\"GB2312\"?>\n"
        "<Response>\n"
        "<CmdType>ConfigDownload</CmdType>\n"
        "<SN>%d</SN>\n"
        "<DeviceID>%s</DeviceID>\n"
        "<Result>OK</Result>\n"
        "<BasicParam>\n"
        "<Name>IPC</Name>\n"
        "<Expiration>3600</Expiration>\n"
        "<HeartBeatInterval>60</HeartBeatInterval>\n"
        "<HeartBeatCount>1</HeartBeatCount>\n"
        "</BasicParam>\n"
        "</Response>", SN, SIP_USER);
    osip_uri_t *new_uri = NULL;
    osip_uri_init(&new_uri);
    int res = osip_uri_parse(new_uri, to);
    if (res != 0) {
        printf("Failed to parse new URI.\n");
        return -1;
    }
    osip_message_set_uri(response, new_uri);
    osip_message_set_content_type(response, "Application/MANSCDP+xml");
    osip_message_set_body(response, config_download_xml, strlen(config_download_xml));

    ret = eXosip_message_send_request(ctx, response);
   // printf("已发送configDownload响应: \n%s\n", config_download_xml);
    return ret;
}

int gb28181_functions::send_Subscribe_response(eXosip_t *ctx, int tid, int SN) {
    osip_message_t *response = NULL;
    int ret = eXosip_insubscription_build_answer(ctx, tid, 200, &response);

    if (ret != 0) {
        printf("构建设备信息响应消息失败\n");
        return -1;
    }
    osip_message_set_content_type(response, "Application/MANSCDP+xml");
    osip_message_set_expires(response, 0);
    osip_message_set_header(response, "Event", "presence");

    osip_message_set_contact(response, to);

    char xml_body[1204];
    snprintf(xml_body, sizeof(xml_body),
        "<?xml version=\"1.0\" encoding=\"GB2312\"?>\n"
        "<Response>\n"
        "<CmdType>Catalog</CmdType>\n"
        "<SN>%d</SN>\n"
        "<DeviceID>%s</DeviceID>\n"
        "<Result>OK</Result>\n"
        "</Response>", SN, SIP_USER);

    osip_message_set_body(response, xml_body, strlen(xml_body));
    ret = eXosip_insubscription_send_answer(ctx, tid, 200, response);
   // printf("已发送subsribe响应: \n%s\n", xml_body);
    return ret;
}