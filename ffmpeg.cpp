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


const char *szFilePath = "C://yuv.mp4"; // 읽을 파일 이름
const char *file_out = "C://example2.yuv"; // 디코딩해서 출력할 파일 이름 (yuv만 가능함)
int videoStream; //첫번째 비디오 스트림 인덱스
AVFormatContext   *pFormatCtx = NULL; // 파일헤더 읽어오기

// 파일 정보를 읽는 함수
void get_infor();

int main(void)
{
	// Initalizing these to NULL prevents segfaults!
	int               i;
	AVCodecContext    *pCodecCtxOrig = NULL; // 코덱 정보 담기
	AVCodecContext    *pCodecCtx = NULL, *codCon = NULL;
	AVCodec           *pCodec = NULL, *cod = NULL; // 코덱을 저장
	AVFrame           *pFrame = NULL;
	AVFrame           *pFrameYUV = NULL;
	AVPacket          packet;
	int               frameFinished;
	int               numBytes;
	uint8_t           *buffer = NULL;
	struct SwsContext *sws_ctx = NULL;

	// Register all formats and codecs
	av_register_all();

	// 파일의 정보를 읽는다.
	get_infor();

	// Get a pointer to the codec context for the video stream
	// 비디오 스트림에 어떤 코덱 사용했는지 codec context에 전달, 후에 디코딩에 필요
	pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;


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


	// 디코딩할 때 입력할 코덱 설정
	cod = avcodec_find_decoder(CODEC_ID_H264);
	if (cod == NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1; // Codec not found
	}

	codCon = avcodec_alloc_context3(cod);


	// 코덱값 설정
	codCon->width = pCodecCtx->width;
	codCon->height = pCodecCtx->height;
	codCon->codec_id = CODEC_ID_H264;
	codCon->codec_type = AVMEDIA_TYPE_VIDEO;
	codCon->flags |= CODEC_FLAG2_FAST;
	codCon->pix_fmt = PIX_FMT_YUV420P;

	if (avcodec_open2(codCon, cod, NULL) < 0)
		return -1;


	//저장할 파일 불러오기
	FILE *f = NULL;
	f = fopen(file_out, "wb");
	if (!f){
		fprintf(stderr, "Opening error");
		exit(-1);
	}


	// Allocate video frame
	pFrame = av_frame_alloc();

	// Determine required buffer size and allocate buffer , 변환시에 Raw Data를 저장할 장소
	numBytes = avpicture_get_size(PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);
	buffer = (uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

	// Read frames and save first five frames to disk
	int frame = 0;
	while (av_read_frame(pFormatCtx, &packet) >= 0) {

		av_init_packet(&packet);

		// Is this a packet from the video stream?
		if (packet.stream_index == videoStream) {
			// Decode video frame (packet을 프레임으로 변환시킨다.)
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

			// Did we get a video frame?
			if (frameFinished) {

				//픽셀 데이터를 buffer로 전송한다.
				avpicture_layout((AVPicture*)pFrame, codCon->pix_fmt, codCon->width, codCon->height, buffer, numBytes);

				//파일 저장
				fwrite(buffer, numBytes, 1, f);

				frame++;
			}

		}

		// Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);
	}

	printf("디코딩 된 총 frame의 갯수 : %3d\n", frame);

	// 파일 종료
	fclose(f);

	// Free the RGB image
	av_free(buffer);
	av_frame_free(&pFrameYUV);

	// Free the YUV frame
	av_frame_free(&pFrame);

	// Close the codecs
	avcodec_close(pCodecCtx);
	avcodec_close(pCodecCtxOrig);

	// Close the video file
	avformat_close_input(&pFormatCtx);

	return 0;

}

void get_infor()
{

	int ret, i;

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



	// 비디오스트림이 있는 첫번째 인덱스 찾기
	// pFormatCtx -> nb_streams 파일에 있는 스트림의 개수
	videoStream = -1;
	for (i = 0; i<pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}

	if (videoStream == -1)
		exit(0); // Didn't find a video stream
}