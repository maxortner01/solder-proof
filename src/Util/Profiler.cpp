#include "Profiler.hpp"

#include <numeric>
#include <iostream>

namespace Util
{
    double Profiler::BlockData::getAverageRuntime(double over_last_sec)
    {
        using namespace std::chrono;

        // Get current time,
        // go through runs starting at end and going back
        // accrue the duration values until you hit an instance that is
        // beyond over_last_sec
        // take the average and return
        const auto now = high_resolution_clock::now();
        std::vector<double> runtime;

        for (auto it = runs.end(); it != runs.begin(); it--)
        {
            if (it == runs.end()) continue;

            const auto secs_ago = duration<double, seconds::period>( now - it->second.timestamp );
            if (secs_ago.count() > over_last_sec) break;

            if (it->second.completion)
            {
                const auto exec_time = duration<double, milliseconds::period>( *it->second.completion - it->second.timestamp );
                runtime.push_back(exec_time.count());
            }
        }

        double total = 0.0;
        for (const auto& d : runtime) total += d;
        return total / (double)runtime.size();
    }

    std::size_t Profiler::beginBlock(const std::string& name)
    {
        static std::size_t id = 0;
        if (!data.count(name)) data[name] = std::make_shared<BlockData>(BlockData());
        data[name]->runs[id] = BlockData::Run{ .timestamp = std::chrono::high_resolution_clock::now() };
        return id++;
    }   

    void Profiler::endBlock(std::size_t id, const std::string& name)
    {
        data[name]->runs[id].completion = std::chrono::high_resolution_clock::now();
    }

    std::shared_ptr<Profiler::BlockData>
    Profiler::getBlock(const std::string& name)
    {
        if (!data.count(name)) data.emplace(name, std::make_shared<BlockData>());
        return data.at(name);
    }
}