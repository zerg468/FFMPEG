#define inline _inline

///> Include FFMpeg

extern "C"{

#include <libavformat/avformat.h> 
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libswresample/swresample.h>
};

#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"swscale.lib")
#pragma comment(lib,"avutil.lib")

const char* src_filename = "E:\\a.mov";
const char* video_dst_filename = "E:\\demuxing.mov";

static int audio_is_eof, video_is_eof;
#define STREAM_DURATION   10.0
#define STREAM_FRAME_RATE 30 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

static int sws_flags = SWS_FAST_BILINEAR;

static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
	/* rescale output packet timestamp values from codec to stream timebase */
	pkt->pts = av_rescale_q_rnd(pkt->pts, *time_base, st->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	pkt->dts = av_rescale_q_rnd(pkt->dts, *time_base, st->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	pkt->duration = av_rescale_q(pkt->duration, *time_base, st->time_base);
	pkt->stream_index = st->index;
	/* Write the compressed frame to the media file. */
	return av_interleaved_write_frame(fmt_ctx, pkt); // return 0 은 성공을 뜻한다.
}
/* Add an output stream. */
static AVStream *add_stream(AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id)
{
	AVCodecContext *c;
	AVStream *st;
	int i = 0, videoStream = -1;

	/* find the encoder */
	*codec = avcodec_find_encoder(codec_id);

	if (!(*codec)) {
		fprintf(stderr, "Could not find encoder for '%s'\n", avcodec_get_name(codec_id));
		exit(1);
	}

	st = avformat_new_stream(oc, *codec); //add a new stream to media file.

	if (!st) {
		fprintf(stderr, "Could not allocate stream\n");
		exit(1);
	}

	// 비디오스트림이 있는 첫번째 인덱스 찾기
	// pFormatCtx -> nb_streams 파일에 있는 스트림의 개수
	videoStream = -1;
	for (i = 0; i<oc->nb_streams; i++)
		if (oc->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}

	if (videoStream == -1)
		exit(0); // Didn't find a video stream


	//st->id = oc->nb_streams - 1; // stream index

	st->id = videoStream; // stream index

	c = st->codec; //codeccontext에 저장

	if ((*codec)->type == AVMEDIA_TYPE_VIDEO)
	{

		c->codec_id = codec_id;
		c->bit_rate = 500 * 1024;
		/* Resolution must be a multiple of two. */
		c->width = 1096;
		c->height = 960;
		/* timebase: This is the fundamental unit of time (in seconds) in terms
		* of which frame timestamps are represented. For fixed-fps content,
		* timebase should be 1/framerate and timestamp increments should be
		* identical to 1. */
		c->time_base.den = STREAM_FRAME_RATE;
		c->time_base.num = 1;
		c->gop_size = 15; /* emit one intra frame every twelve frames at most */
		c->pix_fmt = STREAM_PIX_FMT;

		if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
			/* just for testing, we also add B frames */
			c->max_b_frames = 2;
		}
		else if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
			/* Needed to avoid using macroblocks in which some coeffs overflow.
			* This does not happen with normal video, it just happens here as
			* the motion of the chroma plane does not match the luma plane. */
			c->mb_decision = 2;
		}

		else if (c->codec_id == AV_CODEC_ID_H264)
		{
			c->max_b_frames = 0;

		}

	}

	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;


	return st;
}

/**************************************************************/
/* video output */
static AVFrame *frame;
static AVPicture src_picture, dst_picture;
static int frame_count;


static void open_video(AVFormatContext *oc, AVCodec *codec, AVStream *st)
{
	int ret;
	AVCodecContext *c = st->codec;

	/* open the codec */
	ret = avcodec_open2(c, codec, NULL);

	if (ret < 0) {
		char buf[256];
		av_strerror(ret, buf, sizeof(buf));
		fprintf(stderr, "Could not open video codec: %s\n", buf);
		exit(1);
	}

	/* allocate and init a re-usable frame */
	frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}

	frame->format = c->pix_fmt;
	frame->width = c->width;
	frame->height = c->height;

	/* Allocate the encoded raw picture. */

	ret = avpicture_alloc(&dst_picture, c->pix_fmt, c->width, c->height);
	if (ret < 0) {
		char buf[256];
		av_strerror(ret, buf, sizeof(buf));
		fprintf(stderr, "Could not allocate picture: %s\n", buf);
		exit(1);
	}

	/* If the output format is not YUV420P, then a temporary YUV420P
	* picture is needed too. It is then converted to the required
	* output format. */
	if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
		ret = avpicture_alloc(&src_picture, AV_PIX_FMT_YUV420P, c->width, c->height);
		if (ret < 0) {
			char buf[256];
			av_strerror(ret, buf, sizeof(buf));
			fprintf(stderr, "Could not allocate temporary picture: %s\n", buf);
			exit(1);
		}
	}

	/* copy data and linesize picture pointers to frame */
	*((AVPicture *)frame) = dst_picture;
}
/* Prepare a dummy image. */
static void fill_yuv_image(AVPicture *pict, int frame_index, int width, int height)
{
	int x, y, i;
	i = frame_index;
	/* Y */
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
			pict->data[0][y * pict->linesize[0] + x] = x + y + i * 3;
	/* Cb and Cr */
	for (y = 0; y < height / 2; y++) {
		for (x = 0; x < width / 2; x++) {
			pict->data[1][y * pict->linesize[1] + x] = 128 + y + i * 2;
			pict->data[2][y * pict->linesize[2] + x] = 64 + x + i * 5;
		}
	}

}
static void write_video_frame(AVFormatContext *oc, AVStream *st, int flush)
{

	int ret;
	static struct SwsContext *sws_ctx = NULL;
	AVCodecContext *c = st->codec;

	//flush가 0 일 경우에만 검사한다.
	if (!flush) {
		if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
			/* as we only generate a YUV420P picture, we must convert it to the codec pixel format if needed */
			if (!sws_ctx) {
				//YUV파일로 변환하기 위한 설정
				sws_ctx = sws_getContext(c->width, c->height, AV_PIX_FMT_YUV420P, c->width, c->height, c->pix_fmt, sws_flags, NULL, NULL, NULL);
				if (!sws_ctx) {
					fprintf(stderr, "Could not initialize the conversion context\n");
					exit(1);
				}
			}

			fill_yuv_image(&src_picture, frame_count, c->width, c->height); //yuv 더미 파일 생성

			sws_scale(sws_ctx, (const uint8_t * const *)src_picture.data, src_picture.linesize, 0, c->height, dst_picture.data, dst_picture.linesize);
			/* srcSlice 이미지를 dst 이미지로 바꿔주는 함수
			int sws_scale 	( 	struct SwsContext *  	c,
			const uint8_t *const  	srcSlice[],
			const int  	srcStride[],  	the array containing the strides for each plane of the source image
			int  	srcSliceY, 	the position in the source image of the slice to process, that is the number (counted starting from zero) in the image of the first row of the slice
			int  	srcSliceH, the height of the source slice, that is the number of rows in the slice
			uint8_t *const  	dst[],
			const int  	dstStride[]
			)
			*/
		}

		//YUV 파일일 경우
		else {
			fill_yuv_image(&dst_picture, frame_count, c->width, c->height);
		}
	}

	/* Raw video case - directly store the picture in the packet */
	if (oc->oformat->flags & AVFMT_RAWPICTURE && !flush) {


		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.flags |= AV_PKT_FLAG_KEY;
		pkt.stream_index = st->index;
		pkt.data = dst_picture.data[0];
		pkt.size = sizeof(AVPicture);
		ret = av_interleaved_write_frame(oc, &pkt); //rawvideo은 그냥 interleave로 저장

	}

	//RAWPicture 가 아닐 경우
	else {

		AVPacket pkt = { 0 };
		int got_packet;
		av_init_packet(&pkt);

		/* encode the image */
		frame->pts = frame_count;
		ret = avcodec_encode_video2(c, &pkt, flush ? NULL : frame, &got_packet); //frame - > pkt 파일로
		//got_packet : This field is set to 1 by libavcodec if the output packet is non-empty, 
		//and to 0 if it is empty. 
		//If the function returns an error, the packet can be assumed to be invalid, and the value of got_packet_ptr is undefined and should not be used. 

		if (ret < 0) {
			char buf[256];
			av_strerror(ret, buf, sizeof(buf));
			fprintf(stderr, "Error encoding video frame: %s\n", buf);
			exit(1);
		}

		/* If size is zero, it means the image was buffered. */
		if (got_packet) {
			ret = write_frame(oc, &c->time_base, st, &pkt);
		}

		// empty일 경우
		else {
			if (flush)
				video_is_eof = 1;
			ret = 0;
		}
	}

	if (ret < 0) {
		char buf[256];
		av_strerror(ret, buf, sizeof(buf));
		fprintf(stderr, "Error while writing video frame: %s\n", buf);
		exit(1);
	}

	frame_count++;
}

static void close_video(AVFormatContext *oc, AVStream *st)
{
	avcodec_close(st->codec);
	av_free(src_picture.data[0]);
	av_free(dst_picture.data[0]);
	av_frame_free(&frame);
}

/**************************************************************/
/* media file output */
int main(int argc, char **argv)
{
	const char *filename;
	AVOutputFormat *fmt;
	AVFormatContext *oc;
	AVStream *video_st = NULL;
	AVCodec *video_codec = NULL;
	double video_time;
	int flush, ret;

	/* Initialize libavcodec, and register all codecs and formats. */
	av_register_all();

	filename = "E:\\muxing.mp4";
	/* allocate the output media context */
	avformat_alloc_output_context2(&oc, NULL, NULL, filename);

	if (!oc) {
		printf("Could not deduce output format from file extension: using MPEG.\n");
		avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
	}
	if (!oc)
		return 1;


	fmt = oc->oformat; //muxing 할 때 outputformat 설정

	/* Add the audio and video streams using the default format codecs
	* and initialize the codecs. */

	//fmt->video_codec = AV_CODEC_ID_H264;
	fmt->video_codec = AV_CODEC_ID_MPEG4;

	if (fmt->video_codec != AV_CODEC_ID_NONE)
		video_st = add_stream(oc, &video_codec, fmt->video_codec); // add_stream(AVFormatContext *oc, AVCodec **codec,enum AVCodecID codec_id)
	// codec parameters set 함수

	/* Now that all the parameters are set, we can open the audio and
	* video codecs and allocate the necessary encode buffers. */
	if (video_st)
		open_video(oc, video_codec, video_st); // (AVFormatContext *oc, AVCodec *codec, AVStream *st)
	// 코댁 열기, 프레임 설정

	av_dump_format(oc, 0, filename, 1); // 정보 출력 디버깅 함수


	/* open the output file, if needed */
	if (!(fmt->flags & AVFMT_NOFILE)) {

		ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);

		if (ret < 0) {
			char buf[256];
			av_strerror(ret, buf, sizeof(buf));
			fprintf(stderr, "Could not open '%s': %s\n", filename, buf);
			return 1;
		}
	}

	/* Write the stream header, if any. */
	ret = avformat_write_header(oc, NULL); // Allocate the stream private data and write the stream header to an output media file. 
	// 헤더파일 생성 -> 본체 생성 -> 마무리 작업

	if (ret < 0) {
		char buf[256];
		av_strerror(ret, buf, sizeof(buf));
		fprintf(stderr, "Error occurred when opening output file: %s\n", buf);
		return 1;
	}

	flush = 0;

	while ((video_st && !video_is_eof)) {

		if (!flush && (!video_st)) {
			flush = 1;
		}
		if (video_st && !video_is_eof) {
			write_video_frame(oc, video_st, flush); // 본체 생성
		}

		if (frame_count == 10000)
			break;
	}

	/* Write the trailer, if any. The trailer must be written before you
	* close the CodecContexts open when you wrote the header; otherwise
	* av_write_trailer() may try to use memory that was freed on
	* av_codec_close(). */

	av_write_trailer(oc);

	/* Close each codec. */
	if (video_st)
		close_video(oc, video_st);

	if (!(fmt->flags & AVFMT_NOFILE))
		/* Close the output file. */
		avio_close(oc->pb);

	/* free the stream */
	avformat_free_context(oc);

	return 0;
}