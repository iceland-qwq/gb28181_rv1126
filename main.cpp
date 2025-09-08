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
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <iostream>
#include <chrono>
#include <inttypes.h>
#include <sstream>
#include <random>
#include <csignal>

#include "tool.h"
#include "record.h"
#include "xml_parser.h"
#include "RtpSendPs.h"
#include "sdp_parser.h"
#include "shared_queue.h"
#include "venc.h"
#include "message_queue.h"
#include "tcp_receiver.h"
#include "gb28181_functions.h"
#include "config.h"
#include "tcp_client_receiver.h"
#include "mpp_guard.hpp"
extern "C"{
#include "common/sample_common.h"
}
#include "rkmedia/rkmedia_api.h"
#include "rkmedia/rkmedia_venc.h"
using namespace  std ;
extern bool thread0_start_play_ontime;
char LOCAL_IP[30];
char from[50];

extern message_queue mq ;
extern tcp_client_receiver receiver;

std::atomic<bool> running(true);

SharedQueue sharedQueue; // 必须有实际定义

SharedQueue sharedQueue_0; // 必须有实际定义

void signal_handler(int) {
    running = false;
}


int do_register(eXosip_t * context_exosip, char *from, char *proxy, char *contact, char * local_ip,int expire = 3600) {
    int reg_id;
    int ret ;
    osip_message_t *reg = NULL;


    reg_id = eXosip_register_build_initial_register(context_exosip, from, proxy, contact, 3600, &reg);
    if (reg_id < 0) {
        printf("eXosip_register_build_initial_register failed\n");

        return -1;
    }


    ret = eXosip_register_send_register(context_exosip, reg_id, reg);
    if (ret != 0) {
        printf("eXosip_register_send_register failed\n");

        return -1;
    }

    return 0 ;
}

int main() {
    //MppGuard::instance().install_signal_handlers();


    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    receiver.start();
    if (get_interface_ip(NET_NAME, LOCAL_IP, sizeof(LOCAL_IP)) == 0) {
        printf("%s IP Address: %s\n", NET_NAME,LOCAL_IP);
    } else {
        printf("Failed to get IP for %s\n",NET_NAME);
    }
    gb28181_functions* gb28181 = new gb28181_functions();
    sprintf(from, "sip:%s@%s:5060", SIP_USER,LOCAL_IP);


    eXosip_t *context_exosip;

    int heartbeatInterval = 60;

    RtpSendPs *  mRtpSendPs= NULL;
    RtpSendPs *  mRtpSendPs_0= NULL;
    RtpSendPs *  mRtpSendPs_record= NULL;
    RtpSendPs *  mRtpSendPs_download = NULL;
    xml_parser * parser = new xml_parser;
    int  ret;
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
    ret = do_register(context_exosip, from, proxy, contact,LOCAL_IP,3600) ;
    printf("register failed , try again later ...\n");
    int try_count = 0;
    while ( ret != 0) {
        ret = do_register(context_exosip, from, proxy, contact,LOCAL_IP,3600) ;
        sleep(try_count*2);
        printf("register try ...... try count = %d",try_count++);
        if (try_count >=5) {
            eXosip_quit(context_exosip);
            printf("register failed and kill the main");
            return -1 ;

        }
    }




    std::thread heartbeatThread(&gb28181_functions::heartbeatLoop, gb28181,heartbeatInterval,context_exosip,from, proxy, contact);
    std::thread chh0Thread(thread_chh0,std::ref(mq),context_exosip);

    //tcp_receiver receiver_(std::ref(mq), GB28181_TCP_PORT);

    //receiver_.start();

    while (running) {

        eXosip_event_t *event = eXosip_event_wait(context_exosip, 0, 50);

        if (event == NULL) {
            usleep(1000);
            continue;
        }
        eXosip_lock(context_exosip);
        if(event->type){
        //printf("event-type=%d",event->type);
        }	

        if (event->type == EXOSIP_CALL_GLOBALFAILURE) {
            printf("Call Global Failure! Status: %d %s\n",
        event->response->status_code,
        event->textinfo);
        }
        if (event->type == EXOSIP_CALL_INVITE) {

            osip_body_t *body = NULL;
            osip_message_t *response = NULL;


            if (osip_message_get_body(event->request, 0, &body) == 0 && body != NULL && body->body != NULL) {
                int sdp_type = extract_Slabel_content(body->body);
                switch (sdp_type) {

                    case Play: {
                        cout<<"get body success!"<<endl;

                        cout << "invite request coming" <<endl;
                        cout << event->type<<endl;


                    if ( strstr(extract_o_from_sdp(body->body),CHANNEL_ID)){
                        ret = eXosip_call_build_answer(context_exosip, event->tid, 200, &response);
                        if (ret != 0) {
                            cout << "eXosip_call_build_answer failed" << endl;
                        }
                        else {
                            cout << "eXosip_call_build_answer success"<<endl;
                        }


                        char* SSRC = extract_ssrc_from_sdp(body->body);
                        char* endptr;
                        uint32_t SSRC_t =strtoul(SSRC, &endptr, 10);



                        char sdp_body[1024];
                        snprintf(sdp_body, sizeof(sdp_body),
                        "v=0\r\n"
                        "o=34020000001320000001 0 0 IN IP4 %s\r\n"
                        "s=Play\r\n"
                        "c=IN IP4 %s\r\n"
                        "t=0 0\r\n"
                        "m=video 20002 RTP/AVP 96\r\n"
                        "a=setup:active\r\n"
                        "a=connection:new\r\n"
                        "a=rtpmap:96 PS/90000\r\n"
                        "a=sendonly\r\n"
                        "y=%s \r\n"
                        "f=v/2////a/6//1\r\n" , LOCAL_IP,LOCAL_IP,SSRC);



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
                        free(SSRC);
                        mRtpSendPs = new RtpSendPs(SERVER_IP,port,20002,SSRC_t);
                        mRtpSendPs->start(start_play);
                    }
                    else if ( strstr(extract_o_from_sdp(body->body),CHANNEL_ID_)) {

                        ret = eXosip_call_build_answer(context_exosip, event->tid, 200, &response);
                        if (ret != 0) {
                            cout << "eXosip_call_build_answer failed" << endl;
                        }
                        else {
                            cout << "eXosip_call_build_answer success"<<endl;
                        }


                        char* SSRC = extract_ssrc_from_sdp(body->body);
                        char* endptr;
                        uint32_t SSRC_t =strtoul(SSRC, &endptr, 10);



                        char sdp_body[1024];
                        snprintf(sdp_body, sizeof(sdp_body),
                        "v=0\r\n"
                        "o=34020000001320000001 0 0 IN IP4 %s\r\n"
                        "s=Play\r\n"
                        "c=IN IP4 %s\r\n"
                        "t=0 0\r\n"
                        "m=video 20004 RTP/AVP 96\r\n"
                        "a=setup:active\r\n"
                        "a=connection:new\r\n"
                        "a=rtpmap:96 PS/90000\r\n"
                        "a=sendonly\r\n"
                        "y=%s \r\n"
                        "f=v/2////a/6//1\r\n" , LOCAL_IP,LOCAL_IP,SSRC);



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
                        free(SSRC);
                        mRtpSendPs_0 = new RtpSendPs(SERVER_IP,port,20004,SSRC_t);
                        thread0_start_play_ontime = true;
                        mRtpSendPs_0->start(start_play_0);
                    }


                        break;
                    }
                    case Playback: {
                        char start_time[20];
                        char end_time[20];
                        std::random_device rd;
                        std::mt19937 gen(rd());
                        std::uniform_int_distribution<> dis(25000, 30000);

                        // 生成随机端口
                        int random_port = dis(gen);
                        extract_and_format_times(body->body, start_time, end_time);

                        printf("开始时间: %s\n", start_time);  // 输出: 2025-08-24T01:30:00
                        printf("结束时间: %s\n", end_time);    // 输出: 2025-08-24T02:00:00
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

                        char* SSRC = extract_ssrc_from_sdp(body->body);
                        char* endptr;
                        uint32_t SSRC_t =strtoul(SSRC, &endptr, 10);

                        char sdp_body[1024];
                        snprintf(sdp_body, sizeof(sdp_body),
                        "v=0\r\n"
                        "o=34020000001320000001 0 0 IN IP4 %s\r\n"
                        "s=Playback\r\n"
                        "c=IN IP4 %s\r\n"
                        "t=%s %s\r\n"
                        "m=video %d RTP/AVP 96\r\n"
                        "a=setup:active\r\n"
                        "a=connection:new\r\n"
                        "a=rtpmap:96 PS/90000\r\n"
                        "a=sendonly\r\n"
                        "y=%s \r\n"
                        "f=v/2////a/6//1\r\n" , LOCAL_IP,LOCAL_IP,start_time,end_time,random_port,SSRC);

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
                        free(SSRC);
                        RecordCsv record_parser(RECORD_FILE,RECORD_PATH);

                        if (record_parser.hasRecordWithTime(start_time, end_time)) {
                            char* new_filepath = (char*)malloc(512);
                            strcpy(new_filepath, RECORD_PATH);      // 复制第一个字符串
                            strcat(new_filepath, start_time);
                            strcat(new_filepath, "_");        // 添加分隔符
                            strcat(new_filepath, end_time);
                            strcat(new_filepath, ".h264");


                            mRtpSendPs_record = new RtpSendPs(SERVER_IP, port, random_port, SSRC_t);

                            mRtpSendPs_record->set_filename(new_filepath);
                            mRtpSendPs_record->start(start_record);
                        }



                        break;
                    }
                        case Download: {
                        char start_time[20];
                        char end_time[20];

                        extract_and_format_times(body->body, start_time, end_time);

                        printf("开始时间: %s\n", start_time);  // 输出: 2025-08-24T01:30:00
                        printf("结束时间: %s\n", end_time);    // 输出: 2025-08-24T02:00:00
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

                        char* SSRC = extract_ssrc_from_sdp(body->body);
                        char* endptr;
                        uint32_t SSRC_t =strtoul(SSRC, &endptr, 10);

                        char sdp_body[1024];
                        snprintf(sdp_body, sizeof(sdp_body),
                        "v=0\r\n"
                        "o=34020000001320000001 0 0 IN IP4 %s\r\n"
                        "s=Play\r\n"
                        "c=IN IP4 %s\r\n"
                        "t=%s %s\r\n"
                        "m=video 20005 RTP/AVP 96\r\n"
                        "a=setup:active\r\n"
                        "a=connection:new\r\n"
                        "a=rtpmap:96 PS/90000\r\n"
                        "a=sendonly\r\n"
                        "y=%s \r\n"
                        "f=v/2////a/6//1\r\n" , LOCAL_IP,LOCAL_IP,start_time,end_time,SSRC);

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
                        free(SSRC);
                        RecordCsv record_parser(RECORD_FILE,RECORD_PATH);

                        if (record_parser.hasRecordWithTime(start_time, end_time)) {
                            char* new_filepath = (char*)malloc(512);
                            strcpy(new_filepath, RECORD_PATH);      // 复制第一个字符串
                            strcat(new_filepath, start_time);
                            strcat(new_filepath, "_");        // 添加分隔符
                            strcat(new_filepath, end_time);
                            strcat(new_filepath, ".h264");


                            mRtpSendPs_download = new RtpSendPs(SERVER_IP, port, 20005, SSRC_t);

                            mRtpSendPs_download->set_filename(new_filepath);
                            mRtpSendPs_download->start(start_download);
                        }

                        break;
                    }
                }


            }

        }
        if (event->type == EXOSIP_MESSAGE_NEW) {
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
                    gb28181->send_200_OK(context_exosip, event->tid);
                    gb28181->send_catalog_response(context_exosip, event->tid,atoi(SN));
                }
                else if (strstr(body->body, "<CmdType>RecordInfo</CmdType>")) {
                    printf("收到recordinfo查询请求\n");
                    char SN[64] ;
                    if (parser->extractXmlElement(body->body, "SN", SN, sizeof(SN))) {
                        printf("提取到的 SN: %s\n", SN);
                    } else {
                        printf("提取 SN 失败。\n");
                    }
                    gb28181->send_200_OK(context_exosip, event->tid);
                    gb28181->send_record_info_response(context_exosip, event->tid,atoi(SN));
                }
                else if (strstr(body->body, "<CmdType>DeviceInfo</CmdType>")) {
                    printf("收到设备信息查询请求\n");
                    char SN[64] ;

                    if (parser->extractXmlElement(body->body, "SN", SN, sizeof(SN))) {
                        printf("提取到的 SN: %s\n", SN);
                    } else {
                        printf("提取 SN 失败。\n");
                    }
                    gb28181->send_200_OK(context_exosip, event->tid);

                    gb28181->send_device_info_response(context_exosip, event->did,atoi(SN));
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
                    gb28181->send_200_OK(context_exosip, event->tid);

                    gb28181->send_DeviceStatus_response(context_exosip, event->did,atoi(SN));
                }
                else if (strstr(body->body, "<CmdType>ConfigDownload</CmdType>")) {
                    printf("收到Configdownload查询请求\n");
                    //return send_device_status_response(ctx, evt->tid);
                    char SN[64] ;

                    if (parser->extractXmlElement(body->body, "SN", SN, sizeof(SN))) {
                        printf("提取到的 SN: %s\n", SN);
                    } else {
                        printf("提取 SN 失败。\n");
                    }
                    gb28181->send_200_OK(context_exosip, event->tid);

                    gb28181->send_ConfigDownload_response(context_exosip, event->did,atoi(SN));
                }
                else if (strstr(body->body, "<GuardCmd>SetGuard</GuardCmd>")) {
                    gb28181->send_200_OK(context_exosip, event->tid);

                }
            }

        }

        if (event->type == EXOSIP_REGISTRATION_FAILURE)
        {


            if (event->response == NULL) {
                printf("Registration failed: no response received (timeout or network issue)\n");
                // 可以选择重试注册，而不是继续解析
                ret = do_register(context_exosip, from, proxy, contact,LOCAL_IP,3600);
                printf("register failed , try again later ...\n");
                int try_count = 0;
                while ( ret != 0) {
                    ret = do_register(context_exosip, from, proxy, contact,LOCAL_IP,3600) ;
                    sleep(try_count*2);
                    printf("register try ...... try count = %d",try_count++);
                    if (try_count >=5) {

                        printf("register failed and kill the main");
                        eXosip_quit(context_exosip);
                        return 0 ;
                        break;
                    }
                }


                continue; // 或 continue;
            }

            osip_message_t *reg = NULL;
            osip_www_authenticate_t *dest = NULL;

            // 此时 event->response 非空，可以安全调用
            int result = osip_message_get_www_authenticate(event->response, 0, &dest);
            if (result != 0 || dest == NULL) {
                printf("No WWW-Authenticate header in response\n");
                continue;
            }

            const char *realm_str = osip_www_authenticate_get_realm(dest);
            if (realm_str == NULL) {
                printf("No realm found in WWW-Authenticate header\n");
                continue;
            }

            char realm[256];
            strncpy(realm, realm_str, sizeof(realm) - 1);
            realm[sizeof(realm) - 1] = '\0';

            eXosip_clear_authentication_info(context_exosip);
            eXosip_add_authentication_info(context_exosip, USERNAME, SIP_USER, PASSWORD, "MD5", realm);

            int build_result = eXosip_register_build_register(context_exosip, event->rid, 3600, &reg);
            if (build_result != 0 || reg == NULL) {
                printf("eXosip_register_build_register failed!\n");
                continue;
            }

            printf("authenticate=%s  ver=%s\n", realm, reg->sip_version);
            int ret = eXosip_register_send_register(context_exosip, event->rid, reg);
            if (ret != 0) {
                printf("eXosip_register_send_register failed: %d\n", ret);
                osip_message_free(reg);  // 发送失败，手动释放
                // 注意：不要轻易 eXosip_quit()，应设计重试机制
            }


        }


        if (event->type == EXOSIP_IN_SUBSCRIPTION_NEW) {
            osip_body_t *body = NULL;
            if (osip_message_get_body(event->request, 0, &body) == 0 && body && body->body) {
                printf("收到subscribe请求: %s\n", body->body);

                if (strstr(body->body, "<CmdType>Catalog</CmdType>")) {
                    char SN[64] ;

                    if (parser->extractXmlElement(body->body, "SN", SN, sizeof(SN))) {
                        printf("提取到的 SN: %s\n", SN);
                    } else {
                        printf("提取 SN 失败。\n");
                    }

                    gb28181->send_Subscribe_response(context_exosip, event->tid, atoi(SN));
                }
            }
        }


        if (event->type == EXOSIP_CALL_CLOSED) {
            if (mRtpSendPs!=NULL && mRtpSendPs->m_bRun) {
                mRtpSendPs->stop();
            }
            if (mRtpSendPs_record!=NULL&& mRtpSendPs_record->m_bRun) {
                mRtpSendPs_record->stop();
            }
            if (mRtpSendPs_0!=NULL&& mRtpSendPs_0->m_bRun) {
                mRtpSendPs_0->stop();
            }
            if (mRtpSendPs_download!=NULL&& mRtpSendPs_download->m_bRun) {
                mRtpSendPs_download->stop();
            }
            cout<<"call closed"<<endl;
        }
        eXosip_unlock(context_exosip);


    }
    mRtpSendPs->stop();
    mRtpSendPs_0->stop();
    mRtpSendPs_record->stop();
    mRtpSendPs_download->stop();

    stop_all_media();
    delete mRtpSendPs;
    delete mRtpSendPs_0;
    delete mRtpSendPs_record;
    delete mRtpSendPs_download;

    return 0 ;

}
