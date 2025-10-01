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

    constexpr void log(std::string str)
    {
        std::cout << str << std::endl;
    }

    template<typename T>
    constexpr T read(std::ifstream& stream)
    {
        T data;
        stream.read(reinterpret_cast<char*>(&data), sizeof(T));
        return data;
    }

    constexpr uint32_t getChecksum(const std::vector<char>& data)
    {
        boost::crc_32_type crc;
        crc.process_bytes(data.data(), data.size());
        return crc.checksum();
    }

} // namespace dscstools