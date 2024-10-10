#pragma once

namespace Util
{
    enum UnitType
    {
        Bytes, Kilobytes, Megabytes, Gigabytes
    };

    template<UnitType In, UnitType Out>
    struct Converter
    {
        template<typename T>
        static T convert(const T&);
    };

    template<UnitType In, UnitType Out, typename T>
    T convert(const T& value)
    {
        return Converter<In, Out>::convert(value);
    }

    template<>
    struct Converter<UnitType::Bytes, UnitType::Kilobytes>
    {
        template<typename T>
        static T convert(const T& bytes)
        {
            return bytes / 1024;
        }
    };

    template<>
    struct Converter<UnitType::Bytes, UnitType::Megabytes>
    {
        template<typename T>
        static T convert(const T& bytes)
        {
            return bytes / (1024 * 1024);
        }
    };
}