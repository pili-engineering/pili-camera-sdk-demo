/*
 * @file plive.c
 * @author Akagi201
 * @date 2014/12/10
 */

#include <stdio.h>
#include <stdlib.h>
#include "flv-parser.h"
#include "push.h"

#define RTMP_URL    "rtmp://localhost/live/inrtmp"

void usage(char *program_name) {
    printf("Usage: %s [input.flv]\n", program_name);
    exit(-1);
}

void audio_sent_callback(pili_audio_packet_p packet) {
    pili_audio_packet_release(packet);
}

void video_sent_callback(pili_video_packet_p packet) {
    pili_video_packet_release(packet);
}

pili_stream_context_p g_ctx = NULL;
bool g_ready_to_send_packet = false;

void start_push(pili_metadata_packet_p metadata_packet) {
    const char *url = RTMP_URL;
    
    int cnt = 0, ret = -1;
    
    while (ret == -1 && cnt < 3) {
        ret = pili_stream_push_open(g_ctx, metadata_packet, url);
        cnt++;
    }
    
    if (!ret) {
        g_ready_to_send_packet = true;
    } else {
        printf("pili_stream_push_open failed.");
    }
}

void parsed_metadata(pili_metadata_packet_p metadata_packet) {
    g_ready_to_send_packet = false;
    pili_stream_context_create(&g_ctx);
    pili_stream_context_init(g_ctx, audio_sent_callback, video_sent_callback);
    start_push(metadata_packet);
}

void parsed_video_tag(pili_video_packet_p v_packt) {
    if (g_ctx && g_ready_to_send_packet) {
        pili_write_video_packet(g_ctx, v_packt);
    }
}

void parsed_audio_tag(pili_audio_packet_p a_packet) {
    if (g_ctx && g_ready_to_send_packet) {
        pili_write_audio_packet(g_ctx, a_packet);
    }
}

int main(int argc, char *argv[]) {
    FILE *infile = NULL;
    
    if (argc == 1) {
        infile = stdin;
    } else {
        infile = fopen(argv[1], "r");
        if (!infile) {
            usage(argv[0]);
        }
    }
    
    flv_parser_init(infile);
    
    flv_parser_run(parsed_metadata, parsed_video_tag, parsed_audio_tag);
    
    pili_stream_push_close(g_ctx);
    pili_stream_context_release(g_ctx);
    
    return 0;
}