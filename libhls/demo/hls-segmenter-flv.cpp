#include "mpeg-ps.h"
#include "hls-m3u8.h"
#include "hls-media.h"
#include "hls-param.h"
#include "flv-proto.h"
#include "flv-reader.h"
#include "flv-demuxer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void hls_handler(void* m3u8, const void* data, size_t bytes, int64_t pts, int64_t dts, int64_t duration)
{
	static int64_t s_dts = -1;
	int discontinue = -1 != s_dts ? 0 : (dts > s_dts + HLS_DURATION / 2 ? 1 : 0);
	s_dts = dts;

	static int i = 0;
	char name[128] = {0};
	snprintf(name, sizeof(name), "%d.ts", i++);
	hls_m3u8_add(m3u8, name, pts, duration, discontinue);

	FILE* fp = fopen(name, "wb");
	fwrite(data, 1, bytes, fp);
	fclose(fp);
}

static void flv_handler(void* param, int type, int format, const void* data, size_t bytes, uint32_t pts, uint32_t dts)
{
	format = (FLV_TYPE_AUDIO == type) ? (format << 8) : format;
	switch (format)
	{
	case (FLV_AUDIO_AAC << 8):
		hls_media_input(param, STREAM_AUDIO_AAC, data, bytes, pts, dts, 0);
		break;

	case (FLV_AUDIO_MP3 << 8):
		hls_media_input(param, STREAM_AUDIO_MP3, data, bytes, pts, dts, 0);
		break;

	case FLV_VIDEO_H264:
		hls_media_input(param, STREAM_VIDEO_H264, data, bytes, pts, dts, 0);
		break;

	default:
		// nothing to do
		break;
	}

	printf("\n");
}

void hls_segmenter_flv(const char* file)
{
	void* m3u = hls_m3u8_create(0);
	void* hls = hls_media_create(HLS_DURATION * 1000, hls_handler, m3u);
	void* flv = flv_reader_create(file);
	void* demuxer = flv_demuxer_create(flv_handler, hls);

	int r, type;
	uint32_t timestamp;
	static char data[2 * 1024 * 1024];
	while ((r = flv_reader_read(flv, &type, &timestamp, data, sizeof(data))) > 0)
	{
		flv_demuxer_input(demuxer, type, data, r, timestamp);
	}

	// write m3u8 file
	hls_media_input(hls, STREAM_VIDEO_H264, NULL, 0, 0, 0, 1);
	hls_m3u8_playlist(m3u, 1, data, sizeof(data));
	FILE* fp = fopen("playlist.m3u8", "wb");
	fwrite(data, 1, strlen(data), fp);
	fclose(fp);

	flv_demuxer_destroy(demuxer);
	flv_reader_destroy(flv);
	hls_media_destroy(hls);
	hls_m3u8_destroy(m3u);
}
