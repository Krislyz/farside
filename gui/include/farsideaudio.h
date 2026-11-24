// Farside: how much sound may wait to be played.
//
// Sound comes from the console as it is made and is played at the same speed, so what waits in front of
// the player stays as long as it is. After a stall on the network the late sound comes in one bunch; kept,
// it would play behind the picture for the rest of the stream. So the oldest of it is left out.
#ifndef FARSIDE_AUDIO_H
#define FARSIDE_AUDIO_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Farside {

// What waits in all when the line is as usual, in bytes: what AudioPace keeps (need_ms, measured before
// a frame is added), the frame, and 10 ms. frame_size is one sample of all channels (48000 a second).
inline size_t audioUsual(unsigned need_ms, size_t frame_size)
{
	return (size_t)(need_ms + 20) * 48 * frame_size;
}

// Sound waits in two places: queued bytes are with the player and cannot be taken back, ring_fill bytes
// are behind them. usual is audioUsual(). Returns how many of the newest ring bytes to keep: ring_fill
// when nothing is too old.
inline size_t audioRingKeep(size_t queued, size_t ring_fill, size_t usual, size_t frame_size)
{
	const size_t ms = 48 * frame_size;
	// Up to 60 ms more is an ordinary bunch and goes away by itself.
	if(queued + ring_fill <= usual + 60 * ms)
		return ring_fill;
	const size_t wanted = usual > queued ? usual - queued : 0;
	if(wanted >= ring_fill)
		return ring_fill;
	// Whole samples only, or left and right would swap.
	const size_t drop = ring_fill - wanted;
	return ring_fill - (drop - drop % frame_size);
}

// Sound is played a hair faster or slower, so that what waits stays near what the line needs.
//
// Nothing else brings the waiting sound down: what piled up at the start stays, and the console's sound
// clock runs a little ahead of this PC's player, so more piles up by the hour. A frame is made a few
// samples shorter (at most 4 of 480, under 1 % faster) while too much waits, one sample longer while
// too little does.
class AudioPace
{
public:
	// A frame came: waiting_ms waited to be played before it, since_ms passed after the frame before
	// (0 for the first). Returns how many samples to leave out of this frame; negative: to add.
	int frame(unsigned waiting_ms, unsigned since_ms)
	{
		// The first second of a stream is its start, not the line.
		if(frames_seen < WindowFrames)
		{
			frames_seen++;
			since_ms = 0;
		}
		if(since_ms > window_gap_ms)
			window_gap_ms = since_ms;
		if(waiting_ms < window_floor_ms)
			window_floor_ms = waiting_ms;
		if(++window_frames < WindowFrames)
			return leave;

		// Once a second. The least that waited in this second is what counts: that is how near the
		// player came to running out. One sample of a frame is 2 ms in a second.
		gaps[gap_at] = window_gap_ms;
		gap_at = (gap_at + 1) % Windows;
		window_gap_ms = 0;
		const unsigned need = needMs();
		last_floor_ms = window_floor_ms;
		if(window_floor_ms > need + 5)
		{
			const unsigned steps = 1 + (window_floor_ms - need - 5) / 10;
			leave = steps < 4 ? (int)steps : 4;
		}
		else
			leave = window_floor_ms < need ? -1 : 0;
		window_frames = 0;
		window_floor_ms = ~0u;
		return leave;
	}

	// The least that should wait when a frame comes: the longest wait for a frame in the last minute
	// (a late one counts at once), and 5 ms. Never under 25 ms. A wait over 95 ms is a stall, and no
	// sound is kept waiting for those: 100 ms at most.
	unsigned needMs() const
	{
		unsigned worst = window_gap_ms > MinGapMs ? window_gap_ms : MinGapMs;
		for(unsigned gap : gaps)
			if(gap > worst)
				worst = gap;
		return (worst < MaxGapMs ? worst : MaxGapMs) + 5;
	}
	// The least that waited in the last full second.
	unsigned floorMs() const { return last_floor_ms; }

private:
	// ponytail: steps set from two evenings on a home network, where a frame now and then comes 33 ms
	// after the one before. Tune after use on a line through the internet.
	static constexpr unsigned WindowFrames = 100, Windows = 60, MinGapMs = 20, MaxGapMs = 95;
	unsigned gaps[Windows] = {}, gap_at = 0;
	unsigned window_gap_ms = 0, window_floor_ms = ~0u, window_frames = 0, frames_seen = 0, last_floor_ms = 0;
	int leave = 0;
};

// A frame of n_in samples (of all channels, 16 bit, one after the other) played in the time of n_out.
// The first and the last sample stay what they are, so frames still join.
inline void audioResample(const int16_t *in, size_t n_in, int16_t *out, size_t n_out, size_t channels)
{
	if(n_in < 2 || n_out < 2)
		return;
	for(size_t i = 0; i < n_out; i++)
	{
		// Place of this sample in the frame that came, in 65536ths of a sample; between two samples
		// the value lies on the line through them.
		const uint64_t at = (uint64_t)i * (n_in - 1) * 65536 / (n_out - 1);
		const size_t a = (size_t)(at >> 16), b = a + 1 < n_in ? a + 1 : a;
		const int64_t part = (int64_t)(at & 0xffff);
		for(size_t c = 0; c < channels; c++)
			out[i * channels + c] = (int16_t)(in[a * channels + c] + ((in[b * channels + c] - in[a * channels + c]) * part) / 65536);
	}
}

// 0 = pass, otherwise the number of the first failing check (70 to 83).
inline int selfTestAudio()
{
	const size_t ms = 48 * 4; // stereo, 16 bit
	// 70: the usual amount (here 80 ms) stays whole.
	if(audioRingKeep(50 * ms, 30 * ms, 80 * ms, 4) != 30 * ms)
		return 70;
	// 71: after a stall, 400 ms wait. The player holds more than the usual amount by itself: the ring goes.
	if(audioRingKeep(100 * ms, 300 * ms, 80 * ms, 4) != 0)
		return 71;
	// 72: the same with little at the player: the newest sound stays, up to the usual 80 ms in all.
	if(audioRingKeep(10 * ms, 400 * ms, 80 * ms, 4) != 70 * ms)
		return 72;
	// 73: only whole samples are left out, or left and right would swap.
	if((300 * ms + 2 - audioRingKeep(100 * ms, 300 * ms + 2, 80 * ms, 4)) % 4 != 0)
		return 73;
	// 74: bunches up to 60 ms over the usual amount are not touched, also when little is usual.
	if(audioRingKeep(10 * ms, 90 * ms, 40 * ms, 4) != 90 * ms || audioRingKeep(100 * ms, 40 * ms, 80 * ms, 4) != 40 * ms)
		return 74;
	// 75: 90 ms wait on an even line (a frame every 10 ms): after a second, frames are made shorter.
	{
		AudioPace pace;
		int leave = 0;
		for(int i = 0; i < 101; i++)
			leave = pace.frame(90, 10);
		if(leave < 1 || leave > 4)
			return 75;
	}
	// 76: what waits is what the line needs: frames stay as they are.
	{
		AudioPace pace;
		const unsigned need = pace.needMs();
		if(need < 25 || need > 60)
			return 76;
		for(int i = 0; i < 301; i++)
			if(pace.frame(need + 2, 10) != 0)
				return 76;
	}
	// 77: a frame that came 80 ms late counts at once: 85 ms must wait, and with 60 frames get longer.
	{
		AudioPace pace;
		for(int i = 0; i < 100; i++)
			pace.frame(60, 10);
		pace.frame(60, 80);
		if(pace.needMs() != 85)
			return 77;
		int leave = 0;
		for(int i = 0; i < 100; i++)
			leave = pace.frame(60, 10);
		if(leave != -1)
			return 77;
	}
	// 78: a late frame counts for a minute, then no more: 25 ms is the least that is kept.
	{
		AudioPace pace;
		for(int i = 0; i < 100; i++)
			pace.frame(85, 10);
		pace.frame(85, 80);
		for(int i = 0; i < 3000; i++)
			pace.frame(85, 10);
		if(pace.needMs() != 85)
			return 78;
		for(int i = 0; i < 3100; i++)
			pace.frame(85, 10);
		if(pace.needMs() != 25)
			return 78;
	}
	// 79: a frame made one sample shorter or longer keeps its first and last sample and its slope.
	{
		std::vector<int16_t> in(480 * 2), shorter(479 * 2), longer(481 * 2);
		for(int i = 0; i < 480; i++)
		{
			in[2 * i] = (int16_t)(i * 10);
			in[2 * i + 1] = (int16_t)(-i * 10);
		}
		audioResample(in.data(), 480, shorter.data(), 479, 2);
		audioResample(in.data(), 480, longer.data(), 481, 2);
		if(shorter[0] != 0 || shorter[1] != 0 || shorter[478 * 2] != 4790 || shorter[478 * 2 + 1] != -4790)
			return 79;
		if(longer[0] != 0 || longer[480 * 2] != 4790 || longer[480 * 2 + 1] != -4790)
			return 79;
		for(int i = 1; i < 479; i++)
		{
			const int step = shorter[2 * i] - shorter[2 * (i - 1)];
			if(step < 10 || step > 11)
				return 79;
		}
	}
	// 80: the whole of it: 90 ms wait at the start; after a minute it is what the line needs, at most 6 ms more.
	{
		AudioPace pace;
		double waiting = 90;
		for(int i = 0; i < 6000; i++)
			waiting -= pace.frame((unsigned)waiting, 10) / 48.0;
		if(waiting < pace.needMs() || waiting > pace.needMs() + 6)
			return 80;
	}
	// 81: the first second of a stream is its start, not the line: a late frame there does not count.
	// (Seen 2026-11-24: 136 ms at the start kept 140 ms of sound waiting for the whole test.)
	{
		AudioPace pace;
		pace.frame(0, 0);
		pace.frame(0, 136);
		if(pace.needMs() != 25)
			return 81;
	}
	// 82: a stall is not what the line needs: after half a second without sound, 100 ms at most.
	{
		AudioPace pace;
		for(int i = 0; i < 100; i++)
			pace.frame(60, 10);
		pace.frame(60, 500);
		if(pace.needMs() != 100)
			return 82;
	}
	// 83: what the pace keeps waiting is never left out as "too old", whatever the line needs.
	// (Seen 2026-11-24: the pace kept 135 ms, the other rule cut at 140: 80 ms of sound gone every 30 s.)
	for(unsigned gap = 10; gap <= 600; gap += 10)
	{
		AudioPace pace;
		for(int i = 0; i < 100; i++)
			pace.frame(60, 10);
		pace.frame(60, gap);
		const unsigned need = pace.needMs();
		// need waits when a frame comes; the pace lets 5 ms more be, the player's own go is 10 ms, the
		// frame 10 ms, and frames may come three at once: 50 ms over need is nothing unusual.
		if(audioRingKeep((size_t)need * ms, 50 * ms, audioUsual(need, 4), 4) != 50 * ms)
			return 83;
	}
	return 0;
}

} // namespace Farside

#endif
