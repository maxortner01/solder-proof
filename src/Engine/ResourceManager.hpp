#pragma once

#include <unordered_map>
#include <memory>
#include <typeindex>

#include <cxxabi.h>

namespace Engine
{
    struct ResourceManager
    {
        template<typename T>
        struct Entry
        {
            std::string name;
            std::shared_ptr<T> value;
        };

        template<typename T, typename... Args>
        Entry<T>
        create(const std::string& name, Args&&... args)
        {
            const auto tid = std::type_index(typeid(T));
            auto ptr = std::make_shared<T>(std::forward<Args>(args)...);
            resources[tid][name] = ptr;
            return Entry<T>{ .name = name, .value = ptr };
        }

        template<typename T>
        Entry<T>
        get(const std::string& name)
        {
            const auto tid = std::type_index(typeid(T));
            assert(resources.count(tid));
            assert(resources[tid].count(name));
            return Entry<T>{ .name = name, .value = std::reinterpret_pointer_cast<T>(resources.at(tid).at(name)) };
        }

        template<typename T>
        void destroy(const std::string& name)
        {
            const auto tid = std::type_index(typeid(T));
            assert(resources.count(tid));
            assert(resources[tid].count(name));
            resources[tid].erase(name);
        }

        // Creates a copy of the resource map, so somewhat expensive
        template<typename T>
        std::unordered_map<std::string, std::shared_ptr<T>>
        get_type_map() const
        {
            const auto tid = std::type_index(typeid(T));
            std::unordered_map<std::string, std::shared_ptr<T>> ret;
            if (resources.count(tid))
                for (const auto& [ name, ptr ] : resources.at(tid))
                    ret[name] = std::reinterpret_pointer_cast<T>( ptr );
            return ret;
        }
    
    private:
        std::unordered_map<std::type_index, std::unordered_map<std::string, std::shared_ptr<void>>> resources;
    };
}