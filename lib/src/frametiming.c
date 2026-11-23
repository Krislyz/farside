// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// See frametiming.h.

#include <chiaki/frametiming.h>

#include <string.h>

CHIAKI_EXPORT ChiakiErrorCode chiaki_frame_timing_init(ChiakiFrameTiming *timing)
{
	memset(timing, 0, sizeof(*timing));
	return chiaki_mutex_init(&timing->mutex, false);
}

CHIAKI_EXPORT void chiaki_frame_timing_fini(ChiakiFrameTiming *timing)
{
	chiaki_mutex_fini(&timing->mutex);
}

CHIAKI_EXPORT void chiaki_frame_timing_frame(ChiakiFrameTiming *timing, uint16_t frame_index, uint64_t first_us, uint64_t whole_us)
{
	chiaki_mutex_lock(&timing->mutex);
	size_t slot = frame_index % CHIAKI_FRAME_TIMING_FRAMES;
	timing->frames[slot].set = true;
	timing->frames[slot].index = frame_index;
	timing->frames[slot].first_us = first_us;
	timing->frames[slot].whole_us = whole_us;
	chiaki_mutex_unlock(&timing->mutex);
}

// The record's picture, if it got whole here and was not timed yet; its console clock in *clock.
static int frame_of_record(ChiakiFrameTiming *timing, const uint8_t *record, uint64_t *clock)
{
	uint64_t c = 0;
	for(int b = 7; b >= 0; b--)
		c = (c << 8) | record[b];
	uint16_t index = (uint16_t)(record[8] | (record[9] << 8));
	size_t slot = index % CHIAKI_FRAME_TIMING_FRAMES;
	if(!timing->frames[slot].set || timing->frames[slot].index != index)
		return -1;
	*clock = c;
	return (int)slot;
}

CHIAKI_EXPORT void chiaki_frame_timing_stamps(ChiakiFrameTiming *timing, const uint8_t *info, size_t info_size)
{
	if(!info || !info_size || info_size % CHIAKI_FRAME_TIMING_RECORD_SIZE)
		return;
	chiaki_mutex_lock(&timing->mutex);

	// The best picture of this list...
	bool any = false;
	int64_t best = 0;
	for(size_t i = 0; i < info_size; i += CHIAKI_FRAME_TIMING_RECORD_SIZE)
	{
		uint64_t clock;
		int slot = frame_of_record(timing, info + i, &clock);
		if(slot < 0)
			continue;
		int64_t took = (int64_t)timing->frames[slot].whole_us - (int64_t)clock;
		if(!any || took < best)
			best = took;
		any = true;
	}
	if(!any)
	{
		chiaki_mutex_unlock(&timing->mutex);
		return;
	}
	timing->list_best[timing->lists++ % CHIAKI_FRAME_TIMING_LISTS] = best;
	// ...and of the lists before it that still count.
	size_t counted = timing->lists < CHIAKI_FRAME_TIMING_LISTS ? timing->lists : CHIAKI_FRAME_TIMING_LISTS;
	for(size_t i = 0; i < counted; i++)
		if(timing->list_best[i] < best)
			best = timing->list_best[i];

	for(size_t i = 0; i < info_size; i += CHIAKI_FRAME_TIMING_RECORD_SIZE)
	{
		uint64_t clock;
		int slot = frame_of_record(timing, info + i, &clock);
		if(slot < 0)
			continue;
		// Nobody read for a while: the oldest sample makes room.
		if(timing->samples_count == CHIAKI_FRAME_TIMING_SAMPLES)
		{
			timing->samples_begin = (timing->samples_begin + 1) % CHIAKI_FRAME_TIMING_SAMPLES;
			timing->samples_count--;
		}
		ChiakiFrameTimingSample *sample = &timing->samples[(timing->samples_begin + timing->samples_count++) % CHIAKI_FRAME_TIMING_SAMPLES];
		sample->arrival_us = timing->frames[slot].whole_us;
		sample->late_ms = (float)((int64_t)timing->frames[slot].whole_us - (int64_t)clock - best) / 1000.0f;
		sample->receive_ms = (float)(timing->frames[slot].whole_us - timing->frames[slot].first_us) / 1000.0f;
		timing->frames[slot].set = false; // timed once
	}
	chiaki_mutex_unlock(&timing->mutex);
}

CHIAKI_EXPORT size_t chiaki_frame_timing_read(ChiakiFrameTiming *timing, ChiakiFrameTimingSample *samples, size_t max)
{
	chiaki_mutex_lock(&timing->mutex);
	size_t count = timing->samples_count < max ? timing->samples_count : max;
	for(size_t i = 0; i < count; i++)
		samples[i] = timing->samples[(timing->samples_begin + i) % CHIAKI_FRAME_TIMING_SAMPLES];
	timing->samples_begin = (timing->samples_begin + count) % CHIAKI_FRAME_TIMING_SAMPLES;
	timing->samples_count -= count;
	chiaki_mutex_unlock(&timing->mutex);
	return count;
}
