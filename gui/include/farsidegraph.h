// Farside: the numbers behind the graph in the stream (the "Stream statistics" panel).
//
// Each line keeps the last 20 seconds in 200 buckets of 100 ms. A value goes into the bucket of its own
// time, not of the moment it is handed in: the console's picture clock comes about a second after the
// pictures, and those values must still line up with the others.
#ifndef FARSIDE_GRAPH_H
#define FARSIDE_GRAPH_H

#include <cstdint>

namespace Farside {

class Graph
{
	public:
		static constexpr int Lines = 7;
		static constexpr int Buckets = 200;
		static constexpr uint64_t BucketUs = 100000;

		void add(int line, uint64_t time_us, float value)
		{
			if(line < 0 || line >= Lines)
				return;
			const uint64_t number = time_us / BucketUs;
			Cell &cell = cells[line][number % Buckets];
			if(cell.number != number)
				cell = Cell { number, value, 0, 0 };
			if(value > cell.high)
				cell.high = value;
			cell.sum += value;
			cell.count++;
		}

		// Highest value of the bucket that is `age` buckets old (0 = the one of now_us). -1: it has none.
		float bucket(int line, uint64_t now_us, int age) const
		{
			const Cell *cell = at(line, now_us, age);
			return cell ? cell->high : -1;
		}

		// "Now": the newest bucket of the window that has a value. -1: the window has none.
		float now(int line, uint64_t now_us) const
		{
			for(int age = 0; age < Buckets; age++)
				if(const Cell *cell = at(line, now_us, age))
					return cell->high;
			return -1;
		}

		// Mean of every value in the window. -1: the window has none.
		float average(int line, uint64_t now_us) const
		{
			double sum = 0;
			int count = 0;
			for(int age = 0; age < Buckets; age++)
				if(const Cell *cell = at(line, now_us, age))
				{
					sum += cell->sum;
					count += cell->count;
				}
			return count ? (float)(sum / count) : -1;
		}

	private:
		struct Cell;
		// The cell of the bucket `age` buckets before now_us, if it holds values of that very bucket.
		const Cell *at(int line, uint64_t now_us, int age) const
		{
			const uint64_t current = now_us / BucketUs;
			if(line < 0 || line >= Lines || age < 0 || age >= Buckets || (uint64_t)age > current)
				return nullptr;
			const Cell &cell = cells[line][(current - age) % Buckets];
			return cell.number == current - age && cell.count ? &cell : nullptr;
		}

		struct Cell
		{
			uint64_t number = ~0ull; // which bucket of all time this cell holds
			float high = 0, sum = 0;
			int count = 0;
		};
		Cell cells[Lines][Buckets];
};

// 0 = pass, otherwise the number of the first failing check (80 to 84).
inline int selfTestGraph()
{
	const uint64_t t = 374525487409ull; // any moment
	const uint64_t second = 1000000;
	auto same = [](float a, float b) { return a > b - 0.001f && a < b + 0.001f; };
	{
		// 80: two values in one bucket: "now" is the higher, the average is their mean.
		Graph g;
		g.add(0, t, 2);
		g.add(0, t + 10000, 6);
		if(!same(g.now(0, t + 20000), 6) || !same(g.average(0, t + 20000), 4))
			return 80;
		// 81: 25 seconds later they have left the window.
		if(g.now(0, t + 25 * second) != -1 || g.average(0, t + 25 * second) != -1)
			return 81;
		// 83: the other lines know nothing of it.
		if(g.now(1, t + 20000) != -1)
			return 83;
	}
	{
		// 82: a value handed in late sits in the bucket of its own time; "now" still finds it.
		Graph g;
		const uint64_t now = t + 3 * second;
		g.add(0, now - 1500000, 7);
		if(!same(g.now(0, now), 7) || !same(g.bucket(0, now, 15), 7) || g.bucket(0, now, 0) != -1 || g.bucket(0, now, 14) != -1)
			return 82;
	}
	{
		// 84: 20 seconds on, the same cell is used again: the old value is gone, not mixed in.
		Graph g;
		g.add(0, t, 9);
		g.add(0, t + Graph::Buckets * Graph::BucketUs, 1);
		if(!same(g.average(0, t + Graph::Buckets * Graph::BucketUs), 1))
			return 84;
	}
	return 0;
}

} // namespace Farside

#endif
