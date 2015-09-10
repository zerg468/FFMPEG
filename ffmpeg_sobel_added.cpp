extern "C" {
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

									 // Function Declaration
int get_header_input(AVFormatContext **pFormatCtx, char *FilePath, int check_yuv); //get input_file header information function, return is videostream

void encode_init(AVFrame **pictureEncoded, AVCodecContext **ctxEncode); // encode init function
void decode_init(int videoStream, AVCodecContext **pCodecCtx, AVFormatContext *pFormatCtx); // decode init function
void init_parameter(AVFrame **pFrame, AVPacket *input_pkt, AVPacket *output_pkt, struct SwsContext **sws_ctx, AVCodecContext *pCodecCtx); // parameter initiate function

AVStream *output_set(AVFormatContext **ofmt_ctx, char *file_out, int videoStream); //output file setting function
AVStream *add_stream(AVFormatContext *oc, enum AVCodecID codec_id, int videoStream, AVCodec **codecDecode); //codec parameter set

void open_video(AVFormatContext *oc, AVStream *st, AVCodec *codecDecode); // codec open, frame set

void display_info(char *file_input, char *file_output, AVFormatContext *pFmtCtx,
	AVFormatContext *ofmt_ctx, AVCodecContext *ctxDecode, AVCodecContext *ctxEncode, double time, int frame_num);// show encoder information

int check_file(char *file_input);
int sobel(int frameData[288][352]);

int main(void)
{
	char *file_input = "C:/ffmpeg/result30/mobile/cqp/cqp_30.mp4";
	char *file_out = "C:/ffmpeg/sample.mp4";
	int frame = 0, ret = 0, got_picture = 0, frameFinished = 0, videoStream = 0, check_yuv = 0;
	int frame_size = 0, bitrate = 0, frame_num = 0;
	struct SwsContext *sws_ctx = NULL;
	AVStream *video_st = NULL;
	AVFormatContext    *pFormatCtx = NULL, *ofmt_ctx = NULL;
	AVCodecContext    *pCodecCtx = NULL, *ctxEncode = NULL;
	AVFrame           *pFrame = NULL, *pictureEncoded = NULL;
	AVPacket          input_pkt, output_pkt;
	clock_t start_t, end_t;
	double time;

	check_yuv = check_file(file_input);

	start_t = clock();
	// Register all formats and codecs
	av_register_all();

	// get input_file header information function, return is videostream
	videoStream = get_header_input(&pFormatCtx, file_input, check_yuv);

	//encode,decode init function
	decode_init(videoStream, &pCodecCtx, pFormatCtx);
	encode_init(&pictureEncoded, &ctxEncode);

	init_parameter(&pFrame, &input_pkt, &output_pkt, &sws_ctx, pCodecCtx); //init parameter function

	video_st = output_set(&ofmt_ctx, file_out, videoStream);

	/* Write the stream header, if any. */
	ret = avformat_write_header(ofmt_ctx, NULL); // Allocate the stream private data and write the stream header to an output media file. 

	if (ret < 0) {
		char buf[256];
		av_strerror(ret, buf, sizeof(buf));
		fprintf(stderr, "Error occurred when opening output file: %s\n", buf);
		return 1;
	}

	AVFrameSideData *sd;
	int x = 0;
	int y = 0;
	int count = 0;
	int index = 0;
	int frame1[288][352] = { 0 };
	int frame2[288][352] = { 0 };

	double sum[50] = { 0 };
	int sobel_sum = 0;


	while (av_read_frame(pFormatCtx, &input_pkt) >= 0) {


		if (input_pkt.stream_index == AVMEDIA_TYPE_VIDEO) {

			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &input_pkt); 		// Decode video frame (input_pkt-> pFrame)

			if (frameFinished) {

				frame++;

				sws_scale(sws_ctx, (const uint8_t * const *)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pictureEncoded->data, pictureEncoded->linesize);

				pictureEncoded->pts = frame;

				if (frame % 2 == 1) {
					for (y = 0; y < clip_height; y++) {
						for (x = 0; x < clip_width; x++) {
							//  printf("(%d)(%d) : %d \n", x, y, pictureEncoded->data[0][y*pictureEncoded->linesize[0] + x]);
							frame1[y][x] = pictureEncoded->data[0][y*pictureEncoded->linesize[0] + x];
							

						}

					}
					sobel_sum += sobel(frame1);
				}


				if (frame % 2 == 0) {

					for (y = 0; y < clip_height; y++) {
						for (x = 0; x < clip_width; x++) {
							//  printf("(%d)(%d) : %d \n", x, y, pictureEncoded->data[0][y*pictureEncoded->linesize[0] + x]);
							frame2[y][x] = pictureEncoded->data[0][y*pictureEncoded->linesize[0] + x];
							sum[index] += abs(frame2[y][x] - frame1[y][x]);
						
						}

					}

					//printf("[%d] sum : %.2f \n ", index, sum[index] / 101376);
					index++;
					sobel_sum += sobel(frame2);


				}



				//sd = av_frame_get_side_data(pictureEncoded, AV_FRAME_DATA_MOTION_VECTORS);

				//printf("sd->data: %d\n", sd->data);

				//pictureEncoded-> output_pkt
				avcodec_encode_video2(ctxEncode, &output_pkt, pictureEncoded, &got_picture);

				//printf("output_pkt.data[0]  : %d\n \n", output_pkt.data);

				//if the function is working
				if (got_picture) {

					av_packet_rescale_ts(&output_pkt, ctxEncode->time_base, video_st->time_base);

					frame_size = output_pkt.size / 8;
					bitrate = (frame_size * 8) / av_q2d(ctxEncode->time_base) / 1000.0;
					//printf("frame= %5d   size= %6d Byte   type= %c\n",++frame_num, frame_size, ctxEncode->coded_frame ? av_get_picture_type_char(ctxEncode->coded_frame->pict_type) : 'I');
					//if (ctxEncode->coded_frame && (ctxEncode->flags&CODEC_FLAG_PSNR)){
					//	//printf("PSNR= %6.2f \n", psnr(ctxEncode->coded_frame->error[0] / (ctxEncode->width * ctxEncode->height * 255.0 * 255.0)));
					//	printf("ctxEncode->coded_frame->error[0] : %d\t", ctxEncode->coded_frame->error[0]);
					//	printf("ctxEncode->error[0] : %d\n", ctxEncode->error[0]);
					//}


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

	printf("sobel_sum = %d\n", sobel_sum);

	double total = 0;
	for (int i = 0; i < 44; i++) {
		total += sum[i] / 101376;
	}

	printf("total : %.2f \n ", total);

	end_t = clock();
	time = ((double)(end_t - start_t)) / CLOCKS_PER_SEC;
	//display_info(file_input, file_out, pFormatCtx, ofmt_ctx, pCodecCtx, ctxEncode, time, frame_num);

	/* Write the trailer, if any. The trailer must be written before you
	* close the CodecContexts open when you wrote the header; otherwise
	* av_write_trailer() may try to use memory that was freed on
	* av_codec_close(). */
	av_write_trailer(ofmt_ctx);

	// Free the YUV frame
	av_frame_free(&pFrame);
	av_frame_free(&pictureEncoded);

	// Close the codecs
	avcodec_close(pCodecCtx);

	// Close the video file
	avformat_close_input(&pFormatCtx);
	avcodec_close(ctxEncode);

	return 0;
}
int check_file(char *file_input) {

	int check_yuv = 0, len = 0;

	len = strlen(file_input);

	if (file_input[len - 3] == 'y' && file_input[len - 2] == 'u' && file_input[len - 1] == 'v') {
		check_yuv = 1;
	}

	return check_yuv;
}

int sobel(int frameData[288][352])
{
	int MaskSobelX[3][3] = { { -1,0,1 },
	{ -2,0,2 },
	{ -1,0,1 } };
	int MaskSobelY[3][3] = { { 1,2,1 },
	{ 0,0,0 },
	{ -1,-2,-1 } };

	int heightm1 = clip_height - 1;
	int widthm1 = clip_width - 1;
	int mr, mc;
	int newValue;
	int i = 0, j = 0;
	int sum = 0;

	int *pImgSobelX, *pImgSobelY;
	int min, max, where;
	float constVal1, constVal2;

	//정수값을 갖는 이미지 동적 메모리 할당
	pImgSobelX = new int[clip_height*clip_width];
	pImgSobelY = new int[clip_height*clip_width];
	
	//X 방향 에지 강도 계산 
	for (i = 1; i<heightm1; i++)
	{
		for (j = 1; j<widthm1; j++)
		{
			newValue = 0; //0으로 초기화
			for (mr = 0; mr<3; mr++)
				for (mc = 0; mc<3; mc++)
				newValue += (MaskSobelX[mr][mc] * ((frameData)[i + mr - 1][j + mc - 1]));
			pImgSobelX[i*clip_width + j] = newValue;
		}
	}
	
	//Y 방향 에지 강도 계산 
	for (i = 1; i<heightm1; i++)
	{
		for (j = 1; j<widthm1; j++)
		{
			newValue = 0; //0으로 초기화
			for (mr = 0; mr<3; mr++)
				for (mc = 0; mc<3; mc++)
				newValue += (MaskSobelY[mr][mc] * (frameData)[i + mr - 1][j + mc - 1]);
			pImgSobelY[i*clip_width + j] = newValue;
		}
	}

	//에지 강도 계산 절대값(X)+절대값(Y)후 pImgSobelX[]에 저장
	for (i = 1; i<heightm1; i++)
		for (j = 1; j < widthm1; j++)
		{
			where = i*clip_width + j;
			constVal1 = pImgSobelX[where];
			constVal2 = pImgSobelY[where];
			if (constVal1 < 0)
				constVal1 = -constVal1;
			if (constVal2 < 0)
				constVal2 = -constVal2;
			pImgSobelX[where] = constVal1 + constVal2;
			
		}

	// 디스플레이를 위해 0에서 255사이로 값의 범위를 매핑
		//이를 위해 먼저 최대,최소값을 찾은후 이를 이용하여 매핑한다.
	min = (int)10e10;
	max = (int)-10e10;
	for (i = 1; i<heightm1; i++)
	{
		for (j = 1; j<widthm1; j++)
		{
			newValue = pImgSobelX[i*clip_width + j];
			if (newValue<min)
				min = newValue;
			if (newValue>max)
				max = newValue;
		}
	}

	//변환시 상수값을 미리 계산 
	constVal1 = (float)(255.0 / (max - min));
	constVal2 = (float)(-255.0*min / (max - min));

	for (i = 1; i < heightm1; i++)
	{
		for (j = 1; j < widthm1; j++)
		{
			//[min,max]사이의 값을 [0,255]값으로 변환 
			newValue = pImgSobelX[i*clip_width + j];
			newValue = constVal1*newValue + constVal2;
			sum += newValue;
		}
	}


	
	delete[] pImgSobelX;
	delete[] pImgSobelY;

	printf("sobe : %d\n", sum / (clip_height*clip_width));
	return 	 sum / (clip_height*clip_width);
}

AVStream *output_set(AVFormatContext **ofmt_ctx, char *file_out, int videoStream) {

	int ret = 0;
	AVOutputFormat *ofmt = NULL;
	AVCodec *codecDecode = NULL;
	AVStream *video_st = NULL;

	avformat_alloc_output_context2(ofmt_ctx, NULL, NULL, file_out);
	if (!(*ofmt_ctx)) {
		fprintf(stderr, "Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		exit(1);
	}


	ofmt = (*ofmt_ctx)->oformat;

	ofmt->video_codec = AV_CODEC_ID_H264;

	// codec parameters set function
	if (ofmt->video_codec != AV_CODEC_ID_NONE)
		video_st = add_stream((*ofmt_ctx), ofmt->video_codec, videoStream, &codecDecode);

	if (video_st)
		open_video((*ofmt_ctx), video_st, codecDecode);


	//av_dump_format((*ofmt_ctx), 0, file_out, 1); 


	/* open the output file, if needed */
	if (!(ofmt->flags & AVFMT_NOFILE)) {

		ret = avio_open(&(*ofmt_ctx)->pb, file_out, AVIO_FLAG_WRITE);

		if (ret < 0) {
			char buf[256];
			av_strerror(ret, buf, sizeof(buf));
			fprintf(stderr, "Could not open '%s': %s\n", file_out, buf);
			exit(1);
		}
	}

	return video_st;
}

void init_parameter(AVFrame **pFrame, AVPacket *input_pkt, AVPacket *output_pkt, struct SwsContext **sws_ctx, AVCodecContext *pCodecCtx)
{

	// Allocate video frame
	(*pFrame) = av_frame_alloc();


	av_init_packet(input_pkt);
	av_init_packet(output_pkt);

	(*input_pkt).data = NULL;
	(*input_pkt).size = 0;
	(*output_pkt).data = NULL;
	(*output_pkt).size = 0;


	// initialize SWS context for software scaling
	(*sws_ctx) = sws_getContext(pCodecCtx->width,
		pCodecCtx->height,
		pCodecCtx->pix_fmt,
		clip_width,
		clip_height,
		PIX_FMT_YUV420P,
		SWS_BILINEAR,
		NULL,
		NULL,
		NULL
		);

}

AVStream *add_stream(AVFormatContext *oc, enum AVCodecID codec_id, int videoStream, AVCodec **codecDecode)
{
	AVStream *st;
	AVCodecContext *ctxDecode = NULL;


	/* find the encoder */
	(*codecDecode) = avcodec_find_encoder(codec_id);

	if (!(*codecDecode)) {
		fprintf(stderr, "Could not find encoder for '%s'\n", avcodec_get_name(codec_id));
		exit(1);
	}

	st = avformat_new_stream(oc, *codecDecode); //add a new stream to media file.

	if (!st) {
		fprintf(stderr, "Could not allocate stream\n");
		exit(1);
	}


	st->id = videoStream; // stream index

	ctxDecode = st->codec; //save the information to codeccontext

	if ((*codecDecode)->type == AVMEDIA_TYPE_VIDEO)
	{
		ctxDecode->pix_fmt = PIX_FMT_YUV420P;

		///* Resolution must be a multiple of two. */
		ctxDecode->width = clip_width;
		ctxDecode->height = clip_height;

	}

	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		ctxDecode->flags |= CODEC_FLAG_GLOBAL_HEADER;


	avcodec_close(ctxDecode);


	return st;

}

void open_video(AVFormatContext *oc, AVStream *st, AVCodec *codecDecode)
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

void decode_init(int videoStream, AVCodecContext **pCodecCtx, AVFormatContext *pFormatCtx)
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

void encode_init(AVFrame **pictureEncoded, AVCodecContext **ctxEncode)
{

	AVCodec *codecEncode = NULL;
	int pic_size = 0;

	uint8_t *picEncodeBuf = NULL;

	codecEncode = avcodec_find_encoder(CODEC_ID_H264);
	if (!codecEncode) {
		printf("codec not found\n");
		exit(1);
	}


	(*ctxEncode) = avcodec_alloc_context3(codecEncode);

	(*ctxEncode)->coder_type = AVMEDIA_TYPE_VIDEO; // codec_type : VIDEO

	(*ctxEncode)->flags |= CODEC_FLAG_PSNR;
	(*ctxEncode)->flags |= CODEC_FLAG_LOOP_FILTER; // flags=+loop filter

	(*ctxEncode)->profile = FF_PROFILE_H264_HIGH_422; //high profile
													  //(*ctxEncode)->level = 51;

	(*ctxEncode)->time_base.num = 1; //framte rate  -- 30
	(*ctxEncode)->time_base.den = 30;

	(*ctxEncode)->width = clip_width; //frame size
	(*ctxEncode)->height = clip_height;

	(*ctxEncode)->pix_fmt = PIX_FMT_YUV420P;

	(*ctxEncode)->qcompress = 1.0; // QP curve compression: 0.0 => CBR, 1.0 => CQP. Recommended default: -qcomp 0.60

	(*ctxEncode)->qmin = 10; // CQP 
	(*ctxEncode)->qmax = (*ctxEncode)->qmin;

	(*ctxEncode)->gop_size = 100;
	(*ctxEncode)->keyint_min = 30; //gop size 

	(*ctxEncode)->refs = 1; // reference frame numbers

	(*ctxEncode)->me_range = 16; // motion estimation search range
	(*ctxEncode)->me_method = ME_FULL; //motion estimation algorithm

	(*ctxEncode)->max_b_frames = 0;

	/* open codec for encoder*/
	if (avcodec_open2((*ctxEncode), codecEncode, NULL) < 0) {
		printf("could not open codec\n");
		exit(1);
	}

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
	(*pictureEncoded)->linesize[0] = (*ctxEncode)->width;
	(*pictureEncoded)->linesize[1] = (*ctxEncode)->width / 2;
	(*pictureEncoded)->linesize[2] = (*ctxEncode)->width / 2;

}

int get_header_input(AVFormatContext **pFormatCtx, char *FilePath, int check_yuv)
{

	int ret = 0;
	int videoStream = 0;

	// Open video file and read header
	//first parameter : I/O and information of Muxing/DeMuxing
	//second parameter : input source
	ret = avformat_open_input(pFormatCtx, FilePath, NULL, NULL);
	if (ret != 0) {
		av_log(NULL, AV_LOG_ERROR, "file [%s] Open Fail(ret : %d)\n", FilePath, ret);
		exit(1);
	}

	if (check_yuv == 1) {
		printf("it is yuvfile\n");
		printf("So you must input media size\n");
		printf("width : ");
		scanf("%d", &((*pFormatCtx)->streams[0]->codec->width));
		printf("height : ");
		scanf("%d", &((*pFormatCtx)->streams[0]->codec->height));

	}




	av_log(NULL, AV_LOG_INFO, "File [%s] Open Success\n", FilePath); // file success debug

																	 //Retrive stream information
	if (avformat_find_stream_info((*pFormatCtx), NULL) < 0)
		exit(1);

	//Dump information about file onto standard error 
	//av_dump_format(*pFormatCtx, 0, FilePath, 0); 


	// find index for videostream
	// pFormatCtx -> nb_streams means that numbers of file streams
	videoStream = -1;

	for (int i = 0; i<(*pFormatCtx)->nb_streams; i++)
		if ((*pFormatCtx)->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}

	// Didn't find a video stream, finished file
	if (videoStream == -1)
		exit(0);


	return videoStream;
}

void display_info(char *file_input, char *file_output, AVFormatContext *pFmtCtx,
	AVFormatContext *ofmt_ctx, AVCodecContext *ctxDecode, AVCodecContext *ctxEncode, double time, int frame_num)
{
	int in_size = 0, out_size = 0;
	int bitrate = 0;
	char *string = NULL;
	int hours, mins, secs, us;
	int64_t duration = pFmtCtx->duration + 5000;

	if (duration < 0)
		duration = (frame_num * AV_TIME_BASE) / ctxEncode->time_base.den;

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

	printf("Output File\t = %s\n", file_output);
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
	if (out_size > 124000000)
		printf("Output Bitrate\t = %.0f kbits/s\n\n", ((out_size / 1000) * 8) / (duration / AV_TIME_BASE / 1.0));
	else printf("Output Bitrate\t = %.0f kbits/s\n\n", ((out_size * 8) / 1000) / (duration / AV_TIME_BASE / 1.0));
}