//
//  flv-parser.c
//  camera-sdk-demo
//
//  Created on 14/12/10
//  Copyright (c) Pili Engineering, Qiniu Inc. All rights reserved.
//

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "flv-parser.h"

#define HTONTIME(x) ((x>>16&0xff)|(x<<16&0xff0000)|(x&0xff00)|(x&0xff000000))

char *put_be16(char *output, uint16_t nVal) {
    output[1] = nVal & 0xff;
    output[0] = nVal >> 8;
    
    return output + 2;
}

char *put_amf_string(char *c, const char *str) {
    uint16_t len = (uint16_t) strlen(str);
    *c = 0x02; // AMF0 type: String
    ++c;
    c = put_be16(c, len);
    memcpy(c, str, len);
    
    return c + len;
}

// File-scope ("global") variables
const char *flv_signature = "FLV";

const char *sound_formats[] = {
        "Linear PCM, platform endian",
        "ADPCM",
        "MP3",
        "Linear PCM, little endian",
        "Nellymoser 16-kHz mono",
        "Nellymoser 8-kHz mono",
        "Nellymoser",
        "G.711 A-law logarithmic PCM",
        "G.711 mu-law logarithmic PCM",
        "not defined by standard",
        "AAC",
        "Speex",
        "not defined by standard",
        "not defined by standard",
        "MP3 8-Khz",
        "Device-specific sound"
};

const char *sound_rates[] = {
        "5.5-Khz",
        "11-Khz",
        "22-Khz",
        "44-Khz"
};

const char *sound_sizes[] = {
        "8 bit",
        "16 bit"
};

const char *sound_types[] = {
        "Mono",
        "Stereo"
};

const char *frame_types[] = {
        "not defined by standard",
        "keyframe (for AVC, a seekable frame)",
        "inter frame (for AVC, a non-seekable frame)",
        "disposable inter frame (H.263 only)",
        "generated keyframe (reserved for server use only)",
        "video info/command frame"
};

const char *codec_ids[] = {
        "not defined by standard",
        "JPEG (currently unused)",
        "Sorenson H.263",
        "Screen video",
        "On2 VP6",
        "On2 VP6 with alpha channel",
        "Screen video version 2",
        "AVC"
};

const char *avc_packet_types[] = {
        "AVC sequence header",
        "AVC NALU",
        "AVC end of sequence (lower level NALU sequence ender is not required or supported)"
};

static FILE *g_infile;

void die(void) {
    printf("Error!\n");
    exit(-1);
}

/*
 * @brief read bits from 1 byte
 * @param[in] value: 1 byte to analysize
 * @param[in] start_bit: start from the low bit side
 * @param[in] count: number of bits
 */
uint8_t flv_get_bits(uint8_t value, uint8_t start_bit, uint8_t count) {
    uint8_t mask = 0;

    mask = (uint8_t) (((1 << count) - 1) << start_bit);
    return (mask & value) >> start_bit;

}

void flv_print_header(flv_header_t *flv_header) {

    printf("FLV file version %u\n", flv_header->version);
    printf("  Contains audio tags: ");
    if (flv_header->type_flags & (1 << FLV_HEADER_AUDIO_BIT)) {
        printf("Yes\n");
    } else {
        printf("No\n");
    }
    printf("  Contains video tags: ");
    if (flv_header->type_flags & (1 << FLV_HEADER_VIDEO_BIT)) {
        printf("Yes\n");
    } else {
        printf("No\n");
    }
    printf("  Data offset: %lu\n", (unsigned long) flv_header->data_offset);

    return;
}

size_t fread_1(uint8_t *ptr) {
    assert(NULL != ptr);
    return fread(ptr, 1, 1, g_infile);
}

size_t fread_3(uint32_t *ptr) {
    assert(NULL != ptr);
    size_t count = 0;
    uint8_t bytes[3] = {0};
    *ptr = 0;
    count = fread(bytes, 3, 1, g_infile);
    *ptr = (bytes[0] << 16) | (bytes[1] << 8) | bytes[2];
    return count * 3;
}

size_t fread_4(uint32_t *ptr) {
    assert(NULL != ptr);
    size_t count = 0;
    uint8_t bytes[4] = {0};
    *ptr = 0;
    count = fread(bytes, 4, 1, g_infile);
    *ptr = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
    return count * 4;
}

/*read 4 byte and convert to time format*/
int read_time(uint32_t *utime) {
    if (fread(utime, 4, 1, g_infile) != 1) {
        return 0;
    }
    *utime = HTONTIME(*utime);
    return 1;
}

/*
 * @brief skip 4 bytes in the file stream
 */
size_t fread_4s(uint32_t *ptr) {
    assert(NULL != ptr);
    size_t count = 0;
    uint8_t bytes[4] = {0};
    *ptr = 0;
    count = fread(bytes, 4, 1, g_infile);
    return count * 4;
}

/*
 * @brief read audio tag
 */
void read_audio_tag(flv_tag_p flv_tag) {
    assert(NULL != flv_tag);
    uint8_t byte = 0;

    fread_1(&byte);
    fseek(g_infile, -1, SEEK_CUR);
    
    int sound_format = flv_get_bits(byte, 4, 4);
    int sound_rate = flv_get_bits(byte, 2, 2);
    int sound_size = flv_get_bits(byte, 1, 1);
    int sound_type = flv_get_bits(byte, 0, 1);

    printf("  Audio tag:\n");
    printf("  - Sound format: %u - %s\n", sound_format, sound_formats[sound_format]);
    printf("  - Sound rate: %u - %s\n", sound_rate, sound_rates[sound_rate]);

    printf("  - Sound size: %u - %s\n", sound_size, sound_sizes[sound_size]);
    printf("  - Sound type: %u - %s\n", sound_type, sound_types[sound_type]);
    
    flv_tag->data = malloc((size_t) flv_tag->data_size);
    fread(flv_tag->data, (size_t) flv_tag->data_size, 1, g_infile);
}

/*
 * @brief read video tag
 */
void read_video_tag(flv_tag_p flv_tag) {
    uint8_t byte = 0;

    fread_1(&byte);
    fseek(g_infile, -1, SEEK_CUR);

    int frame_type = flv_get_bits(byte, 4, 4);
    int codec_id = flv_get_bits(byte, 0, 4);

    printf("  Video tag:\n");
    printf("  - Frame type: %u - %s\n", frame_type, frame_types[frame_type]);
    printf("  - Codec ID: %u - %s\n", codec_id, codec_ids[codec_id]);

    flv_tag->data = malloc((size_t) flv_tag->data_size);
    fread(flv_tag->data, (size_t) flv_tag->data_size, 1, g_infile);
}

void flv_parser_init(FILE *in_file) {
    g_infile = in_file;
}

extern char *put_be16(char *output, uint16_t nVal);
extern char *put_amf_string(char *c, const char *str);


int flv_parser_run(flv_tag_callback cb) {
    flv_tag_p tag = NULL;
    flv_read_header();
    
    uint32_t start_time = 0;
    long first_frame_time = -1;
    //the timestamp of the previous frame
    long pre_frame_time = 0;
    long lasttime = 0;
    int b_next_is_key = 1;
    
    int hasMetaDataParsed = 0;
    
    //jump over previousTagSizen
    fseek(g_infile, 4, SEEK_CUR);
    start_time = RTMP_GetTime();
    for (; ;) {
        uint32_t now = RTMP_GetTime();
        if (((now - start_time)
             < (pre_frame_time - first_frame_time)) && b_next_is_key) {
            //wait for 1 sec if the send process is too fast
            //this mechanism is not very good,need some improvement
            if (pre_frame_time > lasttime) {
                printf("Need to wait. TimeStamp:%8lu ms\n", pre_frame_time);
                lasttime = pre_frame_time;
            }
            sleep(1);
            continue;
        }
        
        tag = flv_read_tag(&b_next_is_key); // read the tag
        if (!tag) {
            return 0;
        }
        
        pre_frame_time = tag->timestamp;
        if (-1 == first_frame_time) {
            first_frame_time = pre_frame_time;
        }
        
        if (FLV_TAG_TYPE_SCRIPT == tag->tag_type) {
            hasMetaDataParsed = 1;
        }
        if (hasMetaDataParsed) {
            cb(tag);
        }
        
        flv_release_tag(tag);
    }
}

int flv_read_header(void) {
    int i = 0;
    flv_header_t *flv_header = NULL;

    flv_header = malloc(sizeof(flv_header_t));
    fread(flv_header, sizeof(flv_header_t), 1, g_infile);

    // XXX strncmp
    for (i = 0; i < strlen(flv_signature); i++) {
        assert(flv_header->signature[i] == flv_signature[i]);
    }

    flv_header->data_offset = ntohl(flv_header->data_offset);

    flv_print_header(flv_header);

    return 0;

}

flv_tag_p flv_read_tag(int *b_next_is_key) {
    uint32_t prev_tag_size = 0;
    flv_tag_p tag = (flv_tag_p)malloc(sizeof(flv_tag_t));
    
    if (feof(g_infile)) {
        return NULL;
    }
    // Start reading next tag
    fread_1(&(tag->tag_type));
    fread_3(&(tag->data_size));
    read_time(&(tag->timestamp));
    fread_3(&(tag->stream_id));
    
    printf("\n");
    printf("Prev tag size: %lu\n", (unsigned long) prev_tag_size);
    
    printf("Tag type: %u - Tag size: %u\n", tag->tag_type, tag->data_size);
    switch (tag->tag_type) {
        case FLV_TAG_TYPE_AUDIO:
            read_audio_tag(tag);
            break;
        case FLV_TAG_TYPE_VIDEO:
            read_video_tag(tag);
            break;
        case FLV_TAG_TYPE_SCRIPT:
            tag->data = malloc((size_t) tag->data_size);
            fread(tag->data, (size_t)tag->data_size, 1, g_infile);
            
            uint8_t *body = (uint8_t *)malloc(tag->data_size + 16);
            memset(body, 0, tag->data_size + 16);
            uint8_t *tmp_body = body;
            tmp_body = (uint8_t *)put_amf_string((char *)tmp_body, "@setDataFrame");
            memcpy(tmp_body, tag->data, tag->data_size);
            
            free(tag->data);
            tag->data = body;
            tag->data_size += 16;
            
            break;
        default:
            printf("Unknown tag type!\n");
            die();
    }
    
    fread_4(&prev_tag_size);
    
    uint8_t type = 0;
    if (1 == fread(&type, 1, 1, g_infile)) {
        fseek(g_infile, -1, SEEK_CUR);
        
        if (FLV_TAG_TYPE_VIDEO == type) {
            fseek(g_infile, 11, SEEK_CUR);
            fread(&type, 1, 1, g_infile);
            fseek(g_infile, -1, SEEK_CUR);
            
            if (type == 0x17) {
                *b_next_is_key = 1;
            } else {
                *b_next_is_key = 0;
            }
            
            fseek(g_infile, -11, SEEK_CUR);
        }
    }

    return tag;
}
