/*
    LB-VK 03a) read IYUV sequence, verify pixel values
    Author: Ammerer Jakob
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> // for uint8_t

// YCbCr 4:2:0, Y at full res, Cb/Cr at half res (three separate channels)
typedef struct {
    uint8_t *Y;
    uint8_t *Cb;
    uint8_t *Cr;
} Frame;

// prototypes
Frame *allocFrame(int width, int height);   // allocate memory for one frame
void freeFrame(Frame *frame);               // free all three planes + the frame itself
Frame **readYuv(const char *path, int width, int height, int *frameCount); // open file, read all frames into array

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <path> <width> <height>\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];

    // converts arg string to int: https://www.geeksforgeeks.org/atoi-function-in-c/
    int width = atoi(argv[2]);
    int height = atoi(argv[3]);

    // 4:2:0 divides width and height by 2, so both must be even
    if (width <= 0 || height <= 0 || width % 2 != 0 || height % 2 != 0) {
        fprintf(stderr, "width and height must be positive and even\n");
        return 1;
    }

    int frameCount = 0;
    Frame **frames = readYuv(path, width, height, &frameCount); // ** = pointer to pointer (array of Frame pointers)
    if (frames == NULL) {
        return 1;
    }

    printf("frames: %d\tres: %dx%d\n", frameCount, width, height);

    // verification: avoid frame 0, row 0, col 0 (as said in task a)
    // printing the file offset so the values can be cross-checked in ghex
    int verifyFrame = 1;
    int verifyRow = 10;
    int verifyCol = 15;

    if (frameCount <= verifyFrame) {
        fprintf(stderr, "not enough frames for verification\n");
    } else {
        size_t ySize = (size_t)width * height;
        size_t chromaSize = (size_t)(width / 2) * (height / 2); // Cb and Cr are each (width/2)*(height/2) because of 4:2:0
        size_t frameSize = ySize + 2 * chromaSize;

        // AI-Usage: correct byte offset formulas for Y, Cb and Cr in raw yuv, Claude Sonnet 4.6
        long yOffset = (long)verifyFrame * (long)frameSize + verifyRow * width + verifyCol;
        long cbOffset = (long)verifyFrame * (long)frameSize + (long)ySize + (verifyRow / 2) * (width / 2) + verifyCol / 2;
        long crOffset = cbOffset + (long)chromaSize;

        uint8_t yVal = frames[verifyFrame]->Y[verifyRow * width + verifyCol]; // 2D index as 1D: row * width + col // -> same as (*frame).Y
        uint8_t cbVal = frames[verifyFrame]->Cb[(verifyRow / 2) * (width / 2) + verifyCol / 2]; // chroma coords halved
        uint8_t crVal = frames[verifyFrame]->Cr[(verifyRow / 2) * (width / 2) + verifyCol / 2];

        printf("frame=%d row=%d col=%d\n", verifyFrame, verifyRow, verifyCol);
        printf("Y\t%d\t0x%lX\n", yVal, yOffset);
        printf("Cb\t%d\t0x%lX\n", cbVal, cbOffset);
        printf("Cr\t%d\t0x%lX\n", crVal, crOffset);
    }

    for (int i = 0; i < frameCount; i++) {
        freeFrame(frames[i]);
    }
    free(frames);

    return 0;
}

// Y at full resolution, Cb/Cr at half resolution per dimension because of 4:2:0
Frame *allocFrame(int width, int height) {
    Frame *frame = malloc(sizeof(Frame));
    if (frame == NULL) {
        return NULL;
    }

    frame->Y = malloc((size_t)width * height);
    frame->Cb = malloc((size_t)(width / 2) * (height / 2));
    frame->Cr = malloc((size_t)(width / 2) * (height / 2));

    if (frame->Y == NULL || frame->Cb == NULL || frame->Cr == NULL) {
        return NULL;
    }

    return frame;
}

void freeFrame(Frame *frame) {
    free(frame->Y);
    free(frame->Cb);
    free(frame->Cr);
    free(frame);
}

// open yuv file, calculate frame count from file size, read all frames into array
// caller must free each frame and the array itself
// returns NULL on error
Frame **readYuv(const char *path, int width, int height, int *frameCount) {
    FILE *file = fopen(path, "rb"); // rb = read binary, without b Windows misinterprets line endings
    if (file == NULL) {
        fprintf(stderr, "cannot open: %s\n", path);
        return NULL;
    }

    size_t ySize = (size_t)width * height;
    size_t chromaSize = (size_t)(width / 2) * (height / 2);
    size_t frameSize = ySize + 2 * chromaSize;

    fseek(file, 0, SEEK_END); // jump to end of file
    long fileSize = ftell(file); // read current position = file size in bytes
    fseek(file, 0, SEEK_SET); // jump back to start

    *frameCount = (int)((long)fileSize / (long)frameSize); // total bytes / bytes per frame
    if (*frameCount == 0) {
        fprintf(stderr, "file too small for one frame\n");
        fclose(file);
        return NULL;
    }

    Frame **frames = malloc((size_t)*frameCount * sizeof(Frame *)); // array of pointers, one per frame
    if (frames == NULL) {
        fclose(file);
        return NULL;
    }

    // read Y, then Cb, then Cr per frame - planar order matches IYUV format
    for (int i = 0; i < *frameCount; i++) {
        frames[i] = allocFrame(width, height);
        if (frames[i] == NULL) {
            fclose(file);
            return NULL;
        }
        fread(frames[i]->Y, 1, ySize, file);
        fread(frames[i]->Cb, 1, chromaSize, file);
        fread(frames[i]->Cr, 1, chromaSize, file);
    }

    fclose(file);
    return frames;
}
