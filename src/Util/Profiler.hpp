#pragma once

#include <string>
#include <map>
#include <unordered_map>
#include <vector>

#include <chrono>

namespace Util
{
    struct Profiler
    {
        struct BlockData
        {
            struct Run
            {
                std::chrono::steady_clock::time_point timestamp;
                std::optional<std::chrono::steady_clock::time_point> completion;
            };

            double getAverageRuntime(double over_last_sec = 1.0);

            std::map<std::size_t, Run> runs;
        };
        

        std::size_t beginBlock(const std::string& name);
        void endBlock(std::size_t id, const std::string& name);

        std::shared_ptr<BlockData>
        getBlock(const std::string& name);

    private:
        std::unordered_map<std::string, std::shared_ptr<BlockData>> data;
    };  

    struct ProfilerBlock
    {
        ProfilerBlock(Profiler& profiler, const std::string& block_name) :
            p{&profiler},
            id{profiler.beginBlock(block_name)},
            name{block_name}
        {   }

        ~ProfilerBlock()
        {
            p->endBlock(id, name);
        }

    private:
        std::string name;
        std::size_t id;
        Profiler* p;
    };
}   