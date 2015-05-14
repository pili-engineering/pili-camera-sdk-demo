//
//  flv-parser.h
//  camera-sdk-demo
//
//  Created on 15/02/04
//  Copyright (c) Pili Engineering, Qiniu Inc. All rights reserved.
//

#ifndef FLV_PARSER_H_
#define FLV_PARSER_H_ (1)

#include <stdint.h>
#include <stdio.h>

#include "pili_type.h"
#include "flv.h"

#define FLV_HEADER_AUDIO_BIT (2)
#define FLV_HEADER_VIDEO_BIT (0)

/*
 * @brief flv file header 9 bytes
 */
struct flv_header {
    uint8_t signature[3];
    uint8_t version;
    uint8_t type_flags;
    uint32_t data_offset; // header size, always 9
} __attribute__((__packed__));

typedef struct flv_header flv_header_t;

/*
 * @brief flv tag general header 11 bytes
 */

int flv_read_header(void);
void flv_print_header(flv_header_t *flv_header);

flv_tag_p flv_read_tag(int *b_next_is_key);

void read_audio_tag(flv_tag_p flv_tag);
void read_video_tag(flv_tag_p flv_tag);

uint8_t flv_get_bits(uint8_t value, uint8_t start_bit, uint8_t count);
size_t fread_1(uint8_t *ptr);
size_t fread_3(uint32_t *ptr);
size_t fread_4(uint32_t *ptr);
size_t fread_4s(uint32_t *ptr);

void flv_parser_init(FILE *in_file);

typedef void (*flv_tag_callback)(flv_tag_p flv_tag);

int flv_parser_run(flv_tag_callback cb);

#endif // FLV_PARSER_H_