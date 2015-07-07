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


const char *szFilePath = "E://BlackBerry.mov"; // 읽을 파일 이름
const char *file_out = "E://example2.yuv"; // 디코딩해서 출력할 파일 이름 (yuv만 가능함)
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

int clip_width = 640;
int clip_height = 480;

// 파일 정보를 읽는 함수
void get_infor();

// codec 초기화
void decode_init();
void encode_init();


int main(void)
{
	// Initalizing these to NULL prevents segfaults!
	AVCodecContext    *pCodecCtxOrig = NULL; // 코덱에 관한 스트림 정보
	AVCodecContext    *pCodecCtx = NULL;
	AVCodec           *pCodec = NULL; // 코덱을 저장
	AVFrame           *pFrame = NULL;
	AVPacket          packet; //stream 저장소
	int               frameFinished;
	int               numBytes;
	uint8_t           *buffer = NULL;
	struct SwsContext *sws_ctx = NULL;

	// Register all formats and codecs
	av_register_all();

	// 파일의 정보를 읽는다.
	get_infor();


	printf("가로 길이 입력: ");
	scanf("%d", &clip_width);

	printf("세로 길이 입력: ");
	scanf("%d", &clip_height);


	decode_init();
	encode_init();



	// Get a pointer to the codec context for the video stream
	// 비디오 스트림에 어떤 코덱 사용했는지 codec context에 전달, 후에 디코딩에 필요
	pCodecCtxOrig = pFormatCtx->streams[0]->codec;


	// Find the decoder for the video stream
	pCodec = avcodec_find_decoder(CODEC_ID_H264);
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

	//저장할 파일 불러오기
	FILE *f = NULL;
	f = fopen(file_out, "wb");
	if (!f){
		fprintf(stderr, "Opening error");
		exit(-1);
	}

	//읽을 파일 불러오기
	FILE *f_in = NULL;
	f_in = fopen(szFilePath, "rb");
	if (!f){
		fprintf(stderr, "Opening error");
		exit(0);
	}


	// Allocate video frame
	pFrame = av_frame_alloc();

	// Determine required buffer size and allocate buffer , 변환시에 Raw Data를 저장할 장소
	numBytes = avpicture_get_size(PIX_FMT_YUV420P, clip_width, clip_height);
	buffer = (uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

	// Read frames and save first five frames to disk
	int frame = 0, len = 0;


	// initialize SWS context for software scaling
	sws_ctx = sws_getContext(pCodecCtx->width,
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


	//av_read_frame 함수는 파일에서 packet 에 저장하는 함수
	while (av_read_frame(pFormatCtx, &packet) >= 0) {

		fflush(stdout);

		// Decode video frame (packet을 프레임으로 변환시킨다.)
		avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

		// Did we get a video frame?
		if (frameFinished) {

			//픽셀 데이터를 buffer로 전송한다.
			//avpicture_layout((AVPicture*)pFrame, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, buffer, numBytes);

			//파일 저장
			//fwrite(buffer, numBytes, 1, f);

			frame++;

		}

		if (frame != 0){

			//fread(pictureEncoded->data[0], ctxEncode->width * ctxEncode->height, 1, f_in);
			//fread(pictureEncoded->data[1], ctxEncode->width * ctxEncode->height / 4, 1, f_in);
			//fread(pictureEncoded->data[2], ctxEncode->width * ctxEncode->height / 4, 1, f_in);

			sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
				pFrame->linesize, 0, pCodecCtx->height,
				pictureEncoded->data, pictureEncoded->linesize);

			//pictureEncoded->data[0] = pFrame->data[0];
			//pictureEncoded->data[1] = pFrame->data[1];
			//pictureEncoded->data[2] = pFrame->data[2];

			pictureEncoded->data[1] = pictureEncoded->data[0] + pic_size;
			pictureEncoded->data[2] = pictureEncoded->data[1] + pic_size / 4;

			pictureEncoded->linesize[1] = ctxEncode->width / 2;
			pictureEncoded->linesize[2] = ctxEncode->width / 2;

			pictureEncoded->pts = frame;
			pFrame->pts = frame;

			//encoderOutSize = avcodec_encode_video(ctxEncode, encoderOut, clip_height*clip_width*3, pFrame);

			//encoderOutSize = avcodec_encode_video(ctxEncode, encoderOut, clip_height*clip_width * 3, pictureEncoded);
			
			//avpkt.size = encoderOutSize;
			//avpkt.data = encoderOut;


			avcodec_encode_video2(ctxEncode, &avpkt, pictureEncoded, &got_picture);

			
			len = avcodec_decode_video2(ctxDecode, pictureDecoded, &got_picture, &avpkt);

			if (got_picture){

				fflush(stdout);

				//픽셀 데이터를 buffer로 전송한다.
				avpicture_layout((AVPicture*)pictureDecoded, ctxDecode->pix_fmt, ctxDecode->width, ctxDecode->height, decodedOut, pic_size);

				//파일 저장
				fwrite(decodedOut, pic_size, 1, f);


			}
		}




		// Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);
		av_free_packet(&avpkt);
	}

	printf("디코딩 된 총 frame의 갯수 : %3d\n", frame);

	// 파일 종료
	fclose(f);
	fclose(f_in);

	// Free the RGB image
	av_free(buffer);



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

void decode_init()
{
	codecDecode = avcodec_find_decoder(CODEC_ID_H264);
	if (!codecDecode) {
		fprintf(stderr, "codec not found\n");
		exit(1);
	}

	ctxDecode = avcodec_alloc_context3(codecDecode);
	ctxDecode->flags2 |= CODEC_FLAG2_FAST;
	ctxDecode->pix_fmt = PIX_FMT_YUV420P;
	ctxDecode->width = clip_width;
	ctxDecode->height = clip_height;



	if (avcodec_open2(ctxDecode, codecDecode, NULL) < 0) {
		fprintf(stderr, "could not open codec\n");
		exit(1);
	}

	pictureDecoded = av_frame_alloc();

	pic_size = avpicture_get_size(PIX_FMT_YUV420P, clip_width, clip_height);

	decodedOut = (uint8_t *)malloc(pic_size*sizeof(uint8_t));

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
	ctxEncode->qmin = 1; // qmin=10
	ctxEncode->qmax = 50; // qmax=51
	ctxEncode->max_qdiff = 4; // qdiff=4
	ctxEncode->max_b_frames = 3; // bf=3
	ctxEncode->refs = 3; // refs=3
	ctxEncode->trellis = 1; // trellis=1
	ctxEncode->flags2 |= CODEC_FLAG2_FAST; // flags2=+bpyramid+mixed_refs+wpred+dct8x8+fastpskip
	ctxEncode->bit_rate = 1000 * 1024;
	ctxEncode->width = clip_width;
	ctxEncode->height = clip_height;
	ctxEncode->time_base.num = 1;
	ctxEncode->time_base.den = 30;
	ctxEncode->pix_fmt = PIX_FMT_YUV420P;
	ctxEncode->max_b_frames = 0;
	ctxEncode->b_frame_strategy = 1;
	ctxEncode->chromaoffset = 0;
	ctxEncode->thread_count = 1;
	ctxEncode->gop_size = 30 * 3; // Each 3 seconds
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

	av_log(NULL, AV_LOG_INFO, "File [%s] Open Success\n", szFilePath); // 화면에 출력

	//Retrive stream information(pFormatCtx->streams 에 고유정보 저장)
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
		exit(0); //Couldn't find stream information

	//Dump information about file onto standard error (디버깅 함수)
	av_dump_format(pFormatCtx, 0, szFilePath, 0); // 프로그램 정보를 추출한다.


}