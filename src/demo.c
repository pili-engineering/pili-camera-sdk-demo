//
//  demo.c
//  camera-sdk-demo
//
//  Created on 14/12/10
//  Copyright (c) Pili Engineering, Qiniu Inc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include "flv-parser.h"
#include "push.h"

char *g_url = NULL;

void usage(char *program_name) {
    printf("Usage: %s [input.flv] [your_push_url]\n", program_name);
    exit(-1);
}

pili_stream_context_p g_ctx = NULL;
int g_ready_to_send_packet = 0;

const char *stream_states[] = {
    "Stream state: Unknow",
    "Stream state: Connecting",
    "Stream state: Connected",
    "Stream state: Disconnected",
    "Stream state: Error"
};

void stream_state_cb(uint8_t state) {
    printf("=========== %s ===========\n", stream_states[state]);
}

void start_push() {
    g_ready_to_send_packet = 0;
    const char *url = g_url;
    
    g_ctx = pili_create_stream_context();
    pili_init_stream_context(g_ctx,
                             PILI_STREAM_DROP_FRAME_POLICY_RANDOM,
                             PILI_STREAM_BUFFER_TIME_INTERVAL_DEFAULT,
                             stream_state_cb);
    
    int cnt = 0, ret = -1;
    
    while (ret == -1 && cnt < 3) {
        ret = pili_stream_push_open(g_ctx, url);
        cnt++;
    }
    
    if (!ret) {
        g_ready_to_send_packet = 1;
    } else {
        printf("pili_stream_push_open failed.");
    }
}

void parsed_flv_tag(flv_tag_p flv_tag) {
    if (g_ctx && g_ready_to_send_packet) {
        pili_write_packet(g_ctx, flv_tag);
    }
}

int main(int argc, char *argv[]) {
    FILE *infile = NULL;
    
    if (3 != argc) {
        usage(argv[0]);
    } else {
        infile = fopen(argv[1], "r");
        if (!infile) {
            usage(argv[0]);
        }
        
        g_url = argv[2];
    }
    
    start_push();
    
    flv_parser_init(infile);
    
    flv_parser_run(parsed_flv_tag);
    
    pili_stream_push_close(g_ctx);
    pili_release_stream_context(g_ctx);
    
    return 0;
}