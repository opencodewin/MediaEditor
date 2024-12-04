#include <chrono>
#include <functional>

using namespace std;

namespace SysUtils
{
static const auto _ProcessStartTick = chrono::steady_clock::now();

chrono::steady_clock::rep GetTickSinceProcessStart()
{
    return (chrono::steady_clock::now()-_ProcessStartTick).count();
}

static hash<chrono::steady_clock::rep> _TickHashFn;

size_t GetTickHash()
{
    return _TickHashFn(chrono::steady_clock::now().time_since_epoch().count());
}
}