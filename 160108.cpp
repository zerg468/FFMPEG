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
const char *file_input = "F://akiyo.yuv", *file_out = "F://sample.mp4";
// Function Declaration
int get_header_input(int check_yuv); //get input_file header information function, return is videostream

//void encode_init(AVFrame **pictureEncoded, AVCodecContext **ctxEncode); // encode init function
void encode_init(AVCodecContext **ctxEncode); // encode init function

void pictureEncoded_init(AVFrame **pictureEncoded);

void decode_init(int videoStream, AVCodecContext **pCodecCtx); // decode init function
void init_parameter(AVPacket *output_pkt); // parameter initiate function

AVStream *output_set(int videoStream); //output file setting function
AVStream *add_stream(AVStream *instream, enum AVCodecID codec_id ,AVCodec **codecDecode); //codec parameter set

void open_video(AVStream *st, AVCodec *codecDecode); // codec open, frame set

void display_info(AVFormatContext *pFmtCtx, AVCodecContext *ctxDecode, AVCodecContext *ctxEncode, double time, int frame_num);// show encoder information

int check_file();

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


int main(void)
{

	int frame = 0, ret = 0, got_picture = 0, frameFinished = 0, videoStream = 0, check_yuv = 0;
	int frame_size = 0, bitrate = 0, frame_num = 1;
	struct SwsContext *sws_ctx = NULL;
	AVStream *video_st = NULL;
	AVCodecContext    *pCodecCtx = NULL, *ctxEncode = NULL;
	AVFrame           *pFrame = NULL, *pictureEncoded = NULL;
	AVPacket          input_pkt, output_pkt;
	clock_t start_t, end_t;
	double time;
	int count = 0;
	int i = 0;
	int streamIdx;
	enum AVMediaType mediaType;

	check_yuv = check_file();

	start_t = clock();
	// Register all formats and codecs
	av_register_all();

	// get input_file header information function, return is videostream
	//videoStream = get_header_input(check_yuv);

	//encode,decode init function
	//decode_init(videoStream, &pCodecCtx);
	//encode_init(&ctxEncode);

	//video_st = output_set(videoStream);

	if (open_input_file(check_yuv) < 0)
		exit(1);

	if (open_output_file() < 0)
		exit(1);


	//initialize packet, set data to NULL
	av_init_packet(&input_pkt);

	(input_pkt).data = NULL;
	(input_pkt).size = 0;





	// initialize SWS context for software scaling
	sws_ctx = sws_getContext(clip_width, clip_height, pFormatCtx->streams[videoStream]->codec->pix_fmt, clip_width, clip_height, PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);


	while (av_read_frame(pFormatCtx, &input_pkt) >= 0) {

		count++;
		streamIdx = input_pkt.stream_index;
		mediaType = pFormatCtx->streams[streamIdx]->codec->codec_type;
		av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n", streamIdx);
		av_log(NULL, AV_LOG_DEBUG, "Going to reencode \n");

		pFrame = av_frame_alloc();

		if (!pFrame){
			av_log(NULL, AV_LOG_ERROR, "pFrame allocating is error\n");
			exit(-1);
		}

		av_packet_rescale_ts(&input_pkt, pFormatCtx->streams[streamIdx]->time_base, pFormatCtx->streams[streamIdx]->codec->time_base);

		if (mediaType == AVMEDIA_TYPE_VIDEO){

			//ret = avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &input_pkt); 		// Decode video frame (input_pkt-> pFrame)
			ret = avcodec_decode_video2(pFormatCtx->streams[streamIdx]->codec, pFrame, &frameFinished, &input_pkt); 		// Decode video frame (input_pkt-> pFrame)

			if (ret < 0)
			{
				av_frame_free(&pFrame);
				av_log(NULL, AV_LOG_ERROR, "Decoing is failed\n");
				exit(-1);
			}

			if (frameFinished){

				frame++;

				pictureEncoded_init(&pictureEncoded);

				//sws_scale(sws_ctx, (const uint8_t * const *)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pictureEncoded->data, pictureEncoded->linesize);
				sws_scale(sws_ctx, (const uint8_t * const *)pFrame->data, pFrame->linesize, 0, clip_height, pictureEncoded->data, pictureEncoded->linesize);

				//pictureEncoded->pts = frame;
				pictureEncoded->pts = av_frame_get_best_effort_timestamp(pFrame);

				init_parameter(&output_pkt); //init parameter function

				//pictureEncoded-> output_pkt
				//avcodec_encode_video2(ctxEncode, &output_pkt, pictureEncoded, &got_picture);
				avcodec_encode_video2(ofmt_ctx->streams[streamIdx]->codec, &output_pkt, pictureEncoded, &got_picture);

				//if the function is working
				if (got_picture){

					output_pkt.stream_index = input_pkt.stream_index;
					//av_packet_rescale_ts(&output_pkt, ctxEncode->time_base, video_st->time_base);
					//av_packet_rescale_ts(&output_pkt, ctxEncode->time_base, ofmt_ctx->streams[streamIdx]->time_base);
					av_packet_rescale_ts(&output_pkt, ofmt_ctx->streams[streamIdx]->codec->time_base, ofmt_ctx->streams[streamIdx]->time_base);

					//frame_size = output_pkt.size / 8;
					//bitrate = (frame_size * 8) / av_q2d(ctxEncode->time_base) / 1000.0;

					//printf("frame= %5d   size= %6d Byte   type= %c\n",	frame_num++, frame_size, ctxEncode->coded_frame ? av_get_picture_type_char(ctxEncode->coded_frame->pict_type) : 'I');

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
			//av_free_packet(&output_pkt);


		}
	}

	frame = 1;
	for (i = 0; i < pFormatCtx->nb_streams; i++){

		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
			if (!(ofmt_ctx->streams[i]->codec->codec->capabilities & CODEC_CAP_DELAY))
				return 0;

			while (1){

				av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder \n", frame++);

				if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
				{
					pictureEncoded_init(&pictureEncoded);

					//sws_scale(sws_ctx, (const uint8_t * const *)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pictureEncoded->data, pictureEncoded->linesize);
					sws_scale(sws_ctx, (const uint8_t * const *)pFrame->data, pFrame->linesize, 0, clip_height, pictureEncoded->data, pictureEncoded->linesize);

					//pictureEncoded->pts = frame;
					pictureEncoded->pts = av_frame_get_best_effort_timestamp(pFrame);

					init_parameter(&output_pkt); //init parameter function

					//ret = avcodec_encode_video2(ctxEncode, &output_pkt, pictureEncoded, &got_picture);
					ret = avcodec_encode_video2(ofmt_ctx->streams[i]->codec, &output_pkt, pictureEncoded, &got_picture);

					if (ret < 0)
						break;

					output_pkt.stream_index = i;
					//av_packet_rescale_ts(&output_pkt, ctxEncode->time_base, ofmt_ctx->streams[streamIdx]->time_base);
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


	end_t = clock();
	time = ((double)(end_t - start_t)) / CLOCKS_PER_SEC;
	//display_info(pFormatCtx, ofmt_ctx, pCodecCtx, ctxEncode, time, frame_num);

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

	// Close the codecs
	//avcodec_close(pCodecCtx);

	// Close the video file
	//avcodec_close(ctxEncode);

	printf("count = %5d   frame= %5d  inside_decode : %5d \n", count, frame, frame_num);

	return 0;
}



AVStream *output_set(int videoStream){

	int ret = 0;
	AVOutputFormat *ofmt = NULL;
	AVCodec *codecDecode = NULL;
	AVStream *video_st = NULL;

	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, file_out);
	if (!(ofmt_ctx)) {
		av_log(NULL, AV_LOG_ERROR, "Could not create output context\m");
		exit(1);
	}


	ofmt = ofmt_ctx->oformat;

	ofmt->video_codec = AV_CODEC_ID_H264;

	// codec parameters set function
	if (ofmt->video_codec != AV_CODEC_ID_NONE)
	//	video_st = add_stream(ofmt->video_codec,&codecDecode);

	if (video_st)
		open_video(video_st, codecDecode);


	//av_dump_format((*ofmt_ctx), 0, file_out, 1); 


	/* open the output file, if needed */
	if (!(ofmt->flags & AVFMT_NOFILE)) {

		ret = avio_open(&(ofmt_ctx)->pb, file_out, AVIO_FLAG_WRITE);

		if (ret < 0) {
			char buf[256];
			av_strerror(ret, buf, sizeof(buf));
			fprintf(stderr, "Could not open '%s': %s\n", file_out, buf);
			exit(1);
		}
	}

	return video_st;
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

void decode_init(int videoStream, AVCodecContext **pCodecCtx)
{
	AVCodec *pCodec = NULL;

	// Find the decoder for the video stream
	pCodec = avcodec_find_decoder(pFormatCtx->streams[videoStream]->codec->codec_id);
	if (pCodec == NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		exit(0); // Codec not found
	}

	// Avcodeccontext structure is needed for opening codec
	(*pCodecCtx) = avcodec_alloc_context3(pCodec);

	// Get a pointer to the codec context for the video stream
	(*pCodecCtx) = pFormatCtx->streams[videoStream]->codec;

	// Open codec
	if (avcodec_open2((*pCodecCtx), pCodec, NULL)<0)
		exit(0); // Could not open codec

	clip_width = (*pCodecCtx)->width;
	clip_height = (*pCodecCtx)->height;

}

void encode_init(AVCodecContext **ctxEncode)
{

	AVCodec *codecEncode = NULL;

	codecEncode = avcodec_find_encoder(CODEC_ID_H264);
	if (!codecEncode) {
		printf("codec not found\n");
		exit(1);
	}

	(*ctxEncode) = avcodec_alloc_context3(codecEncode);

	(*ctxEncode)->coder_type = AVMEDIA_TYPE_VIDEO; // codec_type : VIDEO

	//(*ctxEncode)->flags |= CODEC_FLAG_PSNR;
	(*ctxEncode)->flags |= CODEC_FLAG_LOOP_FILTER; // flags=+loop filter

	(*ctxEncode)->time_base.num = 1; //fps
	(*ctxEncode)->time_base.den = 30;

	(*ctxEncode)->width = clip_width; //frame size
	(*ctxEncode)->height = clip_height;

	(*ctxEncode)->pix_fmt = PIX_FMT_YUV420P;

	(*ctxEncode)->qmin = 10; // qmin=10
	(*ctxEncode)->qmax = 51; // qmax=51

	(*ctxEncode)->profile = FF_PROFILE_H264_HIGH_422; //high profile

	/* open codec for encoder*/
	if (avcodec_open2((*ctxEncode), codecEncode, NULL) < 0) {
		printf("could not open codec\n");
		exit(1);
	}



}

int get_header_input(int check_yuv)
{

	int ret = 0;
	int videoStream = 0;
	int i = 0;

	// Open video file and read header
	//first parameter : I/O and information of Muxing/DeMuxing
	//second parameter : input source
	ret = avformat_open_input(&pFormatCtx, file_input, NULL, NULL);
	if (ret != 0){
		av_log(NULL, AV_LOG_ERROR, "file [%s] Open Fail(ret : %d)\n", file_input, ret);
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

	//Retrive stream information
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
		exit(1);

	//Dump information about file onto standard error 
	//av_dump_format(*pFormatCtx, 0, FilePath, 0); 


	// find index for videostream
	// pFormatCtx -> nb_streams means that numbers of file streams
	videoStream = -1;

	for (i = 0; i<(pFormatCtx)->nb_streams; i++)
		if ((pFormatCtx)->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}

	// Didn't find a video stream, finished file
	if (videoStream == -1)
		exit(0);


	return videoStream;
}

int check_file(){

	int check_yuv = 0, len = 0;

	len = strlen(file_input);

	if (file_input[len - 3] == 'y' && file_input[len - 2] == 'u' && file_input[len - 1] == 'v'){
		check_yuv = 1;
	}

	return check_yuv;
}

void display_info(AVFormatContext *pFmtCtx, AVCodecContext *ctxDecode, AVCodecContext *ctxEncode, double time, int frame_num)
{
	int in_size = 0, out_size = 0;
	int bitrate = 0;
	char *string = NULL;
	int hours, mins, secs, us;
	int64_t duration = pFmtCtx->duration + 5000;

	secs = duration / AV_TIME_BASE;
	us = duration % AV_TIME_BASE;
	mins = secs / 60;
	secs %= 60;
	hours = mins / 60;
	mins %= 60;

	in_size = avio_size(pFmtCtx->pb);
	if (in_size <= 0) // FIXME improve avio_size() so it works with non seekable output too
		in_size = avio_tell(pFmtCtx->pb);

	out_size = avio_size(ofmt_ctx->pb);
	if (out_size <= 0)
		out_size = avio_tell(ofmt_ctx->pb);

	printf("\n\n################################\n");
	printf("#\t Files\n");
	printf("################################\n");

	printf("Input File\t = %s\n", file_input);
	printf("Input File Size  = %dkB\n", in_size / 1000);
	printf("Frame Rate\t = %d\n", ctxEncode->time_base.den);
	printf("Source Width\t = %d\n", ctxDecode->width);
	printf("Source Height\t = %d\n", ctxDecode->height);
	printf("Source Duration\t = %02d:%02d:%02d.%02d\n", hours, mins, secs, (100 * us) / AV_TIME_BASE);
	printf("Source Bitrate\t = %d kbits/s\n\n", pFmtCtx->bit_rate / 1000);

	printf("Frame Encoded\t = %d\n\n", frame_num);

	printf("Output File\t = %s\n", file_out);
	printf("Output File Size = %dkB\n", out_size / 1000);
	printf("Output Width\t = %d\n", ctxEncode->width);
	printf("Output Height\t = %d\n", ctxEncode->height);


	printf("\n\n################################\n");
	printf("#\t Encoder Control\n");
	printf("################################\n");

	string = (ctxEncode->profile == FF_PROFILE_H264_BASELINE) ? "Baseline" :
		(ctxEncode->profile == FF_PROFILE_H264_MAIN) ? "Main" :
		(ctxEncode->profile == FF_PROFILE_H264_EXTENDED) ? "Extended" :
		(ctxEncode->profile == FF_PROFILE_H264_HIGH) ? "High" :
		(ctxEncode->profile == FF_PROFILE_H264_HIGH_10) ? "High10" :
		(ctxEncode->profile == FF_PROFILE_H264_HIGH_422) ? "High422" :
		(ctxEncode->profile == FF_PROFILE_H264_HIGH_444) ? "High444" : "Unknown";
	printf("profile\t\t\t = %s\n", string);
	printf("level\t\t\t = %d\n", ctxEncode->level);

	printf("bit rate\t\t = %d\n", ctxEncode->bit_rate);
	printf("bit rate tolerance\t = %d\n", ctxEncode->bit_rate_tolerance);
	printf("GOP size\t\t = %d\n", ctxEncode->gop_size);
	printf("minimum GOP size\t = %d\n", ctxEncode->keyint_min);
	string = (ctxEncode->pix_fmt == AV_PIX_FMT_YUV420P) ? "YUV420" :
		(ctxEncode->pix_fmt == AV_PIX_FMT_YUV422P) ? "YUV422" :
		(ctxEncode->pix_fmt == AV_PIX_FMT_YUV444P) ? "YUV444" : "Unknown";
	printf("pixel format\t\t = %s\n", string);
	printf("ME method\t\t = %d\n", ctxEncode->me_method);
	printf("ME range\t\t = %d\n", ctxEncode->me_range);
	printf("maximum B-frame\t\t = %d\n", ctxEncode->max_b_frames);
	printf("luminance masking\t = %.2f\n", ctxEncode->lumi_masking);
	printf("temporal_cplx_masking\t = %.2f\n", ctxEncode->temporal_cplx_masking);
	printf("spatial_cplx_masking\t = %.2f\n", ctxEncode->spatial_cplx_masking);
	printf("p_masking\t\t = %.2f\n", ctxEncode->p_masking);
	printf("refs\t\t\t = %d\n", ctxEncode->refs);
	printf("q compress\t\t = %.2f\n", ctxEncode->qcompress);
	printf("q blur\t\t\t = %.2f\n", ctxEncode->qblur);
	printf("q min\t\t\t = %d\n", ctxEncode->qmin);
	printf("q max\t\t\t = %d\n", ctxEncode->qmax);
	printf("max_qdiff\t\t = %d\n", ctxEncode->max_qdiff);
	printf("trellis qunatization\t = %d\n", ctxEncode->trellis);
	printf("DCT algorithm\t\t = %d\n", ctxEncode->dct_algo);
	printf("IDCT algorithm\t\t = %d\n", ctxEncode->idct_algo);

	printf("\n\n################################\n");
	printf("#\t Result\n");
	printf("################################\n");

	printf("Encoding Time %.2f second\n", time);
	printf("Input File Size\t = %dkB\n", in_size / 1000);
	printf("Output File Size = %dkB\n", out_size / 1000);
	printf("Compression \t = %.3f\n", out_size / (float)in_size);
	printf("Output Bitrate\t = %d kbits/s\n\n", (out_size * 8) / (duration / AV_TIME_BASE) / 1000);
}
