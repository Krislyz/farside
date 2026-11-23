// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Farside: how late each picture is by the console's own clock (lib/include/chiaki/frametiming.h).
// The record layout is the one a PS5 sent in a real stream on 2026-11-23.

#include <munit.h>

#include <chiaki/frametiming.h>

#include <string.h>

// The console's clock is this far behind this PC's clock in the tests; no test may depend on the number.
#define CLOCK_GAP 5000000000ull
// A picture every 16683 us, as a PS5 does at 60 pictures a second.
#define STEP 16683ull

static uint64_t console_clock(uint16_t frame)
{
	return 463171653637ull + (uint64_t)frame * STEP;
}

// The picture is whole here took_us after the console stamped it; receiving it took 300 us.
static void frame_arrives(ChiakiFrameTiming *timing, uint16_t frame, uint64_t took_us)
{
	uint64_t whole = console_clock(frame) + CLOCK_GAP + took_us;
	chiaki_frame_timing_frame(timing, frame, whole - 300, whole);
}

// One list from the console with the pictures first..first+count-1.
static void list_arrives(ChiakiFrameTiming *timing, uint16_t first, size_t count)
{
	uint8_t info[60 * CHIAKI_FRAME_TIMING_RECORD_SIZE];
	munit_assert_size(count, <=, 60);
	for(size_t i = 0; i < count; i++)
	{
		uint8_t *r = info + i * CHIAKI_FRAME_TIMING_RECORD_SIZE;
		uint64_t clock = console_clock((uint16_t)(first + i));
		for(int b = 0; b < 8; b++)
			r[b] = (uint8_t)(clock >> (8 * b));
		r[8] = (uint8_t)((first + i) & 0xff);
		r[9] = (uint8_t)(((first + i) >> 8) & 0xff);
		r[10] = 0;
	}
	chiaki_frame_timing_stamps(timing, info, count * CHIAKI_FRAME_TIMING_RECORD_SIZE);
}

static void assert_ms(float got, double want)
{
	munit_assert_double((double)got, >, want - 0.01);
	munit_assert_double((double)got, <, want + 0.01);
}

// Every picture takes the same time: none is late, whatever the gap between the two clocks is.
static MunitResult test_all_in_time(const MunitParameter params[], void *user)
{
	ChiakiFrameTiming timing;
	munit_assert_int(chiaki_frame_timing_init(&timing), ==, CHIAKI_ERR_SUCCESS);
	for(uint16_t frame = 10; frame < 13; frame++)
		frame_arrives(&timing, frame, 2000);
	list_arrives(&timing, 10, 3);
	ChiakiFrameTimingSample samples[8];
	munit_assert_size(chiaki_frame_timing_read(&timing, samples, 8), ==, 3);
	for(int i = 0; i < 3; i++)
	{
		assert_ms(samples[i].late_ms, 0.0);
		assert_ms(samples[i].receive_ms, 0.3);
		munit_assert_uint64(samples[i].arrival_us, ==, console_clock((uint16_t)(10 + i)) + CLOCK_GAP + 2000);
	}
	// Taken out once: nothing is left for a second call.
	munit_assert_size(chiaki_frame_timing_read(&timing, samples, 8), ==, 0);
	chiaki_frame_timing_fini(&timing);
	return MUNIT_OK;
}

// One picture is held back by 7 ms: it alone is 7 ms late.
static MunitResult test_one_late(const MunitParameter params[], void *user)
{
	ChiakiFrameTiming timing;
	chiaki_frame_timing_init(&timing);
	frame_arrives(&timing, 10, 2000);
	frame_arrives(&timing, 11, 9000);
	frame_arrives(&timing, 12, 2000);
	list_arrives(&timing, 10, 3);
	ChiakiFrameTimingSample samples[8];
	munit_assert_size(chiaki_frame_timing_read(&timing, samples, 8), ==, 3);
	assert_ms(samples[0].late_ms, 0.0);
	assert_ms(samples[1].late_ms, 7.0);
	assert_ms(samples[2].late_ms, 0.0);
	chiaki_frame_timing_fini(&timing);
	return MUNIT_OK;
}

// A whole second of pictures is 3 ms slower than the second before: all of it shows as 3 ms late.
static MunitResult test_late_against_earlier_list(const MunitParameter params[], void *user)
{
	ChiakiFrameTiming timing;
	chiaki_frame_timing_init(&timing);
	for(uint16_t frame = 0; frame < 60; frame++)
		frame_arrives(&timing, frame, 2000);
	list_arrives(&timing, 0, 60);
	for(uint16_t frame = 60; frame < 120; frame++)
		frame_arrives(&timing, frame, 5000);
	list_arrives(&timing, 60, 60);
	ChiakiFrameTimingSample samples[200];
	munit_assert_size(chiaki_frame_timing_read(&timing, samples, 200), ==, 120);
	assert_ms(samples[0].late_ms, 0.0);
	assert_ms(samples[59].late_ms, 0.0);
	assert_ms(samples[60].late_ms, 3.0);
	assert_ms(samples[119].late_ms, 3.0);
	chiaki_frame_timing_fini(&timing);
	return MUNIT_OK;
}

// The two clocks drift apart (here 0.1 ms a second). Only the last 32 lists count for "the best
// picture", so the drift shows as at most 3.1 ms and does not grow for ever.
static MunitResult test_drift_is_forgotten(const MunitParameter params[], void *user)
{
	ChiakiFrameTiming timing;
	chiaki_frame_timing_init(&timing);
	ChiakiFrameTimingSample samples[64];
	float last = 0;
	for(unsigned int second = 0; second < 50; second++)
	{
		uint16_t first = (uint16_t)(second * 60);
		for(uint16_t i = 0; i < 60; i++)
			frame_arrives(&timing, (uint16_t)(first + i), 2000 + 100 * second);
		list_arrives(&timing, first, 60);
		munit_assert_size(chiaki_frame_timing_read(&timing, samples, 64), ==, 60);
		last = samples[0].late_ms;
		if(second == 10)
			assert_ms(last, 1.0);
	}
	assert_ms(last, 3.1);
	chiaki_frame_timing_fini(&timing);
	return MUNIT_OK;
}

// A record for a picture that never got whole here (lost) gives no sample; the others still do.
static MunitResult test_unknown_picture_left_out(const MunitParameter params[], void *user)
{
	ChiakiFrameTiming timing;
	chiaki_frame_timing_init(&timing);
	frame_arrives(&timing, 10, 2000);
	frame_arrives(&timing, 12, 2500);
	list_arrives(&timing, 10, 3);
	ChiakiFrameTimingSample samples[8];
	munit_assert_size(chiaki_frame_timing_read(&timing, samples, 8), ==, 2);
	assert_ms(samples[0].late_ms, 0.0);
	assert_ms(samples[1].late_ms, 0.5);
	// The same list again (or a picture number that came round again) times nothing twice.
	list_arrives(&timing, 10, 3);
	munit_assert_size(chiaki_frame_timing_read(&timing, samples, 8), ==, 0);
	chiaki_frame_timing_fini(&timing);
	return MUNIT_OK;
}

// A list of another shape (not whole records) is left out and harms nothing.
static MunitResult test_bad_list_left_out(const MunitParameter params[], void *user)
{
	ChiakiFrameTiming timing;
	chiaki_frame_timing_init(&timing);
	frame_arrives(&timing, 10, 2000);
	uint8_t bad[CHIAKI_FRAME_TIMING_RECORD_SIZE + 3] = { 0 };
	chiaki_frame_timing_stamps(&timing, bad, sizeof(bad));
	chiaki_frame_timing_stamps(&timing, bad, 0);
	chiaki_frame_timing_stamps(&timing, NULL, 0);
	ChiakiFrameTimingSample samples[8];
	munit_assert_size(chiaki_frame_timing_read(&timing, samples, 8), ==, 0);
	list_arrives(&timing, 10, 1);
	munit_assert_size(chiaki_frame_timing_read(&timing, samples, 8), ==, 1);
	assert_ms(samples[0].late_ms, 0.0);
	chiaki_frame_timing_fini(&timing);
	return MUNIT_OK;
}

// Picture numbers are 16 bit and start again at 0.
static MunitResult test_picture_number_wraps(const MunitParameter params[], void *user)
{
	ChiakiFrameTiming timing;
	chiaki_frame_timing_init(&timing);
	frame_arrives(&timing, 65535, 2000);
	frame_arrives(&timing, 0, 6000);
	list_arrives(&timing, 65535, 2);
	ChiakiFrameTimingSample samples[8];
	munit_assert_size(chiaki_frame_timing_read(&timing, samples, 8), ==, 2);
	assert_ms(samples[0].late_ms, 0.0);
	munit_assert_double((double)samples[1].late_ms, >, 0.0);
	chiaki_frame_timing_fini(&timing);
	return MUNIT_OK;
}

// Nobody reads (the graph is off): the oldest samples go, the newest stay, nothing overflows.
static MunitResult test_unread_samples_are_dropped(const MunitParameter params[], void *user)
{
	ChiakiFrameTiming timing;
	chiaki_frame_timing_init(&timing);
	for(unsigned int second = 0; second < 20; second++)
	{
		uint16_t first = (uint16_t)(second * 60);
		for(uint16_t i = 0; i < 60; i++)
			frame_arrives(&timing, (uint16_t)(first + i), 2000);
		list_arrives(&timing, first, 60);
	}
	static ChiakiFrameTimingSample samples[2 * CHIAKI_FRAME_TIMING_SAMPLES];
	size_t count = chiaki_frame_timing_read(&timing, samples, 2 * CHIAKI_FRAME_TIMING_SAMPLES);
	munit_assert_size(count, ==, CHIAKI_FRAME_TIMING_SAMPLES);
	munit_assert_uint64(samples[count - 1].arrival_us, ==, console_clock(1199) + CLOCK_GAP + 2000);
	munit_assert_uint64(samples[0].arrival_us, <, samples[1].arrival_us);
	chiaki_frame_timing_fini(&timing);
	return MUNIT_OK;
}

MunitTest tests_frame_timing[] = {
	{ "/all_in_time", test_all_in_time, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ "/one_late", test_one_late, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ "/late_against_earlier_list", test_late_against_earlier_list, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ "/drift_is_forgotten", test_drift_is_forgotten, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ "/unknown_picture_left_out", test_unknown_picture_left_out, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ "/bad_list_left_out", test_bad_list_left_out, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ "/picture_number_wraps", test_picture_number_wraps, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ "/unread_samples_are_dropped", test_unread_samples_are_dropped, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};
