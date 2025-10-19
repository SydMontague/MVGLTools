
#include "Compressors.h"

#include <Common.h>
#include <Compressor.h>
#include <Decompressor.h>
#include <lz4hc.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace dscstools
{
    auto Doboz::decompress(const std::vector<char>& input, size_t size) -> std::expected<std::vector<char>, std::string>
    {
        doboz::Decompressor decomp;
        doboz::CompressionInfo info{};
        auto result1 = decomp.getCompressionInfo(input.data(), input.size(), info);

        if (result1 != doboz::RESULT_OK) return input;
        if (info.compressedSize != input.size()) return input;
        if (info.version != 0) return input;
        if (info.uncompressedSize != size) return input;

        std::vector<char> output(info.uncompressedSize);

        auto result = decomp.decompress(input.data(), input.size(), output.data(), output.size());
        if (result != doboz::RESULT_OK)
            return std::unexpected(std::format("Error: something went wrong while decompressing, doboz error code: {}",
                                               std::to_underlying(result)));

        return output;
    }

    auto Doboz::compress(const std::vector<char>& input) -> std::expected<std::vector<char>, std::string>
    {
        doboz::Compressor comp;
        auto maxSize = doboz::Compressor::getMaxCompressedSize(input.size());
        std::vector<char> output(maxSize);
        size_t destSize = 0;

        auto result = comp.compress(input.data(), input.size(), output.data(), output.size(), destSize);

        if (result != doboz::RESULT_OK)
            return std::unexpected(std::format("Error: something went wrong while compressing, doboz error code: {}",
                                               std::to_underlying(result)));

        output.resize(destSize);
        return output;
    }

    auto Doboz::isCompressed(const std::vector<char>& input) -> bool
    {
        doboz::Decompressor decomp;
        doboz::CompressionInfo info{};
        auto result = decomp.getCompressionInfo(input.data(), input.size(), info);

        if (result != doboz::RESULT_OK) return false;
        if (info.version == 0) return false;
        if (info.compressedSize != input.size()) return false;

        return true;
    }

    auto LZ4::decompress(const std::vector<char>& input, size_t size) -> std::expected<std::vector<char>, std::string>
    {
        if (input.size() == size) return input;

        std::vector<char> output(size);
        auto result = LZ4_decompress_safe(input.data(),
                                          output.data(),
                                          static_cast<int32_t>(input.size()),
                                          static_cast<int32_t>(output.size()));

        if (result != size) return std::unexpected(std::format("Error: something went wrong while decompressing."));
        return output;
    }

    auto LZ4::compress(const std::vector<char>& input) -> std::expected<std::vector<char>, std::string>
    {
        auto inSize  = static_cast<int32_t>(input.size());
        auto outSize = LZ4_compressBound(inSize);
        std::vector<char> output(outSize);

        auto result = LZ4_compress_HC(input.data(), output.data(), inSize, outSize, LZ4HC_CLEVEL_MAX);
        if (result == 0) return std::unexpected(std::format("Error: something went wrong while compressing."));

        output.resize(result);
        return output;
    }

    auto LZ4::isCompressed(const std::vector<char>& input) -> bool
    {
        std::vector<char> output(256);

        auto inSize  = static_cast<int32_t>(input.size());
        auto outSize = static_cast<int32_t>(output.size());
        auto result  = LZ4_decompress_safe_partial(input.data(), output.data(), inSize, outSize, outSize);

        return result >= 0;
    }
} // namespace dscstools