//
// Created by liketao on 25-6-6.
//

#include "xml_parser.h"
#include "string.h"
#include "stdio.h"


xml_parser::xml_parser() {
    printf("xml_parser init finished");
}
xml_parser::~xml_parser() {
    printf("xml_parser del finished");
}

int xml_parser::extractXmlElement(const char *xml, const char *tag, char *outBuffer, int bufferSize) {
    char startTag[64], endTag[64];
    snprintf(startTag, sizeof(startTag), "<%s>", tag);
    snprintf(endTag, sizeof(endTag), "</%s>", tag);

    // 查找开始标签和结束标签的位置
    const char *startPos = strstr(xml, startTag);
    if (startPos == NULL) {
        return 0; // 没找到开始标签
    }

    const char *endPos = strstr(startPos + strlen(startTag), endTag);
    if (endPos == NULL) {
        return 0; // 没找到结束标签
    }

    // 计算内容长度
    size_t contentLength = endPos - (startPos + strlen(startTag));

    // 判断是否超出缓冲区范围或内容为空
    if (contentLength <= 0 || contentLength >= bufferSize) {
        return 0;
    }

    // 复制内容到输出缓冲区并添加字符串结尾
    strncpy(outBuffer, startPos + strlen(startTag), contentLength);
    outBuffer[contentLength] = '\0';

    return 1; // 成功
}
