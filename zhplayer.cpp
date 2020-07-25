#include <iostream>
#include <cstring>
#include "zhplayer.h"
#include "Queue.h"
#define SDL_MAIN_HANDLED
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <SDL/SDL.h>
#include <SDL/SDL_main.h>
}
using std::cin;
using std::cout;
using std::endl;
using std::flush;
using std::string;
using namespace std;
#pragma comment(lib ,"SDL2.lib")
#pragma comment(lib ,"SDL2main.lib")
#define MAX_AUDIO_FRAME_SIZE 192000 //�����ʣ�1 second of 48khz 32bit audio 

//��Ƶ
void decode_video_thread(PlayerContext* playerCtx) {
	AVFrame* pFrame = nullptr;
	while (playerCtx && !playerCtx->quit) {
		AVPacket* pkt;
		if (!playerCtx->video_queue.pull(pkt)) { // ����Ƶ������ȡ����Ƶ��
			SDL_Delay(1);
			continue;
		}
		//������Ƶ�������һ����Ƶ֡
		if (avcodec_send_packet(playerCtx->videoCodecCtx, pkt)) {
			exit(1);
			return;
		}
		int ret = 0;
		while (ret >= 0) {
			if (!pFrame) {
				if (!(pFrame = av_frame_alloc())) {
					cout <<"Could not allocate audio frame" <<endl;
					exit(1);
				}
			}
			// �ӱ���������һ��frame
			ret = avcodec_receive_frame(playerCtx->videoCodecCtx, pFrame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				break;
			//������Ƶʱ���
			double pts = 0;
			int64_t f_pts = pFrame->pts;
			int64_t pkt_dts = pFrame->pkt_dts;
			if (pkt_dts != AV_NOPTS_VALUE) {// ���ã��ò�����pts��Ϊ��Ƶ��ʱ���
				pts = av_frame_get_best_effort_timestamp(pFrame);  
			}
			else {
				pts = pFrame->pts; 
			}
			//av_q2d����תС����pts����time_base�õ�ʱ���
			pts *= av_q2d(playerCtx->video_stream->time_base);
			if (pts == 0.0) {
				pts = playerCtx->video_clk;
			}
			//������Ƶ����ʱ���
			double frame_delay = av_q2d(playerCtx->video_stream->time_base);
			//������Ƶ�ӳ٣�extra_delay = repeat_pict / (2*fps) => extra_delay = repeat_pict * fps * 0.5
			frame_delay += pFrame->repeat_pict * (frame_delay * 0.5); 
			//ʵ��ʱ���=��Ƶ֡+�ӳ٣�������Ƶʱ���
			pts += frame_delay; 
			playerCtx->video_clk = pts; 

			//����Ƶͬ����΢��->����
			int64_t delay = (playerCtx->video_clk - (playerCtx->audio_clk + playerCtx->audio_pts_duration)) * 1000; 
			if (delay > 0) { //��Ƶʱ�����ǰ�ˣ��ȴ�һ��ʱ������ʾ��Ƶ��
				SDL_Delay(delay);
			}

			//SDL ��ʾ��Ƶ���������ٸ��£�������Ⱦ�ӿ�������ʾ��Ҫfree
			sws_scale(playerCtx->vi_convert_ctx, pFrame->data, pFrame->linesize, 0 // ����Ƶ���ݽ�������
				, playerCtx->videoCodecCtx->height, playerCtx->pFrameYUV->data, playerCtx->pFrameYUV->linesize);
			// texture����rect����nullptrΪ����ȫ�֣�plane�������ݣ�pitch���ݴ�С
			SDL_UpdateYUVTexture(playerCtx->texture, nullptr,
				playerCtx->pFrameYUV->data[0], playerCtx->pFrameYUV->linesize[0], //y
				playerCtx->pFrameYUV->data[1], playerCtx->pFrameYUV->linesize[1], //u
				playerCtx->pFrameYUV->data[2], playerCtx->pFrameYUV->linesize[2]); //v
			SDL_RenderClear(playerCtx->renderer);
			SDL_RenderCopy(playerCtx->renderer, playerCtx->texture, nullptr, &playerCtx->sdlRect);
			SDL_RenderPresent(playerCtx->renderer);
			av_frame_unref(pFrame);
		}

		av_frame_free(&pFrame);
		av_packet_unref(pkt);
		av_packet_free(&pkt);
	}

}
//��Ƶ
void decode_audio_thread(PlayerContext* playerCtx) {
	AVFrame* pFrame = nullptr;
	while (playerCtx && !playerCtx->quit) {
		AVPacket* pkt;
		if (!playerCtx->audio_queue.pull(pkt)) { //��Ƶ��
			SDL_Delay(1);
			continue;
		}

		//�����һ����Ƶ֡
		if (avcodec_send_packet(playerCtx->audioCodecCtx, pkt)) {
			exit(1);
			return;
		}
		int ret = 0;
		while (ret >= 0) {
			if (!pFrame) {
				if (!(pFrame = av_frame_alloc())) {
					cout << "Could not allocate audio frame" << endl;
					exit(1);
				}
			}
			// �ӱ���������һ��frame
			ret = avcodec_receive_frame(playerCtx->audioCodecCtx, pFrame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				break;
			else if (ret < 0){
				cout << "Error during decoding" << endl;
				exit(1);
				return;
			}
			//����Ƶ֡�����ز���������ϵͳ����������
			swr_convert(playerCtx->au_convert_ctx, &playerCtx->out_buffer, playerCtx->out_buffer_size, // ������ת��
				(const uint8_t**)pFrame->data, playerCtx->audioCodecCtx->frame_size);
			while (playerCtx->audio_len > 0) {// ��Ƶ����δ������ʱ
				SDL_Delay(1);
			}
			//����������Ƶ����
			playerCtx->audio_pos = (Uint8*)playerCtx->out_buffer;
			playerCtx->audio_len = playerCtx->out_buffer_size;
			//������Ƶʱ����ͳ���ʱ�䣬�ǵ�free
			if (pFrame->pts != AV_NOPTS_VALUE) {
				playerCtx->audio_clk = av_q2d(playerCtx->audio_stream->time_base) * pFrame->pts; 
			}
			playerCtx->audio_pts_duration = av_q2d(playerCtx->audio_stream->time_base) * pFrame->pkt_duration;
			av_frame_unref(pFrame);
		}
		av_frame_free(&pFrame);
		av_packet_unref(pkt);
		av_packet_free(&pkt);
	}
}
//SDL�ص�����
void sdl_audio_callback(void* userdata, Uint8* stream, int len) {
	PlayerContext* playerCtx = (PlayerContext*)userdata;
	if (!playerCtx || playerCtx->quit) {
		return;
	}
	// ���stream�е��ڴ� SDL 2.0  
	SDL_memset(stream, 0, len);
	if (playerCtx->audio_len == 0)
		return;
	// �����ܵĸ�ϵͳ������Ƶbuffer�������ᳬ���Ѿ������Ĵ�С(audio_len),Ҳ����������ϵͳ��Ҫ�Ĵ�С(len)
	Uint32 streamlen = ((Uint32)len > playerCtx->audio_len ? playerCtx->audio_len : len);
	SDL_MixAudio(stream, playerCtx->audio_pos, streamlen, SDL_MIX_MAXVOLUME); // ��ϵͳҪ����Ŀؼ���ֵ
	playerCtx->audio_pos += streamlen; // ��Ƶ�����λ����ǰ
	playerCtx->audio_len -= streamlen; // ��Ƶ����ȥ���Ѿ�������Ĵ�С
}

//��Ƶ��ʼ�����ز�������
int init_audio_parameters(PlayerContext& playerCtx) {
	//��ȡ��Ƶ����������
	playerCtx.audioCodecParameter = playerCtx.pFormateCtx->streams[playerCtx.au_stream_index]->codecpar;
	//��ȡ��Ƶ������
	playerCtx.audioCodec = avcodec_find_decoder(playerCtx.audioCodecParameter->codec_id);
	if (nullptr == playerCtx.audioCodec) {
		cout << "audio avcodec_find_decoder failed" << endl;
		return -1;
	}
	//��ȡ������������
	playerCtx.audioCodecCtx = avcodec_alloc_context3(playerCtx.audioCodec);
	//������Ƶ����������Ƶ������
	if (avcodec_parameters_to_context(playerCtx.audioCodecCtx, playerCtx.audioCodecParameter) < 0) {
		cout << "audio avcodec_parameters_to_context failed" << endl;
		return -1;
	}
	//����������������Ƶ������
	avcodec_open2(playerCtx.audioCodecCtx, playerCtx.audioCodec, nullptr);

	//�����ز�����ز���
	uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO; // ˫�������
	int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
	playerCtx.out_sample_fmt = AV_SAMPLE_FMT_S16; // �������Ƶ��ʽ
	int out_sample_rate = 44100; // ������
	int64_t in_channel_layout = av_get_default_channel_layout(playerCtx.audioCodecCtx->channels); //����ͨ����
	playerCtx.audioCodecCtx->channel_layout = in_channel_layout;
	playerCtx.au_convert_ctx = swr_alloc(); // ��ʼ���ز����ṹ��
	playerCtx.au_convert_ctx = swr_alloc_set_opts(playerCtx.au_convert_ctx, out_channel_layout, playerCtx.out_sample_fmt, out_sample_rate,
		in_channel_layout, playerCtx.audioCodecCtx->sample_fmt, playerCtx.audioCodecCtx->sample_rate, 0, nullptr); //�����ز�����
	swr_init(playerCtx.au_convert_ctx); // ��ʼ���ز�����
	int out_nb_samples = playerCtx.audioCodecCtx->frame_size;
	//������ز�������Ҫ��buffer��С�����ڴ���ת�������Ƶ����ʱ��
	playerCtx.out_buffer_size = av_samples_get_buffer_size(nullptr, playerCtx.audioCodecCtx->channels, out_nb_samples, playerCtx.out_sample_fmt, 1);
	playerCtx.out_buffer = (uint8_t*)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);

	//SDL������Ƶʱ�Ĳ���
	playerCtx.wanted_spec.freq = out_sample_rate;//44100;
	playerCtx.wanted_spec.format = AUDIO_S16SYS;
	playerCtx.wanted_spec.channels = out_channels;//playerCtx.audioCodecCtx->channels;
	playerCtx.wanted_spec.silence = 0;
	playerCtx.wanted_spec.samples = out_nb_samples;//frame->nb_samples;
	playerCtx.wanted_spec.callback = sdl_audio_callback;//sdlϵͳ�ص�
	playerCtx.wanted_spec.userdata = &playerCtx; //�ص�ʱ�����ȥ�Ĳ���

	// SDL����Ƶ�����豸
	if (SDL_OpenAudio(&playerCtx.wanted_spec, nullptr) < 0) {
		cout << "can't open audio." << endl;
		return -1;
	}
	// ��ͣ/������Ƶ������Ϊ0������Ƶ����0��ͣ��Ƶ
	SDL_PauseAudio(0);
	return 0;
}
//��Ƶ��ʼ��������ת��������
int init_video_paramerters(PlayerContext& playerCtx) {
	//��ȡ��Ƶ����������
	playerCtx.videoCodecParameter = playerCtx.pFormateCtx->streams[playerCtx.video_stream_index]->codecpar;
	//��ȡ��Ƶ������
	playerCtx.pVideoCodec = avcodec_find_decoder(playerCtx.videoCodecParameter->codec_id);
	if (nullptr == playerCtx.pVideoCodec) {
		cout << "video avcodec_find_decoder failed." << endl;
		return -1;
	}
	//��ȡ������������
	playerCtx.videoCodecCtx = avcodec_alloc_context3(playerCtx.pVideoCodec);
	//������Ƶ����������Ƶ������
	if (avcodec_parameters_to_context(playerCtx.videoCodecCtx, playerCtx.videoCodecParameter) < 0) {
		cout << "video avcodec_parameters_to_context failed" << endl;
		return -1;
	}
	//����������������Ƶ������
	avcodec_open2(playerCtx.videoCodecCtx, playerCtx.pVideoCodec, nullptr);

	//����һ��SDL���ڣ���Ƶ�����ɵ��������ļ��е��
	playerCtx.screen = SDL_CreateWindow("MediaPlayer",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		//playerCtx.videoCodecCtx->width, playerCtx.videoCodecCtx->height,
		1280, 720,
		SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
	if (!playerCtx.screen) {
		cout << "SDL: could not set video mode - exiting" << endl;
		exit(1);
	}
	// ����һ��SDL��Ⱦ��
	playerCtx.renderer = SDL_CreateRenderer(playerCtx.screen, -1, 0);
	// ����һ��SDL����
	playerCtx.texture = SDL_CreateTexture(playerCtx.renderer,
		SDL_PIXELFORMAT_YV12,
		SDL_TEXTUREACCESS_STREAMING,
		playerCtx.videoCodecCtx->width, playerCtx.videoCodecCtx->height);
	// ����SDL��Ⱦ������
	playerCtx.sdlRect.x = 0;
	playerCtx.sdlRect.y = 0;
	playerCtx.sdlRect.w = 1280;// playerCtx.videoCodecCtx->width;
	playerCtx.sdlRect.h = 720;// playerCtx.videoCodecCtx->height;
	// ������Ƶ����ת����
	playerCtx.vi_convert_ctx = sws_getContext(playerCtx.videoCodecCtx->width, playerCtx.videoCodecCtx->height, playerCtx.videoCodecCtx->pix_fmt
		, playerCtx.videoCodecCtx->width, playerCtx.videoCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, nullptr, nullptr, nullptr);
	// ������Ƶ֡����Ƶ���ؿռ�
	unsigned char* out_buffer = (unsigned char*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P
		, playerCtx.videoCodecCtx->width, playerCtx.videoCodecCtx->height, 1));
	playerCtx.pFrameYUV = av_frame_alloc(); // ������Ƶ֡
	// ������Ƶ֡���������ݿռ�
	av_image_fill_arrays(playerCtx.pFrameYUV->data, playerCtx.pFrameYUV->linesize
		, out_buffer, AV_PIX_FMT_YUV420P, playerCtx.videoCodecCtx->width, playerCtx.videoCodecCtx->height, 1);
	return 0;
}

//int _tmain()
int main(int argc, char* argv[])
{
	//������Ƶ�ļ��ĵ�ַ����ֱ�Ӹ�Ϊ���ɱ��ص�·��
	//char filePath[] = "F:/forgit/gitrep/mp4player/mp4player/42stest.mp4";
	string filePath;
	cout << "Enter file path:" << flush;
	cin >> filePath;
	
	// ע�����б�����
	av_register_all();

	// ����Ƶ�Ļ���
	PlayerContext playerCtx;

	//��ȡ�ļ�ͷ�ĸ�ʽ��Ϣ���浽pFormateCtx��
	if (avformat_open_input(&playerCtx.pFormateCtx, filePath.c_str(), nullptr, 0) != 0) {
		cout << "avformat_open_input failed" << endl;
		return -1;
	}
	//��ȡ�ļ��е�����Ϣ���浽pFormateCtx��
	if (avformat_find_stream_info(playerCtx.pFormateCtx, nullptr) < 0) {
		cout << "avformat_find_stream_info failed" << endl;
		return -1;
	}
	// ���ļ���Ϣ���浽��׼������
	av_dump_format(playerCtx.pFormateCtx, 0, filePath.c_str(), 0);

	// ������Ƶ������Ƶ����λ��
	for (unsigned i = 0; i < playerCtx.pFormateCtx->nb_streams; ++i)
	{
		if (AVMEDIA_TYPE_VIDEO == playerCtx.pFormateCtx->streams[i]->codecpar->codec_type
			&& playerCtx.video_stream_index < 0) { // ��ȡ��Ƶ��λ��
			playerCtx.video_stream_index = i;
			playerCtx.video_stream = playerCtx.pFormateCtx->streams[i];
		}
		if (AVMEDIA_TYPE_AUDIO == playerCtx.pFormateCtx->streams[i]->codecpar->codec_type
			&& playerCtx.au_stream_index < 0) { // ��ȡ��Ƶ��λ��
			playerCtx.au_stream_index = i;
			playerCtx.audio_stream = playerCtx.pFormateCtx->streams[i];
			continue;
		}
	}
	// �쳣����
	if (playerCtx.video_stream_index == -1)
		return -1;
	if (playerCtx.au_stream_index == -1)
		return -1;

	// ��ʼ�� SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
	{
		cout << "Could not initialize SDL - "<< SDL_GetError() << endl;
		return -1;
	}
	// ��ʼ����Ƶ����
	if (init_audio_parameters(playerCtx) < 0) {
		return -1;
	}
	// ��ʼ����Ƶ����
	if (init_video_paramerters(playerCtx) < 0) {
		return -1;
	}

	// ������Ƶpkt�߳�
	std::thread decodeAudioThread(decode_audio_thread, &playerCtx);
	decodeAudioThread.detach();
	// ������Ƶpkt�߳�
	std::thread decodeVideoThread(decode_video_thread, &playerCtx);
	decodeVideoThread.detach();

	// ��ȡAVFrame������pkt�����ͷ�����Ƶ���л���Ƶ������
	AVPacket* packet = nullptr;
	while (!playerCtx.quit) {
		// �жϻ����Ƿ�����,������ȴ����ĺ��ټ������
		if (playerCtx.audio_queue.size() > 50 ||
			playerCtx.video_queue.size() > 100) {
			SDL_Delay(10);
			continue;
		}
		packet = av_packet_alloc();
		av_init_packet(packet);
		if (av_read_frame(playerCtx.pFormateCtx, packet) < 0) { // ��AV�ļ��ж�ȡFrame
			break;
		}
		// ����Ƶ֡���뵽��Ƶ��������У�����Ƶ�Ľ����߳��н���
		if (packet->stream_index == playerCtx.au_stream_index) {
			playerCtx.audio_queue.push(packet);
		}
		//����Ƶ���뵽��Ƶ��������У�����Ƶ�Ľ����߳��н���
		else if (packet->stream_index == playerCtx.video_stream_index) {
			playerCtx.video_queue.push(packet);
		}
		else {
			av_packet_unref(packet);
			av_packet_free(&packet);
		}
	}

	// �ر�SDL��Ƶ�����豸
	SDL_CloseAudio();
	// �ر�SDL
	SDL_Quit();
	// �ͷ��ڴ�
	avcodec_parameters_free(&playerCtx.audioCodecParameter);
	avcodec_free_context(&playerCtx.audioCodecCtx);
	swr_free(&playerCtx.au_convert_ctx);
	av_free(playerCtx.audioCodecCtx);
	av_free(playerCtx.out_buffer);

	avcodec_free_context(&playerCtx.videoCodecCtx);
	sws_freeContext(playerCtx.vi_convert_ctx);
	av_frame_free(&playerCtx.pFrameYUV);

	return 0;
}

PlayerContext::PlayerContext() {
	pFormateCtx = nullptr; // AV�ļ���������
	quit = false;		   // �����ı�־

	// --------------------------- ��Ƶ��ز��� ---------------------------- //
	audioCodecParameter = nullptr; // ��Ƶ�������Ĳ���
	audioCodecCtx = nullptr; // ��Ƶ��������������
	audioCodec = nullptr; // ��Ƶ������
	audio_stream = nullptr; // ��Ƶ��
	au_stream_index = -1;	   // ��¼��Ƶ����λ��
	audio_clk = 0.0;	   // ��ǰ��Ƶʱ��
	audio_pts = 0;	   // ��¼��ǰ�Ѿ�������Ƶ��ʱ��
	audio_pts_duration = 0;	   // ��¼��ǰ�Ѿ�������Ƶ��ʱ��
	audio_pos = nullptr; // ��������ÿ��
	audio_len = 0;	   // ��������

	au_convert_ctx = nullptr;			 // ��Ƶת����
	out_sample_fmt = AV_SAMPLE_FMT_S16; // �ز�����ʽ
	out_buffer_size = 0;				 // �ز������buffer��С
	out_buffer = nullptr;			 // �ز������buffer

	wanted_spec = {};				 // sdl ϵͳ������Ƶ�ĸ��������Ϣ
	// ------------------------------ end --------------------------------- //

	// ------------------------ ��Ƶ��ز��� --------------------------- //
	videoCodecParameter = nullptr; // ��Ƶ�������Ĳ���
	videoCodecCtx = nullptr; // ��Ƶ��������������
	pVideoCodec = nullptr; // ��Ƶ������
	video_stream = nullptr; // ��Ƶ��

	vi_convert_ctx = nullptr; // ��Ƶת����
	pFrameYUV = nullptr; // ���ת�������Ƶ
	video_stream_index = -1;	   // ��¼��Ƶ����λ��
	video_pts = 0;	   // ��¼��ǰ�Ѿ������˵���Ƶʱ��
	video_clk = 0.0;	   // ��ǰ��Ƶ֡��ʱ���
	// --------------------------- end ------------------------------ //

	// ---------------------------- sdl ----------------------------- //
	screen = nullptr; // ��Ƶ����
	renderer = nullptr; // ��Ⱦ��
	texture = nullptr; // ����
	sdlRect.x = 0;
	sdlRect.y = 0;
	sdlRect.w = 1280;
	sdlRect.h = 720;
	// --------------------------- end ------------------------------ //
}