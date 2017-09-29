extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/frame.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
}

namespace
{
    const char* kFilename = "test.mov";
    const auto kWidth = 1280;
    const auto kHeight = 720;
    const auto kPixFormat = AV_PIX_FMT_YUV422P10LE;
    const auto kDuration = 30 * 2;
}

int main()
{
    av_register_all();

    // Guess the format from the fileame.
    auto* format = avformat_alloc_context();
    format->oformat = av_guess_format(nullptr, kFilename, nullptr);
    format->oformat->video_codec = AV_CODEC_ID_PRORES;

    // Open a stream with the codec.
    auto* encoder = avcodec_find_encoder(format->oformat->video_codec);
    auto* stream = avformat_new_stream(format, encoder);

    // 30 fps
    stream->time_base.num = 1;
    stream->time_base.den = 30;

    // Codec settings
    auto* codec = stream->codec;
    avcodec_get_context_defaults3(codec, codec->codec);
    codec->width = kWidth;
    codec->height = kHeight;
    codec->pix_fmt = kPixFormat;
    codec->time_base = stream->time_base;
    codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

    // Open the output stream.
    if (avcodec_open2(codec, nullptr, nullptr) < 0) abort();
    if (avio_open(&format->pb, kFilename, AVIO_FLAG_WRITE) < 0) abort();
    avformat_write_header(format, nullptr);
    av_dump_format(format, 0, kFilename, 1);

    // Allocate temp/output frame buffers.
    const auto tmp_pix_format = AV_PIX_FMT_RGB24;
    const auto out_pix_format = kPixFormat;

    auto* tmp_frame = av_frame_alloc();
    auto* out_frame = av_frame_alloc();

    tmp_frame->width  = out_frame->width  = kWidth;
    tmp_frame->height = out_frame->height = kHeight;

    tmp_frame->format = tmp_pix_format;
    out_frame->format = out_pix_format;

    auto* tmp_buffer = reinterpret_cast<uint8_t*>(av_malloc(avpicture_get_size(tmp_pix_format, kWidth, kHeight)));
    auto* out_buffer = reinterpret_cast<uint8_t*>(av_malloc(avpicture_get_size(out_pix_format, kWidth, kHeight)));

    avpicture_fill(reinterpret_cast<AVPicture*>(tmp_frame), tmp_buffer, tmp_pix_format, kWidth, kHeight);
    avpicture_fill(reinterpret_cast<AVPicture*>(out_frame), out_buffer, out_pix_format, kWidth, kHeight);

    // Pixel format conversion context
    auto* converter = sws_getContext(
        kWidth, kHeight, tmp_pix_format,
        kWidth, kHeight, out_pix_format,
        SWS_POINT, nullptr, nullptr, nullptr
    );

    // Encode and write frames.
    for (auto i = 0; i < kDuration; i++)
    {
        for (auto y = 0; y < kHeight; y++)
            for (auto x = 0; x < tmp_frame->linesize[0]; x++)
                tmp_frame->data[0][y * tmp_frame->linesize[0] + x] = x + y + i * 3;

        // Pixel format conversion
        sws_scale(converter, tmp_frame->data, tmp_frame->linesize, 0, kHeight, out_frame->data, out_frame->linesize);

        // Encode the frame.
        AVPacket packet = { 0 };
        av_init_packet(&packet);

        int got;
        avcodec_encode_video2(codec, &packet, out_frame, &got);

        // Write the enocder output.
        if (got > 0)
        {
            packet.pts = packet.dts = av_rescale_q(i, codec->time_base, stream->time_base);
            packet.stream_index = stream->index;
            av_interleaved_write_frame(format, &packet);
        }

        av_free_packet(&packet);
    }

    // terminate the stream.
    av_write_trailer(format);

    // Release all the resources.
    av_free(tmp_frame->data[0]);
    av_free(out_frame->data[0]);
    av_free(tmp_frame);
    av_free(out_frame);
    avcodec_close(stream->codec);
    avio_close(format->pb);
    av_free(format);

    return 0;
}
