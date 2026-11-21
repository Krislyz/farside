// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Farside: what the audio receiver hands to the decoder when sound packets are lost or overtake each
// other. Every packet carries its own frame and copies of the two frames before it. An empty frame
// handed on means "invent this one" (Opus packet loss concealment), which is heard as warbling.

#include <munit.h>

#include <chiaki/audioreceiver.h>
#include <chiaki/session.h>

#include <stdlib.h>
#include <string.h>

#include "test_log.h"

#define UNIT_SIZE 4
#define COPIES 2

typedef struct
{
	int frames[64]; // the frame number each delivered frame carried, or -1 for an empty (invented) one
	size_t count;
} Delivered;

static void frame_cb(uint8_t *buf, size_t buf_size, void *user)
{
	Delivered *d = user;
	munit_assert_size(d->count, <, 64);
	d->frames[d->count++] = buf_size ? (int)(buf[0] | (buf[1] << 8)) : -1;
}

// One audio packet as the console sends it: the frame itself, then copies of the COPIES frames before it.
static void send_packet(ChiakiAudioReceiver *receiver, uint16_t frame)
{
	uint8_t data[UNIT_SIZE * (1 + COPIES)] = { 0 };
	data[0] = frame & 0xff;
	data[1] = frame >> 8;
	for(int i = 0; i < COPIES; i++)
	{
		uint16_t copy_of = frame - COPIES + i;
		data[UNIT_SIZE * (1 + i)] = copy_of & 0xff;
		data[UNIT_SIZE * (1 + i) + 1] = copy_of >> 8;
	}
	ChiakiTakionAVPacket packet = { 0 };
	packet.codec = 5;
	packet.frame_index = frame;
	packet.units_in_frame_total = 1 + COPIES;
	packet.units_in_frame_fec = (UNIT_SIZE << 8) | (COPIES << 4) | 1;
	packet.data = data;
	packet.data_size = sizeof(data);
	chiaki_audio_receiver_av_packet(receiver, &packet);
}

typedef struct
{
	ChiakiSession *session;
	ChiakiAudioReceiver receiver;
	Delivered delivered;
} Fixture;

static void fixture_init(Fixture *f)
{
	memset(f, 0, sizeof(*f));
	f->session = calloc(1, sizeof(ChiakiSession));
	munit_assert_not_null(f->session);
	f->session->log = get_test_log();
	f->session->audio_sink.frame_cb = frame_cb;
	f->session->audio_sink.user = &f->delivered;
	munit_assert_int(chiaki_audio_receiver_init(&f->receiver, f->session, NULL), ==, CHIAKI_ERR_SUCCESS);
}

static void fixture_fini(Fixture *f)
{
	chiaki_audio_receiver_fini(&f->receiver);
	free(f->session);
}

// Every frame from first to last was delivered once, in order, with its real data.
static void assert_all_real(const Delivered *d, int first, int last)
{
	munit_assert_size(d->count, ==, (size_t)(last - first + 1));
	for(size_t i = 0; i < d->count; i++)
		munit_assert_int(d->frames[i], ==, first + (int)i);
}

static MunitResult test_in_order(const MunitParameter params[], void *user)
{
	Fixture f;
	fixture_init(&f);
	for(uint16_t frame = 10; frame <= 30; frame++)
		send_packet(&f.receiver, frame);
	// The first packet's copies (frames 8 and 9) are played too.
	assert_all_real(&f.delivered, 8, 30);
	fixture_fini(&f);
	return MUNIT_OK;
}

// One packet never arrives. The next packet carries a copy of its frame: nothing may be invented.
static MunitResult test_one_packet_lost(const MunitParameter params[], void *user)
{
	Fixture f;
	fixture_init(&f);
	for(uint16_t frame = 10; frame <= 30; frame++)
		if(frame != 20)
			send_packet(&f.receiver, frame);
	assert_all_real(&f.delivered, 8, 30);
	fixture_fini(&f);
	return MUNIT_OK;
}

// As many packets in a row are lost as there are copies: the copies still cover all of them.
static MunitResult test_two_packets_lost(const MunitParameter params[], void *user)
{
	Fixture f;
	fixture_init(&f);
	for(uint16_t frame = 10; frame <= 30; frame++)
		if(frame != 20 && frame != 21)
			send_packet(&f.receiver, frame);
	assert_all_real(&f.delivered, 8, 30);
	fixture_fini(&f);
	return MUNIT_OK;
}

// Two packets arrive the wrong way round, as happens on some internet routes: nothing may be invented.
static MunitResult test_two_packets_swapped(const MunitParameter params[], void *user)
{
	Fixture f;
	fixture_init(&f);
	for(uint16_t frame = 10; frame <= 30; frame++)
	{
		if(frame == 20)
		{
			send_packet(&f.receiver, 21);
			send_packet(&f.receiver, 20);
			frame = 21;
			continue;
		}
		send_packet(&f.receiver, frame);
	}
	assert_all_real(&f.delivered, 8, 30);
	fixture_fini(&f);
	return MUNIT_OK;
}

// More packets in a row are lost than there are copies: the one frame nobody has is invented, the
// rest is real, and the order holds.
static MunitResult test_more_lost_than_copies(const MunitParameter params[], void *user)
{
	Fixture f;
	fixture_init(&f);
	for(uint16_t frame = 10; frame <= 30; frame++)
		if(frame < 20 || frame > 22)
			send_packet(&f.receiver, frame);
	munit_assert_size(f.delivered.count, ==, 23);
	for(size_t i = 0; i < f.delivered.count; i++)
		munit_assert_int(f.delivered.frames[i], ==, (int)i + 8 == 20 ? -1 : (int)i + 8);
	fixture_fini(&f);
	return MUNIT_OK;
}

MunitTest tests_audio_receiver[] = {
	{ "/in_order", test_in_order, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ "/one_packet_lost", test_one_packet_lost, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ "/two_packets_lost", test_two_packets_lost, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ "/two_packets_swapped", test_two_packets_swapped, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ "/more_lost_than_copies", test_more_lost_than_copies, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};
