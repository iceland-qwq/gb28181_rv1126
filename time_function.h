//
// Created by liketao on 25-8-24.
//

#ifndef TIME_FUNCTION_H
#define TIME_FUNCTION_H
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <string>

char* format_timestamp_iso8601(time_t timestamp, char* buffer) ;
std::string add_seconds_to_timestamp(const std::string& ts_str, int seconds_offset) ;


#endif //TIME_FUNCTION_H
