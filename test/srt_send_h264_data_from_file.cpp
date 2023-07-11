/**
 * @file srt_send_h264_data_from_file.cpp
 * @author your name (you@domain.com)
 * @brief Test send h264 data from file using SRT
 * @version 0.1
 * @date 2023-06-09
 * 
 *  Sử dụng trên RPI or PC
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// #include "h264_capture.h"
#include "logger.h"
#include "srt/srt.h"


int ss = 0;
int st = 0 ;
int their_fd = -1;
FILE *file;
int sock_fd, send_bytes;
struct sockaddr_in server_addr;

int sendH264FromFile()
{
    if (their_fd == -1)
            return 1;
    char byte[1316];
    
    int size = 0;
    // Read the file byte by byte
    while (!feof(file))
    {
        memset(byte, 0, 1316);
        size = fread(&byte, sizeof(unsigned char), 1316, file);
        if (1)
        {
        }
        
        st = srt_sendmsg2(their_fd, (char *)byte, size, NULL);
        if (st == SRT_ERROR)
        {
            fprintf(stderr, "srt_sendmsg: %s\n", srt_getlasterror_str());
            their_fd = -1;
        }
        usleep(5000);
    }

    // Close the file
    fclose(file);
}

void sending_frame(const void *buffer, int size)
{
    send_bytes = sendto(sock_fd, buffer, size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (send_bytes == -1)
    {
        perror("sendto");
        exit(1);
    }

    int st;
    if (their_fd == -1)
        exit(1);
    st = srt_sendmsg2(their_fd, (char *)buffer, size, NULL);
    if (size < 1316)
        printf("packet: %d\n", size);
    // usleep(600);
    if (st == SRT_ERROR)
    {
        fprintf(stderr, "srt_sendmsg: %s\n", srt_getlasterror_str());
        their_fd = -1;
    }
}


int main(int argc, char **argv)
{
    /// srt
    int st;
    struct sockaddr_in sa;
    int yes = 1;
    struct sockaddr_storage their_addr;
    bool no = false;
    int srt_latency = 3000;

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

    char filename[100];
    

    printf("Enter the filename: ");
    scanf("%s", filename);

    // Open the file in binary mode
    file = fopen(filename, "rb");

    if (file == NULL)
    {
        printf("Error opening the file.\n");
        return 1;
    }

    printf("SRT startup\n");
    srt_startup();

    printf("SRT socket\n");
    ss = srt_create_socket();
    if (ss == SRT_ERROR)
    {
        fprintf(stderr, "srt_socket: %s\n", srt_getlasterror_str());
        return 1;
    }

    printf("SRT bind address\n");
    sa.sin_family = AF_INET;
    sa.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &sa.sin_addr) != 1)
    {
        return 1;
    }

    printf("SRT setsockflag\n");
    if (SRT_ERROR == srt_setsockflag(ss, SRTO_SENDER, &yes, sizeof(yes)))
    {
        fprintf(stderr, "srt_setsockflag: %s\n", srt_getlasterror_str());
        return 1;
    }

    //  printf("SRT setsockflag\n");
    // if (SRT_ERROR == srt_setsockflag(ss, SRTO_TLPKTDROP, &yes, sizeof(yes)))
    // {
    // fprintf(stderr, "srt_setsockflag: %s\n", srt_getlasterror_str());
    // return 1;
    // }

    printf("SRT setsockflag\n");
    if (SRT_ERROR == srt_setsockflag(ss, SRTO_LATENCY, &srt_latency, sizeof(srt_latency)))
    {
        fprintf(stderr, "srt_setsockflag: %s\n", srt_getlasterror_str());
        return 1;
    }

    printf("SRT bind\n");
    st = srt_bind(ss, (struct sockaddr *)&sa, sizeof sa);
    if (st == SRT_ERROR)
    {
        fprintf(stderr, "srt_bind: %s\n", srt_getlasterror_str());
        return 1;
    }

    printf("SRT listen\n");
    st = srt_listen(ss, 2);
    if (st == SRT_ERROR)
    {
        fprintf(stderr, "srt_listen: %s\n", srt_getlasterror_str());
        return 1;
    }

    printf("SRT accept\n");
    int addr_size = sizeof their_addr;
    their_fd = srt_accept(ss, (struct sockaddr *)&their_addr, &addr_size);
    if (their_fd == SRT_INVALID_SOCK)
    {
        fprintf(stderr, "srt_accept: %s\n", srt_getlasterror_str);
        return 1;
    }

    st = sendH264FromFile();
    printf("srt close\n");
    st = srt_close(ss);
    if (st == SRT_ERROR)
    {
        fprintf(stderr, "srt_close: %s\n", srt_getlasterror_str());
        return 1;
    }

    printf("srt cleanup\n");
    srt_cleanup();

    close(sock_fd);
    return 0;
}