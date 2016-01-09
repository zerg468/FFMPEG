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

int clip_width = 0, clip_height = 0; // resolution size
AVFormatContext    *pFormatCtx = NULL, *ofmt_ctx = NULL;
const char *file_input = "F://ring.mp4", *file_out = "F://sample.mp4";

// Function Declaration

//void encode_init(AVFrame **pictureEncoded, AVCodecContext **ctxEncode); // encode init function
void encode_init(AVCodecContext **ctxEncode); // encode init function

void pictureEncoded_init(AVFrame **pictureEncoded);

void init_parameter(AVPacket *output_pkt); // parameter initiate function

AVStream *add_stream(AVStream *instream, enum AVCodecID codec_id, AVCodec **codecDecode); //codec parameter set

void open_video(AVStream *st, AVCodec *codecDecode); // codec open, frame set

int check_file();

int open_input_file(int check_yuv);

int open_output_file();


int main(void)
{

	int frame = 0, ret = 0, got_picture = 0, frameFinished = 0, check_yuv = 0;
	int frame_num = 1;
	struct SwsContext *sws_ctx = NULL;
	AVStream *video_st = NULL;
	AVFrame           *pFrame = NULL, *pictureEncoded = NULL;
	AVPacket          input_pkt, output_pkt;
	int count = 0;
	int i = 0;
	int streamIdx;
	enum AVMediaType mediaType;

	check_yuv = check_file();

	// Register all formats and codecs
	av_register_all();


	if (open_input_file(check_yuv) < 0)
		exit(1);

	if (open_output_file() < 0)
		exit(1);


	//initialize packet, set data to NULL
	av_init_packet(&input_pkt);

	(input_pkt).data = NULL;
	(input_pkt).size = 0;


	// initialize SWS context for software scaling
	sws_ctx = sws_getContext(clip_width, clip_height, pFormatCtx->streams[0]->codec->pix_fmt, clip_width, clip_height, PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);

	

	while (av_read_frame(pFormatCtx, &input_pkt) >= 0) {

		count++;
		streamIdx = input_pkt.stream_index;
		mediaType = pFormatCtx->streams[streamIdx]->codec->codec_type;
		av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n", streamIdx);
		av_log(NULL, AV_LOG_DEBUG, "Going to reencode \n");

		pFrame = av_frame_alloc();
		pictureEncoded_init(&pictureEncoded);

		if (!pFrame){
			av_log(NULL, AV_LOG_ERROR, "pFrame allocating is error\n");
			exit(-1);
		}

		av_packet_rescale_ts(&input_pkt, pFormatCtx->streams[streamIdx]->time_base, pFormatCtx->streams[streamIdx]->codec->time_base);

		if (mediaType == AVMEDIA_TYPE_VIDEO){


			ret = avcodec_decode_video2(pFormatCtx->streams[streamIdx]->codec, pFrame, &frameFinished, &input_pkt); 		// Decode video frame (input_pkt-> pFrame)

			if (ret < 0)
			{
				av_frame_free(&pFrame);
				av_log(NULL, AV_LOG_ERROR, "Decoing is failed\n");
				exit(-1);
			}
		}
		
			if (frameFinished){

				frame++;


				sws_scale(sws_ctx, (const uint8_t * const *)pFrame->data, pFrame->linesize, 0, clip_height, pictureEncoded->data, pictureEncoded->linesize);

				//pictureEncoded->pts = frame;
				pictureEncoded->pts = av_frame_get_best_effort_timestamp(pFrame);

				init_parameter(&output_pkt); //init parameter function


				avcodec_encode_video2(ofmt_ctx->streams[streamIdx]->codec, &output_pkt, pictureEncoded, &got_picture);

				//if the function is working
				if (got_picture){

					output_pkt.stream_index = input_pkt.stream_index;
				
					av_packet_rescale_ts(&output_pkt, ofmt_ctx->streams[streamIdx]->codec->time_base, ofmt_ctx->streams[streamIdx]->time_base);

					ret = av_interleaved_write_frame(ofmt_ctx, &output_pkt);

					if (ret < 0) {
						fprintf(stderr, "Error muxing packet\n");
						break;
					}

					frame_num++;

				}

			}

			else
				av_frame_free(&pFrame);

			av_free_packet(&input_pkt);
	
		
	}

	frame = 1;
	for (i = 0; i < pFormatCtx->nb_streams; i++){

		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
			if (!(ofmt_ctx->streams[i]->codec->codec->capabilities & CODEC_CAP_DELAY))
				return 0;

			while (1){

				//av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder \n", frame++);

				if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
				{
					frame_num++;
					pictureEncoded_init(&pictureEncoded);

					//sws_scale(sws_ctx, (const uint8_t * const *)pFrame->data, pFrame->linesize, 0, clip_height, pictureEncoded->data, pictureEncoded->linesize);


					//pictureEncoded->pts = av_frame_get_best_effort_timestamp(pFrame);

					init_parameter(&output_pkt); //init parameter function

					//ret = avcodec_encode_video2(ctxEncode, &output_pkt, pictureEncoded, &got_picture);
					ret = avcodec_encode_video2(ofmt_ctx->streams[i]->codec, &output_pkt, NULL, &got_picture);

					if (ret < 0)
						break;

					output_pkt.stream_index = i;
				
					av_packet_rescale_ts(&output_pkt, ofmt_ctx->streams[i]->codec->time_base, ofmt_ctx->streams[i]->time_base);

					ret = av_interleaved_write_frame(ofmt_ctx, &output_pkt);

					if (ret < 0) {
						fprintf(stderr, "Error muxing packet\n");
						break;
					}

				}
			}
		}

	}



	/* Write the trailer, if any. The trailer must be written before you
	* close the CodecContexts open when you wrote the header; otherwise
	* av_write_trailer() may try to use memory that was freed on
	* av_codec_close(). */
	av_write_trailer(ofmt_ctx);

	av_free_packet(&output_pkt);

	// Free the YUV frame
	av_frame_free(&pFrame);
	av_frame_free(&pictureEncoded);

	for (i = 0; i < pFormatCtx->nb_streams; i++){

		avcodec_close(pFormatCtx->streams[i]->codec);
		if (ofmt_ctx && ofmt_ctx->nb_streams > i && ofmt_ctx->streams[i] && ofmt_ctx->streams[i]->codec)
			avcodec_close(ofmt_ctx->streams[i]->codec);

	}

	avformat_close_input(&pFormatCtx);
	if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
		avio_closep(&ofmt_ctx->pb);

	avformat_free_context(ofmt_ctx);

	printf("count = %5d  inside_encode : %5d \n", count, frame_num);

	return 0;
}


void init_parameter(AVPacket *output_pkt)
{

	av_init_packet(output_pkt);

	(*output_pkt).data = NULL;
	(*output_pkt).size = 0;



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

	for (i = 0; i<(pFormatCtx)->nb_streams; i++)
		if ((pFormatCtx)->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}

	st->id = videoStream; // stream index

	ctxDecode = st->codec; //save the information to codeccontext

	instream = pFormatCtx->streams[videoStream];
	decCtx = instream->codec;

	if ((*codecDecode)->type == AVMEDIA_TYPE_VIDEO)
	{
		ctxDecode->pix_fmt = PIX_FMT_YUV420P;

		///* Resolution must be a multiple of two. */
		ctxDecode->width = clip_width;
		ctxDecode->height = clip_height;

		ctxDecode->coder_type = AVMEDIA_TYPE_VIDEO;// codec_type : VIDEO

		ctxDecode->qmin = 20; // qmin=20
		ctxDecode->qmax = 51; // qmax=51

		//(*ctxEncode)->flags |= CODEC_FLAG_PSNR;
		ctxDecode->flags |= CODEC_FLAG_LOOP_FILTER; // flags=+loop filter

		ctxDecode->sample_aspect_ratio = ctxDecode->sample_aspect_ratio;

		ctxDecode->pix_fmt = (*codecDecode)->pix_fmts[0];

		ctxDecode->profile = FF_PROFILE_H264_HIGH_422; //high profile

		ctxDecode->time_base = decCtx->time_base;


	}

	/* Some formats want stream headers to be separate. */
	if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		ctxDecode->flags |= CODEC_FLAG_GLOBAL_HEADER;


	avcodec_close(ctxDecode);


	return st;

}

void open_video(AVStream *st, AVCodec *codecDecode)
{
	int ret = 0;
	AVCodecContext *c = st->codec;

	/* open the codec */
	ret = avcodec_open2(c, codecDecode, NULL);

	if (ret < 0) {
		char buf[256];
		av_strerror(ret, buf, sizeof(buf));
		fprintf(stderr, "Could not open video codec: %s\n", buf);
		exit(1);
	}

}

void pictureEncoded_init(AVFrame **pictureEncoded)
{
	int pic_size = 0;
	uint8_t *picEncodeBuf = NULL;


	*pictureEncoded = av_frame_alloc(); //frame init();

	pic_size = avpicture_get_size(PIX_FMT_YUV420P, clip_width, clip_height);

	picEncodeBuf = (uint8_t*)malloc(3 * pic_size / 2); // size for YUV 420p

	(*pictureEncoded)->format = PIX_FMT_YUV420P;
	(*pictureEncoded)->width = clip_width;
	(*pictureEncoded)->height = clip_height;

	//set up input size
	(*pictureEncoded)->data[0] = picEncodeBuf;
	(*pictureEncoded)->data[1] = (*pictureEncoded)->data[0] + pic_size;
	(*pictureEncoded)->data[2] = (*pictureEncoded)->data[1] + pic_size / 4;
	(*pictureEncoded)->linesize[0] = clip_width;
	(*pictureEncoded)->linesize[1] = clip_width / 2;
	(*pictureEncoded)->linesize[2] = clip_width / 2;


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
	unsigned int i;

	/*
	*	reads file header and stores information
	*	about the file format in the AVFormatContext structure
	*	@param fmt, options If NULL acuto-detect file format,
	*						buffer size, and format options
	*/

	// Open video file and read header
	//first parameter : I/O and information of Muxing/DeMuxing
	//second parameter : input source
	ret = avformat_open_input(&pFormatCtx, file_input, NULL, NULL);
	if (ret < 0){
		av_log(NULL, AV_LOG_ERROR, "file [%s] Open Fail\n", file_input);
		exit(1);
	}


	if (check_yuv == 1){
		printf("it is yuvfile\n");
		printf("So you must input media size\n");
		printf("width : ");
		scanf("%d", &(pFormatCtx)->streams[0]->codec->width);
		printf("height : ");
		scanf("%d", &(pFormatCtx)->streams[0]->codec->height);

	}


	av_log(NULL, AV_LOG_INFO, "File [%s] Open Success\n", file_input); // file success debug
	av_log(NULL, AV_LOG_INFO, "Format: %s\n", pFormatCtx->iformat->name);

	ret = avformat_find_stream_info(pFormatCtx, NULL);
	//Retrive stream information
	if (ret < 0){
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information");
		exit(1);
	}

	av_log(NULL, AV_LOG_INFO, "Get Stream Information Success\n");

	//Dump information about file onto standard error 
	//av_dump_format(*pFormatCtx, 0, FilePath, 0); 

	for (i = 0; i < pFormatCtx->nb_streams; i++){

		stream = pFormatCtx->streams[i];
		codecCtx = stream->codec;

		if (codecCtx->codec_type == AVMEDIA_TYPE_VIDEO){

			//find decoder

			dec = avcodec_find_decoder(codecCtx->codec_id);
			if (dec < 0)
			{
				av_log(NULL, AV_LOG_ERROR, "Could not Find [%s] Codec\n", av_get_media_type_string(dec->type));
				exit(1);
			}

			//codecCtx = avcodec_alloc_context3(dec);

			ret = avcodec_open2(codecCtx, dec, NULL);

			if (ret < 0)
			{
				av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
				exit(1);
			}

		}
	}


	clip_width = pFormatCtx->streams[0]->codec->width;
	clip_height = pFormatCtx->streams[0]->codec->height;


	av_dump_format(pFormatCtx, 0, file_input, 0);

	return 0;

}

int open_output_file()
{
	AVStream *outStream = NULL;
	AVStream *inStream = NULL;
	AVOutputFormat *ofmt = NULL;
	AVCodecContext *decCtx = NULL, *encCtx = NULL;
	AVCodec *encoder;
	AVCodec *codecDecode = NULL;
	int ret;
	unsigned int i;

	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, file_out);
	if (!(ofmt_ctx)) {
		av_log(NULL, AV_LOG_ERROR, "Could not create output context\m");
		return -1;
	}

	ofmt = ofmt_ctx->oformat;

	ofmt->video_codec = AV_CODEC_ID_H264;

	if (ofmt->video_codec != AV_CODEC_ID_NONE)
		outStream = add_stream(inStream, ofmt->video_codec, &codecDecode);

	if (outStream)
		open_video(outStream, codecDecode);

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


	//initailze muxer, write output file header
	ret = avformat_write_header(ofmt_ctx, NULL); // Allocate the stream private data and write the stream header to an output media file. 

	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error occured when opening output file\n");
		exit(1);
	}

	return 0;

}
