/**
 * author: jakob ammerer
 * LB-OKUE 04 b)
 * Angabe: 
    passen Sie Ihr Programm aus a) derart an, dass anstatt der Kodierungs-
    delays der einzelnen Frames eine Statistik ausgegeben wird, die die durch-
    schnittlichen und maximalen Kodierungsdelays in Millisekunden ausgibt.
    Die Kodierungsdelays sollen dabei pro Frametyp (I, P, B) aufgeschl¨usselt
    werden. Verwenden Sie zur Frametypbestimmung die libx264 -Makros
    IS X264 TYPE I und IS X264 TYPE B
 
 * Opaque = opaque is a void * Field in x264_picture_t. => so to say: just throw any pointer in it idc 
 * 
 * Codebase from  MTMa.c)
 * 
 * what changed?: 
 *  EncodeFrame, FlushEncoder => instead of prints now we add the stats.  
 *  New functions => UpdateStats, PrintStats
 *  added FrameStats struct.
 * 
 * 
 * Sources:
 *   https://en.cppreference.com/w/c/chrono/clock (clock_gettime / CLOCK_MONOTONIC)
 * 
 * AI-USAGE Claude Sonnet 4.6 and duckduckgo ai chat:  
 *  formatting of code, formatting and grammar / spelling fixes in comments.  
 *  proper Makefile
 *  to explain and resolve errors i had during coding / debugging.  
 *  also used to format print statements so they are readable
 *  Line 219 (see comment)
 *  Line 256 (see comment)
 * 
 * EXAMPLE CALL: ./OKUEa.exe foreman.yuv 352 288 out.h264
 */

#define _POSIX_C_SOURCE 199309L // required for clock_gettime + CLOCK_MONOTONIC

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <x264.h>

// Struct for the frame st
typedef struct {
    int count;
    double sum;
    double max;
} FrameStats;

FILE *OpenFile(const char *path, const char *mode);
x264_t *InitEncoder(int width, int height);
void WriteHeaders(x264_t *encoder, FILE *outFile);
uint8_t *AllocFrameBuffer(int frameSize);
void FillPicture(x264_picture_t *pic, const uint8_t *rawFrame, int lumaSize, int chromaSize);

void EncodeFrame(x264_t *encoder, x264_picture_t *picIn, x264_picture_t *picOut, FILE *outFile, FrameStats *statsI, FrameStats *statsP, FrameStats *statsB);
void FlushEncoder(x264_t *encoder, x264_picture_t *picOut, FILE *outFile, FrameStats *statsI, FrameStats *statsP, FrameStats *statsB);

void WriteNals(x264_nal_t *nals, int nalCount, FILE *outFile);
void FreeAll(uint8_t *rawFrame, x264_picture_t *picIn, x264_t *encoder, FILE *inFile, FILE *outFile);
double CalcDelayMs(struct timespec *start); // elapsed ms from start to now
void UpdateStats(FrameStats *stats, double delay);
void PrintStats(const char *type, FrameStats *stats);


int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <input.yuv> <width> <height> <output.h264>\n", argv[0]);
        return 1;
    }

    const char *inputPath = argv[1];
    int width = atoi(argv[2]);
    int height = atoi(argv[3]);
    const char *outputPath = argv[4];

    FILE *inFile = OpenFile(inputPath, "rb");
    FILE *outFile = OpenFile(outputPath, "wb");

    int lumaSize = width * height; // Y plane
    int chromaSize = (width / 2) * (height / 2); // U and V plane (4:2:0)
    int frameSize = lumaSize + 2 * chromaSize; // one IYUV frame in bytes

    x264_t *encoder = InitEncoder(width, height);
    WriteHeaders(encoder, outFile);

    x264_picture_t picIn;
    x264_picture_t picOut;
    x264_picture_alloc(&picIn, X264_CSP_I420, width, height);

    uint8_t *rawFrame = AllocFrameBuffer(frameSize);
    int frameNum = 0;

    FrameStats statsI = {0, 0.0, 0.0};
    FrameStats statsP = {0, 0.0, 0.0};
    FrameStats statsB = {0, 0.0, 0.0};

    while (fread(rawFrame, 1, frameSize, inFile) == (size_t)frameSize) {

        FillPicture(&picIn, rawFrame, lumaSize, chromaSize);

        picIn.i_pts = frameNum++;

        struct timespec *ts = malloc(sizeof(struct timespec)); // heap: survives B-frame buffering

        clock_gettime(CLOCK_MONOTONIC, ts); // timestamp before encode

        picIn.opaque = ts; // x264 passes this through to picOut unchanged

        EncodeFrame(encoder, &picIn, &picOut, outFile, &statsI, &statsP, &statsB);
    }

    FlushEncoder(encoder, &picOut, outFile, &statsI, &statsP, &statsB);
    fprintf(stderr, "encoded %d frames\n", frameNum);

    printf("\n--- Kodierungsdelay Statistik ---\n");
    printf("%-6s %6s %10s %10s\n", "Typ", "Frames", "Avg (ms)", "Max (ms)");
    PrintStats("I", &statsI);
    PrintStats("P", &statsP);
    PrintStats("B", &statsB);

    FreeAll(rawFrame, &picIn, encoder, inFile, outFile);
    return 0;
}

double CalcDelayMs(struct timespec *start) { 
    // elapsed ms between *start and now
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double sec = (double)(now.tv_sec - start->tv_sec);
    double nsec = (double)(now.tv_nsec - start->tv_nsec);
    return sec * 1000.0 + nsec / 1.0e6; // convert to ms
}

void UpdateStats(FrameStats *stats, double delay) {
    stats->count++;
    stats->sum += delay;
    if (delay > stats->max) {
        stats->max = delay;
    }
}

void PrintStats(const char *type, FrameStats *stats) {
    if (stats->count == 0) {
        return; // skip which did not apper
    }
    printf("%-6s %6d %10.3f %10.3f\n", type, stats->count, stats->sum / stats->count, stats->max);
}

FILE *OpenFile(const char *path, const char *mode) {
    FILE *file = fopen(path, mode);
    if (!file) {
        fprintf(stderr, "error opening file: %s\n", path);
        exit(1);
    }
    return file;
}

x264_t *InitEncoder(int width, int height) {
    x264_param_t param;
    x264_param_default_preset(&param, "medium", NULL);

    param.i_width = width;
    param.i_height = height;
    param.i_csp = X264_CSP_I420;
    param.i_fps_num = 25;
    param.i_fps_den = 1;
    param.b_annexb = 1;
    param.i_log_level = X264_LOG_NONE; // silence x264 so delay output is readable

    x264_param_apply_profile(&param, "high");

    x264_t *encoder = x264_encoder_open(&param);
    if (!encoder) {
        fprintf(stderr, "error opening x264 encoder\n");
        exit(1);
    }
    return encoder;
}

void WriteHeaders(x264_t *encoder, FILE *outFile) {
    x264_nal_t *nals;
    int nalCount;
    x264_encoder_headers(encoder, &nals, &nalCount);
    WriteNals(nals, nalCount, outFile);
}

uint8_t *AllocFrameBuffer(int frameSize) {
    uint8_t *buffer = malloc(frameSize);
    if (!buffer) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }
    return buffer;
}

void FillPicture(x264_picture_t *pic, const uint8_t *rawFrame, int lumaSize, int chromaSize) {
    memcpy(pic->img.plane[0], rawFrame, lumaSize);                   // Y
    memcpy(pic->img.plane[1], rawFrame + lumaSize, chromaSize);      // U
    memcpy(pic->img.plane[2], rawFrame + lumaSize + chromaSize, chromaSize); // V
}

void EncodeFrame(x264_t *encoder, x264_picture_t *picIn, x264_picture_t *picOut, FILE *outFile, FrameStats *statsI, FrameStats *statsP, FrameStats *statsB) {
    x264_nal_t *nals;
    int nalCount;

    int result = x264_encoder_encode(encoder, &nals, &nalCount, picIn, picOut);

    if (result < 0) {
        fprintf(stderr, "encoding error\n");
        exit(1);
    }

    if (nalCount > 0 && picOut->opaque != NULL) {
        
        struct timespec *ts = (struct timespec *)picOut->opaque;
        double delay = CalcDelayMs(ts);
        
        // AI-Usage: IS_X264_TYPE_I catches both X264_TYPE_I and X264_TYPE_IDR.
        // direct compare against X264_TYPE_IDR alone would miss non-IDR I-frames.
        // same for IS_X264_TYPE_B => covers all B-frame variants.

        if (IS_X264_TYPE_I(picOut->i_type)) {
            UpdateStats(statsI, delay);
        } else if (IS_X264_TYPE_B(picOut->i_type)) {
            UpdateStats(statsB, delay);
        } else {
            UpdateStats(statsP, delay);
        }
        free(ts);
    }

    WriteNals(nals, nalCount, outFile);
}

void FlushEncoder(x264_t *encoder, x264_picture_t *picOut, FILE *outFile, FrameStats *statsI, FrameStats *statsP, FrameStats *statsB) {
    x264_nal_t *nals;
    int nalCount;

    while (x264_encoder_delayed_frames(encoder)) {
        
        int result = x264_encoder_encode(encoder, &nals, &nalCount, NULL, picOut);        
        if (result < 0) {
            break;
        }

        if (nalCount > 0 && picOut->opaque != NULL) {
            
            struct timespec *ts = (struct timespec *)picOut->opaque;
            
            double delay = CalcDelayMs(ts);
            
            // AI-Usage: IS_X264_TYPE_I catches both X264_TYPE_I and X264_TYPE_IDR.
            // direct compare against X264_TYPE_IDR alone would miss non-IDR I-frames.
            // same for IS_X264_TYPE_B => covers all B-frame variants.
            if (IS_X264_TYPE_I(picOut->i_type)) {
                UpdateStats(statsI, delay);
            } else if (IS_X264_TYPE_B(picOut->i_type)) {            
                UpdateStats(statsB, delay);
            } else {
                UpdateStats(statsP, delay);
            }
            free(ts);
        }
        WriteNals(nals, nalCount, outFile);
    }
}


void WriteNals(x264_nal_t *nals, int nalCount, FILE *outFile) {
    for (int i = 0; i < nalCount; i++) {
        fwrite(nals[i].p_payload, 1, nals[i].i_payload, outFile);
    }
}

void FreeAll(uint8_t *rawFrame, x264_picture_t *picIn, x264_t *encoder, FILE *inFile, FILE *outFile) {
    free(rawFrame);
    x264_picture_clean(picIn);
    x264_encoder_close(encoder);
    fclose(inFile);
    fclose(outFile);
}