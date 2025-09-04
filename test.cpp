//
// Created by liketao on 25-6-5.
//
#include <eXosip2/eXosip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream>
#include <chrono>
#include "xml_parser.h"
#include "RtpSendPs.h"
#include "sdp_parser.h"

#define SIP_USER "34020000001320000001"
#define SIP_PASSWD "123456"
#define LOCAL_IP "192.168.31.36"
#define LOCAL_PORT 5060
#define SERVER_IP "192.168.31.223"
#define SERVER_PORT 15060
#define EXPIRES 3600
#define KEEPALIVE_INTERVAL 60
#define PS_FILE_PATH "./test.ps"
#define CHANNEL_ID "34020000001320000001"  // 通道编号，这里用设备ID
#define to "sip:340200000002000000001@192.168.31.223:15060"
#define from "sip:34020000001320000001@192.168.31.36:5060"

using namespace  std ;


// 添加全局SN计数器
static unsigned int g_sn_counter = 0;

// 获取递增的SN
static unsigned int get_next_sn() {
    return ++g_sn_counter;
}



int send_200_OK(eXosip_t *ctx,int tid ) {
    osip_message_t *response = NULL;
    int ret = eXosip_message_build_answer(ctx, tid, 200, &response);
    if (ret != 0) {
        printf("构建设备信息响应消息失败\n");
        return -1;
    }

    osip_message_set_content_length(response,0);

    ret = eXosip_message_send_answer(ctx, tid, 200, response);
    printf("已发送设备信息响应: \n%s\n",response);

    return ret;
}

int send_keep_alive(eXosip_t *ctx ) {

    osip_message_t *response = NULL;

    int ret = eXosip_message_build_request(ctx, &response, "MESSAGE",to, from, NULL);
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
    int res = osip_uri_parse(new_uri, "sip:340200000002000000001@192.168.31.223:15060");
    if (res != 0) {
        printf("Failed to parse new URI.\n");
        return -1;
    }
    osip_message_set_uri(response,new_uri);
    osip_message_set_content_type(response, "Application/MANSCDP+xml");
    osip_message_set_body(response, keepalive_xml, strlen(keepalive_xml));

    ret = eXosip_message_send_request(ctx,response);
    printf("已发送keepalive : \n%s\n", keepalive_xml);
    return ret;
}

void sendHeartbeat() {
    std::cout << "Sending heartbeat..." << std::endl;
    // 这里调用你的实际心跳发送函数
}

void heartbeatLoop(int interval,eXosip_t *ctx) {
    while (true) {
        send_keep_alive(ctx);
        std::this_thread::sleep_for(std::chrono::seconds(interval));
    }
}


int send_device_info_response(eXosip_t *ctx, int tid,int SN) {
    osip_message_t *response = NULL;

    int ret = eXosip_message_build_request(ctx, &response, "MESSAGE",to, from, NULL);

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
    int res = osip_uri_parse(new_uri, "sip:340200000002000000001@192.168.31.223:15060");
    if (res != 0) {
        printf("Failed to parse new URI.\n");
        return -1;
    }
    osip_message_set_uri(response,new_uri);
    osip_message_set_content_type(response, "Application/MANSCDP+xml");
    osip_message_set_body(response, device_info_xml, strlen(device_info_xml));

    ret = eXosip_message_send_request(ctx,response);
    printf("已发送设备信息响应: \n%s\n", device_info_xml);
    return ret;
}
int send_catalog_response(eXosip_t *ctx, int tid,int SN) {
    osip_message_t *response = NULL;
    int ret = eXosip_message_build_request(ctx, &response, "MESSAGE",to, from, NULL);
    if (ret != 0) {
        printf("构建目录响应消息失败\n");
        return -1;
    }

    char catalog_xml[2048];
    snprintf(catalog_xml, sizeof(catalog_xml),
        "<?xml version=\"1.0\" encoding=\"GB2312\"?>\n"
        "<Response>\n"
        "<CmdType>Catalog</CmdType>\n"
        "<SN>%u</SN>\n"
        "<DeviceID>%s</DeviceID>\n"
        "<SumNum>1</SumNum>\n"
        "<DeviceList Num=\"1\">\n"
        "<Item>\n"
        "<DeviceID>%s</DeviceID>\n"
        "<Name>IPC</Name>\n"
        "<Manufacturer>Manufacturer</Manufacturer>\n"
        "<Model>Model</Model>\n"
        "<Owner>Owner</Owner>\n"
        "<CivilCode>3402000000</CivilCode>\n"
        "<Address>Address</Address>\n"
        "<Parental>0</Parental>\n"
        "<ParentID>34020000001320000001</ParentID>\n"
        "<RegisterWay>1</RegisterWay>\n"
        "<Secrecy>0</Secrecy>\n"
        "<Status>ON</Status>\n"
        "</Item>\n"
        "</DeviceList>\n"
        "</Response>",
        SN, SIP_USER, CHANNEL_ID);
    osip_uri_t *new_uri = NULL;
    osip_uri_init(&new_uri);
    int res = osip_uri_parse(new_uri, "sip:340200000002000000001@192.168.31.223:15060");
    if (res != 0) {
        printf("Failed to parse new URI.\n");
        return -1;
    }
    osip_message_set_uri(response,new_uri);
    osip_message_set_content_type(response, "Application/MANSCDP+xml");
    osip_message_set_body(response, catalog_xml, strlen(catalog_xml));

    ret = eXosip_message_send_request(ctx,response);
    printf("已发送目录响应: \n%s\n", catalog_xml);
    return ret;
}

int send_DeviceStatus_response(eXosip_t *ctx, int tid,int SN) {
    osip_message_t *response = NULL;
    int ret = eXosip_message_build_request(ctx, &response, "MESSAGE",to, from, NULL);


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
    int res = osip_uri_parse(new_uri, "sip:340200000002000000001@192.168.31.223:15060");
    if (res != 0) {
        printf("Failed to parse new URI.\n");
        return -1;
    }
    osip_message_set_uri(response,new_uri);
    osip_message_set_content_type(response, "Application/MANSCDP+xml");
    osip_message_set_body(response, device_status_xml, strlen(device_status_xml));

    ret = eXosip_message_send_request(ctx,response);
    printf("已发送目录响应: \n%s\n", device_status_xml);
    return ret;

}


/*

    // below is test code
    osip_message_t *message = NULL;
    const char *to_url = "sip:34020000002000000001@3402000000";
    const char *from_url = "sip:34020000001320000001@3402000000";
    const char *route_url = NULL; // 如果有代理服务器，请填写
    if (eXosip_listen_addr(context_exosip, IPPROTO_UDP, LOCAL_IP, LOCAL_PORT, AF_INET, 0) != 0) {
        printf("eXosip_listen_addr failed\n");
        eXosip_quit(context_exosip);
        return -1;
    }

    ret = eXosip_message_build_request(context_exosip, &message, "MESSAGE",to, from, proxy);

    if (ret != 0 || message == NULL) {
        printf("Failed to build MESSAGE request! Error code: %d\n", ret);
        return -1;
    }
    ret = eXosip_message_send_request(context_exosip,message);

 */


int main() {

    eXosip_t *context_exosip;
    osip_message_t *reg = NULL;
    int heartbeatInterval = 20;

    xml_parser * parser = new xml_parser;
    int reg_id, ret;
    int auth_ok = 0;

    context_exosip = eXosip_malloc();
    if (eXosip_init(context_exosip)!=0) {
        printf("eXosip_init failed\n");
        return -1;
    }


    if (eXosip_listen_addr(context_exosip, IPPROTO_UDP, LOCAL_IP, LOCAL_PORT, AF_INET, 0) != 0) {
        printf("eXosip_listen_addr failed\n");
        eXosip_quit(context_exosip);
        return -1;
    }
    char proxy[128], contact[128];


    snprintf(proxy, sizeof(proxy), "sip:%s:%d", SERVER_IP, SERVER_PORT);
    snprintf(contact, sizeof(contact), "sip:%s@%s:%d", SIP_USER, LOCAL_IP, LOCAL_PORT);


    reg_id = eXosip_register_build_initial_register(context_exosip, from, proxy, contact, EXPIRES, &reg);
    if (reg_id < 0) {
        printf("eXosip_register_build_initial_register failed\n");
        eXosip_quit(context_exosip);
        return -1;
    }


    ret = eXosip_register_send_register(context_exosip, reg_id, reg);
    if (ret != 0) {
        printf("eXosip_register_send_register failed\n");
        eXosip_quit(context_exosip);
        return -1;
    }

    send_keep_alive(context_exosip);
    printf("等待服务器响应...\n");


    std::thread heartbeatThread(heartbeatLoop, heartbeatInterval,context_exosip);
    while (1) {
        eXosip_event_t *event = eXosip_event_wait(context_exosip, 0, 50);
        if (event == NULL) {
            usleep(1000);
            continue;
        }
        eXosip_lock(context_exosip);


        cout << event->type<<endl;


        if (event->type == 2) {
            osip_body_t *body = NULL;
            osip_message_t *response = NULL;
            if (osip_message_get_body(event->request, 0, &body) == 0 && body != NULL && body->body != NULL) {
                cout<<"get body success!"<<endl;

                cout << "invite request coming" <<endl;
                cout << event->type<<endl;


                ret = eXosip_call_build_answer(context_exosip, event->tid, 200, &response);
                if (ret != 0) {
                    cout << "eXosip_call_build_answer failed" << endl;
                }
                else {
                    cout << "eXosip_call_build_answer success"<<endl;
                }


                const char *sdp_body =
                    "v=0\r\n"
                    "o=34020000001320000001 0 0 IN IP4 192.168.31.36\r\n"
                    "s=Play\r\n"
                    "c=IN IP4 192.168.31.36\r\n"
                    "t=0 0\r\n"
                    "m=video 20002 TCP/RTP/AVP 96\r\n"
                    "a=setup:active\r\n"
                    "a=connection:new\r\n"
                    "a=rtpmap:96 PS/90000\r\n"
                    "a=sendonly\r\n"
                    "y=0200000019\r\n"
                "f=v/2////a/6//1\r\n";


                osip_message_set_content_type(response, "application/sdp");
                osip_message_set_body(response, sdp_body, strlen(sdp_body));

                ret = eXosip_call_send_answer(context_exosip, event->tid, 200, response);
                if (ret != 0) {
                    fprintf(stderr, "Failed to send 200 OK response.\n");
                } else {
                    printf("Sent 200 OK with SDP:\n%s\n", sdp_body);
                }



                int port = get_first_media_port_from_sdp(body->body);
                cout << "port " << port << endl;
                RtpSendPs *  mRtpSendPs = new RtpSendPs("192.168.31.223",port,20002);
                //mRtpSendPs->start();

                // 释放响应消息

            }

        }
        if (event->type == 23) {
            osip_body_t *body = NULL;
            if (osip_message_get_body(event->request, 0, &body) == 0 && body && body->body) {
                printf("收到MESSAGE请求: %s\n", body->body);

                if (strstr(body->body, "<CmdType>Catalog</CmdType>")) {
                    printf("收到目录查询请求\n");
                    char SN[64] ;

                    if (parser->extractXmlElement(body->body, "SN", SN, sizeof(SN))) {
                        printf("提取到的 SN: %s\n", SN);
                    } else {
                        printf("提取 SN 失败。\n");
                    }
                    send_200_OK(context_exosip, event->tid);
                    send_catalog_response(context_exosip, event->tid,atoi(SN));
                }
                else if (strstr(body->body, "<CmdType>DeviceInfo</CmdType>")) {
                    printf("收到设备信息查询请求\n");
                    char SN[64] ;

                    if (parser->extractXmlElement(body->body, "SN", SN, sizeof(SN))) {
                        printf("提取到的 SN: %s\n", SN);
                    } else {
                        printf("提取 SN 失败。\n");
                    }
                    send_200_OK(context_exosip, event->tid);

                    send_device_info_response(context_exosip, event->did,atoi(SN));
                }
                else if (strstr(body->body, "<CmdType>DeviceStatus</CmdType>")) {
                    printf("收到设备状态查询请求\n");
                    //return send_device_status_response(ctx, evt->tid);
                    char SN[64] ;

                    if (parser->extractXmlElement(body->body, "SN", SN, sizeof(SN))) {
                        printf("提取到的 SN: %s\n", SN);
                    } else {
                        printf("提取 SN 失败。\n");
                    }
                    send_200_OK(context_exosip, event->tid);

                    send_DeviceStatus_response(context_exosip, event->did,atoi(SN));
                }
            }

        }


        eXosip_unlock(context_exosip);


    }

    return 0 ;

}