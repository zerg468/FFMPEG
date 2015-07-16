#define inline _inline

///> Include FFMpeg

extern "C"{

#include <libavformat/avformat.h> 
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

};


#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"swscale.lib")
#pragma comment(lib,"avutil.lib")


const char *szFilePath = "E://foreman.yuv"; // 읽을 파일 이름
//const char *szFilePath = "E://a.mov"; // 읽을 파일 이름
const char *file_out = "E://yuv_change.mp4"; // 디코딩해서 출력할 파일 이름 (yuv만 가능함)

AVFormatContext   *pFormatCtx = NULL; // 파일헤더 읽어오기

AVCodec *codecEncode, *codecDecode;
AVCodecContext *ctxEncode = NULL, *ctxDecode = NULL;
AVFrame *pictureEncoded, *pictureDecoded;
uint8_t *encoderOut, *picEncodeBuf;
int encoderOutSize, decoderOutSize;

int pic_size;

AVPacket avpkt;
int got_picture, len;

uint8_t *decodedOut;

int clip_width = 352;
int clip_height = 288;

// 파일 정보를 읽는 함수
void get_infor();

// codec 초기화
void encode_init();

//codec parameter set
AVStream *add_stream(AVFormatContext *oc, enum AVCodecID codec_id);

// codec open, frame set
void open_video(AVFormatContext *oc, AVStream *st);

int videoStream = -1; // videostream index 표시


int main(void)
{
	// Initalizing these to NULL prevents segfaults!
	AVOutputFormat *ofmt = NULL;
	AVFormatContext *ofmt_ctx = NULL;
	AVCodecContext    *pCodecCtxOrig = NULL; // 코덱에 관한 스트림 정보
	AVCodecContext    *pCodecCtx = NULL;
	AVCodec           *pCodec = NULL; // 코덱을 저장
	AVFrame           *pFrame = NULL;
	AVPacket          packet; //stream 저장소
	int               frameFinished;
	int               numBytes;
	uint8_t           *buffer = NULL;
	struct SwsContext *sws_ctx = NULL;

	// Read frames and save first five frames to disk
	int frame = 0, len = 0,ret=0;


	AVStream *video_st = NULL;



	// Register all formats and codecs
	av_register_all();

	// 파일의 정보를 읽는다.
	get_infor();


	//printf("가로 길이 입력: ");
	//scanf("%d", &clip_width);

	//printf("세로 길이 입력: ");
	//scanf("%d", &clip_height);

	encode_init();


	// Get a pointer to the codec context for the video stream
	// 비디오 스트림에 어떤 코덱 사용했는지 codec context에 전달, 후에 디코딩에 필요
	pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;


	// Find the decoder for the video stream
	pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
	if (pCodec == NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1; // Codec not found
	}

	// Copy context , codec을 open 하기 위해서 Avcodeccontext structure 필요
	pCodecCtx = avcodec_alloc_context3(pCodec);

	// 코덱 복사
	if (avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
		fprintf(stderr, "Couldn't copy codec context");
		return -1; // Error copying codec context
	}

	// Open codec
	if (avcodec_open2(pCodecCtx, pCodec, NULL)<0)
		return -1; // Could not open codec



	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, file_out);
	if (!ofmt_ctx) {
		fprintf(stderr, "Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		exit(0);
	}

	ofmt = ofmt_ctx->oformat;

	ofmt->video_codec = AV_CODEC_ID_H264;

	// codec parameters 설정 함수
	if (ofmt->video_codec != AV_CODEC_ID_NONE)
		video_st = add_stream(ofmt_ctx,ofmt->video_codec);

	if (video_st)
		open_video(ofmt_ctx, video_st);


	av_dump_format(ofmt_ctx, 0, file_out, 1); // 정보 출력 디버깅 함수


	/* open the output file, if needed */
	if (!(ofmt->flags & AVFMT_NOFILE)) {

		ret = avio_open(&ofmt_ctx->pb, file_out, AVIO_FLAG_WRITE);

		if (ret < 0) {
			char buf[256];
			av_strerror(ret, buf, sizeof(buf));
			fprintf(stderr, "Could not open '%s': %s\n", file_out, buf);
			return 1;
		}
	}

	// Allocate video frame
	pFrame = av_frame_alloc();


	// Determine required buffer size and allocate buffer , 변환시에 Raw Data를 저장할 장소
	numBytes = avpicture_get_size(PIX_FMT_YUV420P, clip_width, clip_height);
	buffer = (uint8_t *)av_malloc(numBytes*sizeof(uint8_t));



	// initialize SWS context for software scaling
	sws_ctx = sws_getContext(ctxEncode->width,
		ctxEncode->height,
		ctxEncode->pix_fmt,
		clip_width,
		clip_height,
		PIX_FMT_YUV420P,
		SWS_BILINEAR,
		NULL,
		NULL,
		NULL
		);


	/* Write the stream header, if any. */
	ret = avformat_write_header(ofmt_ctx, NULL); // Allocate the stream private data and write the stream header to an output media file. 
	// 헤더파일 생성 -> 본체 생성 -> 마무리 작업

	if (ret < 0) {
		char buf[256];
		av_strerror(ret, buf, sizeof(buf));
		fprintf(stderr, "Error occurred when opening output file: %s\n", buf);
		return 1;
	}

	//av_read_frame 함수는 파일에서 packet 에 저장하는 함수

		while (av_read_frame(pFormatCtx, &packet) >= 0) {

			fflush(stdout);

			if (packet.stream_index == AVMEDIA_TYPE_VIDEO){

				// Decode video frame (packet을 프레임으로 변환시킨다.)
				
				avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

				if (frameFinished){

					frame++;

					pFrame->pts = frame;
					
					//pictureEncoded에 있는 데이터를 avpkt 패킷으로 옮긴다.
					avcodec_encode_video2(ctxEncode, &avpkt, pFrame, &got_picture);
					
					av_packet_rescale_ts(&avpkt, pFormatCtx->streams[videoStream]->time_base, video_st->time_base);
				

					ret = av_write_frame(ofmt_ctx, &avpkt);

					if (ret < 0) {
						fprintf(stderr, "Error muxing packet\n");
						break;
					}
				}

				av_free_packet(&packet);
				av_free_packet(&avpkt);
			}
		}

	/* Write the trailer, if any. The trailer must be written before you
	* close the CodecContexts open when you wrote the header; otherwise
	* av_write_trailer() may try to use memory that was freed on
	* av_codec_close(). */

	av_write_trailer(ofmt_ctx);

	// Free the YUV frame
	av_frame_free(&pFrame);

	// Close the codecs
	avcodec_close(pCodecCtx);
	avcodec_close(pCodecCtxOrig);

	// Close the video file
	avformat_close_input(&pFormatCtx);


	avcodec_close(ctxEncode);
	avcodec_close(ctxDecode);
	av_free(ctxEncode);
	av_free(ctxDecode);
	av_free(pictureEncoded);
	av_free(pictureDecoded);

	return 0;

}
AVStream *add_stream(AVFormatContext *oc, enum AVCodecID codec_id)
{
	AVStream *st;
	int i = 0, video_Stream = -1;

	/* find the encoder */
	codecDecode = avcodec_find_encoder(codec_id);

	if (!codecDecode) {
		fprintf(stderr, "Could not find encoder for '%s'\n", avcodec_get_name(codec_id));
		exit(1);
	}

	st = avformat_new_stream(oc, codecDecode); //add a new stream to media file.

	if (!st) {
		fprintf(stderr, "Could not allocate stream\n");
		exit(1);
	}

	// 비디오스트림이 있는 첫번째 인덱스 찾기
	// pFormatCtx -> nb_streams 파일에 있는 스트림의 개수
	video_Stream = -1;
	for (i = 0; i<oc->nb_streams; i++)
		if (oc->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_Stream = i;
			break;
		}

	if (video_Stream == -1)
		exit(0); // Didn't find a video stream


	//st->id = oc->nb_streams - 1; // stream index

	st->id = video_Stream; // stream index

	ctxDecode = st->codec; //codeccontext에 저장

	if ((codecDecode)->type == AVMEDIA_TYPE_VIDEO)
	{

		ctxDecode->codec_id = codec_id;
		ctxDecode->bit_rate = 1000 * 1024;
		/* Resolution must be a multiple of two. */
		ctxDecode->width = clip_width;
		ctxDecode->height = clip_height;
		/* timebase: This is the fundamental unit of time (in seconds) in terms
		* of which frame timestamps are represented. For fixed-fps content,
		* timebase should be 1/framerate and timestamp increments should be
		* identical to 1. */
		ctxDecode->time_base.den = 25;
		ctxDecode->time_base.num = 1;
		ctxDecode->gop_size = 15; /* emit one intra frame every twelve frames at most */
		ctxDecode->pix_fmt = PIX_FMT_YUV420P;

		ctxDecode->max_b_frames = 0;

		ctxDecode->qmin = 10;
		ctxDecode->qmax = 41;
		

	}

	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		ctxDecode->flags |= CODEC_FLAG_GLOBAL_HEADER;


	return st;

}
void open_video(AVFormatContext *oc, AVStream *st)
{
	int ret;
	AVCodecContext *c = st->codec;

	/* open the codec */
	ret = avcodec_open2(c, codecDecode, NULL);

	if (ret < 0) {
		char buf[256];
		av_strerror(ret, buf, sizeof(buf));
		fprintf(stderr, "Could not open video codec: %s\n", buf);
		exit(1);
	}

	
	pictureDecoded = av_frame_alloc();
	if (!pictureDecoded) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}

	pictureDecoded->format = c->pix_fmt;
	pictureDecoded->width = c->width;
	pictureDecoded->height = c->height;


	pic_size = avpicture_get_size(PIX_FMT_YUV420P, clip_width, clip_height);
	decodedOut = (uint8_t *)malloc(pic_size*sizeof(uint8_t));

	/* copy data and linesize picture pointers to frame */
	//*((AVPicture *)frame) = dst_picture;
}

void encode_init()
{
	codecEncode = avcodec_find_encoder(CODEC_ID_H264);
	if (!codecEncode) {
		printf("codec not found\n");
		exit(1);
	}

	ctxEncode = avcodec_alloc_context3(codecEncode);


	ctxEncode->coder_type = 0; // coder = 1
	ctxEncode->flags |= CODEC_FLAG_LOOP_FILTER; // flags=+loop
	ctxEncode->me_cmp |= 1; // cmp=+chroma, where CHROMA = 1
	ctxEncode->me_method = ME_HEX; // me_method=hex
	ctxEncode->me_subpel_quality = 0; // subq=7
	ctxEncode->me_range = 16; // me_range=16
	ctxEncode->scenechange_threshold = 40; // sc_threshold=40
	ctxEncode->i_quant_factor = 0.71; // i_qfactor=0.71
	ctxEncode->b_frame_strategy = 1; // b_strategy=1
	ctxEncode->qcompress = 0.6; // qcomp=0.6
	ctxEncode->qmin = 10; // qmin=10
	ctxEncode->qmax = 51; // qmax=51
	ctxEncode->max_qdiff = 4; // qdiff=4
	ctxEncode->max_b_frames = 0; // bf=3
	ctxEncode->refs = 3; // refs=3
	ctxEncode->trellis = 1; // trellis=1
	ctxEncode->flags2 |= CODEC_FLAG2_FAST; // flags2=+bpyramid+mixed_refs+wpred+dct8x8+fastpskip
	ctxEncode->bit_rate = 8000 * 1024;
	ctxEncode->width = clip_width;
	ctxEncode->height = clip_height;
	ctxEncode->time_base.num = 1;
	ctxEncode->time_base.den = 25;
	ctxEncode->pix_fmt = PIX_FMT_YUV420P;
	ctxEncode->max_b_frames = 0;
	ctxEncode->b_frame_strategy = 1;
	ctxEncode->chromaoffset = 0;
	ctxEncode->thread_count = 1;
	ctxEncode->gop_size = 30; // Each 3 seconds
	ctxEncode->profile = FF_PROFILE_H264_HIGH_422;

	/* open codec for encoder*/
	if (avcodec_open2(ctxEncode, codecEncode, NULL) < 0) {
		printf("could not open codec\n");
		exit(1);
	}

	pictureEncoded = av_frame_alloc();

	encoderOut = (uint8_t*)malloc(clip_height*clip_width * 3);
	picEncodeBuf = (uint8_t*)malloc(3 * pic_size / 2); // size for YUV 420p

	pictureEncoded->format = PIX_FMT_YUV420P;
	pictureEncoded->width = clip_width;
	pictureEncoded->height = clip_height;

	//input size 설정 , stride 오류
	pictureEncoded->data[0] = picEncodeBuf;
	pictureEncoded->data[1] = pictureEncoded->data[0] + pic_size;
	pictureEncoded->data[2] = pictureEncoded->data[1] + pic_size / 4;
	pictureEncoded->linesize[0] = ctxEncode->width;
	pictureEncoded->linesize[1] = ctxEncode->width / 2;
	pictureEncoded->linesize[2] = ctxEncode->width / 2;

}

void get_infor()
{

	int ret = 0;

	// Open video file
	//첫번째 인자에서 I/O 및 Muxing/DeMuxing에 관한 정보 저장, 두번째 input source(파일위치), UDP 스트리밍 Url
	ret = avformat_open_input(&pFormatCtx, szFilePath, NULL, NULL);
	if (ret != 0){
		av_log(NULL, AV_LOG_ERROR, "file [%s] Open Fail(ret : %d)\n", szFilePath, ret);
		exit(-1);
	}
	
	pFormatCtx->streams[0]->codec->width = 352;
	pFormatCtx->streams[0]->codec->height = 288;


	av_log(NULL, AV_LOG_INFO, "File [%s] Open Success\n", szFilePath); // 화면에 출력

	//Retrive stream information(pFormatCtx->streams 에 고유정보 저장)
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
		exit(0); //Couldn't find stream information

	//Dump information about file onto standard error (디버깅 함수)
	av_dump_format(pFormatCtx, 0, szFilePath, 0); // 프로그램 정보를 추출한다.

	
	// 비디오스트림이 있는 첫번째 인덱스 찾기
	// pFormatCtx -> nb_streams 파일에 있는 스트림의 개수
	videoStream = -1;
	for (int i = 0; i<pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}

	if (videoStream == -1)
		exit(0); // Didn't find a video stream
}

