/// Medienlabor Transkodierung Jakob
/// AI-Usage CLAUDE Sonnet 4.6 :
/// 1. Die FIFO-Pufferung für den frame_size-Mismatch (MP3=1152, AAC=1024): Das Problem war klar, AVAudioFifo hätte ich aus
/// der Doku alleine aber nicht hingekriegt. KI hat mir gezeigt wie init, write, flush zusammenhängen.
/// 2. --enable-decoder=mp3float: NaN/Inf auf jedem Frame, leerer Output, nichts zu finden beim Googlen. Claude hat rausgefunden,
/// dass in FFmpeg 8 mp3 und mp3float zwei getrennte Einträge sind und man letzteren explizit aktivieren muss.
/// 3. Formatierung und Rechtschreibfehler-behebung bei comments

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/audio_fifo.h>   // doc/examples/transcode_aac.c - init_fifo()
#include <libavutil/channel_layout.h>
#include <stdio.h>
#include <stdlib.h>

// prototypes
void checkError(int ret, const char *msg);
AVFormatContext *openInput(const char *path, int *audioIdx);
AVCodecContext *createDecoder(AVStream *stream);
AVCodecContext *createEncoder(AVFormatContext *outFmtCtx, AVCodecContext *decCtx);
AVStream *setupOutput(AVFormatContext *outFmtCtx, AVCodecContext *encCtx, const char *path);
void flushFifo(AVAudioFifo *fifo, AVCodecContext *encCtx, AVFormatContext *outFmtCtx, AVStream *outStream, AVPacket *outPkt, int64_t *pts, int flush);
void encodeAndWrite(AVCodecContext *encCtx, AVFormatContext *outFmtCtx, AVStream *outStream, AVFrame *frame, AVPacket *outPkt);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <input.mp3> <output.mp4>\n", argv[0]);
        return 1;
    }

    int audioIdx;
    AVFormatContext *inFmtCtx = openInput(argv[1], &audioIdx);
    AVCodecContext *decCtx = createDecoder(inFmtCtx->streams[audioIdx]);

    AVFormatContext *outFmtCtx = NULL;
    checkError(avformat_alloc_output_context2(&outFmtCtx, NULL, "mp4", argv[2]), "could not create output context"); // doc/examples/muxing.c

    AVCodecContext *encCtx = createEncoder(outFmtCtx, decCtx);
    AVStream *outStream = setupOutput(outFmtCtx, encCtx, argv[2]);

    // FIFO buffers decoded samples - MP3 outputs 1152 samples/frame, AC3 needs exactly 1536
    // doc/examples/transcode_aac.c - init_fifo()
    AVAudioFifo *fifo = av_audio_fifo_alloc(encCtx->sample_fmt, encCtx->ch_layout.nb_channels, encCtx->frame_size);
    if (!fifo) {
        fprintf(stderr, "error: could not alloc audio fifo\n");
        return 1;
    }

    AVPacket *inPkt = av_packet_alloc();
    AVPacket *outPkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    if (!inPkt || !outPkt || !frame) {
        fprintf(stderr, "error: alloc failed\n");
        return 1;
    }

    int64_t pts = 0;

    // decode => encode loop
    // doc/examples/transcode_aac.c - read_decode_convert_and_store()
    while (av_read_frame(inFmtCtx, inPkt) >= 0) {
        if (inPkt->stream_index != audioIdx) {
            av_packet_unref(inPkt);
            continue;
        }

        int ret = avcodec_send_packet(decCtx, inPkt);
        av_packet_unref(inPkt);
        if (ret < 0) {
            continue;
        }

        while (1) {
            ret = avcodec_receive_frame(decCtx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            checkError(ret, "receive frame failed");
            av_audio_fifo_write(fifo, (void **)frame->data, frame->nb_samples); // doc/examples/transcode_aac.c - read_decode_convert_and_store()
            av_frame_unref(frame);
            flushFifo(fifo, encCtx, outFmtCtx, outStream, outPkt, &pts, 0);
        }
    }

    // flush decoder
    avcodec_send_packet(decCtx, NULL);
    while (avcodec_receive_frame(decCtx, frame) >= 0) {
        av_audio_fifo_write(fifo, (void **)frame->data, frame->nb_samples);
        av_frame_unref(frame);
        flushFifo(fifo, encCtx, outFmtCtx, outStream, outPkt, &pts, 0);
    }

    // flush remaining samples in fifo, then flush encoder
    flushFifo(fifo, encCtx, outFmtCtx, outStream, outPkt, &pts, 1);
    encodeAndWrite(encCtx, outFmtCtx, outStream, NULL, outPkt);

    checkError(av_write_trailer(outFmtCtx), "could not write trailer");

    //free all
    av_audio_fifo_free(fifo);
    av_frame_free(&frame);
    av_packet_free(&inPkt);
    av_packet_free(&outPkt);
    avcodec_free_context(&decCtx);
    avcodec_free_context(&encCtx);
    avformat_close_input(&inFmtCtx);
    avio_closep(&outFmtCtx->pb);
    avformat_free_context(outFmtCtx);

    printf("done: %s\n", argv[2]);
    return 0;
}

void checkError(int ret, const char *msg) {
    if (ret < 0) {
        fprintf(stderr, "error: %s\n", msg);
        exit(1);
    }
}

// open input file, find first audio stream -> write index to audioIdx
AVFormatContext *openInput(const char *path, int *audioIdx) {
    AVFormatContext *fmtCtx = NULL;
    checkError(avformat_open_input(&fmtCtx, path, NULL, NULL), "could not open input");
    checkError(avformat_find_stream_info(fmtCtx, NULL), "could not read stream info");

    *audioIdx = -1;
    for (unsigned i = 0; i < fmtCtx->nb_streams; i++) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            *audioIdx = (int)i;
            break;
        }
    }
    if (*audioIdx < 0) {
        fprintf(stderr, "error: no audio stream\n");
        exit(1);
    }
    return fmtCtx;
}

// init decoder for the given input stream
// doc/examples/transcode_aac.c - open_input_file()
AVCodecContext *createDecoder(AVStream *stream) {
    const AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!dec) {
        fprintf(stderr, "error: decoder not found\n");
        exit(1);
    }

    AVCodecContext *ctx = avcodec_alloc_context3(dec);
    if (!ctx) {
        fprintf(stderr, "error: alloc decoder ctx failed\n");
        exit(1);
    }

    checkError(avcodec_parameters_to_context(ctx, stream->codecpar), "decoder params failed");
    checkError(avcodec_open2(ctx, dec, NULL), "could not open decoder");
    return ctx;
}

// init AC3 encoder, match sample rate and channel layout from decoder
// doc/examples/transcode_aac.c - open_output_file()
AVCodecContext *createEncoder(AVFormatContext *outFmtCtx, AVCodecContext *decCtx) {
    const AVCodec *enc = avcodec_find_encoder(AV_CODEC_ID_AC3);
    if (!enc) {
        fprintf(stderr, "error: AC3 encoder not found\n");
        exit(1);
    }

    AVCodecContext *ctx = avcodec_alloc_context3(enc);
    if (!ctx) {
        fprintf(stderr, "error: alloc encoder ctx failed\n");
        exit(1);
    }

    ctx->bit_rate = 192000; // higher bitrate than main_b (64k AAC) - 192k standard for stereo AC3
    ctx->sample_rate = decCtx->sample_rate;
    ctx->sample_fmt = AV_SAMPLE_FMT_FLTP; // AC3 encoder uses FLTP
    av_channel_layout_copy(&ctx->ch_layout, &decCtx->ch_layout); // doc/examples/transcode_aac.c

    if (outFmtCtx->oformat->flags & AVFMT_GLOBALHEADER) { // doc/examples/muxing.c - MP4 needs codec params in container header
        ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    checkError(avcodec_open2(ctx, enc, NULL), "could not open encoder");
    return ctx;
}

// add audio stream to output, open file, write header
// doc/examples/transcode_aac.c - open_output_file()
AVStream *setupOutput(AVFormatContext *outFmtCtx, AVCodecContext *encCtx, const char *path) {
    AVStream *stream = avformat_new_stream(outFmtCtx, NULL);
    if (!stream) {
        fprintf(stderr, "error: new stream failed\n");
        exit(1);
    }

    checkError(avcodec_parameters_from_context(stream->codecpar, encCtx), "encoder params failed");
    stream->time_base = (AVRational){1, encCtx->sample_rate}; // 1 tick = 1 sample

    checkError(avio_open(&outFmtCtx->pb, path, AVIO_FLAG_WRITE), "could not open output file");
    checkError(avformat_write_header(outFmtCtx, NULL), "could not write header");
    return stream;
}

// read frame_size chunks from fifo and encode them
// flush=1 encodes remaining samples at end of file even if less than frame_size
// doc/examples/transcode_aac.c - load_encode_and_write()
void flushFifo(AVAudioFifo *fifo, AVCodecContext *encCtx, AVFormatContext *outFmtCtx, AVStream *outStream, AVPacket *outPkt, int64_t *pts, int flush) {
    while (av_audio_fifo_size(fifo) >= encCtx->frame_size || (flush && av_audio_fifo_size(fifo) > 0)) {
        int readSamples = FFMIN(av_audio_fifo_size(fifo), encCtx->frame_size); // FFMIN from libavutil/common.h

        AVFrame *encFrame = av_frame_alloc();
        if (!encFrame) {
            fprintf(stderr, "error: alloc enc frame failed\n");
            exit(1);
        }

        encFrame->nb_samples = readSamples;
        encFrame->format = encCtx->sample_fmt;
        encFrame->sample_rate = encCtx->sample_rate; // must match encoder - encoder rejects frames with sample_rate=0
        encFrame->pts = *pts; // manual PTS counter in samples, ref: doc/examples/transcode_aac.c
        av_channel_layout_copy(&encFrame->ch_layout, &encCtx->ch_layout);
        checkError(av_frame_get_buffer(encFrame, 0), "enc frame buffer alloc failed"); // doc/examples/transcode_aac.c

        av_audio_fifo_read(fifo, (void **)encFrame->data, readSamples);
        *pts += readSamples;

        encodeAndWrite(encCtx, outFmtCtx, outStream, encFrame, outPkt);
        av_frame_free(&encFrame);
    }
}

// send frame to encoder, write all output packets
// doc/examples/transcode_aac.c - encode_audio_frame()
void encodeAndWrite(AVCodecContext *encCtx, AVFormatContext *outFmtCtx, AVStream *outStream, AVFrame *frame, AVPacket *outPkt) {
    int sendRet = avcodec_send_frame(encCtx, frame); // skip invalid frames - MP3 encoder delay at start causes NaN/Inf samples, which the AC3 encoder rejects
    if (sendRet < 0) {
        return;
    }

    int ret;
    while (1) {
        ret = avcodec_receive_packet(encCtx, outPkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        checkError(ret, "receive packet failed");
        av_packet_rescale_ts(outPkt, encCtx->time_base, outStream->time_base); // rescale timestamps: encoder timebase -> stream timebase, ref: doc/examples/transcode_aac.c
        outPkt->stream_index = outStream->index;
        checkError(av_interleaved_write_frame(outFmtCtx, outPkt), "write frame failed");
        av_packet_unref(outPkt);
    }
}
