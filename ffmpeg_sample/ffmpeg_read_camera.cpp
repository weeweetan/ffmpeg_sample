#include <stdio.h>

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include "SDL/SDL.h"
}
#else
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include "SDL/SDL.h"
#ifdef __cplusplus
}
#endif
#endif

#define OUTPUT_YUV420P 0
//'1' Use Dshow
//'0' Use VFW
#define USE_DSHOW 0

//Refresh Event
#define SFM_REFRESH_EVENT (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT (SDL_USEREVENT + 2)

int thread_exit = 0;

int sfp_refresh_thread(void *opaque)
{
    thread_exit = 0;
    while (!thread_exit)
    {
        SDL_Event event;
        event.type = SFM_REFRESH_EVENT;
        SDL_PushEvent(&event);
        SDL_Delay(10);
    }
    thread_exit = 0;
    SDL_Event event;
    event.type = SFM_BREAK_EVENT;
    SDL_PushEvent(&event);

    return 0;
}

//Show Dshow Device
void show_dshow_device()
{
    AVFormatContext *fmt_ctx = avformat_alloc_context();
    AVDictionary *options = NULL;
    av_dict_set(&options, "list_devices", "true", 0);
    AVInputFormat *ifmt = av_find_input_format("dshow");
    printf("===================Device Info========================\n");
    avformat_open_input(&fmt_ctx, "video=dummy", ifmt, &options);
    printf("======================================================\n");

}

//Show Dshow Device Option
void show_dshow_device_option()
{
    AVFormatContext *fmt_ctx = avformat_alloc_context();
    AVDictionary *options = NULL;
    av_dict_set(&options, "list_options", "true", 0);
    AVInputFormat *ifmt = av_find_input_format("dshow");
    printf("===================Device Info========================\n");
    avformat_open_input(&fmt_ctx, "video=HD Webcam", ifmt, &options);
    printf("======================================================\n");

}


//Show VFW Device
void show_vfw_device()
{
    AVFormatContext *fmt_ctx = avformat_alloc_context();
    AVInputFormat *ifmt = av_find_input_format("vfwcap");
    printf("===================Device Info========================\n");
    avformat_open_input(&fmt_ctx, "list", ifmt, NULL);
    printf("======================================================\n");

}


//Show AVFoundation Device
void show_avfoundation_device()
{
    AVFormatContext *fmt_ctx = avformat_alloc_context();
    AVDictionary *options = NULL;
    av_dict_set(&options, "list_devices", "true", 0);
    AVInputFormat *ifmt = av_find_input_format("avfoundation");
    printf("===================Device Info========================\n");
    avformat_open_input(&fmt_ctx, "", ifmt, &options);
    printf("======================================================\n");

}


int read_camera()
{
    AVFormatContext *fmt_ctx = NULL;
    int i, video_index;
    AVCodecContext *codec_ctx;
    AVCodec *codec;
    AVDictionary *options = NULL;

    av_register_all();
    avformat_network_init();
    fmt_ctx = avformat_alloc_context();

    avdevice_register_all();

#ifdef _WIN32

    //Show Dshow Device
    show_dshow_device();
    //Show Device Options
    show_dshow_device_option();
    //Show VFW Device
    show_vfw_device();

#if USE_DSHOW
    AVInputFormat *ifmt = av_find_input_format("dshow");
    //Set video device's name
    if (avformat_open_input(&fmt_ctx, "video=HD Webcam", ifmt, NULL) != 0)
    {
        printf("Could not open input stream.\n");
        return -1;
    }
#else
    AVInputFormat *ifmt = av_find_input_format("vfwcap");
    if (avformat_open_input(&fmt_ctx, "0", ifmt, NULL) != 0)
    {
        printf("Could not open input stream.\n");
        return -1;
    }
#endif
#elif defined linux
    AVInputFormat *ifmt = av_find_input_format("video4linux2");
    if (avformat_open_input(&fmt_ctx, "/dev/video0", ifmt, NULL) != 0)
    {
        printf("Could not open input stream.\n");
        return -1;
    }
#else
    show_avfoundation_device();
    AVInputFormat *ifmt = av_find_input_format("avfoundation");
    if (avformat_open_input(&fmt_ctx, "0", ifmt, NULL) != 0)
    {
        printf("Could not open input stream.\n");
        return -1;
    }
#endif

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0)
    {
        printf("Could not find stream inforamtion.\n");
        return -1;
    }

    video_index = -1;
    for (i = 0; i < fmt_ctx->nb_streams; i++)
    {
        if (fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_index = i;
            break;
        }
    }

    if (video_index == -1)
    {
        printf("Could not find a video stream.\n");
        return -1;
    }

    codec_ctx = fmt_ctx->streams[video_index]->codec;
    codec = avcodec_find_decoder(codec_ctx->codec_id);
    if (codec == NULL)
    {
        printf("Codec not found.\n");
        return -1;
    }
    if (avcodec_open2(codec_ctx, codec, NULL) < 0)
    {
        printf("Could not open codec.\n");
        return -1;
    }

    AVFrame *frame, *frame_yuv;
    frame = av_frame_alloc();
    frame_yuv = av_frame_alloc();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
    {
        printf("Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }

    int screen_w = 0, screen_h = 0;
    SDL_Surface *screen;
    screen_w = codec_ctx->width;
    screen_h = codec_ctx->height;
    screen = SDL_SetVideoMode(screen_w, screen_h, 0, 0);

    if (!screen)
    {
        printf("SDL: could not set video mode - exiting:%s\n", SDL_GetError());
        return -1;
    }

    SDL_Overlay *bmp;
    bmp = SDL_CreateYUVOverlay(codec_ctx->width, codec_ctx->height, SDL_YV12_OVERLAY, screen);
    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = screen_w;
    rect.h = screen_h;

    int ret, got_picture;

    AVPacket *packet = (AVPacket *)av_malloc(sizeof(AVPacket));

#if OUTPUT_YUV420P
    FILE *fp_yuv = fopen("output.yuv", "wb+");
#endif

    struct SwsContext *img_convert_ctx;
    img_convert_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt, codec_ctx->width, codec_ctx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

    SDL_Thread *video_tid = SDL_CreateThread(sfp_refresh_thread, NULL);
    SDL_WM_SetCaption("Simplest FFmpeg Read Camera", NULL);

    //Event loop
    SDL_Event event;

    for (;;)
    {
        //Wait
        SDL_WaitEvent(&event);
        if (event.type == SFM_REFRESH_EVENT)
        {
            if (av_read_frame(fmt_ctx, packet) >= 0)
            {
                if (packet->stream_index == video_index)
                {
                    ret = avcodec_decode_video2(codec_ctx, frame, &got_picture, packet);
                    if (ret < 0)
                    {
                        printf("Decodec Error.\n");
                        return -1;
                    }
                    if (got_picture)
                    {
                        SDL_LockYUVOverlay(bmp);
                        frame_yuv->data[0] = bmp->pixels[0];
                        frame_yuv->data[1] = bmp->pixels[2];
                        frame_yuv->data[2] = bmp->pixels[1];
                        frame_yuv->linesize[0] = bmp->pitches[0];
                        frame_yuv->linesize[1] = bmp->pitches[2];
                        frame_yuv->linesize[2] = bmp->pitches[1];
                        sws_scale(img_convert_ctx, (const unsigned char * const *)frame->data, frame->linesize, 0, codec_ctx->height, frame_yuv->data, frame_yuv->linesize);
#if OUTPUT_YUV420P
                        int y_size = codec_ctx->width * codec_ctx->height;
                        fwrite(frame_yuv->data[0], 1, y_size, fp_yuv);      // Y
                        fwrite(frame_yuv->data[1], 1, y_size / 4, fp_yuv);    // U
                        fwrite(frame_yuv->data[2], 1, y_size / 4, fp_yuv);    // V
#endif
                        SDL_UnlockYUVOverlay(bmp);
                        SDL_DisplayYUVOverlay(bmp, &rect);

                    }
                }
                av_free_packet(packet);
            }
            else
            {
                thread_exit = 1;
            }
        }
        else if (event.type == SDL_QUIT)
        {
            thread_exit = 1;
        }
        else if (event.type == SFM_BREAK_EVENT)
        {
            break;
        }
    }

    sws_freeContext(img_convert_ctx);

#if OUTPUT_YUV420P
    fclose(fp_yuv);
#endif

    SDL_Quit();
    av_free(frame_yuv);
    avcodec_close(codec_ctx);
    avformat_close_input(&fmt_ctx);
    return 0;
}


#if 0
int main(int argc, char **argv)
{
    read_camera();
}
#endif