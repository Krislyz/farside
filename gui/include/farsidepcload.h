// Farside: how busy this PC is, for the bar in the stream: processor and graphics card, in percent.
// Windows only; elsewhere both stay unknown (-1).
#ifndef FARSIDE_PCLOAD_H
#define FARSIDE_PCLOAD_H

#ifdef _WIN32
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <map>
#include <string>
#include <vector>
#endif

namespace Farside {

class PcLoad
{
	public:
		PcLoad()
		{
#ifdef _WIN32
			// Every 3D engine of every graphics card, per program: the counters Task Manager reads.
			if(PdhOpenQueryW(nullptr, 0, &query) == ERROR_SUCCESS)
			{
				if(PdhAddEnglishCounterW(query, L"\\GPU Engine(*engtype_3D)\\Utilization Percentage", 0, &counter) == ERROR_SUCCESS)
					PdhCollectQueryData(query);
				else
					counter = nullptr;
			}
#endif
		}

		~PcLoad()
		{
#ifdef _WIN32
			if(query)
				PdhCloseQuery(query);
#endif
		}

		PcLoad(const PcLoad &) = delete;
		PcLoad &operator=(const PcLoad &) = delete;

		// Call about once a second: both numbers are the share of the time since the call before.
		// -1: not known (the first call, a PC without these counters, not Windows).
		void read(float &cpu, float &gpu)
		{
			cpu = gpu = -1;
#ifdef _WIN32
			FILETIME idle_time, kernel_time, user_time;
			if(GetSystemTimes(&idle_time, &kernel_time, &user_time))
			{
				// The kernel time has the idle time in it.
				const uint64_t idle = ticks(idle_time), all = ticks(kernel_time) + ticks(user_time);
				if(last_all && all > last_all)
					cpu = 100.0f * (float)((all - last_all) - (idle - last_idle)) / (float)(all - last_all);
				last_idle = idle;
				last_all = all;
			}
			if(!counter || PdhCollectQueryData(query) != ERROR_SUCCESS)
				return;
			DWORD size = 0, count = 0;
			if(PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &size, &count, nullptr) != PDH_MORE_DATA || !size)
				return;
			std::vector<unsigned char> buffer(size);
			auto *items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W *>(buffer.data());
			if(PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &size, &count, items) != ERROR_SUCCESS)
				return;
			// A name is "pid_<n>_luid_<card>_phys_<n>_eng_<n>_engtype_3D". The programs' shares of one engine
			// add up; the busiest engine is the card's number, as in Task Manager.
			std::map<std::wstring, double> engines;
			for(DWORD i = 0; i < count; i++)
			{
				if(items[i].FmtValue.CStatus != PDH_CSTATUS_VALID_DATA && items[i].FmtValue.CStatus != PDH_CSTATUS_NEW_DATA)
					continue;
				const std::wstring name = items[i].szName;
				const size_t from = name.find(L"luid_");
				engines[from == std::wstring::npos ? name : name.substr(from)] += items[i].FmtValue.doubleValue;
			}
			double top = 0;
			for(const auto &engine : engines)
				if(engine.second > top)
					top = engine.second;
			gpu = (float)(top > 100 ? 100 : top);
#endif
		}

	private:
#ifdef _WIN32
		static uint64_t ticks(const FILETIME &t) { return ((uint64_t)t.dwHighDateTime << 32) | t.dwLowDateTime; }

		PDH_HQUERY query = nullptr;
		PDH_HCOUNTER counter = nullptr;
		uint64_t last_idle = 0, last_all = 0;
#endif
};

} // namespace Farside

#endif
