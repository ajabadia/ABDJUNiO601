#pragma once

namespace juce
{

struct ThreadPriorities
{
    struct Entry
    {
        Thread::Priority priority;
        int native;
    };

    static inline constexpr Entry table[]
    {
        { Thread::Priority::highest,    0 },
        { Thread::Priority::high,       0 },
        { Thread::Priority::normal,     0 },
        { Thread::Priority::low,        0 },
        { Thread::Priority::background, 0 },
    };

    static_assert (std::size (table) == 5,
                   "The platform may be unsupported or there may be a priority entry missing.");

    static Thread::Priority getJucePriority (const int value)
    {
        const auto iter = std::min_element (std::begin (table),
                                            std::end   (table),
                                            [value] (const auto& a, const auto& b)
                                            {
                                                return std::abs (a.native - value) < std::abs (b.native - value);
                                            });

        return iter != std::end (table) ? iter->priority : Thread::Priority{};
    }

    static int getNativePriority (const Thread::Priority value)
    {
        const auto iter = std::find_if (std::begin (table),
                                        std::end   (table),
                                        [value] (const auto& entry) { return entry.priority == value; });

        return iter != std::end (table) ? iter->native : 0;
    }
};

} // namespace juce
