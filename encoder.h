extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/frame.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
}

namespace avtest
{
    class Encoder
    {
    public:

        static void initGlobal()
        {
            av_register_all();
        }

        Encoder(const char* filename, int width, int height, int fps)
        {
            const auto kPixFormat = AV_PIX_FMT_YUV422P10LE;

            // Output stream formart: Qicktime (mov) with ProRes codec
            format_ = avformat_alloc_context();
            format_->oformat = av_guess_format("mov", nullptr, nullptr);
            format_->oformat->video_codec = AV_CODEC_ID_PRORES;

            // Open a new stream with the codec.
            auto* encoder = avcodec_find_encoder(format_->oformat->video_codec);
            stream_ = avformat_new_stream(format_, encoder);

            // Stream settings
            stream_->time_base.num = 1;
            stream_->time_base.den = fps;

            // Codec settings
            auto* codec = stream_->codec;
            avcodec_get_context_defaults3(codec, codec->codec);
            codec->width = width;
            codec->height = height;
            codec->pix_fmt = kPixFormat;
            codec->time_base = stream_->time_base;
            codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

            // Open the output stream.
            if (avcodec_open2(codec, nullptr, nullptr) < 0) abort();
            if (avio_open(&format_->pb, filename, AVIO_FLAG_WRITE) < 0) abort();
            avformat_write_header(format_, nullptr);
            av_dump_format(format_, 0, filename, 1);

            // Allocate temp/output frame buffers.
            const auto tmp_pix_format = AV_PIX_FMT_RGB24;
            const auto out_pix_format = kPixFormat;

            tmpFrame_ = av_frame_alloc();
            outFrame_ = av_frame_alloc();

            tmpFrame_->width  = outFrame_->width  = width;
            tmpFrame_->height = outFrame_->height = height;

            tmpFrame_->format = tmp_pix_format;
            outFrame_->format = out_pix_format;

            auto* tmp_buffer = reinterpret_cast<uint8_t*>(av_malloc(avpicture_get_size(tmp_pix_format, width, height)));
            auto* out_buffer = reinterpret_cast<uint8_t*>(av_malloc(avpicture_get_size(out_pix_format, width, height)));

            avpicture_fill(reinterpret_cast<AVPicture*>(tmpFrame_), tmp_buffer, tmp_pix_format, width, height);
            avpicture_fill(reinterpret_cast<AVPicture*>(outFrame_), out_buffer, out_pix_format, width, height);

            // Pixel format conversion context
            converter_ = sws_getContext(
                width, height, tmp_pix_format,
                width, height, out_pix_format,
                SWS_POINT, nullptr, nullptr, nullptr
            );

            frameCount_ = 0;
        }

        uint8_t* getRGBFrameBuffer() const
        {
            return tmpFrame_->data[0];
        }

        void processFrame()
        {
            // Pixel format conversion
            sws_scale(
                converter_,
                tmpFrame_->data, tmpFrame_->linesize, 0, tmpFrame_->height,
                outFrame_->data, outFrame_->linesize
            );

            // Encode the frame.
            AVPacket packet = { 0 };
            av_init_packet(&packet);

            int got;
            avcodec_encode_video2(stream_->codec, &packet, outFrame_, &got);

            if (got > 0)
            {
                // Encoder output
                packet.pts = packet.dts = av_rescale_q(frameCount_, stream_->codec->time_base, stream_->time_base);
                packet.stream_index = stream_->index;
                av_interleaved_write_frame(format_, &packet);
            }

            av_free_packet(&packet);

            frameCount_++;
        }

        void close()
        {
            av_write_trailer(format_);
        }

        ~Encoder()
        {
            av_free(tmpFrame_->data[0]);
            av_free(outFrame_->data[0]);
            av_free(tmpFrame_);
            av_free(outFrame_);
            avcodec_close(stream_->codec);
            avio_close(format_->pb);
            av_free(format_);
        }

    private:

        AVFormatContext* format_;
        AVStream* stream_;
        AVFrame* tmpFrame_;
        AVFrame* outFrame_;
        SwsContext* converter_;
        int frameCount_;
    };
}
