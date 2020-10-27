#include <stdio.h>

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
extern "C"
{
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#else
#ifdef __cplusplus
extern "C"
#endif
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#ifdef __cplusplus
};
#endif
#endif

int flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index)
{
    int ret;
    int got_frame;
    AVPacket enc_pkt;
    if (!(fmt_ctx->streams[stream_index]->codec->codec->capabilities & CODEC_CAP_DELAY))
    {
        return 0;
    }

    while (1)
    {
        enc_pkt.data = NULL;
        enc_pkt.size = 0;
        av_init_packet(&enc_pkt);
        ret = avcodec_encode_video2(fmt_ctx->streams[stream_index]->codec, &enc_pkt, NULL, &got_frame);
        av_frame_free(NULL);
        if (ret < 0)
        {
            break;
        }
        if (!got_frame)
        {
            ret = 0;
            break;
        }
        printf("Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n", enc_pkt.size);
        ret = av_write_frame(fmt_ctx, &enc_pkt);
        if (ret < 0)
        {
            break;
        }

    }
    return ret;
}


int encoder()
{
    AVFormatContext *fmt_ctx;
    AVOutputFormat *fmt;
    AVStream *video_st;
    AVCodecContext *codec_ctx;
    AVCodec *codec;
    AVPacket pkt;
    uint8_t *picture_buf;
    AVFrame *frame;
    int picture_size;
    int y_size;
    int frame_cnt = 0;

    FILE *in_file = fopen("../ds_480x272.yuv", "rb");
    int in_w = 480, in_h = 272;
    int frame_num = 100;

    const char *out_file = "ds.h264";

    av_register_all();
    //Method 1
    fmt_ctx = avformat_alloc_context();
    fmt = av_guess_format(NULL, out_file, NULL);
    fmt_ctx->oformat = fmt;

    //Method 2
    //avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, out_file);
    //fmt = fmt_ctx->oformat;

    //Open output URL
    if (avio_open(&fmt_ctx->pb, out_file, AVIO_FLAG_READ_WRITE) < 0)
    {
        printf("Failed to open output file!\n");
        return -1;
    }

    video_st = avformat_new_stream(fmt_ctx, 0);

    if (video_st == NULL)
    {
        return -1;
    }

    //Param that must set
    codec_ctx = video_st->codec;
    codec_ctx->codec_id = fmt->video_codec;
    codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx->width = in_w;
    codec_ctx->height = in_h;
    codec_ctx->bit_rate = 400000;
    codec_ctx->gop_size = 250;

    codec_ctx->time_base.num = 1;
    codec_ctx->time_base.den = 25;

    codec_ctx->qmin = 10;
    codec_ctx->qmax = 51;

    codec_ctx->max_b_frames = 3;

    AVDictionary *param = 0;
    if (codec_ctx->codec_id == AV_CODEC_ID_H264)
    {
        av_dict_set(&param, "preset", "slow", 0);
        av_dict_set(&param, "tune", "zerolatency", 0);
    }

    if (codec_ctx->codec_id == AV_CODEC_ID_H265)
    {
        av_dict_set(&param, "preset", "ultrafast", 0);
        av_dict_set(&param, "tune", "zero-latency", 0);
    }

    //show some Information
    av_dump_format(fmt_ctx, 0, out_file, 1);

    codec = avcodec_find_encoder(codec_ctx->codec_id);
    if (!codec)
    {
        printf("Can not find encoder! \n");
        return -1;
    }

    if (avcodec_open2(codec_ctx, codec, &param) < 0)
    {
        printf("Failed to open encoder\n");
        return -1;
    }

    frame = av_frame_alloc();
    picture_size = avpicture_get_size(codec_ctx->pix_fmt, codec_ctx->width, codec_ctx->height);
    picture_buf = (uint8_t *)av_malloc(picture_size);
    avpicture_fill((AVPicture *)frame, picture_buf, codec_ctx->pix_fmt, codec_ctx->width, codec_ctx->height);

    avformat_write_header(fmt_ctx, NULL);
    av_new_packet(&pkt, picture_size);
    y_size = codec_ctx->width * codec_ctx->height;

    for (int i = 0; i < frame_num; i++)
    {
        if (fread(picture_buf, 1, y_size * 3 / 2, in_file) <= 0)
        {
            printf("Failed to read raw data!\n");
            return -1;
        }
        else if (feof(in_file))
        {
            break;
        }

        frame->data[0] = picture_buf;
        frame->data[1] = picture_buf + y_size;
        frame->data[2] = picture_buf + y_size * 5 / 4;

        //PTS
        frame->pts = i * (video_st->time_base.den) / ((video_st->time_base.num) * 25);
        int got_picture = 0;

        //Encode
        int ret = avcodec_encode_video2(codec_ctx, &pkt, frame, &got_picture);
        if (ret < 0)
        {
            printf("Failed to encode\n");
            return -1;
        }
        if (got_picture == 1)
        {
            printf("Succeed to encode frame: %5d\tsize:%5d\n", frame_cnt, pkt.size);
            frame_cnt++;
            pkt.stream_index = video_st->index;
            ret = av_write_frame(fmt_ctx, &pkt);
            av_free_packet(&pkt);
        }
    }
    //Flush Encoder
    int ret = flush_encoder(fmt_ctx, 0);
    if (ret < 0)
    {
        return -1;
    }

    //Write file trailer
    av_write_trailer(fmt_ctx);

    //clean
    if (video_st)
    {
        avcodec_close(video_st->codec);
        av_free(frame);
        av_free(picture_buf);
    }
    avio_close(fmt_ctx->pb);
    avformat_free_context(fmt_ctx);
    fclose(in_file);

    return 0;
}


//int main(int argc, char **argv)
//{
//    encoder();
//}