//
// Created by liketao on 25-8-22.
//

#include "record.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#include "tool.h"
using std::string;
using std::vector;
static constexpr unsigned int MAX_RECORDS = 3;
// 简单 CSV 转义：含逗号/引号/换行时包双引号




static string escapeCsv(const string& field)
{
    if (field.find_first_of(",\"\n") == string::npos)
        return field;

    string out = "\"";
    for (char c : field)
    {
        if (c == '"') out += "\"\"";
        else          out += c;
    }
    out += '"';
    return out;
}

// 解析一行 CSV（未处理带引号转义，字段里无逗号即可）
static bool parseLine(const string& line, Record& rec)
{
    std::stringstream ss(line);
    string sizeStr;
    std::getline(ss, rec.deviceId, ',');
    std::getline(ss, rec.name,     ',');
    std::getline(ss, rec.startTime,',');
    std::getline(ss, rec.endTime,  ',');
    std::getline(ss, sizeStr, ',');
    try {
        rec.fileSize = std::stoull(sizeStr);
    } catch (...) {
        return false;
    }
    return true;
}


RecordCsv::RecordCsv(const std::string& filePath, const std::string& videoDir)
    : path_(filePath), videoDir_(videoDir) {
    create_directories(videoDir_);
}

bool RecordCsv::append(const Record& rec)
{
    std::vector<Record> oldRecords;
    bool hadFile = loadAll(oldRecords); // 读取当前所有记录

    const unsigned int MAX_RECORDS = 3;

    // 判断是否已存在相同 startTime 和 endTime 的记录
    auto exists = [](const std::vector<Record>& records, const  Record& r) {
        for (const auto& item : records) {
            if (item.startTime == r.startTime && item.endTime == r.endTime) {
                return true;
            }
        }
        return false;
    };

    // 已存在则不做任何操作（不插入，也不删文件）
    if (exists(oldRecords, rec)) {
        return true;
    }

    // 生成安全的 .h264 文件名：替换 ':' 为 '-'
    auto getSafeFilename = [](const std::string& start, const std::string& end) {
        return start + "_" + end + ".h264"; // 直接返回，不替换
    };

    std::vector<Record> newRecords = oldRecords;
    std::string filepathToRemove;

    // 如果已满，准备删除最老的一条记录及其文件
    if (newRecords.size() >= MAX_RECORDS) {
        Record removed = newRecords[0];
        newRecords.erase(newRecords.begin());

        // 构造要删除的文件完整路径
        filepathToRemove = videoDir_ + getSafeFilename(removed.startTime, removed.endTime);
    }

    // 添加新记录
    newRecords.push_back(rec);

    // 重写 CSV 文件
    std::ofstream ofs(path_, std::ios::trunc);
    if (!ofs) return false;

    for (const auto& r : newRecords) {
        ofs << escapeCsv(r.deviceId) << ','
            << escapeCsv(r.name) << ','
            << escapeCsv(r.startTime) << ','
            << escapeCsv(r.endTime) << ','
            << r.fileSize << '\n';
    }

    if (!ofs) {
        return false; // 写入失败
    }

    // ✅ 成功写入后，删除旧文件（如果存在）
    if (!filepathToRemove.empty()) {
        if (std::remove(filepathToRemove.c_str()) == 0) {
            std::cout << "✅ 成功删除: " << filepathToRemove << std::endl;
        } else {
            std::cerr << "❌ 删除失败: " << filepathToRemove
                      << " (errno=" << errno
                      << ": " << strerror(errno) << ")"
                      << std::endl;
        }
    }

    return true;
}
bool RecordCsv::loadAll(vector<Record>& out)
{
    out.clear();
    std::ifstream ifs(path_);
    if (!ifs) return false;
    string line;
    while (std::getline(ifs, line))
    {
        Record r;
        if (parseLine(line, r))
            out.emplace_back(std::move(r));
    }
    return true;
}

bool RecordCsv::loadTopN(vector<Record>& out, unsigned int N)
{
    out.clear();
    std::ifstream ifs(path_);
    if (!ifs) return false;
    string line;
    while (std::getline(ifs, line) && out.size() < N)
    {
        Record r;
        if (parseLine(line, r))
            out.emplace_back(std::move(r));
    }
    return true;
}

#include <algorithm>

// ================= removeByIndex =================
bool RecordCsv::removeByIndex(unsigned int idx)
{
    std::vector<Record> all;
    if (!loadAll(all)) return false;
    if (idx >= all.size()) return false;          // 越界
    all.erase(all.begin() + idx);                 // 删除
    // 重新写整个文件
    std::ofstream ofs(path_, std::ios::trunc);
    if (!ofs) return false;
    for (const auto& r : all)
        ofs << escapeCsv(r.deviceId) << ',' << escapeCsv(r.name) << ','
            << escapeCsv(r.startTime) << ',' << escapeCsv(r.endTime) << ','
            << r.fileSize << '\n';
    return true;
}

// ================= removeByTime =================
unsigned int RecordCsv::removeByTime(const std::string& startTime,
                                     const std::string& endTime)
{
    std::vector<Record> all;
    if (!loadAll(all)) return 0;

    // 先把时间字符串转成 epoch 便于比较
    auto toEpoch = [](const std::string& iso) -> std::time_t {
        struct tm tm = {};
        sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d",
               &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
        tm.tm_year -= 1900;
        tm.tm_mon  -= 1;
        return std::mktime(&tm);
    };

    std::time_t startEpoch = toEpoch(startTime);
    std::time_t endEpoch   = toEpoch(endTime);

    auto oldSize = all.size();
    all.erase(std::remove_if(all.begin(), all.end(),
        [&](const Record& r) {
            std::time_t s = toEpoch(r.startTime);
            std::time_t e = toEpoch(r.endTime);
            // 只要区间有交集就算命中
            return !(e < startEpoch || s > endEpoch);
        }), all.end());

    unsigned int erased = oldSize - all.size();
    if (erased > 0) {
        std::ofstream ofs(path_, std::ios::trunc);
        for (const auto& r : all)
            ofs << escapeCsv(r.deviceId) << ',' << escapeCsv(r.name) << ','
                << escapeCsv(r.startTime) << ',' << escapeCsv(r.endTime) << ','
                << r.fileSize << '\n';
    }
    return erased;
}


// 在 RecordCsv 类中添加函数声明
bool RecordCsv::queryByTime(const std::string& startTime, const std::string& endTime, std::vector<Record>& results)
{
    std::vector<Record> all;
    if (!loadAll(all)) {
        results.clear();
        return false;
    }

    // 将 ISO 时间字符串转换为 time_t 类型
    auto toEpoch = [](const std::string& iso) -> std::time_t {
        struct tm tm = {};
        int parsed = sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d",
                            &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                            &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
        if (parsed != 6) return -1; // 解析失败

        tm.tm_year -= 1900;
        tm.tm_mon -= 1;
        return std::mktime(&tm); // 返回 Unix 时间戳
    };

    std::time_t startEpoch = toEpoch(startTime);
    std::time_t endEpoch = toEpoch(endTime);

    // 检查用户输入的时间是否合法
    if (startEpoch == -1 || endEpoch == -1) {
        results.clear();
        return false;
    }

    results.clear();
    for (const auto& r : all) {
        std::time_t s = toEpoch(r.startTime);
        std::time_t e = toEpoch(r.endTime);

        if (s == -1 || e == -1) continue; // 忽略时间格式错误的记录

        // 判断是否有时间交集
        if (!(e < startEpoch || s > endEpoch)) {
            results.push_back(r);
        }
    }

    return true;
}


// 在 RecordCsv 类中添加函数声明
bool RecordCsv::hasRecordWithTime(const std::string& startTime, const std::string& endTime)
{
    std::vector<Record> all;
    if (!loadAll(all)) {
        return false; // 文件读取失败
    }

    // 遍历所有记录，寻找匹配项
    for (const auto& r : all) {
        if (r.startTime == startTime && r.endTime == endTime) {
            return true; // 找到匹配的记录
        }
    }

    return false; // 没有找到匹配的记录
}