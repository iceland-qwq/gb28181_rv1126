//
// Created by liketao on 25-6-18.
//

#ifndef SDP_PARSER_H
#define SDP_PARSER_H
#include "time_function.h"
enum sdp_type {
    Play = 0,
    Playback = 1,
    Download = 2
};


int get_first_media_port_from_sdp(const char *sdp);
char* extract_ssrc_from_sdp(const char* sdp) ;

int extract_Slabel_content(const char* sdp) ;
void extract_and_format_times(const char* input, char* output1, char* output2) ;
char* extract_u_from_sdp(const char* sdp);
char* extract_o_from_sdp(const char* sdp);
#endif //SDP_PARSER_H
