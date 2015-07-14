extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avcodec.h>
}

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avcodec.lib")

#define ENCODER_ID CODEC_ID_H264

AVFormatContext *inFmtCtx;
AVFormatContext *outFmtCtx;
const char *inFilename = "C:/ffmpeg/sample.mpg";
const char *outFilename = "C:/ffmpeg/sample2.mp4";

int open_input_file() 
{
	AVStream *stream = NULL;
	AVCodecContext *codecCtx = NULL;
	AVCodec *dec = NULL;
	int ret;
	unsigned int i;

	//open input file and allocate format context
	inFmtCtx = NULL;
		/*
		*	reads file header and stores information 
		*	about the file format in the AVFormatContext structure
		*	@param fmt, options If NULL acuto-detect file format,
		*						buffer size, and format options
		*/
	ret = avformat_open_input(&inFmtCtx, inFilename, NULL, NULL);		
	if (ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot open input file [%s]\n", inFilename);
		return ret;
	}
	av_log(NULL, AV_LOG_INFO, "File [%s] Open Success\n", inFilename);
	av_log(NULL, AV_LOG_DEBUG, "Format: %s\n", inFmtCtx->iformat->name);


	//retrieve stream information
	ret = avformat_find_stream_info(inFmtCtx, NULL);
	if (ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information");
		return ret;
	}
	av_log(NULL, AV_LOG_INFO, "Get Stream Information Success\n");

	for (i = 0; i < inFmtCtx->nb_streams; i++)
	{
		stream = inFmtCtx->streams[i];
		codecCtx = stream->codec;

		if (codecCtx->codec_type == AVMEDIA_TYPE_VIDEO )
		{
			//find decoder
			dec = avcodec_find_decoder(codecCtx->codec_id);
			if (dec < 0)
			{
				av_log(NULL, AV_LOG_ERROR, "Could not Find [%s] Codec\n", av_get_media_type_string(dec->type));
				return AVERROR(EINVAL);
			}

			ret = avcodec_open2(codecCtx, dec, NULL);
			if (ret < 0)
			{
				av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
				return ret;
			}
		}
	}

	//debugging function
	av_dump_format(inFmtCtx, 0, inFilename, 0);
	return 0;
}

int open_output_file()
{
	AVStream *outStream = NULL;
	AVStream *inStream = NULL;
	AVCodecContext *decCtx = NULL, *encCtx = NULL;
	AVCodec *encoder;
	int ret;
	unsigned int i;

	outFmtCtx = NULL;
	avformat_alloc_output_context2(&outFmtCtx, NULL, NULL, outFilename);
	if (!outFmtCtx)
	{
		av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
		return AVERROR_UNKNOWN;
	}

	for (i = 0; i < inFmtCtx->nb_streams; i++)
	{
		inStream = inFmtCtx->streams[i];
		decCtx = inStream->codec;

		if (decCtx->codec_type == AVMEDIA_TYPE_VIDEO )
		{
			//create new stream for the output file
			outStream = avformat_new_stream(outFmtCtx, NULL);
			if (!outStream)
			{
				av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
				return AVERROR_UNKNOWN;
			}
			

			encCtx = outStream->codec;
			
			//encode using h.264 codec
			encoder = avcodec_find_encoder(ENCODER_ID);
			if (!encoder)
			{
				av_log(NULL, AV_LOG_ERROR, "Codec Not Found\n");
				return AVERROR_INVALIDDATA;
			}
			else av_log(NULL, AV_LOG_INFO, "%s Codec Found\n", avcodec_get_name(ENCODER_ID));
			
			encCtx->bit_rate = 40 * 1000 * 1000;			// average bitrate
			encCtx->height = decCtx->height;				// resolution
			encCtx->width = decCtx->width;
			encCtx->sample_aspect_ratio = decCtx->sample_aspect_ratio;
			encCtx->pix_fmt = encoder->pix_fmts[0];
			encCtx->time_base = decCtx->time_base;			// video time_base 
			encCtx->gop_size = 25;							// number of pictures in a gop
			encCtx->qmin = 20;                              // minimum quantizer
			encCtx->qmax = 51;                              // maximum quantizer
			encCtx->max_qdiff = 4;                          // maximum quantizer difference between frames
			encCtx->refs = 3;                               // number of reference frames
			encCtx->trellis = 1;                            // trellis RD Quantization
			//encCtx->me_method = 9;						// Motion Estimation algorithm
			encCtx->max_b_frames = 4;						// max number of B-frames bewteen non-B-frames
			encCtx->keyint_min = 15;						// minimum gop size
			encCtx->coder_type = FF_CODER_TYPE_VLC;	
		
			ret = avcodec_open2(encCtx, encoder, NULL);
			if (ret < 0)
				{
					av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder for stream #%u\n", i);
					return ret;
				}
		}
		else if (decCtx->codec_type == AVMEDIA_TYPE_UNKNOWN)
		{
			av_log(NULL, AV_LOG_FATAL, "Elementary stream #%d is of unknown type, cannot proceed\n", i);
			return AVERROR_INVALIDDATA;
		}
		// if this stream must be remuxed 
		//else {
		//	ret = avcodec_copy_context(outFmtCtx->streams[i]->codec, inFmtCtx->streams[i]->codec);
		//	if (ret < 0) {
		//		av_log(NULL, AV_LOG_ERROR, "Copying stream context failed\n");
		//		return ret;
		//	}
		//}


		if (outFmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
			encCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	av_dump_format(outFmtCtx, 0, outFilename, 1);

	if (!(outFmtCtx->oformat->flags & AVFMT_NOFILE))
	{
		ret = avio_open(&outFmtCtx->pb, outFilename, AVIO_FLAG_WRITE);
		if (ret < 0)
		{
			av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", outFilename);
			return ret;
		}
	}

	//initialize muxer, write output file header
	ret = avformat_write_header(outFmtCtx, NULL);
	if (ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
		return ret;
	}

	return 0;
}

int encode_write_frame(AVFrame *frame, int streamIdx, int *gotFrame)
{
	int ret;
	int gotFrame_local;
	AVPacket enc_pkt;

	if (!gotFrame) gotFrame = &gotFrame_local;

	av_log(NULL, AV_LOG_INFO, "Encoding frame\n");
	//Encode
	av_init_packet(&enc_pkt);
	enc_pkt.data = NULL;
	enc_pkt.size = 0;

	if (inFmtCtx->streams[streamIdx]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
	{
		ret = avcodec_encode_video2(outFmtCtx->streams[streamIdx]->codec, &enc_pkt, frame, gotFrame);
		av_frame_free(&frame);
		if (ret < 0) return ret;
		if (!(*gotFrame)) return 0;

		//prepare packet for muxing
		enc_pkt.stream_index = streamIdx;
		av_packet_rescale_ts(&enc_pkt,
			outFmtCtx->streams[streamIdx]->codec->time_base,
			outFmtCtx->streams[streamIdx]->time_base);

		av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");

		// mux encoded frame 
		ret = av_interleaved_write_frame(outFmtCtx, &enc_pkt);
		return ret;
	}
}

int flush_encoder(unsigned int streamIdx)
{
	int ret;
	int got_frame;

	if (!(outFmtCtx->streams[streamIdx]->codec->codec->capabilities &
		CODEC_CAP_DELAY))
		return 0;

	while (1) {
		av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", streamIdx);
		ret = encode_write_frame(NULL, streamIdx, &got_frame);
		if (ret < 0)
			break;
		if (!got_frame)
			return 0;
	}
	return ret;
}

int main()
{
	int ret;
	AVPacket pkt;
	AVFrame *frame = NULL;
	enum AVMediaType mediaType;
	unsigned int streamIdx;
	unsigned int i;
	int gotFrame;


	//inititialize all the registers
	av_register_all();

	if (open_input_file() < 0) exit(1);
	if (open_output_file() < 0) exit(1);

	//initialize packet, set data to NULL
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	//read all packets
	while (av_read_frame(inFmtCtx, &pkt) >= 0)
	{
		streamIdx = pkt.stream_index;
		mediaType = inFmtCtx->streams[streamIdx]->codec->codec_type;
		av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n", streamIdx);
		av_log(NULL, AV_LOG_DEBUG, "Going to reencode \n");
		frame = av_frame_alloc();
		if (!frame)
		{
			ret = AVERROR(ENOMEM);
			break;
		} 
		av_packet_rescale_ts(&pkt,
			inFmtCtx->streams[streamIdx]->time_base,
			inFmtCtx->streams[streamIdx]->codec->time_base);
		if (mediaType == AVMEDIA_TYPE_VIDEO) {
			ret = avcodec_decode_video2(inFmtCtx->streams[streamIdx]->codec, frame, &gotFrame, &pkt);
			if (ret < 0)
			{
				av_frame_free(&frame);
				av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
				break;
			}
		}

		if (gotFrame) 
		{
			frame->pts = av_frame_get_best_effort_timestamp(frame);
			ret = encode_write_frame(frame, streamIdx, &gotFrame);
			//av_frame_free(&frame);
			if (ret < 0)
			{
				av_log(NULL, AV_LOG_ERROR, "Error Encoding Frame");
				exit(1);
			}
		}
		else av_frame_free(&frame);

		av_free_packet(&pkt);
	}

	//flush encoders
	for (i = 0; i < inFmtCtx->nb_streams; i++)
	{
		if (inFmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			ret = flush_encoder(i);
			if (ret < 0)
			{
				av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
				exit(1);
			}
		}
	}


	//free all
	av_write_trailer(outFmtCtx);
	av_free_packet(&pkt);
	av_frame_free(&frame);
	for (i = 0; i < inFmtCtx->nb_streams; i++) 
	{
		avcodec_close(inFmtCtx->streams[i]->codec);
		if (outFmtCtx && outFmtCtx->nb_streams > i && outFmtCtx->streams[i] && outFmtCtx->streams[i]->codec)
			avcodec_close(outFmtCtx->streams[i]->codec);
	}
	avformat_close_input(&inFmtCtx);
	if (outFmtCtx && !(outFmtCtx->oformat->flags & AVFMT_NOFILE))
		avio_closep(&outFmtCtx->pb);
	avformat_free_context(outFmtCtx);

	if (ret < 0)
		av_log(NULL, AV_LOG_ERROR, "Error occurred \n");

	return ret ? 1 : 0;
	
}
