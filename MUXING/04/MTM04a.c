
/**
 * author: jakob ammerer
 * LB-MTM04 a)
 *
 * Sources:
 * https://github.com/mirror/x264/blob/master/example.c
 * Wenn ich mir diesen beispiel Code aus dem Repo so ansehe, weiss ich nicht wirklich was ich davon halten sollte.
 * Meine Frage hierzu wäre (wenn ihnen langweilig ist und sie die zeit dafür im Labor aufwenden wollen):
 * schreibt man so effizienten C Code? Es sieht sehr effizient aus, was ich so gelesen habe sind gotos auch sehr performant,
 * da der Code nicht noch einmal kompiliert wird.
 * Mir wurde immer von gotos abgeraten, da sie als "unsicher" gelten und es es "grauenhafter" code sei und ich hab das nie wirklich hinterfragt.
 * Wo macht man den "cut" zwischen effizienten Code und guten, leserlichen Code?
 *
 * CLAUDE Sonnet 4.6 für: schönen Konsolenoutput, formtierung vom code, initiale idee für die durchführung, hilfe bei errors die ich hatte.
 *
 * EXAMPLE CALL: ./LBMTM04a.exe foreman.yuv 352 288 foremanAsH264.h264
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <x264.h>

// opens file in selected mode, abort if there is an error
FILE *openFile(const char *path, const char *mode);

// init x264 encoder with params
x264_t *initEncoder(int width, int height);

// writes SPS/PPS Header to outputfile
void writeHeaders(x264_t *encoder, FILE * outFile);

// allocates raw data buffer for a IYUV frame
uint8_t *allocFrameBuffer(int frameSize); 

// copies Y U V planes from raw frame into a x264 picture. 
void fillPicture(x264_picture_t *pic, const uint8_t *rawFrame, int lumaSize, int chromeSize);

// encodes frame und write NALs in output file
void encodeFrame(x264_t *encoder, x264_picture_t *picIn, x264_picture_t *picOut, FILE *outFile);

// flush bufferd B frames at the end. (after last input frame) 
void flushEncoder(x264_t *encoder, x264_picture_t *picOut, FILE *outFile);

// write all NAL units from last Encode call into output file 
void writeNals(x264_nal_t *nals, int nalCount, FILE *outFile);

// free all allocated things
void freeAll(uint8_t *rawFrame, x264_picture_t *picIn, x264_t *encoder, FILE *inFile, FILE *outFile);

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <input.iyuv> <width> <height> <output.h264>\n", argv[0]);
        return 1;
    }
    const char *inputPath = argv[1]; // path to input.yuv
    int width = atoi(argv[2]);
    int height = atoi(argv[3]);
    const char *outputPath = argv[4]; // path to output

    FILE *inFile = openFile(inputPath, "rb"); // open file with read binary
    FILE *outFile = openFile(outputPath, "wb"); // open file with write binary

    int lumaSize = width * height; // Y plane full res
    int chromaSize = (width / 2) * (height / 2); // U and V Plane (4:2:0)
    int frameSize = lumaSize + 2 * chromaSize; // one IYUV frame in byte

    x264_t *encoder = initEncoder(width, height); // configure and open encoder
    writeHeaders(encoder, outFile); // SPS PPS into outputfile

    x264_picture_t picIn; // input picture for encoder
    x264_picture_t picOut; // output picture after encoding
    x264_picture_alloc(&picIn, X264_CSP_I420, width, height); //create internal memory buffers inside picIn for each plane. Size according to width and height. So the struct is ready to receive the pixel data

    uint8_t *rawFrame = allocFrameBuffer(frameSize); // alloc buffer for one raw iyuv frame
    int frameNum = 0; //frame count for PTS

    // this is the frame loop: reads until no frame is left
    while (fread(rawFrame, 1, frameSize, inFile) == (size_t)frameSize) {
        fillPicture(&picIn, rawFrame, lumaSize, chromaSize); // copies rawdate in picture plane
        picIn.i_pts = frameNum++; // set PTS then increase counter
        encodeFrame(encoder, &picIn, &picOut, outFile); // encode and write frame
    }

    flushEncoder(encoder, &picOut, outFile);
    fprintf(stderr, "encoded %d frames\n", frameNum);
    freeAll(rawFrame, &picIn, encoder, inFile, outFile);

    return 0;
}

FILE *openFile(const char *path, const char *mode) {
   FILE *file = fopen(path, mode);
    if (!file) { // if not null
        fprintf(stderr, "error when opening file: %s\n", path);
        exit(1); // break application
    }
    return file;
}

// init x264 encoder. this is partially inspired by this file in github: https://github.com/mirror/x264/blob/master/example.c
x264_t *initEncoder(int width, int height) {
    x264_param_t param;
    x264_param_default_preset(&param, "medium", NULL); // load default preset

    param.i_width = width; // set pic width
    param.i_height = height; // set pic height
    param.i_csp = X264_CSP_I420; // set colorspace to: I420 which equals IYUV
    param.i_fps_num = 25; // set framerate numerator count to 25
    param.i_fps_den = 1; // set denumarator to 1
    param.b_annexb = 1; // Annex B startcode for rwa h264
    param.i_log_level = X264_LOG_INFO; // log level info for information.

    x264_param_apply_profile(&param, "high"); // apply high profile here.

    x264_t *encoder = x264_encoder_open(&param); // open encoder with params
    if (!encoder) { // if null => init error happend
        fprintf(stderr, "error opening x264 encoder\n");
        exit(1);
    }
    return encoder; // return encoder handle here
}

void writeHeaders(x264_t *encoder, FILE *outFile) {
    x264_nal_t *nals; // pointer which points to NAL array
    int nalCount;
    x264_encoder_headers(encoder, &nals, &nalCount); // generate sps pps
    writeNals(nals, nalCount, outFile);
}

// allocates heap memory for a IYUV frame
uint8_t *allocFrameBuffer(int frameSize) {
    uint8_t *buffer = malloc(frameSize); //allocate framesize bytes
    if (!buffer) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }
    return buffer;
}

// converts plane Y U V from rawFrame into the x264 picture structure
void fillPicture(x264_picture_t *pic, const uint8_t *rawFrame, int lumaSize, int chromaSize) {
    memcpy(pic->img.plane[0], rawFrame, lumaSize); //Y plane full res
    memcpy(pic->img.plane[1], rawFrame + lumaSize, chromaSize); // U plane
    memcpy(pic->img.plane[2], rawFrame + lumaSize + chromaSize, chromaSize); // V plane
}

// encodes each frame and write the NALs into outFile
void encodeFrame(x264_t *encoder, x264_picture_t *picIn, x264_picture_t *picOut, FILE *outFile) {
    x264_nal_t *nals; // pointer to created NAL unit
    int nalCount;
    int result = x264_encoder_encode(encoder, &nals, &nalCount, picIn, picOut); // encode Frame
    if (result < 0) { // negative return = error
        fprintf(stderr, "encoding error\n");
        exit(1);
    }
    writeNals(nals, nalCount, outFile); // encoded NALs into outfile
}

// flushes all buffered flames, which are still in the encoder (B-frames e.g)
void flushEncoder(x264_t *encoder, x264_picture_t *picOut, FILE *outFile) {
    x264_nal_t *nals;
    int nalCount;
    while (x264_encoder_delayed_frames(encoder)) { // runs unitil there are no more frames in buffer
        int result = x264_encoder_encode(encoder, &nals, &nalCount, NULL, picOut); //NULL here is like saying no new input here
        if (result < 0) {
            break;
        }
        writeNals(nals, nalCount, outFile); // write flushed NALs#
    }
}

void writeNals(x264_nal_t *nals, int nalCount, FILE *outFile) {
    for (int i = 0; i < nalCount; i++) { // foreach NAL unit
        fwrite(nals[i].p_payload, 1, nals[i].i_payload, outFile); // writes encoded video data byte per byte
    }
}

// free all
void freeAll(uint8_t *rawFrame, x264_picture_t *picIn, x264_t *encoder, FILE *inFile, FILE *outFile) {
    free(rawFrame);
    x264_picture_clean(picIn);
    x264_encoder_close(encoder);
    fclose(inFile);
    fclose(outFile);
}
