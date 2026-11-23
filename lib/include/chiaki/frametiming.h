// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Farside: how late each picture is, by the console's own clock.
//
// Asked to (ServerSettings, send_periodic_timestamp), a PS5 sends about once a second a list with one
// record per picture: its clock in microseconds at that picture and the picture's number. Its clock and
// this PC's clock are not set to each other, so the time a picture takes cannot be read from it. What
// can be read is how much longer a picture took than the best picture of the last half minute: that is
// what the console or the network held it back by.

#ifndef CHIAKI_FRAMETIMING_H
#define CHIAKI_FRAMETIMING_H

#include "common.h"
#include "thread.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHIAKI_FRAME_TIMING_FRAMES 256
// The best picture is looked for in this many lists (about half a minute). Not longer: the two clocks
// drift apart, some milliseconds a minute.
#define CHIAKI_FRAME_TIMING_LISTS 32
#define CHIAKI_FRAME_TIMING_SAMPLES 512
// One record: uint64 console clock in microseconds, uint16 picture number, one byte (0), little endian.
#define CHIAKI_FRAME_TIMING_RECORD_SIZE 11

typedef struct chiaki_frame_timing_sample_t
{
	uint64_t arrival_us; // when the picture was whole here, on this PC's clock
	float late_ms;       // how much longer it took than the best picture of the last lists
	float receive_ms;    // from its first part arriving to whole
} ChiakiFrameTimingSample;

typedef struct chiaki_frame_timing_t
{
	ChiakiMutex mutex;
	struct
	{
		bool set;
		uint16_t index;
		uint64_t first_us;
		uint64_t whole_us;
	} frames[CHIAKI_FRAME_TIMING_FRAMES];
	int64_t list_best[CHIAKI_FRAME_TIMING_LISTS]; // smallest (arrival - console clock) of each list
	size_t lists; // how many lists so far
	ChiakiFrameTimingSample samples[CHIAKI_FRAME_TIMING_SAMPLES];
	size_t samples_begin;
	size_t samples_count;
} ChiakiFrameTiming;

CHIAKI_EXPORT ChiakiErrorCode chiaki_frame_timing_init(ChiakiFrameTiming *timing);
CHIAKI_EXPORT void chiaki_frame_timing_fini(ChiakiFrameTiming *timing);

/** A picture is whole. first_us: when its first part came; whole_us: now. Both on this PC's clock. */
CHIAKI_EXPORT void chiaki_frame_timing_frame(ChiakiFrameTiming *timing, uint16_t frame_index, uint64_t first_us, uint64_t whole_us);

/** The console's list (periodic_ts_info of PeriodicTimestampPayload). A list of another shape is left out. */
CHIAKI_EXPORT void chiaki_frame_timing_stamps(ChiakiFrameTiming *timing, const uint8_t *info, size_t info_size);

/** Takes out the pictures timed since the last call, oldest first. @return how many were written */
CHIAKI_EXPORT size_t chiaki_frame_timing_read(ChiakiFrameTiming *timing, ChiakiFrameTimingSample *samples, size_t max);

#ifdef __cplusplus
}
#endif

#endif // CHIAKI_FRAMETIMING_H
