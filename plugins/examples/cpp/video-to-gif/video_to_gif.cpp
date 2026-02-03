/**
 * uniconv Native Plugin: video-to-gif
 *
 * Converts video files (MP4, MOV, AVI, etc.) to animated GIF using libav.
 *
 * Supported options:
 *   --fps       Output frames per second (default: 10)
 *   --width     Output width (height auto-scaled)
 *   --start     Start time in seconds
 *   --duration  Duration in seconds
 *   --loop      Number of loops (0 = infinite)
 *
 * Build:
 *   mkdir build && cd build
 *   cmake ..
 *   cmake --build .
 */

#include <uniconv/plugin_api.h>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

// Plugin info
static const char *targets[] = {"gif", nullptr};
static const char *input_formats[] = {"mp4", "mov", "avi", "webm", "mkv", "m4v", nullptr};

// Data type information
static UniconvDataType input_types[] = {UNICONV_DATA_VIDEO, UNICONV_DATA_FILE, (UniconvDataType)0};
static UniconvDataType output_types[] = {UNICONV_DATA_IMAGE, (UniconvDataType)0};

static UniconvPluginInfo plugin_info = {
    .name = "video-to-gif",
    .group = "video-to-gif",
    .version = "1.0.0",
    .description = "Convert video to GIF using libav",
    .targets = targets,
    .input_formats = input_formats,
    .input_types = input_types,
    .output_types = output_types};

namespace
{

    // RAII wrappers for FFmpeg resources
    struct AVFormatContextDeleter
    {
        void operator()(AVFormatContext *ctx) const
        {
            if (ctx)
                avformat_close_input(&ctx);
        }
    };

    struct AVFormatContextOutputDeleter
    {
        void operator()(AVFormatContext *ctx) const
        {
            if (ctx)
            {
                if (ctx->pb)
                    avio_closep(&ctx->pb);
                avformat_free_context(ctx);
            }
        }
    };

    struct AVCodecContextDeleter
    {
        void operator()(AVCodecContext *ctx) const
        {
            if (ctx)
                avcodec_free_context(&ctx);
        }
    };

    struct AVFrameDeleter
    {
        void operator()(AVFrame *frame) const
        {
            if (frame)
                av_frame_free(&frame);
        }
    };

    struct AVPacketDeleter
    {
        void operator()(AVPacket *pkt) const
        {
            if (pkt)
                av_packet_free(&pkt);
        }
    };

    struct SwsContextDeleter
    {
        void operator()(SwsContext *ctx) const
        {
            if (ctx)
                sws_freeContext(ctx);
        }
    };

    struct AVFilterGraphDeleter
    {
        void operator()(AVFilterGraph *graph) const
        {
            if (graph)
                avfilter_graph_free(&graph);
        }
    };

    using FormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
    using FormatContextOutputPtr = std::unique_ptr<AVFormatContext, AVFormatContextOutputDeleter>;
    using CodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
    using FramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
    using PacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;
    using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;
    using FilterGraphPtr = std::unique_ptr<AVFilterGraph, AVFilterGraphDeleter>;

    /**
     * Check if a file exists
     */
    bool file_exists(const std::string &path)
    {
        FILE *f = fopen(path.c_str(), "rb");
        if (f)
        {
            fclose(f);
            return true;
        }
        return false;
    }

    /**
     * Get file size
     */
    size_t get_file_size(const std::string &path)
    {
        FILE *f = fopen(path.c_str(), "rb");
        if (!f)
            return 0;
        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        fclose(f);
        return size;
    }

    /**
     * Get option value as string
     */
    std::string get_option(const UniconvRequest *req, const char *key)
    {
        if (req->get_plugin_option && req->options_ctx)
        {
            const char *val = req->get_plugin_option(key, req->options_ctx);
            if (val)
                return val;
        }
        if (req->get_core_option && req->options_ctx)
        {
            const char *val = req->get_core_option(key, req->options_ctx);
            if (val)
                return val;
        }
        return "";
    }

    /**
     * Get option as integer with default
     */
    int get_option_int(const UniconvRequest *req, const char *key, int default_val)
    {
        std::string val = get_option(req, key);
        if (val.empty())
            return default_val;
        try
        {
            return std::stoi(val);
        }
        catch (...)
        {
            return default_val;
        }
    }

    /**
     * Get option as float with default
     */
    float get_option_float(const UniconvRequest *req, const char *key, float default_val)
    {
        std::string val = get_option(req, key);
        if (val.empty())
            return default_val;
        try
        {
            return std::stof(val);
        }
        catch (...)
        {
            return default_val;
        }
    }

    /**
     * Video to GIF converter using libav with filter graph for palette generation
     */
    class VideoToGifConverter
    {
    public:
        struct Options
        {
            int fps = 10;
            int width = -1; // -1 = keep original
            float start = 0.0f;
            float duration = 0.0f; // 0 = full duration
            int loop = 0;          // 0 = infinite
        };

        std::string convert(const std::string &input_path,
                            const std::string &output_path,
                            const Options &opts)
        {
            int ret;

            // Open input
            AVFormatContext *fmt_ctx_raw = nullptr;
            ret = avformat_open_input(&fmt_ctx_raw, input_path.c_str(), nullptr, nullptr);
            if (ret < 0)
            {
                return av_err_str(ret, "Failed to open input file");
            }
            FormatContextPtr fmt_ctx(fmt_ctx_raw);

            ret = avformat_find_stream_info(fmt_ctx.get(), nullptr);
            if (ret < 0)
            {
                return av_err_str(ret, "Failed to find stream info");
            }

            // Find video stream
            int video_stream_idx = -1;
            for (unsigned i = 0; i < fmt_ctx->nb_streams; i++)
            {
                if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
                {
                    video_stream_idx = i;
                    break;
                }
            }
            if (video_stream_idx < 0)
            {
                return "No video stream found";
            }

            AVStream *video_stream = fmt_ctx->streams[video_stream_idx];
            AVCodecParameters *codecpar = video_stream->codecpar;

            // Find decoder
            const AVCodec *decoder = avcodec_find_decoder(codecpar->codec_id);
            if (!decoder)
            {
                return "Decoder not found for input video";
            }

            // Create decoder context
            CodecContextPtr dec_ctx(avcodec_alloc_context3(decoder));
            if (!dec_ctx)
            {
                return "Failed to allocate decoder context";
            }

            ret = avcodec_parameters_to_context(dec_ctx.get(), codecpar);
            if (ret < 0)
            {
                return av_err_str(ret, "Failed to copy codec parameters");
            }

            ret = avcodec_open2(dec_ctx.get(), decoder, nullptr);
            if (ret < 0)
            {
                return av_err_str(ret, "Failed to open decoder");
            }

            // Calculate output dimensions
            int out_width = opts.width > 0 ? opts.width : dec_ctx->width;
            int out_height;
            if (opts.width > 0)
            {
                // Preserve aspect ratio
                out_height = (dec_ctx->height * opts.width) / dec_ctx->width;
                // Ensure even dimensions
                out_height = (out_height / 2) * 2;
            }
            else
            {
                out_height = dec_ctx->height;
            }
            out_width = (out_width / 2) * 2;

            // Setup filter graph for palette generation
            // This produces much better quality GIFs than direct RGB8 conversion
            FilterGraphPtr filter_graph(avfilter_graph_alloc());
            if (!filter_graph)
            {
                return "Failed to allocate filter graph";
            }

            AVFilterContext *buffersrc_ctx = nullptr;
            AVFilterContext *buffersink_ctx = nullptr;

            // Build filter description
            std::ostringstream filter_descr;
            filter_descr << "fps=" << opts.fps;
            if (opts.width > 0)
            {
                filter_descr << ",scale=" << out_width << ":" << out_height << ":flags=lanczos";
            }
            filter_descr << ",split[s0][s1];";
            filter_descr << "[s0]palettegen=max_colors=256:stats_mode=diff[p];";
            filter_descr << "[s1][p]paletteuse=dither=floyd_steinberg";

            // Create buffer source
            const AVFilter *buffersrc = avfilter_get_by_name("buffer");
            const AVFilter *buffersink = avfilter_get_by_name("buffersink");

            char args[512];
            snprintf(args, sizeof(args),
                     "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                     dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
                     video_stream->time_base.num, video_stream->time_base.den,
                     dec_ctx->sample_aspect_ratio.num,
                     dec_ctx->sample_aspect_ratio.den > 0 ? dec_ctx->sample_aspect_ratio.den : 1);

            ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                               args, nullptr, filter_graph.get());
            if (ret < 0)
            {
                return av_err_str(ret, "Failed to create buffer source");
            }

            ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                               nullptr, nullptr, filter_graph.get());
            if (ret < 0)
            {
                return av_err_str(ret, "Failed to create buffer sink");
            }

            // Note: paletteuse filter outputs PAL8 format, no need to set pix_fmts

            // Parse and configure filter graph
            AVFilterInOut *outputs = avfilter_inout_alloc();
            AVFilterInOut *inputs = avfilter_inout_alloc();

            outputs->name = av_strdup("in");
            outputs->filter_ctx = buffersrc_ctx;
            outputs->pad_idx = 0;
            outputs->next = nullptr;

            inputs->name = av_strdup("out");
            inputs->filter_ctx = buffersink_ctx;
            inputs->pad_idx = 0;
            inputs->next = nullptr;

            ret = avfilter_graph_parse_ptr(filter_graph.get(), filter_descr.str().c_str(),
                                           &inputs, &outputs, nullptr);
            avfilter_inout_free(&inputs);
            avfilter_inout_free(&outputs);

            if (ret < 0)
            {
                return av_err_str(ret, "Failed to parse filter graph");
            }

            ret = avfilter_graph_config(filter_graph.get(), nullptr);
            if (ret < 0)
            {
                return av_err_str(ret, "Failed to configure filter graph");
            }

            // Get output dimensions from filter
            out_width = av_buffersink_get_w(buffersink_ctx);
            out_height = av_buffersink_get_h(buffersink_ctx);

            // Open output file
            AVFormatContext *out_fmt_ctx_raw = nullptr;
            ret = avformat_alloc_output_context2(&out_fmt_ctx_raw, nullptr, "gif", output_path.c_str());
            if (ret < 0 || !out_fmt_ctx_raw)
            {
                return av_err_str(ret, "Failed to create output context");
            }
            FormatContextOutputPtr out_fmt_ctx(out_fmt_ctx_raw);

            // Find GIF encoder
            const AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_GIF);
            if (!encoder)
            {
                return "GIF encoder not found";
            }

            // Create output stream
            AVStream *out_stream = avformat_new_stream(out_fmt_ctx.get(), nullptr);
            if (!out_stream)
            {
                return "Failed to create output stream";
            }

            // Create encoder context
            CodecContextPtr enc_ctx(avcodec_alloc_context3(encoder));
            if (!enc_ctx)
            {
                return "Failed to allocate encoder context";
            }

            enc_ctx->width = out_width;
            enc_ctx->height = out_height;
            enc_ctx->pix_fmt = AV_PIX_FMT_PAL8;
            enc_ctx->time_base = AVRational{1, opts.fps};
            enc_ctx->framerate = AVRational{opts.fps, 1};

            if (out_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            {
                enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }

            ret = avcodec_open2(enc_ctx.get(), encoder, nullptr);
            if (ret < 0)
            {
                return av_err_str(ret, "Failed to open encoder");
            }

            ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx.get());
            if (ret < 0)
            {
                return av_err_str(ret, "Failed to copy encoder parameters");
            }

            out_stream->time_base = enc_ctx->time_base;

            // Set GIF loop count in metadata
            // Note: GIF loop is handled via the output format
            av_dict_set(&out_fmt_ctx->metadata, "loop", std::to_string(opts.loop).c_str(), 0);

            // Open output file
            ret = avio_open(&out_fmt_ctx->pb, output_path.c_str(), AVIO_FLAG_WRITE);
            if (ret < 0)
            {
                return av_err_str(ret, "Failed to open output file");
            }

            ret = avformat_write_header(out_fmt_ctx.get(), nullptr);
            if (ret < 0)
            {
                return av_err_str(ret, "Failed to write header");
            }

            // Seek to start position if specified
            if (opts.start > 0)
            {
                int64_t start_ts = static_cast<int64_t>(opts.start * AV_TIME_BASE);
                ret = av_seek_frame(fmt_ctx.get(), -1, start_ts, AVSEEK_FLAG_BACKWARD);
                if (ret < 0)
                {
                    // Seek failed, continue from beginning
                }
            }

            // Calculate end timestamp
            int64_t end_ts = INT64_MAX;
            if (opts.duration > 0)
            {
                end_ts = static_cast<int64_t>((opts.start + opts.duration) * AV_TIME_BASE);
            }

            // Allocate frames and packet
            FramePtr frame(av_frame_alloc());
            FramePtr filt_frame(av_frame_alloc());
            PacketPtr packet(av_packet_alloc());

            if (!frame || !filt_frame || !packet)
            {
                return "Failed to allocate frame/packet";
            }

            int64_t frame_count = 0;

            // Main decode/filter/encode loop
            while (true)
            {
                ret = av_read_frame(fmt_ctx.get(), packet.get());
                if (ret < 0)
                {
                    break; // EOF or error
                }

                if (packet->stream_index != video_stream_idx)
                {
                    av_packet_unref(packet.get());
                    continue;
                }

                // Check if we've passed the duration limit
                if (packet->pts != AV_NOPTS_VALUE)
                {
                    int64_t pts_time = av_rescale_q(packet->pts, video_stream->time_base,
                                                    AVRational{1, AV_TIME_BASE});
                    if (pts_time > end_ts)
                    {
                        av_packet_unref(packet.get());
                        break;
                    }
                }

                ret = avcodec_send_packet(dec_ctx.get(), packet.get());
                av_packet_unref(packet.get());

                if (ret < 0)
                {
                    continue; // Skip bad packets
                }

                while (ret >= 0)
                {
                    ret = avcodec_receive_frame(dec_ctx.get(), frame.get());
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    {
                        break;
                    }
                    if (ret < 0)
                    {
                        return av_err_str(ret, "Error decoding frame");
                    }

                    // Push frame to filter graph
                    ret = av_buffersrc_add_frame_flags(buffersrc_ctx, frame.get(),
                                                       AV_BUFFERSRC_FLAG_KEEP_REF);
                    av_frame_unref(frame.get());

                    if (ret < 0)
                    {
                        return av_err_str(ret, "Error feeding filter graph");
                    }

                    // Pull filtered frames
                    while (true)
                    {
                        ret = av_buffersink_get_frame(buffersink_ctx, filt_frame.get());
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        {
                            break;
                        }
                        if (ret < 0)
                        {
                            return av_err_str(ret, "Error getting filtered frame");
                        }

                        // Set presentation timestamp
                        filt_frame->pts = frame_count++;

                        // Encode frame
                        ret = avcodec_send_frame(enc_ctx.get(), filt_frame.get());
                        av_frame_unref(filt_frame.get());

                        if (ret < 0)
                        {
                            return av_err_str(ret, "Error sending frame to encoder");
                        }

                        while (ret >= 0)
                        {
                            ret = avcodec_receive_packet(enc_ctx.get(), packet.get());
                            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                            {
                                break;
                            }
                            if (ret < 0)
                            {
                                return av_err_str(ret, "Error encoding frame");
                            }

                            av_packet_rescale_ts(packet.get(), enc_ctx->time_base,
                                                 out_stream->time_base);
                            packet->stream_index = out_stream->index;

                            ret = av_interleaved_write_frame(out_fmt_ctx.get(), packet.get());
                            if (ret < 0)
                            {
                                return av_err_str(ret, "Error writing frame");
                            }
                        }
                    }
                }
            }

            // Flush decoder
            avcodec_send_packet(dec_ctx.get(), nullptr);
            while (avcodec_receive_frame(dec_ctx.get(), frame.get()) >= 0)
            {
                av_buffersrc_add_frame_flags(buffersrc_ctx, frame.get(), 0);
                av_frame_unref(frame.get());

                while (av_buffersink_get_frame(buffersink_ctx, filt_frame.get()) >= 0)
                {
                    filt_frame->pts = frame_count++;
                    avcodec_send_frame(enc_ctx.get(), filt_frame.get());
                    av_frame_unref(filt_frame.get());

                    while (avcodec_receive_packet(enc_ctx.get(), packet.get()) >= 0)
                    {
                        av_packet_rescale_ts(packet.get(), enc_ctx->time_base, out_stream->time_base);
                        packet->stream_index = out_stream->index;
                        av_interleaved_write_frame(out_fmt_ctx.get(), packet.get());
                    }
                }
            }

            // Flush filter graph
            av_buffersrc_add_frame_flags(buffersrc_ctx, nullptr, 0);
            while (av_buffersink_get_frame(buffersink_ctx, filt_frame.get()) >= 0)
            {
                filt_frame->pts = frame_count++;
                avcodec_send_frame(enc_ctx.get(), filt_frame.get());
                av_frame_unref(filt_frame.get());

                while (avcodec_receive_packet(enc_ctx.get(), packet.get()) >= 0)
                {
                    av_packet_rescale_ts(packet.get(), enc_ctx->time_base, out_stream->time_base);
                    packet->stream_index = out_stream->index;
                    av_interleaved_write_frame(out_fmt_ctx.get(), packet.get());
                }
            }

            // Flush encoder
            avcodec_send_frame(enc_ctx.get(), nullptr);
            while (avcodec_receive_packet(enc_ctx.get(), packet.get()) >= 0)
            {
                av_packet_rescale_ts(packet.get(), enc_ctx->time_base, out_stream->time_base);
                packet->stream_index = out_stream->index;
                av_interleaved_write_frame(out_fmt_ctx.get(), packet.get());
            }

            // Write trailer
            ret = av_write_trailer(out_fmt_ctx.get());
            if (ret < 0)
            {
                return av_err_str(ret, "Error writing trailer");
            }

            return ""; // Success
        }

    private:
        std::string av_err_str(int errnum, const char *prefix)
        {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(errnum, errbuf, sizeof(errbuf));
            return std::string(prefix) + ": " + errbuf;
        }
    };

} // anonymous namespace

extern "C"
{

    UNICONV_EXPORT UniconvPluginInfo *uniconv_plugin_info(void)
    {
        return &plugin_info;
    }

    UNICONV_EXPORT UniconvResult *uniconv_plugin_execute(const UniconvRequest *request)
    {
        UniconvResult *result = static_cast<UniconvResult *>(calloc(1, sizeof(UniconvResult)));
        if (!result)
        {
            return nullptr;
        }

        // Validate input
        if (!request || !request->source)
        {
            result->status = UNICONV_ERROR;
            result->error = strdup("Invalid request: missing source");
            return result;
        }

        std::string source_path = request->source;

        // Check input file exists
        if (!file_exists(source_path))
        {
            result->status = UNICONV_ERROR;
            std::string msg = "Input file not found: " + source_path;
            result->error = strdup(msg.c_str());
            return result;
        }

        // Determine output path
        std::string output_path;
        if (request->output)
        {
            output_path = request->output;
            // Ensure .gif extension
            if (output_path.size() < 4 ||
                output_path.substr(output_path.size() - 4) != ".gif")
            {
                size_t dot = output_path.rfind('.');
                size_t slash = output_path.find_last_of("/\\");
                if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
                {
                    output_path += ".gif";
                }
            }
        }
        else
        {
            // Use input stem + .gif
            size_t dot = source_path.rfind('.');
            if (dot != std::string::npos)
            {
                output_path = source_path.substr(0, dot) + ".gif";
            }
            else
            {
                output_path = source_path + ".gif";
            }
        }

        // Check if output exists (unless force)
        if (!request->force && file_exists(output_path))
        {
            result->status = UNICONV_ERROR;
            std::string msg = "Output file already exists: " + output_path +
                              " (use --force to overwrite)";
            result->error = strdup(msg.c_str());
            return result;
        }

        // Get options
        VideoToGifConverter::Options opts;
        opts.fps = get_option_int(request, "fps", 10);
        opts.width = get_option_int(request, "width", -1);
        opts.start = get_option_float(request, "start", 0.0f);
        opts.duration = get_option_float(request, "duration", 0.0f);
        opts.loop = get_option_int(request, "loop", 0);

        // Clamp FPS to reasonable range
        if (opts.fps < 1)
            opts.fps = 1;
        if (opts.fps > 30)
            opts.fps = 30;

        // Dry run
        if (request->dry_run)
        {
            result->status = UNICONV_SUCCESS;
            result->output = strdup(output_path.c_str());

            std::ostringstream extra;
            extra << "{\"dry_run\": true, \"fps\": " << opts.fps;
            if (opts.width > 0)
                extra << ", \"width\": " << opts.width;
            if (opts.start > 0)
                extra << ", \"start\": " << opts.start;
            if (opts.duration > 0)
                extra << ", \"duration\": " << opts.duration;
            extra << ", \"loop\": " << opts.loop << "}";
            result->extra_json = strdup(extra.str().c_str());

            return result;
        }

        // Convert
        VideoToGifConverter converter;
        std::string error = converter.convert(source_path, output_path, opts);

        if (!error.empty())
        {
            result->status = UNICONV_ERROR;
            result->error = strdup(error.c_str());
            return result;
        }

        // Verify output was created
        if (!file_exists(output_path))
        {
            result->status = UNICONV_ERROR;
            result->error = strdup("Conversion completed but output file not found");
            return result;
        }

        // Success
        result->status = UNICONV_SUCCESS;
        result->output = strdup(output_path.c_str());
        result->output_size = get_file_size(output_path);

        std::ostringstream extra;
        extra << "{\"fps\": " << opts.fps;
        if (opts.width > 0)
            extra << ", \"width\": " << opts.width;
        if (opts.start > 0)
            extra << ", \"start\": " << opts.start;
        if (opts.duration > 0)
            extra << ", \"duration\": " << opts.duration;
        extra << ", \"loop\": " << opts.loop << "}";
        result->extra_json = strdup(extra.str().c_str());

        return result;
    }

    UNICONV_EXPORT void uniconv_plugin_free_result(UniconvResult *result)
    {
        if (result)
        {
            free(result->output);
            free(result->error);
            free(result->extra_json);
            free(result);
        }
    }

} // extern "C"
