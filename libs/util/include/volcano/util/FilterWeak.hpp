#pragma once
#include <ranges>
#include <memory>

namespace volcano::util
{

    template <class Range>
    auto filter_shared(Range &&container)
    {
        using std::views::filter;
        using std::views::transform;

        // 1) transform weak_ptr -> shared_ptr
        // 2) filter out null (expired)
        return std::forward<Range>(container) | filter([](auto &w)
                                                       { return !w.expired(); }) |
               transform([](auto &w)
                         { return w.lock(); });
    }

    // For a container of weak_ptr<T>, yields T* (non-null)
    // Note: only call on a container that exists outside of the range.
    // example: auto loco = ch->getLocationObjects(); for (auto obj : filter_raw(loco)) { ... }
    // Do not just for(auto obj : filter_raw(ch->getLocationObjects())) you won't like the results.
    template <class Range>
    auto filter_raw(Range &&container)
    {
        using std::views::filter;
        using std::views::transform;

        // 1) transform weak_ptr -> T*
        // 2) filter out null (expired)
        return std::forward<Range>(container) | filter([](auto &w)
                                                       { return !w.expired(); }) |
               transform([](auto &w)
                         { return w.lock().get(); });
    }

}
