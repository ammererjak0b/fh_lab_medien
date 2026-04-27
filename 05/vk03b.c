/*
    LB-VK 03b) extend 03a with motion estimation using SAD
    Author: Ammerer Jakob
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> // for uint8_t
#include <limits.h> // for INT_MAX

#define BLOCK_SIZE 16 // 16x16 block (from Task)
#define SEARCH_RANGE 8 // + - 8px search range, 16x16 block that shifts within a 32x32 area in the reference frame

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
int computeSad(const Frame *currentFrame, const Frame *refFrame, int blockX, int blockY, int refX, int refY, int width); // sum of absolute differences over 16x16 Y block
void printResidual(const Frame *currentFrame, const Frame *refFrame, int blockX, int blockY, int refX, int refY, int width); // prints current - reference per pixel

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

    // a) verification: avoid frame 0, row 0, col 0 as per task
    // printing the file offset so the values can be cross-checked in ghex
    int verifyFrame = 1;
    int verifyRow = 10;
    int verifyCol = 15;

    if (frameCount <= verifyFrame) {
        fprintf(stderr, "not enough frames for verification\n");
    } else {
        size_t ySize = (size_t)width * height;
        size_t chromaSize = (size_t)(width / 2) * (height / 2);
        size_t frameSize = ySize + 2 * chromaSize;

        // AI-Usage: derive correct byte offset formulas for Y, Cb and Cr in raw IYUV, Claude Sonnet 4.6
        long yOffset = (long)verifyFrame * (long)frameSize + verifyRow * width + verifyCol;
        long cbOffset = (long)verifyFrame * (long)frameSize + (long)ySize + (verifyRow / 2) * (width / 2) + verifyCol / 2;
        long crOffset = cbOffset + (long)chromaSize;

        uint8_t yVal = frames[verifyFrame]->Y[verifyRow * width + verifyCol]; // 2D index as 1D: row * width + col //same as (*frame).Y
        uint8_t cbVal = frames[verifyFrame]->Cb[(verifyRow / 2) * (width / 2) + verifyCol / 2]; // chroma coords halved
        uint8_t crVal = frames[verifyFrame]->Cr[(verifyRow / 2) * (width / 2) + verifyCol / 2];

        printf("frame=%d row=%d col=%d\n", verifyFrame, verifyRow, verifyCol);
        printf("Y\t%d\t0x%lX\n", yVal, yOffset);
        printf("Cb\t%d\t0x%lX\n", cbVal, cbOffset);
        printf("Cr\t%d\t0x%lX\n", crVal, crOffset);
    }

    // b) motion estimation
    // frame 10 is index 9 (current), frame 9 is index 8 (reference)
    int currentFrameIndex = 9;
    int referenceFrameIndex = 8;

    if (frameCount <= currentFrameIndex) {
        fprintf(stderr, "at least 10 frames for ME\n");
        for (int i = 0; i < frameCount; i++) {
            freeFrame(frames[i]);
        }
        free(frames);
        return 1;
    }

    // top-left of 16x16 block at frame center: center minus half block size
    int blockX = width / 2 - BLOCK_SIZE / 2;
    int blockY = height / 2 - BLOCK_SIZE / 2;

    int bestDx = 0;
    int bestDy = 0;
    int minSad = INT_MAX; // start with max so first real SAD is always smaller

    // AI-Usage: loop structure for full search motion estimation including bounds check, Claude Sonnet 4.6
    for (int i = -SEARCH_RANGE; i <= SEARCH_RANGE; i++) {      // dy
        for (int j = -SEARCH_RANGE; j <= SEARCH_RANGE; j++) {  // dx
            int refX = blockX + j;
            int refY = blockY + i;

            // skip if search block goes outside the reference frame
            if (refX < 0 || refY < 0 || refX + BLOCK_SIZE > width || refY + BLOCK_SIZE > height) {
                continue;
            }

            int sad = computeSad(frames[currentFrameIndex], frames[referenceFrameIndex], blockX, blockY, refX, refY, width);

            if (sad < minSad) {
                minSad = sad;
                bestDx = j;
                bestDy = i;
            }
        }
    }

    printf("\nME SAD\n");
    printf("MV\tdx=%d\tdy=%d\n", bestDx, bestDy);
    printf("SAD\t%d\n", minSad);
    printf("MC residual at (%d, %d):\n", blockX + bestDx, blockY + bestDy);
    printResidual(frames[currentFrameIndex], frames[referenceFrameIndex],blockX, blockY, blockX + bestDx, blockY + bestDy, width);

    // free all
    for (int i = 0; i < frameCount; i++) {
        freeFrame(frames[i]);
    }
    free(frames);

    return 0;
}

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

Frame **readYuv(const char *path, int width, int height, int *frameCount) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "cannot open: %s\n", path);
        return NULL;
    }

    size_t ySize = (size_t)width * height;
    size_t chromaSize = (size_t)(width / 2) * (height / 2);
    size_t frameSize = ySize + 2 * chromaSize;

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    *frameCount = (int)((long)fileSize / (long)frameSize);
    if (*frameCount == 0) {
        fprintf(stderr, "file too small for one frame\n");
        fclose(file);
        return NULL;
    }

    Frame **frames = malloc((size_t)*frameCount * sizeof(Frame *));
    if (frames == NULL) {
        fclose(file);
        return NULL;
    }

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

// SAD over BLOCK_SIZE x BLOCK_SIZE Y pixels, only luma, chroma not used for ME
int computeSad(const Frame *currentFrame, const Frame *refFrame, int blockX, int blockY, int refX, int refY, int width) {
    int sad = 0;
    int diff = 0;

    for (int i = 0; i < BLOCK_SIZE; i++) {
        for (int j = 0; j < BLOCK_SIZE; j++) {

            int currentVal = currentFrame->Y[(blockY + i) * width + (blockX + j)];

            int refVal = refFrame->Y[(refY + i) * width + (refX + j)];

            diff = currentVal - refVal;
            if (diff < 0) {
                diff = -diff; // absolute value without math.h
            }
            sad += diff;
        }
    }

    return sad;
}

// residual = current - reference, can be negative, shows prediction error per pixel
void printResidual(const Frame *currentFrame, const Frame *refFrame, int blockX, int blockY, int refX, int refY, int width) {
    for (int i = 0; i < BLOCK_SIZE; i++) {
        for (int j = 0; j < BLOCK_SIZE; j++) {
            int currentVal = currentFrame->Y[(blockY + i) * width + (blockX + j)];
            int refVal = refFrame->Y[(refY + i) * width + (refX + j)];

            printf("%d\t", currentVal - refVal);
        }
        printf("\n");
    }
}
