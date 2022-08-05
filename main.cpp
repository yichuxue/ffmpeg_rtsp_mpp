#include <stdio.h>
#include <stdlib.h>
/*Include ffmpeg header file*/
#ifdef __cplusplus
extern "C" {
#endif
#include <libavformat/avformat.h>
#ifdef __cplusplus
};
#endif

#include "mpp_rga/MppDecode.h"
#include "mpp_rga/MppEncoder.h"

static double get_current_time()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);

  return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

void deInit(MppPacket *packet, MppFrame *frame, MppCtx ctx, char *buf, MpiDecLoopData data ) {
	if (packet) {
		mpp_packet_deinit(packet);
		packet = NULL;
	}

	if (frame) {
		mpp_frame_deinit(frame);
		frame = NULL;
	}

	if (ctx) {
		mpp_destroy(ctx);
		ctx = NULL;
	}


	if (buf) {
		mpp_free(buf);
		buf = NULL;
	}


	if (data.pkt_grp) {
		mpp_buffer_group_put(data.pkt_grp);
		data.pkt_grp = NULL;
	}

	if (data.frm_grp) {
		mpp_buffer_group_put(data.frm_grp);
		data.frm_grp = NULL;
	}

	if (data.fp_output) {
		fclose(data.fp_output);
		data.fp_output = NULL;
	}

	if (data.fp_input) {
		fclose(data.fp_input);
		data.fp_input = NULL;
	}
}


int main(int argc, char * argv[]) {
	
	int opt = 0;
	// char filenameA[100];
	// strcpy(filenameA, argv[1]);
	// int dst_width = atoi(argv[2]);
	// int dst_height = atoi(argv[3]);
	const char* filenameA = nullptr;
	const char* filenameB = nullptr;
	int dst_width=0, dst_height=0, fps=0;

	InParams input_params;
	
	int res;
	while ((res = getopt(argc, argv, "w:h:f:i:o:s:")) != -1) {
		switch (res) {
			case 'w':
				dst_width = std::strtoul(optarg, nullptr, 0);
				break;
			case 'h':
				dst_height = std::strtoul(optarg, nullptr, 0);
				break;
			case 'i':
				filenameA = optarg;
				break;
			case 'o':
				filenameB = optarg;
				break;
			case 's':
				fps = std::strtoul(optarg, nullptr, 0);
				break;
		}
	}

	string rtsp_url = filenameA;
	string dst_rtsp_url = filenameB;
	printf("dw:%d, dh:%d, i:%s, o:%s, fps:%d \n", dst_width, dst_height, rtsp_url.c_str(), dst_rtsp_url.c_str(), fps);
	// return 0;

	input_params.src_rtsp_url = rtsp_url;
	input_params.dst_rtsp_url = dst_rtsp_url;
	input_params.dst_height = dst_height;
	input_params.dst_width = dst_width;
	input_params.fps = fps;

	whale::vision::MppEncoder mppenc;
  mppenc.MppEncdoerInit(dst_width, dst_height, fps);
	// printf("rtsp_url: %s \n", rtsp_url);
	FFInfo ff_info = ffmpeg_init(input_params);
	if (ff_info.status == 0) {
		printf("ffmpeg init fail!");
		return 0;
	}

	//// 初始化
	MPP_RET ret         = MPP_OK;

	// base flow context
	MppCtx ctx          = NULL;
	MppApi *mpi         = NULL;

	// input / output
	MppPacket packet    = NULL;
	MppFrame  frame     = NULL;

	MpiCmd mpi_cmd      = MPP_CMD_BASE;
	MppParam param      = NULL;
	RK_U32 need_split   = 1;
	// MppPollType timeout = 5;

	// paramter for resource malloc
	RK_U32 width        = 2560;
	RK_U32 height       = 1440;
	MppCodingType type  = ff_info.codec_type == "h264" ? MPP_VIDEO_CodingAVC : MPP_VIDEO_CodingHEVC;

	// resources
	char *buf           = NULL;
	size_t packet_size  = 8*1024;
	MppBuffer pkt_buf   = NULL;
	MppBuffer frm_buf   = NULL;
	
	MpiDecLoopData data;

	mpp_log("mpi_dec_test start\n");
	memset(&data, 0, sizeof(data));

	data.fp_output = fopen("./tenoutput.yuv", "w+b");
	if (NULL == data.fp_output) {
		mpp_err("failed to open output file %s\n", "tenoutput.yuv");
		deInit(&packet, &frame, ctx, buf, data);
	}

	// mpp_log("mpi_dec_test decoder test start w %d h %d type %d\n", width, height, type);

	// decoder demo
	ret = mpp_create(&ctx, &mpi);

	if (MPP_OK != ret) {
		mpp_err("mpp_create failed\n");
		deInit(&packet, &frame, ctx, buf, data);
	}

	// NOTE: decoder split mode need to be set before init
	mpi_cmd = MPP_DEC_SET_PARSER_SPLIT_MODE;
	param = &need_split;
	ret = mpi->control(ctx, mpi_cmd, param);
	if (MPP_OK != ret) {
		mpp_err("mpi->control failed\n");
		deInit(&packet, &frame, ctx, buf, data);
	}

	mpi_cmd = MPP_SET_INPUT_BLOCK;
	param = &need_split;
	ret = mpi->control(ctx, mpi_cmd, param);
	if (MPP_OK != ret) {
		mpp_err("mpi->control failed\n");
		deInit(&packet, &frame, ctx, buf, data);
	}

	mpi_cmd = MPP_DEC_SET_IMMEDIATE_OUT;
	param = &need_split;
	ret = mpi->control(ctx, mpi_cmd, param);
	if (MPP_OK != ret) {
		mpp_err("mpi->control failed\n");
		deInit(&packet, &frame, ctx, buf, data);
	}

	ret = mpp_init(ctx, MPP_CTX_DEC, type);
	if (MPP_OK != ret) {
		mpp_err("mpp_init failed\n");
		deInit(&packet, &frame, ctx, buf, data);
	}

	data.ctx            = ctx;
	data.mpi            = mpi;
	data.eos            = 0;
	data.packet_size    = packet_size;
	data.frame          = frame;
	data.frame_count    = 0;


	int count = 0;
	//这边可以调整i的大小来改变文件中的视频时间,每 +1 就是一帧数据
	AVFormatContext *pFormatCtx = ff_info.pFormatCtx;
	AVPacket *av_packet = ff_info.av_packet;
	int videoindex = ff_info.videoindex;

	double e_record_time, s_record_time, stime, etime;
	int index = 0;
	while (1) {
		if (av_read_frame(pFormatCtx, av_packet) >= 0) {
			if (av_packet->stream_index == videoindex) {
				// mpp_log("--------------\ndata size is: %d\n-------------", av_packet->size);
				decode_simple(&data, av_packet, &mppenc, input_params);

				index += 1;
				if (index % 100 == 0) {
					e_record_time = get_current_time();
					float runtime = e_record_time - s_record_time;// 100 次总时间
					float fps =  100*1000/runtime; // 1秒的次数
					fprintf(stderr, " =======> video decode: %f ms,%f fps \n",runtime/100, fps);
					s_record_time = e_record_time;
				}
			}
			if (av_packet != NULL)
				av_packet_unref(av_packet);
			// mpp_log("%d", i);

		}
	}

	av_free(av_packet);
	avformat_close_input(&pFormatCtx);

	return 0;
}
