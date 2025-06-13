#include "Chrono.h"

#if defined(_WIN32)
#   include <Windows.h>
#endif

namespace zartrux {

double Chrono::highResolutionMSecs()
{
#   if defined(_WIN32)
    LARGE_INTEGER f, t;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    return static_cast<double>(t.QuadPart) * 1e3 / f.QuadPart;
#   else
    using namespace std::chrono;
    return static_cast<double>(duration_cast<nanoseconds>(high_resolution_clock::now()).time_since_epoch().count()) / 1e6;
#   endif
}

} // namespace zartrux