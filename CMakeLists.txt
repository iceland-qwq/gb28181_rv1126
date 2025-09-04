

project(exosip_project)
set(CMAKE_BUILD_TYPE Debug)
add_definitions(-g -O0 -ggdb -gdwarf -funwind-tables -rdynamic)

aux_source_directory(common COMMON_SRC)
set(THIRD_MEDIA ${CMAKE_CURRENT_LIST_DIR}/../src/rkaudio/libthird_media.so)

find_package(RkAiq REQUIRED)
include_directories(${RKAIQ_INCLUDE_DIRS})
add_definitions(-DRKAIQ)

set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
#set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)


add_executable(exosip_project
        main.cpp
        xml_parser.cpp
        xml_parser.h
        Utils/Log.h
        RtpSendPs.cpp
        RtpSendPs.h
        sdp_parser.cpp
        sdp_parser.h
        venc.cpp
        venc.h
        shared_queue.cpp
        shared_queue.h
        record.cpp
        record.h
        time_function.cpp
        time_function.h
        ${COMMON_SRC}
        message_queue.cpp
        message_queue.h
        tcp_receiver.cpp
        tcp_receiver.h
        cJSON.h
        tcp_sender.cpp
        tcp_sender.h
        gb28181_functions.cpp
        gb28181_functions.h
        config.h
        tool.cpp
        tool.h
        tcp_client_receiver.cpp
        tcp_client_receiver.h
        mpp_guard.hpp
)



include_directories("/home/liketao/gb_project/include/")
include_directories("./common/")
include_directories("/Desktop/atk-rv1126/buildroot/output/alientek_rv1126/build/rkmedia/include/")
link_directories("/home/liketao/gb_project/lib")
add_dependencies(exosip_project easymedia )
target_include_directories(exosip_project PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(exosip_project  pthread easymedia  ${THIRD_MEDIA}
   /home/liketao/gb_project/lib/libeXosip2.so
    /home/liketao/gb_project/lib/libosip2.so
    /home/liketao/gb_project/lib/libosipparser2.so
        /home/liketao/gb_project/lib/libcjson.so

)

