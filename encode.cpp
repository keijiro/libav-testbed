extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/frame.h>
    #include <libavutil/imgutils.h>
}

namespace
{
    const char* kFilename = "test.mp4";
    const auto kWidth = 640;
    const auto kHeight = 480;
    const auto kFormat = AV_PIX_FMT_YUV420P;
    const auto kDuration = 30 * 5;
}

int main()
{
    av_register_all();

    auto* format = avformat_alloc_context();

    format->oformat = av_guess_format(nullptr, kFilename, nullptr);

    auto* stream = avformat_new_stream(
        format, avcodec_find_encoder(format->oformat->video_codec)
    );

    stream->time_base.den = 30;
    stream->time_base.num = 1;

    auto* codec = stream->codec;

    codec->bit_rate = 400000;
    codec->width = kWidth;
    codec->height = kHeight;
    codec->gop_size = 1;
    codec->pix_fmt = kFormat;
    codec->max_b_frames = 2;
    codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(codec, nullptr, nullptr) < 0) abort();

    avio_open(&format->pb, kFilename, AVIO_FLAG_WRITE);
    avformat_write_header(format, nullptr);
    av_dump_format(format, 0, kFilename, 1);

    auto* frame = av_frame_alloc();
    frame->width = kWidth;
    frame->height = kHeight;
    frame->format = kFormat;
    auto* frame_buffer = reinterpret_cast<uint8_t*>(av_malloc(avpicture_get_size(kFormat, kWidth, kHeight)));
    avpicture_fill(reinterpret_cast<AVPicture*>(frame), frame_buffer, kFormat, kWidth, kHeight);

    for (auto i = 0; i < kDuration; i++)
    {
        auto video_pts = static_cast<double>(av_stream_get_end_pts(stream)) *
            stream->time_base.num / stream->time_base.den;

        if (video_pts >= kDuration) break;

        for (auto y = 0; y < kHeight; y++)
            for (auto x = 0; x < kWidth; x++)
                frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;

        for (auto y = 0; y < kHeight / 2; y++)
        {
            for (auto x = 0; x < kWidth / 2; x++)
            {
                frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
                frame->data[2][y * frame->linesize[2] + x] =  64 + x + i * 5;
            }
        }

        frame->pts = (1.0 / 30) * 90 * i;

        AVPacket packet = { 0 };
        av_init_packet(&packet);
        int got;
        avcodec_encode_video2(codec, &packet, frame, &got);
        packet.stream_index = stream->index;
        av_interleaved_write_frame(format, &packet);
    }

    av_free(frame->data[0]);
    av_free(frame);

    av_write_trailer(format);
    avcodec_close(stream->codec);
    avio_close(format->pb);
    av_free(format);

    return 0;
}
