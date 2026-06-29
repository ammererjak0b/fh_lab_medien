/**
 * author: jakob ammerer
 * LB-OKUE 04 a)
 * Angabe:
    Schreiben Sie auf Basis von LB-MTM 04. a) ein Programm, das den
    Kodierungsdelay jedes Videoframes misst und in Millisekunden auf der
    Konsole ausgibt. Nutzen Sie das opaque-Feld von x264 picture t, um
    jedem Videoframe einen Zeitstempel der aktuellen Uhrzeit zuzuordnen.
    Dieses Feld wird von der libx264 durchgereicht und kann aus dem da-
    zugeh¨origen kodierten Frame wieder ausgelesen werden. Aus der Diffe-
    renz zwischen der aktuellen Uhrzeit nach der Kodierung und dem aus-
    gelesenen urspr¨unglichen Zeitstempel kann der Kodierungsdelay ermittelt
    werden. Nutzen Sie zur Ermittlung der aktuellen Uhrzeit die Funktion
    clock gettime mit dem Parameter CLOCK MONOTONIC (vgl. Dokumenta-
    tion unter https://en.cppreference.com/w/c/chrono/clock).
 * 
 * Opaque = opaque is a void * Field in x264_picture_t. => so to say: just throw any pointer in it idc 
 * 
 * Codebase from  MTMa.c)
 * 
 * what is the same?:    
 *  OpenFile, WriteHeaders, AllocFrameBuffer, FillPicture, WriteNals, FreeAll are identical
 *  main structure: Args handeling, FrameLoop, Flush
 *  InitEncoder = almost identical
 *
 * what changed?:
 *  #define _POSIX_C_SOURCE and #include <time.h> 
 *  CalcDelayMs() new function
 *  InitEncoder: i_log_level = X264_LOG_NONE instead of INFO now delay output is readable.
 *  FrameLoop changes: malloc + clock_gettime + picIn.opaque = ts 
 *  EncodeFrame / FlushEncoder: opaque read, Delay calc, output, free 
 * 
 * Sources:
 *   https://en.cppreference.com/w/c/chrono/clock (clock_gettime / CLOCK_MONOTONIC)
 * 
 * AI-USAGE Claude Sonnet 4.6 and duckduckgo ai chat:  
 *  formatting of code, formatting and grammar / spelling fixes in comments.  
 *  proper Makefile
 *  to explain and resolve errors i had during coding / debugging.  
 *  also used to format print statements so they are readable
 *  Line 99  (comment)
 *  Line 104 (comment)
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

FILE *OpenFile(const char *path, const char *mode);
x264_t *InitEncoder(int width, int height);
void WriteHeaders(x264_t *encoder, FILE *outFile);
uint8_t *AllocFrameBuffer(int frameSize);
void FillPicture(x264_picture_t *pic, const uint8_t *rawFrame, int lumaSize, int chromaSize);
void EncodeFrame(x264_t *encoder, x264_picture_t *picIn, x264_picture_t *picOut, FILE *outFile);
void FlushEncoder(x264_t *encoder, x264_picture_t *picOut, FILE *outFile);
void WriteNals(x264_nal_t *nals, int nalCount, FILE *outFile);
void FreeAll(uint8_t *rawFrame, x264_picture_t *picIn, x264_t *encoder, FILE *inFile, FILE *outFile);
double CalcDelayMs(struct timespec *start); // elapsed ms from start to now

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <input.yuv> <width> <height> <output.h264>\n", argv[0]);
        return 1;
    }

    const char *inputPath = argv[1];
    int width = atoi(argv[2]);
    int height = atoi(argv[3]);
    const char *outputPath = argv[4];

    FILE *inFile = OpenFile(inputPath, "rb"); // readbinary 
    FILE *outFile = OpenFile(outputPath, "wb"); // writebinary

    int lumaSize = width * height; // Y plane
    int chromaSize = (width / 2) * (height / 2); // U and V plane (4:2:0)
    int frameSize = lumaSize + 2 * chromaSize; // one YUV frame in bytes +2 for chromaplanes

    x264_t *encoder = InitEncoder(width, height);
    WriteHeaders(encoder, outFile);

    x264_picture_t picIn;
    x264_picture_t picOut;
    x264_picture_alloc(&picIn, X264_CSP_I420, width, height);

    uint8_t *rawFrame = AllocFrameBuffer(frameSize);
    int frameNum = 0;

    while (fread(rawFrame, 1, frameSize, inFile) == (size_t)frameSize) {
        FillPicture(&picIn, rawFrame, lumaSize, chromaSize);
        picIn.i_pts = frameNum++;

        // AI-USAGE: heap alloc => x264 buffers B-frames internally, frame submitted at T
        // may not be output until T+N. stack ts would be invalid by then because when EncodeFrame returns => stack frame is gone.
        struct timespec *ts = malloc(sizeof(struct timespec));
        clock_gettime(CLOCK_MONOTONIC, ts); // capture time before encode

        // AI-Usage: opaque is a void* in x264_picture_t, x264 never reads or modifies it,
        // passes it 1:1 from picIn to picOut => only way to attach per-frame data without touching x264 internals
        picIn.opaque = ts; // attach to frame, x264 passes through 

        EncodeFrame(encoder, &picIn, &picOut, outFile);
    }

    FlushEncoder(encoder, &picOut, outFile);
    fprintf(stderr, "encoded %d frames\n", frameNum);
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

void EncodeFrame(x264_t *encoder, x264_picture_t *picIn, x264_picture_t *picOut, FILE *outFile) {
    x264_nal_t *nals;
    int nalCount;

    int result = x264_encoder_encode(encoder, &nals, &nalCount, picIn, picOut);
    if (result < 0) {
        fprintf(stderr, "encoding error\n");
        exit(1);
    }

    if (nalCount > 0 && picOut->opaque != NULL) {
        // opaque holds the timestamp from the input frame that produced this output
        struct timespec *ts = (struct timespec *)picOut->opaque;
        double delay = CalcDelayMs(ts);
        char type = picOut->i_type == X264_TYPE_IDR ? 'I' : 'P';
        printf("frame %4lld: delay = %7.3f ms  [%c]\n", (long long)picOut->i_pts, delay, type);
        free(ts); // done with this timestamp
    }

    WriteNals(nals, nalCount, outFile);
}

void FlushEncoder(x264_t *encoder, x264_picture_t *picOut, FILE *outFile) {
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
            char type = picOut->i_type == X264_TYPE_IDR ? 'I' : 'P';
            printf("frame %4lld: delay = %7.3f ms  [%c]\n", (long long)picOut->i_pts, delay, type);
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