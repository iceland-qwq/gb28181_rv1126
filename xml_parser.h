//
// Created by liketao on 25-6-6.
//

#ifndef XML_PARSER_H
#define XML_PARSER_H



class xml_parser {
public:
    xml_parser();
    ~xml_parser();
    int extractXmlElement(const char *xml, const char *tag, char *outBuffer, int bufferSize);

};



#endif //XML_PARSER_H
