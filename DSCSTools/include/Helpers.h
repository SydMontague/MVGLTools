#pragma once

#include "boost/crc.hpp"

#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>

namespace dscstools
{

    constexpr std::string_view trim(std::string_view view)
    {
        auto firstNull  = view.find_first_of('\0');
        auto firstSpace = view.find_first_of(' ');

        return view.substr(0, std::min(firstNull, firstSpace));
    }

    inline void log(std::string str)
    {
        std::cout << str << std::endl;
    }

    template<typename T>
    inline T read(std::ifstream& stream)
    {
        T data;
        stream.read(reinterpret_cast<char*>(&data), sizeof(T));
        return data;
    }

    template<typename T>
    inline void write(std::ofstream& stream, const T& data)
    {
        stream.write(reinterpret_cast<const char*>(&data), sizeof(T));
    }

    inline void write(std::ofstream& stream, const void* data, size_t size)
    {
        stream.write(reinterpret_cast<const char*>(data), size);
    }

    inline void write(std::ofstream& stream, const char* data, size_t size)
    {
        stream.write(reinterpret_cast<const char*>(data), size);
    }

    inline void write(std::ofstream& stream, const std::string& data, size_t size)
    {
        std::vector<char> copy(size);
        std::ranges::copy(data, copy.begin());
        stream.write(copy.data(), copy.size());
    }

    inline uint32_t getChecksum(const std::vector<char>& data)
    {
        boost::crc_32_type crc;
        crc.process_bytes(data.data(), data.size());
        return crc.checksum();
    }

    template<int32_t step>
    constexpr int32_t ceilInteger(int32_t value)
    {
        return (value + step - 1) / step * step;
    }

    constexpr int32_t ceilInteger(int32_t value, int32_t step)
    {
        if (step == 0) return value;
        return (value + step - 1) / step * step;
    }

    template<int32_t step>
    constexpr void alignStream(std::ifstream& stream)
    {
        stream.seekg(ceilInteger<step>(stream.tellg()));
    }

    constexpr std::string wrapRegex(const std::string& in)
    {
        return "^" + in + "$";
    }

} // namespace dscstools

namespace
{
    using namespace dscstools;

    static_assert(ceilInteger<8>(76) == 80);
    static_assert(ceilInteger<8>(8) == 8);
    static_assert(ceilInteger(76, 8) == 80);
    static_assert(ceilInteger(8, 8) == 8);
} // namespace