# rpi-camera


## Thư mục test là các test case để phục vụ cho mục đích testing
+ Để chạy được các test trong thư mục test.
    - cd đến thư mục script, chạy lệnh "./test.sh", chọn file muốn thực thi
    - ./build_indoor vs RPI, ./build_outdoor với x86_64

+ Đối với bài test sử dụng bidirection streaming
    + srt_snd_rcv_mpegts_bidirection_stream.cpp
    + ./build_outdoor
    + ./abc.sh để đổi tên file thực thi thành 2 file caller và listener
    + đôi với listener: ./listener listener <địa chỉ IP của máy> <port> <file đầu vào .ts> <file_đầu ra .aac> <file đầu ra.h264>
    + đối với caller:   ./caller caller <địa chỉ IP của máy listener> <port> <file đầu vào .ts> <file_đầu ra .aac> <file đầu ra.h264>

+ Đối với bài test sử dụng bind_acquire
    + srt_connect_from_udp_using_bind_acquire.cpp
    + ./build_outdoor
    + ./abc.sh để đổi tên file thực thi thành 2 file caller và listener
    + đôi với listener: ./listener listener <địa chỉ IP của máy> <port> <file đầu vào .ts> <file_đầu ra .aac> <file đầu ra.h264>
    + đối với caller:   ./caller caller <địa chỉ IP của máy listener> <port> <file đầu vào .ts> <file_đầu ra .aac> <file đầu ra.h264>

+ Đối với bài test stream .ts qua SRT sử dụng chế độ caller
    + rpi_send_mpegts_srt_using _caller_mode.cpp
    + ./build_indoor
    + ./send_rpi
    + đến rpi rồi ./h264capture <ip của máy listener> <port>


+ Test Relay Server SRT:
-> Sử dụng file srt-live-server trên thư mục out, hoặc build srt theo hướng dẫn: https://github.com/Haivision/srt/blob/master/docs/build/build-linux.md
    
    + <Camera (listener)> ---- <(caller) srt-live-server (listener)> ---- <Player (caller)>
        ./srt-live-transmit "srt://[ip_camera]:[port_camera]?mode=caller" "srt://[ip_server]:[port_server]?mode=listener"
    
    + <Camera (caller)> ---- <(listener) srt-live-server (listener)> ---- <Player (caller)>
        ./srt-live-transmit "srt://[ip_server]:[port_server_in]?mode=caller" "srt://[ip_server]:[port_server_out]?mode=listener"

+ Test Tính năng encrypt sử dụng SRT
    + test_send_mpegts_from_file_using_srt_listenter_and_encrypt.cpp
        ./h264capture <ip_address_cua_may> <port> <input_file.ts> <"password">

+ Test nhiều listener kết nối đến một UDP port:
    + test_multi_caller_listener.cpp
        ./h264capture <ip_address_cua_may> <port> <input_file1.ts> <input_file2.ts>
        + trên ffplay:
            ffplay -i "srt://<ip_address_nhu_tren>:<port>?mode=caller&streamid=file01"
            ffplay -i "srt://<ip_address_nhu_tren>:<port>?mode=caller&streamid=file02"
+ Test một listener kết nối đến một UDP port và listener đó nhận kết nối của các caller có streamid khác nhau, nhận được dữ liệu khác nhau:
    + test_multi_streamid_listener.cpp
        ./h264capture <ip_address_cua_may> <port>
        + trên ffplay:
            ffplay -i "srt://<ip_address_nhu_tren>:<port>?mode=caller&streamid=stream_01"
            ffplay -i "srt://<ip_address_nhu_tren>:<port>?mode=caller&streamid=stream_02"
+ Test nhiều caller kết nối đến một UDP port (udp port = 5000), gửi cùng một dữ liệu:
    + test_multi_streamid_caller.cpp
        + trên ffplay:
            ffplay -i "srt://<ip_listener_01>:<port_listener_01>?mode=listener"
        + trên Haivision Play Pro
            chọn Play an SRT Stream, chuyển sang chế độ Listener, play

        + thực hiện câu lệnh:
        ./h264capture <ip_listener_01> <port_listener_01> <ip_listener_02> <port_listener_02> <ip_listener_03> <port_listener_03>