//
// Created by liketao on 25-8-22.
//

#ifndef RECORD_H
#define RECORD_H

#include <string>
#include <vector>
#include "config.h"
struct Record
{
    std::string deviceId;
    std::string name;
    std::string startTime; // ISO-8601
    std::string endTime;
    unsigned long long fileSize;   // 字节
};

class RecordCsv
{
public:
    explicit RecordCsv(const std::string& filePath= RECORD_FILE, const std::string& videoDir = RECORD_PATH);

    // 追加一条记录
    bool append(const Record& rec);

    // 读取全部记录
    bool loadAll(std::vector<Record>& out);

    // 读取前 N 条
    bool loadTopN(std::vector<Record>& out, unsigned int N = 10);

    // 删除第 idx 条记录（0 起始），成功返回 true
    bool removeByIndex(unsigned int idx);

    // 删除所有落在 [start, end] 时间段内的记录（闭区间），返回删除条数
    unsigned int removeByTime(const std::string& startTime,
                              const std::string& endTime);
    bool queryByTime(const std::string& startTime, const std::string& endTime, std::vector<Record>& results);
    bool hasRecordWithTime(const std::string& startTime, const std::string& endTime);
private:
    std::string path_;


    std::string videoDir_;
};

#endif


