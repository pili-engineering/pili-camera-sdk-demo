/*
 * @file flv-parser.c
 * @author Akagi201
 * @date 2015/02/04
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "flv-parser.h"

#define HTONTIME(x) ((x>>16&0xff)|(x<<16&0xff0000)|(x&0xff00)|(x&0xff000000))

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
audio_tag_t *read_audio_tag(flv_tag_t *flv_tag) {
    assert(NULL != flv_tag);
    uint8_t byte = 0;
    audio_tag_t *tag = NULL;

    tag = malloc(sizeof(audio_tag_t));
    fread_1(&byte);

    tag->sound_format = flv_get_bits(byte, 4, 4);
    tag->sound_rate = flv_get_bits(byte, 2, 2);
    tag->sound_size = flv_get_bits(byte, 1, 1);
    tag->sound_type = flv_get_bits(byte, 0, 1);

    printf("  Audio tag:\n");
    printf("    Sound format: %u - %s\n", tag->sound_format, sound_formats[tag->sound_format]);
    printf("    Sound rate: %u - %s\n", tag->sound_rate, sound_rates[tag->sound_rate]);

    printf("    Sound size: %u - %s\n", tag->sound_size, sound_sizes[tag->sound_size]);
    printf("    Sound type: %u - %s\n", tag->sound_type, sound_types[tag->sound_type]);

    tag->data = malloc((size_t) flv_tag->data_size - 1);
    fread(tag->data, (size_t) flv_tag->data_size - 1, 1, g_infile); // -1: audo data tag header

    return tag;
}

/*
 * @brief read video tag
 */
video_tag_t *read_video_tag(flv_tag_t *flv_tag) {
    uint8_t byte = 0;
    video_tag_t *tag = NULL;

    tag = (video_tag_t *)malloc(sizeof(video_tag_t));

    fread_1(&byte);
    fseek(g_infile, -1, SEEK_CUR);

    tag->frame_type = flv_get_bits(byte, 4, 4);
    tag->codec_id = flv_get_bits(byte, 0, 4);

    printf("  Video tag:\n");
    printf("    Frame type: %u - %s\n", tag->frame_type, frame_types[tag->frame_type]);
    printf("    Codec ID: %u - %s\n", tag->codec_id, codec_ids[tag->codec_id]);

    // AVC-specific stuff
    if (tag->codec_id == FLV_CODEC_ID_AVC) {
        tag->data = read_avc_video_tag(tag, flv_tag, (uint32_t) (flv_tag->data_size));
    } else {
        tag->data = malloc((size_t) flv_tag->data_size);
        fread(tag->data, (size_t) flv_tag->data_size, 1, g_infile);
    }

    return tag;
}

/*
 * @brief read AVC video tag
 */
avc_video_tag_t *read_avc_video_tag(video_tag_t *video_tag, flv_tag_t *flv_tag, uint32_t data_size) {
    size_t count = 0;
    avc_video_tag_t *tag = NULL;

    tag = malloc(sizeof(avc_video_tag_t));

    count = fread_1(&(tag->frame_type));
    count += fread_1(&(tag->avc_packet_type));
    //count += fread_4s(&(tag->composition_time));
    count += fread_3(&(tag->composition_time));
    count += fread_4(&(tag->nalu_len));

    printf("    AVC video tag:\n");
    printf("    frame type: %x\n", tag->frame_type);
    printf("      AVC packet type: %u - %s\n", tag->avc_packet_type, avc_packet_types[tag->avc_packet_type]);
    printf("      AVC composition time: %i\n", tag->composition_time);
    printf("      AVC nalu length: %i\n", tag->nalu_len);

    tag->data = malloc((size_t) data_size - count);
    fread(tag->data, (size_t) data_size - count, 1, g_infile);

    flv_tag->data = malloc((size_t) data_size);
    fseek(g_infile, -data_size, SEEK_CUR);
    fread(flv_tag->data, (size_t) data_size, 1, g_infile);

    return tag;
}

void flv_parser_init(FILE *in_file) {
    g_infile = in_file;
}

extern char *put_be16(char *output, uint16_t nVal);
extern char *put_amf_string(char *c, const char *str);


int flv_parser_run(metadata_callback m_cb, video_callback v_cb, audio_callback a_cb) {
    flv_tag_t *tag = NULL;
    flv_read_header();
    pili_video_packet_p pili_video_packet = NULL;
    pili_audio_packet_p pili_audio_packet = NULL;
    pili_metadata_packet_p pili_metadata_packet = NULL;
    
    uint32_t start_time = 0;
    //the timestamp of the previous frame
    long pre_frame_time = 0;
    long lasttime = 0;
    int b_next_is_key = 1;
    
    //jump over previousTagSizen
    fseek(g_infile, 4, SEEK_CUR);
    start_time = RTMP_GetTime();
    for (; ;) {
        if (((RTMP_GetTime() - start_time)
             < (pre_frame_time)) && b_next_is_key) {
            //wait for 1 sec if the send process is too fast
            //this mechanism is not very good,need some improvement
            if (pre_frame_time > lasttime) {
                printf("TimeStamp:%8lu ms\n", pre_frame_time);
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
        
        if (TAGTYPE_VIDEODATA == tag->tag_type) {
            pili_video_packet_create(&pili_video_packet);
            pili_video_packet_init(pili_video_packet,
                    (uint8_t *)tag->data,
                    tag->data_size,
                    tag->timestamp, // TODO: + avc_video_tag->composition_time,
                    tag->timestamp,
                    (*(uint8_t *)tag->data >> 4 == 1) ? 1 : 0);
            
            if (v_cb) {
                v_cb(pili_video_packet);
            }
        } else if (TAGTYPE_SCRIPTDATAOBJECT == tag->tag_type) {
            uint8_t *body = (uint8_t *)malloc(tag->data_size + 16);
            memset(body, 0, tag->data_size + 16);
            uint8_t *tmp_body = body;
            tmp_body = (uint8_t *)put_amf_string((char *)tmp_body, "@setDataFrame");
            memcpy(tmp_body, tag->data, tag->data_size);
            
            pili_metadata_packet_create(&pili_metadata_packet);
            pili_metadata_packet_init(pili_metadata_packet,
                                      tag->data_size + 16,
                                      tag->timestamp,
                                      tag->stream_id,
                                      body);
            
            if (m_cb) {
                m_cb(pili_metadata_packet);
            }
        } else if (TAGTYPE_AUDIODATA == tag->tag_type) {
            pili_audio_packet_create(&pili_audio_packet);
            pili_audio_packet_init(pili_audio_packet,
                                   tag->data,
                                   tag->data_size,
                                   tag->timestamp);
            
            if (a_cb) {
                a_cb(pili_audio_packet);
            }
        }
#warning 之后需要额外释放 tag，debug 时先忽略此处内存问题
//        flv_free_tag(tag); // and free flv tag
    }
}

void flv_free_tag(flv_tag_t *tag) {
    audio_tag_t *audio_tag;
    video_tag_t *video_tag;
    avc_video_tag_t *avc_video_tag;

    if (tag->tag_type == TAGTYPE_VIDEODATA) {
        video_tag = (video_tag_t *) tag->data;
        if (video_tag->codec_id == FLV_CODEC_ID_AVC) {
            avc_video_tag = (avc_video_tag_t *) video_tag->data;
            free(avc_video_tag->data);
            free(video_tag->data);
            free(tag->data);
            free(tag);
        } else {
            free(video_tag->data);
            free(tag->data);
            free(tag);
        }
    } else if (tag->tag_type == TAGTYPE_AUDIODATA) {
        audio_tag = (audio_tag_t *) tag->data;
        free(audio_tag->data);
        free(tag->data);
        free(tag);
    } else {
        free(tag->data);
        free(tag);
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

void print_general_tag_info(flv_tag_t *tag) {
    assert(NULL != tag);
    printf("  Data size: %lu\n", (unsigned long) tag->data_size);
    printf("  Timestamp: %lu\n", (unsigned long) tag->timestamp);
    printf("  StreamID: %lu\n", (unsigned long) tag->stream_id);

    return;
}

flv_tag_t *flv_read_tag(int *b_next_is_key) {
    uint32_t prev_tag_size = 0;
    flv_tag_t *tag = NULL;

    tag = malloc(sizeof(flv_tag_t));

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
        case TAGTYPE_AUDIODATA:
            printf("Audio data\n");
            print_general_tag_info(tag);
            tag->data = malloc((size_t)tag->data_size);
            fread(tag->data, (size_t)tag->data_size, 1, g_infile);
            break;
        case TAGTYPE_VIDEODATA:
            printf("Video data\n");
            print_general_tag_info(tag);
            tag->data = malloc((size_t)tag->data_size);
            fread(tag->data, (size_t)tag->data_size, 1, g_infile);
            break;
        case TAGTYPE_SCRIPTDATAOBJECT:
            printf("Script data object\n");
            print_general_tag_info(tag);
            tag->data = malloc((size_t) tag->data_size);
            fread(tag->data, (size_t)tag->data_size, 1, g_infile);
            break;
        default:
            printf("Unknown tag type!\n");
            die();
    }
    
    fread_4(&prev_tag_size);
    
    uint8_t type = 0;
    if (1 == fread(&type, 1, 1, g_infile)) {
        fseek(g_infile, -1, SEEK_CUR);
        
        if (0x09 == type) {
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

#if 0
    // Did we reach end of file?
    if (feof(g_infile)) {
        return NULL;
    }
    #endif

    return tag;
}
