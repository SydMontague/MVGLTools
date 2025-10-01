#include "../libs/doboz/Decompressor.h"
#include "../libs/lz4/lz4hc.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace dscs::mdb1new
{

    constexpr std::string_view trim(std::string_view view)
    {
        auto firstNull  = view.find_first_of('\0');
        auto firstSpace = view.find_first_of(' ');

        return view.substr(0, std::min(firstNull, firstSpace));
    }

    template<typename T> constexpr T read(std::ifstream& stream)
    {
        T data;
        stream.read(reinterpret_cast<char*>(&data), sizeof(T));
        return data;
    }

    constexpr void log(std::string str)
    {
        std::cout << str << std::endl;
    }

    struct MDB1Header32
    {
        uint32_t magicValue;
        uint16_t fileEntryCount;
        uint16_t fileNameCount;
        uint32_t dataEntryCount;
        uint32_t dataStart;
        uint32_t totalSize;
    };

    struct FileTreeEntry32
    {
        uint16_t compareBit;
        uint16_t dataId;
        uint16_t left;
        uint16_t right;
    };

    struct FileDataEntry32
    {
        uint32_t offset;
        uint32_t fullSize;
        uint32_t compressedSize;
    };

    struct MDB1Header64
    {
        uint32_t magicValue;
        uint32_t fileEntryCount;
        uint32_t fileNameCount;
        uint32_t dataEntryCount;
        uint64_t dataStart;
        uint64_t totalSize;
    };

    struct FileTreeEntry64
    {
        uint32_t compareBit;
        uint32_t dataId;
        uint32_t left;
        uint32_t right;
    };

    struct FileDataEntry64
    {
        uint64_t offset;
        uint64_t fullSize;
        uint64_t compressedSize;
    };

    template<size_t name_length, size_t extension_length> struct FileNameEntry
    {
        std::array<char, extension_length> extension;
        std::array<char, name_length> name;

        std::string toString()
        {
            std::string_view nameView(name.data(), name.size());
            std::string_view extensionView(extension.data(), extension.size());

            return std::format("{}.{}", trim(nameView), trim(extensionView));
        }
    };

    struct Doboz
    {
        static std::expected<std::vector<char>, std::string> decompress(const std::vector<char>& input, size_t size)
        {
            if (input.size() == size) return input;

            doboz::Decompressor decomp;
            doboz::CompressionInfo info;
            decomp.getCompressionInfo(input.data(), input.size(), info);

            if (info.compressedSize != input.size() || info.version != 0 || info.uncompressedSize != size)
                return std::unexpected("Error: input file is not doboz compressed!");

            std::vector<char> output(info.uncompressedSize);

            auto result = decomp.decompress(input.data(), input.size(), output.data(), output.size());
            if (result != doboz::RESULT_OK)
                return std::unexpected(
                    std::format("Error: something went wrong while decompressing, doboz error code: {}",
                                std::to_underlying(result)));

            return output;
        }
    };

    struct LZ4
    {
        static std::expected<std::vector<char>, std::string> decompress(const std::vector<char>& input, size_t size)
        {
            if (input.size() == size) return input;
            std::vector<char> output(size);
            LZ4_decompress_safe(input.data(), output.data(), input.size(), output.size());
            return output;
        }
    };

    struct DSCS
    {
        using Header     = MDB1Header32;
        using TreeEntry  = FileTreeEntry32;
        using NameEntry  = FileNameEntry<0x3C, 4>;
        using DataEntry  = FileDataEntry32;
        using Compressor = Doboz;

        static_assert(sizeof(Header) == 0x14);
        static_assert(sizeof(TreeEntry) == 0x08);
        static_assert(sizeof(NameEntry) == 0x40);
        static_assert(sizeof(DataEntry) == 0x0C);
    };

    struct HLTLDA
    {
        using Header     = MDB1Header64;
        using TreeEntry  = FileTreeEntry64;
        using NameEntry  = FileNameEntry<0x7C, 4>;
        using DataEntry  = FileDataEntry64;
        using Compressor = LZ4;

        static_assert(sizeof(Header) == 0x20);
        static_assert(sizeof(TreeEntry) == 0x10);
        static_assert(sizeof(NameEntry) == 0x80);
        static_assert(sizeof(DataEntry) == 0x18);
    };

    template<typename T>
    concept Compressor = requires(const std::vector<char>& input, size_t size) {
        { T::decompress(input, size) } -> std::same_as<std::expected<std::vector<char>, std::string>>;
    };

    template<typename T>
    concept ArchiveType = requires {
        typename T::Header;
        typename T::TreeEntry;
        typename T::NameEntry;
        typename T::DataEntry;
        typename T::Compressor;
    } && Compressor<typename T::Compressor>;

    template<ArchiveType MDB> struct ArchiveInfo
    {
        ArchiveInfo(std::filesystem::path path);

        void extract(std::filesystem::path output)
        {
            std::ranges::for_each(entries,
                                  [this, output](const auto& key) { extractFile(output, key.first, key.second); });
        }

    private:
        struct ArchiveEntry
        {
            uint64_t offset;
            uint64_t fullSize;
            uint64_t compressedSize;
        };

        std::ifstream input;
        std::map<std::string, ArchiveEntry> entries;
        uint64_t dataStart;

        void extractFile(std::filesystem::path output, std::string file, const ArchiveEntry& entry);
    };

    struct FileTreeEntry {
        std::filesystem::path path;
        uint64_t compareBit;
        uint64_t left;
        uint64_t right;
    };

    struct FileTree {

        FileTree(std::filesystem::path input) {

            std::filesystem::recursive_directory_iterator itr(input);

            std::vector<std::filesystem::path> vec;
            
            for(auto& entry : itr) {
                if(!std::filesystem::is_regular_file(entry)) continue;
                vec.push_back(entry);
            }
            std::ranges::sort(vec);
        }

    };

    void test()
    {
        auto dscsPath   = "/home/syd/Development/MyRepos/DSCSTools/build/DSCSToolsCLI/DSDBP.decrypt.bin";
        auto hltldaPath = "/home/syd/Development/MyRepos/DSCSTools/build/DSCSToolsCLI/app_romA_0.dx11.mvgl";
        auto dstsPath = "/home/syd/Development/MyRepos/DSCSTools/build/DSCSToolsCLI/app_0.dx11.mvgl";

        ArchiveInfo<DSCS> info(dscsPath);
        ArchiveInfo<HLTLDA> info2(hltldaPath);
        ArchiveInfo<HLTLDA> info3(dstsPath);

        info.extract("output/");
        info2.extract("output2/");
        info3.extract("DSTS/");
    }

    template<ArchiveType MDB>
    ArchiveInfo<MDB>::ArchiveInfo(std::filesystem::path path)
        : input(path)
    {
        auto header = read<typename MDB::Header>(input);

        dataStart = header.dataStart;

        assert(header.fileEntryCount == header.fileNameCount);

        std::vector<typename MDB::TreeEntry> treeEntries;
        std::vector<typename MDB::NameEntry> nameEntries;
        std::vector<typename MDB::DataEntry> dataEntries;
        for (int32_t i = 0; i < header.fileEntryCount; i++)
            treeEntries.push_back(read<typename MDB::TreeEntry>(input));
        for (int32_t i = 0; i < header.fileNameCount; i++)
            nameEntries.push_back(read<typename MDB::NameEntry>(input));
        for (int32_t i = 0; i < header.dataEntryCount; i++)
            dataEntries.push_back(read<typename MDB::DataEntry>(input));

        for (int32_t i = 0; i < treeEntries.size(); i++)
        {
            auto dataId = treeEntries[i].dataId;
            if (dataId == std::numeric_limits<decltype(dataId)>::max()) continue;
            auto data = dataEntries.at(dataId);

            entries[nameEntries[i].toString()] = {
                .offset         = data.offset,
                .fullSize       = data.fullSize,
                .compressedSize = data.compressedSize,
            };
        }
    }

    template<ArchiveType MDB>
    void ArchiveInfo<MDB>::extractFile(std::filesystem::path output, std::string file, const ArchiveEntry& entry)
    {
        using Comp = MDB::Compressor;

        std::vector<char> inputData(entry.compressedSize);

        input.seekg(dataStart + entry.offset);
        input.read(inputData.data(), inputData.size());

        auto result = Comp::decompress(inputData, entry.fullSize);
        if (result.has_value())
        {
            std::replace(file.begin(), file.end(), '\\', '/');
            std::filesystem::path path = output / file;

            if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());

            std::ofstream output(path, std::ios::out | std::ios::binary);
            output.write(result.value().data(), result.value().size());
        }
        else
            log(result.error());
    }
} // namespace dscs::mdb1new