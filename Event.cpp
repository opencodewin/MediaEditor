#include "Event.h"

using namespace std;

namespace MEC
{
bool Event::CheckEventOverlapped(const Event& e, int64_t start, int64_t end, int32_t z)
{
    if (z == e.Z() &&
       (start >= e.Start() && start < e.End() || end > e.Start() && end <= e.End() ||
        start < e.Start() && end > e.End()))
        return true;
    return false;
}

function<bool(const Event&,const Event&)> Event::EVENT_ORDER_COMPARATOR = [] (const Event& a, const Event& b)
{
    return a.Z() < b.Z() || (a.Z() == b.Z() && a.Start() < b.Start());
};

ostream& operator<<(ostream& os, const Event& e)
{
    os << "{\"id\":" << e.Id() << ", \"start\":" << e.Start() << ", \"end\":" << e.End() << ", \"length\":" << e.Length() << "}";
    return os;
}
}