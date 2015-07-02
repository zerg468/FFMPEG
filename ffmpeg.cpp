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


const char *szFilePath = "C://yuv.mp4"; // ���� ���� �̸�
const char *file_out = "C://example2.yuv"; // ���ڵ��ؼ� ����� ���� �̸� (yuv�� ������)
int videoStream; //ù��° ���� ��Ʈ�� �ε���
AVFormatContext   *pFormatCtx = NULL; // ������� �о����

// ���� ������ �д� �Լ�
void get_infor();

int main(void)
{
	// Initalizing these to NULL prevents segfaults!
	int               i;
	AVCodecContext    *pCodecCtxOrig = NULL; // �ڵ� ���� ���
	AVCodecContext    *pCodecCtx = NULL, *codCon = NULL;
	AVCodec           *pCodec = NULL, *cod = NULL; // �ڵ��� ����
	AVFrame           *pFrame = NULL;
	AVFrame           *pFrameYUV = NULL;
	AVPacket          packet;
	int               frameFinished;
	int               numBytes;
	uint8_t           *buffer = NULL;
	struct SwsContext *sws_ctx = NULL;

	// Register all formats and codecs
	av_register_all();

	// ������ ������ �д´�.
	get_infor();

	// Get a pointer to the codec context for the video stream
	// ���� ��Ʈ���� � �ڵ� ����ߴ��� codec context�� ����, �Ŀ� ���ڵ��� �ʿ�
	pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;


	// Find the decoder for the video stream
	pCodec = avcodec_find_decoder(CODEC_ID_H264);
	if (pCodec == NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1; // Codec not found
	}

	// Copy context , codec�� open �ϱ� ���ؼ� Avcodeccontext structure �ʿ�
	pCodecCtx = avcodec_alloc_context3(pCodec);

	// �ڵ� ����
	if (avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
		fprintf(stderr, "Couldn't copy codec context");
		return -1; // Error copying codec context
	}

	// Open codec
	if (avcodec_open2(pCodecCtx, pCodec, NULL)<0)
		return -1; // Could not open codec


	// ���ڵ��� �� �Է��� �ڵ� ����
	cod = avcodec_find_decoder(CODEC_ID_H264);
	if (cod == NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1; // Codec not found
	}

	codCon = avcodec_alloc_context3(cod);


	// �ڵ��� ����
	codCon->width = pCodecCtx->width;
	codCon->height = pCodecCtx->height;
	codCon->codec_id = CODEC_ID_H264;
	codCon->codec_type = AVMEDIA_TYPE_VIDEO;
	codCon->flags |= CODEC_FLAG2_FAST;
	codCon->pix_fmt = PIX_FMT_YUV420P;

	if (avcodec_open2(codCon, cod, NULL) < 0)
		return -1;


	//������ ���� �ҷ�����
	FILE *f = NULL;
	f = fopen(file_out, "wb");
	if (!f){
		fprintf(stderr, "Opening error");
		exit(-1);
	}


	// Allocate video frame
	pFrame = av_frame_alloc();

	// Determine required buffer size and allocate buffer , ��ȯ�ÿ� Raw Data�� ������ ���
	numBytes = avpicture_get_size(PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);
	buffer = (uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

	// Read frames and save first five frames to disk
	int frame = 0;
	while (av_read_frame(pFormatCtx, &packet) >= 0) {

		av_init_packet(&packet);

		// Is this a packet from the video stream?
		if (packet.stream_index == videoStream) {
			// Decode video frame (packet�� ���������� ��ȯ��Ų��.)
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

			// Did we get a video frame?
			if (frameFinished) {

				//�ȼ� �����͸� buffer�� �����Ѵ�.
				avpicture_layout((AVPicture*)pFrame, codCon->pix_fmt, codCon->width, codCon->height, buffer, numBytes);

				//���� ����
				fwrite(buffer, numBytes, 1, f);

				frame++;
			}

		}

		// Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);
	}

	printf("���ڵ� �� �� frame�� ���� : %3d\n", frame);

	// ���� ����
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
	//ù��° ���ڿ��� I/O �� Muxing/DeMuxing�� ���� ���� ����, �ι�° input source(������ġ), UDP ��Ʈ���� Url
	ret = avformat_open_input(&pFormatCtx, szFilePath, NULL, NULL);
	if (ret != 0){
		av_log(NULL, AV_LOG_ERROR, "file [%s] Open Fail(ret : %d)\n", szFilePath, ret);
		exit(-1);
	}

	av_log(NULL, AV_LOG_INFO, "File [%s] Open Success\n", szFilePath); // ȭ�鿡 ���

	//Retrive stream information(pFormatCtx->streams �� �������� ����)
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
		exit(0); //Couldn't find stream information

	//Dump information about file onto standard error (����� �Լ�)
	av_dump_format(pFormatCtx, 0, szFilePath, 0); // ���α׷� ������ �����Ѵ�.



	// ������Ʈ���� �ִ� ù��° �ε��� ã��
	// pFormatCtx -> nb_streams ���Ͽ� �ִ� ��Ʈ���� ����
	videoStream = -1;
	for (i = 0; i<pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}

	if (videoStream == -1)
		exit(0); // Didn't find a video stream
}