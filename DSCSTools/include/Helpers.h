#pragma once

#include <boost/crc.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace dscstools
{
    inline auto file_equivalent(const std::filesystem::path& file1, const std::filesystem::path& file2) -> bool
    {
        try
        {
            return std::filesystem::equivalent(file1, file2);
        }
        catch (...)
        {
            return false;
        }
    }

    inline void log(const std::string& str)
    {
        std::cout << str << '\n';
    }

    template<typename T>
    inline auto read(std::ifstream& stream) -> T
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

    template<>
    inline void write(std::ofstream& stream, const std::vector<char>& data)
    {
        stream.write(data.data(), static_cast<std::streamsize>(data.size()));
    }

    inline void write(std::ofstream& stream, const void* data, std::streamsize size)
    {
        stream.write(reinterpret_cast<const char*>(data), size);
    }
    inline void write(std::ofstream& stream, const std::string& data, std::streamsize size)
    {
        std::vector<char> copy(size);
        std::ranges::copy(data, copy.begin());
        stream.write(copy.data(), static_cast<std::streamsize>(copy.size()));
    }

    inline auto getChecksum(const std::vector<char>& data) -> uint32_t
    {
        boost::crc_32_type crc;
        crc.process_bytes(data.data(), data.size());
        return crc.checksum();
    }

    constexpr auto trim(std::string_view view) -> std::string_view
    {
        auto firstNull  = view.find_first_of('\0');
        auto firstSpace = view.find_first_of(' ');

        return view.substr(0, std::min(firstNull, firstSpace));
    }

    constexpr auto ceilInteger(int64_t value, int64_t step) -> int64_t
    {
        if (step == 0) return value;
        return (value + step - 1) / step * step;
    }
    
    template<int64_t step>
    inline void alignStream(std::ifstream& stream)
    {
        stream.seekg(ceilInteger(stream.tellg(), step));
    }

    constexpr auto wrapRegex(const std::string& in) -> std::string
    {
        return "^" + in + "$";
    }

} // namespace dscstools

namespace dscstools::test
{
    static_assert(ceilInteger(76, 8) == 80);
    static_assert(ceilInteger(8, 8) == 8);
} // namespace dscstools::test