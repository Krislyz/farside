// Farside: how much sound may wait to be played.
//
// Sound comes from the console as it is made and is played at the same speed, so what waits in front of
// the player stays as long as it is. After a stall on the network the late sound comes in one bunch; kept,
// it would play behind the picture for the rest of the stream. So the oldest of it is left out.
#ifndef FARSIDE_AUDIO_H
#define FARSIDE_AUDIO_H

#include <cstddef>

namespace Farside {

// Sound waits in two places: queued bytes are with the player and cannot be taken back, ring_fill bytes
// are behind them. buffer_size is what the player takes in one go, frame_size one sample of all channels
// (48000 a second). Returns how many of the newest ring bytes to keep: ring_fill when nothing is too old.
inline size_t audioRingKeep(size_t queued, size_t ring_fill, size_t buffer_size, size_t frame_size)
{
	const size_t ms = 48 * frame_size;
	// The usual amount: one go for the player and 30 ms, since frames come up to 30 ms apart on a good line.
	const size_t usual = buffer_size + 30 * ms;
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

// 0 = pass, otherwise the number of the first failing check (70 to 74).
inline int selfTestAudio()
{
	const size_t ms = 48 * 4; // stereo, 16 bit
	// 70: the usual amount (one go of 50 ms and 30 ms for frames that come in bunches) stays whole.
	if(audioRingKeep(50 * ms, 30 * ms, 50 * ms, 4) != 30 * ms)
		return 70;
	// 71: after a stall, 400 ms wait. The player holds more than the usual amount by itself: the ring goes.
	if(audioRingKeep(100 * ms, 300 * ms, 50 * ms, 4) != 0)
		return 71;
	// 72: the same with little at the player: the newest sound stays, up to the usual 80 ms in all.
	if(audioRingKeep(10 * ms, 400 * ms, 50 * ms, 4) != 70 * ms)
		return 72;
	// 73: only whole samples are left out, or left and right would swap.
	if((300 * ms + 2 - audioRingKeep(100 * ms, 300 * ms + 2, 50 * ms, 4)) % 4 != 0)
		return 73;
	// 74: bunches up to 60 ms over the usual amount are not touched, also with a small buffer.
	if(audioRingKeep(10 * ms, 90 * ms, 10 * ms, 4) != 90 * ms || audioRingKeep(100 * ms, 40 * ms, 50 * ms, 4) != 40 * ms)
		return 74;
	return 0;
}

} // namespace Farside

#endif
