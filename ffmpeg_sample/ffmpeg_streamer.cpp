#include <stdio.h>

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
extern "C"
{
#include "libavformat/avformat.h"
#include "libavutil/mathematics.h"
#include "libavutil/time.h"
}
#else
#ifdef __cplusplus
extern "C"
{
#endif
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/time.h"
#ifdef __cplusplus
}
#endif
#endif

int streamer()
{
    AVOutputFormat *ofmt = NULL;
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;
    const char *in_filename, *out_filename;
    int ret, i;
    int video_index = -1;
    int frame_index = 0;
    int64_t start_time = 0;

    //in_filename = "cuc_ieschool.flv";
    in_filename = "E:\html\Cd1.mp4";
    out_filename = "rtmp://192.168.116.132:1935/hls/stream";

    av_register_all();
    avformat_network_init();

    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0)
    {
        printf("Could not open input file.");
        goto end;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0)
    {
        printf("Failed to retrieve input stream information");
        goto end;
    }

    for (i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_index = i;
            break;
        }
    }
    av_dump_format(ifmt_ctx, 0, in_filename, 0);

    avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", out_filename);
    if (!ofmt_ctx)
    {
        printf("Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    ofmt = ofmt_ctx->oformat;
    for (i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
        if (!out_stream)
        {
            printf("Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
        if (ret < 0)
        {
            printf("Failed to copy context from input to output stream codec context\n");
            goto end;
        }
        out_stream->codec->codec_tag = 0;
        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        {
            out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }
    }

    av_dump_format(ofmt_ctx, 0, out_filename, 1);
    if (!(ofmt_ctx->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            printf("Could not open output URL %s", out_filename);
            goto end;
        }
    }

    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0)
    {
        printf("Error occurred when opening output URL\n");
        goto end;
    }

    start_time = av_gettime();
    while (1)
    {
        AVStream *in_stream, *out_stream;
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0)
        {
            break;
        }
        if (pkt.pts == AV_NOPTS_VALUE)
        {
            AVRational time_base1 = ifmt_ctx->streams[video_index]->time_base;
            int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(ifmt_ctx->streams[video_index]->r_frame_rate);
            //Parameters
            pkt.pts = (double)(frame_index*calc_duration) / (double)(av_q2d(time_base1)*AV_TIME_BASE);
            pkt.dts = pkt.pts;
            pkt.duration = (double)calc_duration / (double)(av_q2d(time_base1)*AV_TIME_BASE);
        }

        if (pkt.stream_index == video_index) 
        {
            AVRational time_base = ifmt_ctx->streams[video_index]->time_base;
            AVRational time_base_q = { 1,AV_TIME_BASE };
            int64_t pts_time = av_rescale_q(pkt.dts, time_base, time_base_q);
            int64_t now_time = av_gettime() - start_time;
            if (pts_time > now_time)
                av_usleep(pts_time - now_time);

        }

        in_stream = ifmt_ctx->streams[pkt.stream_index];
        out_stream = ofmt_ctx->streams[pkt.stream_index];
        /* copy packet */
        //Convert PTS/DTS
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        //Print to Screen
        if (pkt.stream_index == video_index) 
        {
            printf("Send %8d video frames to output URL\n", frame_index);
            frame_index++;
        }
        //ret = av_write_frame(ofmt_ctx, &pkt);
        ret = av_interleaved_write_frame(ofmt_ctx, &pkt);

        if (ret < 0) {
            printf("Error muxing packet\n");
            break;
        }

        av_free_packet(&pkt);
    }
    av_write_trailer(ofmt_ctx);
end:
    avformat_close_input(&ifmt_ctx);
    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    if (ret < 0 && ret != AVERROR_EOF) {
        printf("Error occurred.\n");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    streamer();
}