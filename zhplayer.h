#pragma once
#include <iostream>
#include <stdio.h>
#include <thread>
#include <atomic>
#include <tchar.h>
#include <string.h>
#include <assert.h>
#include "Queue.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>

#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
}

using namespace std;
#pragma comment(lib ,"SDL2.lib")
#pragma comment(lib ,"SDL2main.lib")
//#define DECODE_MAX_SIZE 2
#define MAX_AUDIO_FRAME_SIZE 192000 //�����ʣ�1 second of 48khz 32bit audio 

class PlayerContext {
public:
	PlayerContext();
	AVFormatContext* pFormateCtx; // AV�ļ���������
	std::atomic_bool quit;		  // �˳���־

	// --------------------------- ��Ƶ��ز��� ---------------------------- //
	AVCodecParameters* audioCodecParameter; // ��Ƶ�������Ĳ���
	AVCodecContext* audioCodecCtx;	    // ��Ƶ��������������
	AVCodec* audioCodec;			// ��Ƶ������
	AVStream* audio_stream;		// ��Ƶ��
	int				   au_stream_index;	    // ��¼��Ƶ����λ��
	double			   audio_clk;			// ��ǰ��Ƶʱ��
	int64_t			   audio_pts;			// ��¼��ǰ�Ѿ�������Ƶ��ʱ��
	double			   audio_pts_duration;	// ��¼��ǰ�Ѿ�������Ƶ��ʱ��
	Uint8* audio_pos;			// ��������ÿ��
	Uint32			   audio_len;			// ��������

	Queue<AVPacket*>    audio_queue;		// ��Ƶ������
	SwrContext* au_convert_ctx;		// ��Ƶת����
	AVSampleFormat	   out_sample_fmt;		// �ز�����ʽ
	int				   out_buffer_size;		// �ز������buffer��С
	uint8_t* out_buffer;			// �ز������buffer

	SDL_AudioSpec	   wanted_spec;			// sdlϵͳ������Ƶ�ĸ��������Ϣ
	// ------------------------------ end --------------------------------- //

	// ------------------------ ��Ƶ��ز��� --------------------------- //
	AVCodecParameters* videoCodecParameter; // ��Ƶ�������Ĳ���
	AVCodecContext* videoCodecCtx;		// ��Ƶ��������������
	AVCodec* pVideoCodec;			// ��Ƶ������
	AVStream* video_stream;		// ��Ƶ��
	Queue<AVPacket*>    video_queue;		// ��Ƶ������

	SwsContext* vi_convert_ctx;		// ��Ƶת����
	AVFrame* pFrameYUV;			// ���ת�������Ƶ
	int				   video_stream_index;	// ��¼��Ƶ����λ��
	int64_t			   video_pts;			// ��¼��ǰ�Ѿ������˵���Ƶʱ��
	double			   video_clk;			// ��ǰ��Ƶ֡��ʱ���
	// ---------------------------- end ------------------------------ //

	// ---------------------------- sdl ----------------------------- //
	SDL_Window* screen;	 // ��Ƶ����
	SDL_Renderer* renderer; // ��Ⱦ��
	SDL_Texture* texture;  // ����
	SDL_Rect		   sdlRect; // sdl��Ⱦ������
	// --------------------------- end ------------------------------ //
};

// ������Ƶ����packet�����߳�
void decode_video_thread(PlayerContext* playerCtx);
// ������Ƶ����packet�����߳�
void decode_audio_thread(PlayerContext* playerCtx);