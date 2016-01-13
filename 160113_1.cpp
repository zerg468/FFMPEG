extern "C"{
#include <libavformat/avformat.h> 
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
};

#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"swscale.lib")
#pragma comment(lib,"avutil.lib")

//#define CODEC CODEC_ID_RAWVIDEO
#define CODEC AV_CODEC_ID_H264

int clip_width = 0, clip_height = 0; // resolution size
int time_base_den = 0, time_base_num = 0;
char *file_input = "H:\capstone\\yuvfile.mov";
//char *file_input = "F:\\Aspen.mp4";
char *file_out = "H://sample.mp4";
AVFormatContext    *inFmtCtx = NULL, *ofmt_ctx = NULL;
AVFrame *pictureEncoded = NULL;
int frame_num = 1, frame_use = 1;

// Function Declaration

int open_input_file(int check_yuv);
int open_output_file();

void encode_init(AVCodecContext **ctxEncode); // encode init function
void init_parameter(AVPacket *input_pkt, AVPacket *output_pkt); // parameter initiate function

AVStream *add_stream(AVStream *instream, enum AVCodecID codec_id, AVCodec **codecDecode);

void pictureEncoded_init();
int check_file();

int encode_write_frame(AVFrame *frame, int streamIdx, int *gotFrame);
int flush_encoder(unsigned int streamIdx);


int main(void)
{

	int frame = 0, ret = 0, got_picture = 0, frameFinished = 0, videoStream = 0, check_yuv = 0;
	int frame_size = 0, bitrate = 0;
	int streamIdx = 0;
	unsigned i = 0;
	enum AVMediaType mediaType;
	struct SwsContext *sws_ctx = NULL;
	AVStream *video_st = NULL;
	AVCodecContext    *pCodecCtx = NULL, *ctxEncode = NULL;
	AVFrame           *pFrame = NULL;
	AVPacket          input_pkt, output_pkt;

	check_yuv = check_file();

	// Register all formats and codecs
	av_register_all();

	if (open_input_file(check_yuv) < 0) exit(1);
	if (open_output_file() < 0) exit(1);

	init_parameter(&input_pkt, &output_pkt); //init parameter function
	pictureEncoded_init();

	// initialize SWS context for software scaling
	sws_ctx = sws_getContext(inFmtCtx->streams[streamIdx]->codec->width, inFmtCtx->streams[streamIdx]->codec->height, inFmtCtx->streams[streamIdx]->codec->pix_fmt, clip_width, clip_height, PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);

	while (av_read_frame(inFmtCtx, &input_pkt) >= 0) {

		streamIdx = input_pkt.stream_index;
		mediaType = inFmtCtx->streams[streamIdx]->codec->codec_type;

		av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n", streamIdx);
		av_log(NULL, AV_LOG_DEBUG, "Going to reencode \n");

		pFrame = av_frame_alloc();

		if (!pFrame)
		{
			ret = AVERROR(ENOMEM);
			break;
		}

		av_packet_rescale_ts(&input_pkt, inFmtCtx->streams[videoStream]->time_base, inFmtCtx->streams[streamIdx]->codec->time_base);


		if (mediaType == AVMEDIA_TYPE_VIDEO){


			ret = avcodec_decode_video2(inFmtCtx->streams[streamIdx]->codec, pFrame, &frameFinished, &input_pkt);       // Decode video frame (input_pkt-> pFrame)


			if (ret < 0)
			{
				av_frame_free(&pFrame);
				av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
				break;
			}


			if (frameFinished){

				frame_num++;

				sws_scale(sws_ctx, (const uint8_t * const *)pFrame->data, pFrame->linesize, 0, clip_height, pictureEncoded->data, pictureEncoded->linesize);

				pictureEncoded->pts = av_frame_get_best_effort_timestamp(pFrame);

				//pictureEncoded-> output_pkt
				//avcodec_encode_video2(ctxEncode, &output_pkt, pictureEncoded, &got_picture);
				avcodec_encode_video2(ofmt_ctx->streams[streamIdx]->codec, &output_pkt, pictureEncoded, &got_picture);

				av_frame_free(&pFrame);

				//if the function is working
				if (got_picture){

					printf("Encoding %d \n", frame_use);

					frame_use++;


					av_packet_rescale_ts(&output_pkt, ofmt_ctx->streams[streamIdx]->codec->time_base, ofmt_ctx->streams[streamIdx]->time_base);

					//av_packet_rescale_ts(&output_pkt, ctxEncode->time_base, video_st->time_base);

					ret = av_interleaved_write_frame(ofmt_ctx, &output_pkt);

					if (ret < 0) {
						fprintf(stderr, "Error muxing packet\n");
						break;
					}
				}
			}

			av_free_packet(&input_pkt);
			av_free_packet(&output_pkt);

		}

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

	printf("\n\n total frame_num : %d , frame_encode:  %d \n", frame_num - 1, frame_use - 1);


	/* Write the trailer, if any. The trailer must be written before you
	* close the CodecContexts open when you wrote the header; otherwise
	* av_write_trailer() may try to use memory that was freed on
	* av_codec_close(). */
	av_write_trailer(ofmt_ctx);

	// Free the YUV frame
	av_frame_free(&pFrame);
	av_frame_free(&pictureEncoded);

	for (i = 0; i < inFmtCtx->nb_streams; i++)
	{
		avcodec_close(inFmtCtx->streams[i]->codec);
		if (ofmt_ctx && ofmt_ctx->nb_streams > i && ofmt_ctx->streams[i] && ofmt_ctx->streams[i]->codec)
			avcodec_close(ofmt_ctx->streams[i]->codec);
	}

	avformat_close_input(&inFmtCtx);

	if (ofmt_ctx&& !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
		avio_closep(&ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);

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

void init_parameter(AVPacket *input_pkt, AVPacket *output_pkt)
{

	av_init_packet(input_pkt);
	av_init_packet(output_pkt);

	(*input_pkt).data = NULL;
	(*input_pkt).size = 0;
	(*output_pkt).data = NULL;
	(*output_pkt).size = 0;


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

void pictureEncoded_init(){

	int pic_size = 0;

	uint8_t *picEncodeBuf = NULL;

	pictureEncoded = av_frame_alloc(); //frame init();

	pic_size = avpicture_get_size(PIX_FMT_YUV420P, clip_width, clip_height);

	picEncodeBuf = (uint8_t*)malloc(3 * pic_size / 2); // size for YUV 420p

	(pictureEncoded)->format = PIX_FMT_YUV420P;
	(pictureEncoded)->width = clip_width;
	(pictureEncoded)->height = clip_height;

	//set up input size
	(pictureEncoded)->data[0] = picEncodeBuf;
	(pictureEncoded)->data[1] = (pictureEncoded)->data[0] + pic_size;
	(pictureEncoded)->data[2] = (pictureEncoded)->data[1] + pic_size / 4;
	(pictureEncoded)->linesize[0] = clip_width;
	(pictureEncoded)->linesize[1] = clip_width / 2;
	(pictureEncoded)->linesize[2] = clip_width / 2;


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

	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, file_out);
	if (!ofmt_ctx)
	{
		av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
		return AVERROR_UNKNOWN;
	}

	ofmt = ofmt_ctx->oformat;
	ofmt->video_codec = CODEC;

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

	av_dump_format(ofmt_ctx, 0, file_out, 1);

	if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
	{
		ret = avio_open(&ofmt_ctx->pb, file_out, AVIO_FLAG_WRITE);
		if (ret < 0)
		{
			av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", file_out);
			return ret;
		}
	}

	//initialize muxer, write output file header
	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
		return ret;
	}

	ofmt_ctx->streams[streamIdx]->codec->time_base.den = time_base_den;
	ofmt_ctx->streams[streamIdx]->codec->time_base.num = time_base_num;


	return 0;
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

	st = avformat_new_stream(ofmt_ctx, *codecDecode); //add a new stream to media file.

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
		ctxDecode->coder_type = AVMEDIA_TYPE_VIDEO;

		ctxDecode->qmin = 20; // qmin=20
		ctxDecode->qmax = 51; // qmax=51

		//(*ctxEncode)->flags |= CODEC_FLAG_PSNR;
		ctxDecode->flags |= CODEC_FLAG_LOOP_FILTER; // flags=+loop filter

		ctxDecode->sample_aspect_ratio = ctxDecode->sample_aspect_ratio;

		ctxDecode->pix_fmt = PIX_FMT_YUV420P;

		//ctxDecode->profile = FF_PROFILE_H264_HIGH_422; //high profile

	}

	/* Some formats want stream headers to be separate. */
	if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		ctxDecode->flags |= CODEC_FLAG_GLOBAL_HEADER;


	avcodec_close(ctxDecode);


	return st;

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
		ret = avcodec_encode_video2(ofmt_ctx->streams[streamIdx]->codec, &enc_pkt, frame, gotFrame);
		av_frame_free(&frame);
		if (ret < 0) return ret;
		if (!(*gotFrame)) return 0;

		//prepare packet for muxing
		enc_pkt.stream_index = streamIdx;
		av_packet_rescale_ts(&enc_pkt, ofmt_ctx->streams[streamIdx]->codec->time_base, ofmt_ctx->streams[streamIdx]->time_base);

		av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");

		// mux encoded frame 
		ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
		return ret;
	}
}

int flush_encoder(unsigned int streamIdx)
{
	int ret;
	int got_frame;

	if (!(ofmt_ctx->streams[streamIdx]->codec->codec->capabilities & CODEC_CAP_DELAY))
		return 0;

	while (1) {

		ret = encode_write_frame(NULL, streamIdx, &got_frame);
		if (ret < 0)
			break;
		if (!got_frame)
			return 0;

		av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder %d \n", streamIdx, frame_use);
		frame_use++;

	}
	return ret;
}
