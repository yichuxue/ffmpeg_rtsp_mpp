//
// Created by LX on 2020/4/25.
//
#include "rga.h"
#include "RgaUtils.h"
#include "MppDecode.h"




int frame_null = 0;
int count = 0;
FILE *pPipe;

// FILE* fp = fopen("I4201.h264", "wb+");
unsigned char* src_buffer = NULL;
unsigned char* dst_buf = NULL;

size_t mpp_buffer_group_usage(MppBufferGroup group)
{
    if (NULL == group)
    {
        mpp_err_f("input invalid group %p\n", group);
        return MPP_BUFFER_MODE_BUTT;
    }

    MppBufferGroupImpl *p = (MppBufferGroupImpl *)group;
    return p->usage;
}

int decode_simple(MpiDecLoopData *data, AVPacket *av_packet, whale::vision::MppEncoder* mppenc, InParams input_params)
{
    RK_U32 pkt_done = 0;
    RK_U32 pkt_eos  = 0;
    RK_U32 err_info = 0;
    MPP_RET ret = MPP_OK;
    MppCtx ctx  = data->ctx;
    MppApi *mpi = data->mpi;
    // char   *buf = data->buf;
    MppPacket packet = NULL;
    MppFrame  frame  = NULL;
    size_t read_size = 0;
    size_t packet_size = data->packet_size;

		// fwrite(av_packet->data, av_packet->size, 1, pPipe);
  	// fflush(pPipe);
		// return 0;

    ret = mpp_packet_init(&packet, av_packet->data, av_packet->size);
    mpp_packet_set_pts(packet, av_packet->pts);

    do {
        RK_S32 times = 5;
        // send the packet first if packet is not done
        if (!pkt_done) {
            ret = mpi->decode_put_packet(ctx, packet);
            if (MPP_OK == ret)
              pkt_done = 1;
        }

        // then get all available frame and release
        do {
            RK_S32 get_frm = 0;
            RK_U32 frm_eos = 0;

            try_again:
            ret = mpi->decode_get_frame(ctx, &frame);
            if (MPP_ERR_TIMEOUT == ret) {
                if (times > 0) {
                    times--;
                    msleep(2);
                    goto try_again;
                }
                mpp_err("decode_get_frame failed too much time\n");
            }
            if (MPP_OK != ret) {
                mpp_err("decode_get_frame failed ret %d\n", ret);
                break;
            }

            if (frame) {
                if (mpp_frame_get_info_change(frame)) {
                    RK_U32 width = mpp_frame_get_width(frame);
                    RK_U32 height = mpp_frame_get_height(frame);
                    RK_U32 hor_stride = mpp_frame_get_hor_stride(frame);
                    RK_U32 ver_stride = mpp_frame_get_ver_stride(frame);
                    RK_U32 buf_size = mpp_frame_get_buf_size(frame);

                    mpp_log("decode_get_frame get info changed found\n");
                    mpp_log("decoder require buffer w:h [%d:%d] stride [%d:%d] buf_size %d",
                            width, height, hor_stride, ver_stride, buf_size);

                    ret = mpp_buffer_group_get_internal(&data->frm_grp, MPP_BUFFER_TYPE_ION);
                    if (ret) {
                        mpp_err("get mpp buffer group  failed ret %d\n", ret);
                        break;
                    }
                    mpi->control(ctx, MPP_DEC_SET_EXT_BUF_GROUP, data->frm_grp);

                    mpi->control(ctx, MPP_DEC_SET_INFO_CHANGE_READY, NULL);
                } else {
                    err_info = mpp_frame_get_errinfo(frame) | mpp_frame_get_discard(frame);
                    if (err_info) {
                        mpp_log("decoder_get_frame get err info:%d discard:%d.\n",
                                mpp_frame_get_errinfo(frame), mpp_frame_get_discard(frame));
                    }
                    data->frame_count++;
                    // mpp_log("decode_get_frame get frame %d\n", data->frame_count);
                   if (data->fp_output && !err_info){
											// cv::Mat rgbImg;
											// count += 1;
											// if (count % 10 == 0) resize(frame, mppenc);
											// if (count % 10 == 0) YUV420SP2Mat(frame);
											resize(frame, mppenc, input_params);
											
												

											// 测试
											// dump_mpp_frame_to_file(frame, data->fp_output);
                   }
                }
                frm_eos = mpp_frame_get_eos(frame);
                mpp_frame_deinit(&frame);

                frame = NULL;
                get_frm = 1;
            }

            // try get runtime frame memory usage
            if (data->frm_grp) {
                size_t usage = mpp_buffer_group_usage(data->frm_grp);
                if (usage > data->max_usage)
                    data->max_usage = usage;
            }

            // if last packet is send but last frame is not found continue
            if (pkt_eos && pkt_done && !frm_eos) {
                msleep(10);
                continue;
            }

            if (frm_eos) {
                mpp_log("found last frame\n");
                break;
            }

            if (data->frame_num > 0 && data->frame_count >= data->frame_num) {
                data->eos = 1;
                break;
            }

            if (get_frm)
                continue;
            break;
        } while (1);

        if (data->frame_num > 0 && data->frame_count >= data->frame_num) {
            data->eos = 1;
            mpp_log("reach max frame number %d\n", data->frame_count);
            break;
        }

        if (pkt_done)
            break;

        /*
         * why sleep here:
         * mpi->decode_put_packet will failed when packet in internal queue is
         * full,waiting the package is consumed .Usually hardware decode one
         * frame which resolution is 1080p needs 2 ms,so here we sleep 3ms
         * * is enough.
         */
        msleep(3);
    } while (1);
    mpp_packet_deinit(&packet);

    return ret;
}

// void YUV420SP2Mat(MppFrame frame, cv::Mat rgbImg ) {
// 	RK_U32 width = 0;
// 	RK_U32 height = 0;
// 	RK_U32 h_stride = 0;
// 	RK_U32 v_stride = 0;

// 	MppBuffer buffer = NULL;
// 	RK_U8 *base = NULL;

// 	width = mpp_frame_get_width(frame);
// 	height = mpp_frame_get_height(frame);
// 	h_stride = mpp_frame_get_hor_stride(frame);
// 	v_stride = mpp_frame_get_ver_stride(frame);
// 	buffer = mpp_frame_get_buffer(frame);

// 	printf("width: %d, height: %d, h_stride: %d, v_stride: %d \n", width, height, h_stride, v_stride);

// 	// resize(frame);

// 	base = (RK_U8 *)mpp_buffer_get_ptr(buffer);
// 	RK_U32 buf_size = mpp_frame_get_buf_size(frame);
// 	size_t base_length = mpp_buffer_get_size(buffer);
// 	// mpp_log("base_length = %d\n",base_length);

// 	RK_U32 i;
// 	RK_U8 *base_y = base;
// 	RK_U8 *base_c = base + h_stride * v_stride;

// 	cv::Mat yuvImg;
// 	yuvImg.create(height * 3 / 2, width, CV_8UC1);

// 	//转为YUV420p格式
// 	int idx = 0;
// 	for (i = 0; i < height; i++, base_y += h_stride) {
// 		//        fwrite(base_y, 1, width, fp);
// 		memcpy(yuvImg.data + idx, base_y, width);
// 		idx += width;
// 	}
// 	for (i = 0; i < height / 2; i++, base_c += h_stride) {
// 		//        fwrite(base_c, 1, width, fp);
// 		memcpy(yuvImg.data + idx, base_c, width);
// 		idx += width;
// 	}
// 	//这里的转码需要转为RGB 3通道， RGBA四通道则不能检测成功
// 	cv::cvtColor(yuvImg, rgbImg, CV_YUV420sp2RGB);
// 	cv::imwrite("./111.jpg", rgbImg);
	
// }

FFInfo ffmpeg_init(InParams input_params) {
	FFInfo info;
	AVFormatContext *pFormatCtx = NULL;
	AVDictionary *options = NULL;
	AVPacket *av_packet = NULL;
	// char filepath[] = "rtsp://192.168.31.8/test";// rtsp 地址 720p
	// char filepath[] = "rtsp://admin:fuxig888@192.168.26.108/h264/main/sub/av_stream"; // 2k
	// char filepath[] = "rtsp://admin:anlly1205@192.168.131.12/h264/main/sub/av_stream";
	// char filepath[] = "rtsp://192.168.25.188/stream0"; // 1080p
	// h264 16k   h265 256k
	
	dst_buf = (unsigned char*)malloc(input_params.dst_width*input_params.dst_height*get_bpp_from_format(RK_FORMAT_YCrCb_420_SP));

	av_register_all();  //函数在ffmpeg4.0以上版本已经被废弃，所以4.0以下版本就需要注册初始函数
	avformat_network_init();
	// av_dict_set(&options, "buffer_size", "1024000", 0); //设置缓存大小,1080p可将值跳到最大
	av_dict_set(&options, "buffer_size", "2048000", 0); //设置缓存大小,1080p可将值跳到最大
	av_dict_set(&options, "rtsp_transport", "tcp", 0); //以tcp的方式打开,
	av_dict_set(&options, "stimeout", "20000000", 0); //设置超时断开链接时间，单位us
	av_dict_set(&options, "max_delay", "500000", 0); //设置最大时延
	av_dict_set(&options, "probesize","2048",0);
	av_dict_set(&options, "max_analyze_duration","10", 0);
	pFormatCtx = avformat_alloc_context(); //用来申请AVFormatContext类型变量并初始化默认参数,申请的空间
	//打开网络流或文件流
	if (avformat_open_input(&pFormatCtx, input_params.src_rtsp_url.c_str(), NULL, &options) != 0) {
		printf("Couldn't open input stream.\n");
		info.status = 0;
		return info;
	}

	//获取视频文件信息
	if (avformat_find_stream_info(pFormatCtx, NULL)<0) {
		printf("Couldn't find stream information.\n");
		info.status = 0;
		return info;
	}

	//查找码流中是否有视频流
	int videoindex = -1;
	unsigned i = 0;
	for (i = 0; i<pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoindex = i;
			break;
		}
	if (videoindex == -1) {
		printf("Didn't find a video stream.\n");
		info.status = 0;
		return info;
	}

	// Output Info---输出一些文件（RTSP）信息
	printf("---------------- File Information (%s)---------------\n", avcodec_get_name(pFormatCtx->streams[videoindex]->codec->codec_id));
	av_dump_format(pFormatCtx, 0, input_params.src_rtsp_url.c_str(), 0);
	printf("videoindex: %d \n", videoindex);
	printf("-------------------------------------------------\n");
	av_packet = (AVPacket *)av_malloc(sizeof(AVPacket)); // 申请空间，存放的每一帧数据 （h264、h265）
	
	// 打印rtsp 流信息
	uint8_t *printed = (uint8_t *)av_mallocz(pFormatCtx->nb_streams);
	for (int i = 0; i < pFormatCtx->nb_streams; i++) {
		if (!printed[i]) {
			AVStream *st = pFormatCtx->streams[i];
			int fps = 0;
			if (st->avg_frame_rate.den != 0) {
				fps = st->avg_frame_rate.num / st->avg_frame_rate.den;
			}

			int w = st->codecpar->width;
			int h = st->codecpar->height;
			int bit_rate = st->codecpar->bit_rate/1000;
			if (w == 0 || h == 0) continue;
			cout << "==================== src_rtsp ====================" << endl;
			// printf("==================== src_rtsp ==================== \n");
			// printf("width: %d, height: %d, fps: %d, bit_rate: %f \n", w, h, fps, bit_rate);
			cout << "width: " << w << ", height: " << h << ", fps: " << fps << ", bit_rate: " << bit_rate << endl;
			cout << "==================== src_rtsp ====================" << endl;
			// printf("==================== src_rtsp ==================== \n");
		}
	}

	free(printed);

	string cmds = "ffmpeg -y -f h264 -re -r 30 -i - -c:v copy -an -loglevel quiet -rtsp_transport tcp -f rtsp " + input_params.dst_rtsp_url;
	// string cmds = "ffmpeg -y -f h264 -re -r 30 -i - -c:v copy -an -rtsp_transport tcp -f rtsp " + input_params.dst_rtsp_url;
	// printf("==> cmd: %s \n", cmds.c_str());
	pPipe = popen(cmds.c_str(), "w");

	info.status = 1;
	info.videoindex = videoindex;
	info.av_packet = av_packet;
	info.pFormatCtx = pFormatCtx;
	info.codec_type = avcodec_get_name(pFormatCtx->streams[videoindex]->codec->codec_id);
	return info;
}


int get_buf_size_by_w_h_f(int w, int h, int f) {
	float bpp = get_bpp_from_format(f);
	int size = 0;

	size = (int)w * h * bpp;
	return size;
}



void resize(MppFrame frame, whale::vision::MppEncoder* mppenc, InParams input_params) {
	IM_STATUS 			STATUS;
	rga_buffer_t 		src;
  rga_buffer_t 		dst;
	im_rect         src_rect;
  im_rect         dst_rect;
	int 						dst_fd;

	int width = mpp_frame_get_width(frame);
	int height = mpp_frame_get_height(frame);
	int h_stride = mpp_frame_get_hor_stride(frame);
	int v_stride = mpp_frame_get_ver_stride(frame);
	MppBuffer buffer = mpp_frame_get_buffer(frame);
	// printf("resize ===> width: %d, height: %d, h_stride: %d, v_stride: %d \n", width, height, h_stride, v_stride);
	
	src_buffer = (unsigned char *)mpp_buffer_get_ptr(mpp_frame_get_buffer(frame));

	memset(&src_rect, 0, sizeof(src_rect));
  memset(&dst_rect, 0, sizeof(dst_rect));

	memset(&src, 0, sizeof(src));
  memset(&dst, 0, sizeof(dst));

	src = wrapbuffer_virtualaddr(src_buffer, h_stride, v_stride, RK_FORMAT_YCbCr_420_SP);
	dst = wrapbuffer_virtualaddr(dst_buf, input_params.dst_width, input_params.dst_height, RK_FORMAT_YCbCr_420_SP);

	int ret = imcheck(src, dst, src_rect, dst_rect);
	if (IM_STATUS_NOERROR != ret) {
		printf("Rga resize: %d, check error! %s\n", __LINE__, imStrError((IM_STATUS)ret));
		return ;
	}

	STATUS = imresize(src, dst);

	if (STATUS <= 0) {
		printf("Rga resize: resize error for status: %s\n", imStrError(STATUS));
		return ;
	}


	int length = 0;
	char dsts[1024*1024*4];
	char *pdst = dsts;

	mppenc->encode(dst_buf, input_params.dst_width*input_params.dst_height*get_bpp_from_format(RK_FORMAT_YCrCb_420_SP), pdst, &length);
	fwrite(pdst, length, 1, pPipe);
  fflush(pPipe);
	// free(dst_buf);
	// free(src_buffer);

	// printf("resize successful\n");

	// RK_U8 *base = (RK_U8 *)dst_buf;
	// RK_U8 *base_y = base;
	// RK_U8 *base_c = dst_buf + 480 * 640;

	// cv::Mat yuvImg;
	// cv::Mat rgbImg;
	// yuvImg.create(480 * 3 / 2, 640, CV_8UC1);

	// memcpy(yuvImg.data, base, 640*480*1.5);
	// //这里的转码需要转为RGB 3通道， RGBA四通道则不能检测成功
	// cv::cvtColor(yuvImg, rgbImg, CV_YUV420sp2RGB);
	// cv::imwrite("./111.jpg", rgbImg);
	
	
	// printf("================================> length: %d \n", length);
	
	// if ( (pPipe = popen("ffmpeg -y -f h264 -re -r 25 -i - -c:v copy -an -rtsp_transport tcp -f rtsp rtsp://192.168.31.8/lex", "wb")) == NULL) {
	// 	printf("Error: Could not open ffmpeg\n");
	// }
	// printf("=========> %s %d \n", strerror(errno), errno);
	// fwrite(dst_buf, 1, length, pPipe);
	

	// free(dst_buf);

	// fwrite(dsts, length, 1,  fp);
	// fclose(fp);
	// cv::Mat rgbImg;
	// MppFrame dst_frame = NULL;
	// ret = mpp_frame_init(&dst_frame); /* output frame */
	// if (ret) {
	// 	mpp_err("mpp_frame_init failed\n");
	// }
	// mpp_frame_set_height(dst_frame, 480);
	// mpp_frame_set_width(dst_frame, 640);
	// mpp_frame_set_hor_stride(dst_frame, 480);
	// mpp_frame_set_ver_stride(dst_frame, 640);
	// mpp_frame_set_fmt(dst_frame, MPP_FMT_YUV422SP);
	// mpp_frame_set_buffer(dst_frame, mBuffer);
	// printf("==> 00\n");
	// MppBuffer dst_buffer = (unsigned char*)malloc(640*480*get_bpp_from_format(RK_FORMAT_YCrCb_420_SP));
	// printf("==> 11\n");
	// memcpy(dst_buffer, &dst_buf, get_buf_size_by_w_h_f(640, 480, dst.format));
	// printf("00\n");
	// mpp_frame_set_buffer(dst_frame, dst_buffer);
	// printf("11\n");
	// YUV420SP2Mat(dst_frame, rgbImg);

	// mppenc.encode(file.data(), file.size(), pdst, &length);

	// free(src_buffer);
  // free(dst_buf);


	// src.width = width;
	// src.height = height;
	// // src.fd = src_fd;
	// src.phy_addr = src_buffer;
	// src.hstride = h_stride;
	// src.wstride = v_stride;
	// src.format = SRC_FORMAT;
	// src = wrapbuffer_virtualaddr(&buffer, width, height, SRC_FORMAT);
  // dst = wrapbuffer_physicaladdr(dst_buf, 1280, 720, DST_FORMAT);
	// dst = wrapbuffer_virtualaddr(dst_buf, 1280, 720, DST_FORMAT);

	// dst = wrapbuffer_fd(dst_fd, 1280, 720, DST_FORMAT);
	// AHardwareBuffer_Init(1280, 720, DST_FORMAT, &dst_buf);

	// printf("\n ======================== %s\n", querystring(RGA_VENDOR));
	// STATUS = imresize(src, dst);
	// printf("================> STATUS: %d \n", STATUS);

	// unsigned char* src_buf = NULL;
	// unsigned char* dst_buf = NULL;

	// src_buf = (unsigned char*)malloc(800*480*3);
	// dst_buf = (unsigned char*)malloc(400*240*3);

	// testResize(src_buf, dst_buf);
}


void testResize(unsigned char* src_buf, unsigned char* dst_buf) {
	int ret;
	IM_STATUS       STATUS;

	im_rect         src_rect;
	im_rect         dst_rect;
	rga_buffer_t    src;
	rga_buffer_t    dst;

	memset(&src_rect, 0, sizeof(src_rect));
	memset(&dst_rect, 0, sizeof(dst_rect));
	memset(&src, 0, sizeof(src));
	memset(&dst, 0, sizeof(dst));

	// #define RK_FORMAT_BGR_888 0x7 << 8

	src = wrapbuffer_virtualaddr(src_buf, 1280, 720, RK_FORMAT_YCrCb_420_SP);
	dst = wrapbuffer_virtualaddr(dst_buf, 640, 480, RK_FORMAT_YCrCb_420_SP);

	ret = imcheck(src, dst, src_rect, dst_rect);
	if (IM_STATUS_NOERROR != ret) {
		printf("Rga resize: %d, check error! %s\n", __LINE__, imStrError((IM_STATUS)ret));
		return ;
	}

	STATUS = imresize(src, dst);
	if (STATUS <= 0) {
		printf("Rga resize: resize error for status: %s\n", imStrError(STATUS));
		return ;
	}
	printf("resize successful\n");

	cv::Mat rgbImg;
	// YUV420SP2Mat(dst.vir_addr, rgbImg);
}


