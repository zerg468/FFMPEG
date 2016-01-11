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

AVFormatContext *inFmtCtx = NULL,*outFmtCtx = NULL;
int time_base_den = 0, time_base_num = 0;
const char *file_input = "F://airpano.mp4";
const char *outFilename = "F://sample2.mp4";
int clip_width = 0, clip_height = 0;


int open_input_file(int check_yuv);
int open_output_file();
int encode_write_frame(AVFrame *frame, int streamIdx, int *gotFrame);
int flush_encoder(unsigned int streamIdx);
int check_file();
AVStream *add_stream(AVStream *instream, enum AVCodecID codec_id, AVCodec **codecDecode); //codec parameter set


int main()
{
	int ret=0, check_yuv = 0;
	AVPacket pkt;
	AVFrame *frame = NULL;
	enum AVMediaType mediaType;
	unsigned int streamIdx;
	unsigned int i;
	int gotFrame;

	check_yuv = check_file();

	//inititialize all the registers
	av_register_all();

	if (open_input_file(check_yuv) < 0) exit(1);
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

		av_packet_rescale_ts(&pkt,inFmtCtx->streams[streamIdx]->time_base,	inFmtCtx->streams[streamIdx]->codec->time_base);

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
	//av_frame_free(&frame);
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

	return 0;


}


int check_file(){

	int check_yuv = 0, len = 0;

	len = strlen(file_input);

	if (file_input[len - 3] == 'y' && file_input[len - 2] == 'u' && file_input[len - 1] == 'v'){
		check_yuv = 1;
	}

	return check_yuv;
}

int open_input_file(int check_yuv)
{
	AVStream *stream = NULL;
	AVCodecContext *codecCtx = NULL;
	AVCodec *dec = NULL;
	int ret;
	int streamIdx = 0;
	unsigned int i;

	/*
	*	reads file header and stores information
	*	about the file format in the AVFormatContext structure
	*	@param fmt, options If NULL acuto-detect file format,
	*						buffer size, and format options
	*/
	

	ret = avformat_open_input(&inFmtCtx, file_input, NULL, NULL);
	if (ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot open input file [%s]\n", file_input);
		return ret;
	}

	if (check_yuv == 1){
		printf("it is yuvfile\n");
		printf("So you must input media size\n");
		printf("width : ");
		scanf("%d", &(inFmtCtx)->streams[streamIdx]->codec->width);
		printf("height : ");
		scanf("%d", &(inFmtCtx)->streams[streamIdx]->codec->height);

	}


	av_log(NULL, AV_LOG_INFO, "File [%s] Open Success\n", file_input);
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

		if (codecCtx->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			streamIdx = i;
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

	clip_width = inFmtCtx->streams[streamIdx]->codec->width;
	clip_height = inFmtCtx->streams[streamIdx]->codec->height;
	time_base_den = inFmtCtx->streams[streamIdx]->codec->time_base.den;
	time_base_num = inFmtCtx->streams[streamIdx]->codec->time_base.num;

	//debugging function
	av_dump_format(inFmtCtx, 0, file_input, 0);

	return 0;
}

int open_output_file()
{
	AVStream *outStream = NULL;
	AVStream *inStream = NULL;
	AVCodecContext *decCtx = NULL, *encCtx = NULL;
	AVOutputFormat *ofmt = NULL;
	AVCodec *encoder = NULL;
	int ret;
	int streamIdx = 0;
	unsigned int i;

	avformat_alloc_output_context2(&outFmtCtx, NULL, NULL, outFilename);
	if (!outFmtCtx)
	{
		av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
		return AVERROR_UNKNOWN;
	}

	ofmt = outFmtCtx->oformat;
	ofmt->video_codec = ENCODER_ID;

	if (ofmt->video_codec != AV_CODEC_ID_NONE)
		outStream = add_stream(inStream, ofmt->video_codec, &encoder);

	if (outStream)
	{
		encCtx = outStream->codec;
		ret = avcodec_open2(encCtx, encoder, NULL);
		if (ret < 0)
		{
			av_log(NULL, AV_LOG_ERROR, "Could not open video codec\n");
			return ret;

		}
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

	outFmtCtx->streams[streamIdx]->codec->time_base.den = time_base_den;
	outFmtCtx->streams[streamIdx]->codec->time_base.num = time_base_num;


	return 0;
}

int encode_write_frame(AVFrame *frame, int streamIdx, int *gotFrame)
{
	int ret;
	int gotFrame_local;
	AVPacket enc_pkt;

	if (!gotFrame) gotFrame = &gotFrame_local;

	//av_log(NULL, AV_LOG_INFO, "Encoding frame\n");
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

	if (!(outFmtCtx->streams[streamIdx]->codec->codec->capabilities & CODEC_CAP_DELAY))
		return 0;

	while (1) {
		//av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", streamIdx);
		ret = encode_write_frame(NULL, streamIdx, &got_frame);
		if (ret < 0)
			break;
		if (!got_frame)
			return 0;
	}
	return ret;
}

AVStream *add_stream(AVStream *instream, enum AVCodecID codec_id, AVCodec **codecDecode)
{
	AVStream *st;
	AVCodecContext *ctxDecode = NULL;
	AVCodecContext *decCtx = NULL;
	int i = 0, videoStream = 0;

	/* find the encoder */
	(*codecDecode) = avcodec_find_encoder(codec_id);

	if (!(*codecDecode)) {
		fprintf(stderr, "Could not find encoder for '%s'\n", avcodec_get_name(codec_id));
		exit(1);
	}

	st = avformat_new_stream(outFmtCtx, *codecDecode); //add a new stream to media file.

	if (!st) {
		fprintf(stderr, "Could not allocate stream\n");
		exit(1);
	}

	for (i = 0; i<(inFmtCtx)->nb_streams; i++)
		if ((inFmtCtx)->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}

	st->id = videoStream; // stream index

	ctxDecode = st->codec; //save the information to codeccontext

	instream = inFmtCtx->streams[videoStream];
	decCtx = instream->codec;

	if ((*codecDecode)->type == AVMEDIA_TYPE_VIDEO)
	{
		///* Resolution must be a multiple of two. */
		ctxDecode->width = clip_width;
		ctxDecode->height = clip_height;

		//ctxDecode->coder_type = AVMEDIA_TYPE_VIDEO;// codec_type : VIDEO
		ctxDecode->coder_type = FF_CODER_TYPE_VLC;

		ctxDecode->qmin = 20; // qmin=20
		ctxDecode->qmax = 51; // qmax=51

		//(*ctxEncode)->flags |= CODEC_FLAG_PSNR;
		ctxDecode->flags |= CODEC_FLAG_LOOP_FILTER; // flags=+loop filter

		ctxDecode->sample_aspect_ratio = ctxDecode->sample_aspect_ratio;

		ctxDecode->pix_fmt = (*codecDecode)->pix_fmts[0];

		ctxDecode->profile = FF_PROFILE_H264_HIGH_422; //high profile

	}

	/* Some formats want stream headers to be separate. */
	if (outFmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
		ctxDecode->flags |= CODEC_FLAG_GLOBAL_HEADER;


	avcodec_close(ctxDecode);


	return st;

}
