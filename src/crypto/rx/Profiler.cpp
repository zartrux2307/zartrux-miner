

#include "crypto/rx/Profiler.h"



#include <cstring>
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>


#ifdef zartrux_FEATURE_PROFILING


ProfileScopeData* ProfileScopeData::s_data[MAX_DATA_COUNT] = {};
volatile long ProfileScopeData::s_dataCount = 0;
double ProfileScopeData::s_tscSpeed = 0.0;


#ifndef NOINLINE
#ifdef __GNUC__
#define NOINLINE __attribute__ ((noinline))
#elif _MSC_VER
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE
#endif
#endif


static std::string get_thread_id()
{
    std::stringstream ss;
    ss << std::this_thread::get_id();

    std::string s = ss.str();
    if (s.length() > ProfileScopeData::MAX_THREAD_ID_LENGTH) {
        s.resize(ProfileScopeData::MAX_THREAD_ID_LENGTH);
    }

    return s;
}


NOINLINE void ProfileScopeData::Register(ProfileScopeData* data)
{
#ifdef _MSC_VER
    const long id = _InterlockedIncrement(&s_dataCount) - 1;
#else
    const long id = __sync_fetch_and_add(&s_dataCount, 1);
#endif

    if (static_cast<unsigned long>(id) < MAX_DATA_COUNT) {
        s_data[id] = data;

        const std::string s = get_thread_id();
        memcpy(data->m_threadId, s.c_str(), s.length() + 1);
    }
}


NOINLINE void ProfileScopeData::Init()
{
    using namespace std::chrono;

    const uint64_t t1 = static_cast<uint64_t>(time_point_cast<nanoseconds>(high_resolution_clock::now()).time_since_epoch().count());
    const uint64_t count1 = ReadTSC();

    for (;;)
    {
        const uint64_t t2 = static_cast<uint64_t>(time_point_cast<nanoseconds>(high_resolution_clock::now()).time_since_epoch().count());
        const uint64_t count2 = ReadTSC();

        if (t2 - t1 > 1000000000) {
            s_tscSpeed = (count2 - count1) * 1e9 / (t2 - t1);
            LOG_INFO("%s TSC speed = %.3f GHz", xmrig::Tags::profiler(), s_tscSpeed / 1e9);
            return;
        }
    }
}


#endif 
