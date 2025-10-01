#pragma once
#include "Compressors.h"
#include "Helpers.h"

#include <boost/asio.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <future>
#include <map>
#include <string>

namespace dscstools::mdb1new
{
    template<typename T>
    concept Compressor = requires(const std::vector<char>& input, size_t size) {
        { T::decompress(input, size) } -> std::same_as<std::expected<std::vector<char>, std::string>>;
        { T::compress(input) } -> std::same_as<std::expected<std::vector<char>, std::string>>;
        { T::isCompressed(input) } -> std::same_as<bool>;
    };

    template<typename T>
    concept ArchiveType = requires {
        typename T::Header;
        typename T::TreeEntry;
        typename T::NameEntry;
        typename T::DataEntry;
        typename T::Compressor;
    } && Compressor<typename T::Compressor>;

    constexpr uint32_t MDB1_MAGIC_VALUE = 0x3142444d;

    struct MDB1Header32
    {
        uint32_t magicValue{MDB1_MAGIC_VALUE};
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
        uint32_t magicValue{MDB1_MAGIC_VALUE};
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

    template<size_t name_length, size_t extension_length>
    struct FileNameEntry
    {
        std::array<char, extension_length> extension{};
        std::array<char, name_length> name{};

        FileNameEntry() = default;
        FileNameEntry(std::string input)
        {
            std::copy(input.begin(), input.begin() + 4, extension.begin());
            std::copy(input.begin() + 4, input.end(), name.begin());
        }

        std::string toString()
        {
            std::string_view nameView(name.data(), name.size());
            std::string_view extensionView(extension.data(), extension.size());

            return std::format("{}.{}", trim(nameView), trim(extensionView));
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

    struct DSTS
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

    template<ArchiveType MDB>
    struct ArchiveInfo
    {
        ArchiveInfo(std::filesystem::path path);

        void extract(std::filesystem::path output);

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

    template<ArchiveType MDB>
    void packArchive(std::filesystem::path source, std::filesystem::path target, CompressMode compress);

} // namespace dscstools::mdb1new

/* Implementation */
namespace dscstools::mdb1new::detail
{
    struct TreeName
    {
        std::string name;
        std::filesystem::path path;

        friend bool operator==(const TreeName& self, const TreeName& other) { return self.name == other.name; }
    };

    struct TreeNode
    {
        uint64_t compareBit;
        uint64_t left  = 0;
        uint64_t right = 0;
        TreeName name;
    };

    struct CompressionResult
    {
        uint64_t originalSize = 0;
        uint32_t crc          = 0;
        std::vector<char> data;
    };

    constexpr uint64_t INVALID = std::numeric_limits<uint64_t>::max();

    constexpr inline  TreeNode findFirstBitMismatch(const uint16_t first,
                                  const std::vector<TreeName>& nodeless,
                                  const std::vector<TreeName>& withNode)
    {
        if (withNode.size() == 0) return {first, 0, 0, nodeless[0]};

        for (uint16_t i = first; i < 1024; i++)
        {
            bool set   = false;
            bool unset = false;

            for (const auto& file : withNode)
            {
                if ((file.name[i >> 3] >> (i & 7)) & 1)
                    set = true;
                else
                    unset = true;

                if (set && unset) return {i, 0, 0, nodeless[0]};
            }

            auto itr = std::find_if(nodeless.begin(),
                                    nodeless.end(),
                                    [set, unset, i](const auto& file)
                                    {
                                        bool val = (file.name[i >> 3] >> (i & 7)) & 1;
                                        return val && unset || !val && set;
                                    });

            if (itr != nodeless.end()) return {i, 0, 0, *itr};
        }

        return {INVALID, INVALID, 0, ""};
    }

    constexpr inline  std::string buildMDB1Path(const std::filesystem::path& path)
    {
        auto extension = path.extension().string().substr(1, 5);
        auto tmp       = path;
        auto fileName  = tmp.replace_extension("").string();

        if (extension.length() == 3) extension = extension.append(" ");
        std::replace(fileName.begin(), fileName.end(), '/', '\\');

        char name[0x81];
        strncpy(name, extension.c_str(), 4);
        strncpy(name + 4, fileName.c_str(), 0x7C);
        name[0x80] = 0; // prevent overflow

        return std::string(name);
    }

    constexpr inline std::vector<TreeNode> generateTree(const std::vector<std::filesystem::path> paths, std::filesystem::path source)
    {
        std::vector<TreeName> fileNames;
        std::ranges::transform(paths,
                               std::back_inserter(fileNames),
                               [&](const auto& path)
                               {
                                   auto relPath = std::filesystem::relative(path, source);
                                   return TreeName{buildMDB1Path(relPath), path};
                               });

        struct QueueEntry
        {
            uint64_t parentNode;
            uint64_t compareBit;
            std::vector<TreeName> list;
				    std::vector<TreeName> nodeList;
            bool isLeft;
        };

        std::vector<TreeNode> nodes  = {{INVALID, 0, 0, ""}};
        std::deque<QueueEntry> queue = {{0, INVALID, fileNames, {}, false}};

        while (!queue.empty())
        {
            QueueEntry entry = queue.front();
            queue.pop_front();
            TreeNode& parent = nodes[entry.parentNode];

            std::vector<TreeName> nodeless;
            std::vector<TreeName> withNode;

            for (auto file : entry.list)
            {
                if (std::find(entry.nodeList.begin(), entry.nodeList.end(), file) == entry.nodeList.end())
                    nodeless.push_back(file);
                else
                    withNode.push_back(file);
            }

            if (nodeless.size() == 0)
            {
                auto firstFile   = entry.list[0];
                auto itr         = std::find_if(nodes.begin(),
                                        nodes.end(),
                                        [firstFile](const TreeNode& node) { return node.name == firstFile; });
                ptrdiff_t offset = std::distance(nodes.begin(), itr);

                if (entry.isLeft)
                    parent.left = offset;
                else
                    parent.right = offset;

                continue;
            }

            TreeNode child = findFirstBitMismatch(entry.compareBit + 1, nodeless, withNode);

            if (entry.isLeft)
                parent.left = nodes.size();
            else
                parent.right = nodes.size();

            std::vector<TreeName> left;
            std::vector<TreeName> right;

            for (auto file : entry.list)
            {
                if ((file.name[child.compareBit >> 3] >> (child.compareBit & 7)) & 1)
                    right.push_back(file);
                else
                    left.push_back(file);
            }

            std::vector<TreeName> newNodeList = entry.nodeList;
            newNodeList.push_back(child.name);

            if (left.size() > 0) queue.push_front({nodes.size(), child.compareBit, std::move(left), newNodeList, true});
            if (right.size() > 0) queue.push_front({nodes.size(), child.compareBit, std::move(right), newNodeList, false});
            nodes.push_back(child);
        }

        return nodes;
    }

    template<Compressor Compress>
    CompressionResult getFileData(std::filesystem::path file, CompressMode mode)
    {
        std::ifstream input(file, std::ios::in | std::ios::binary);

        if (!input.good())
        {
            log(std::format("Error: something went wrong while reading {}", file.string()));
            return {};
        }

        auto size = std::filesystem::file_size(file);
        std::vector<char> data(size);
        input.read(data.data(), data.size());

        auto checksum = mode == CompressMode::ADVANCED ? getChecksum(data) : 0;

        if (size == 0 || Compress::isCompressed(data) || mode == CompressMode::NONE)
            return {.originalSize = data.size(), .crc = checksum, .data = data};

        auto compressed = Compress::compress(data).value_or(data);

        if (compressed.size() + 4 >= data.size()) compressed = data;

        return {
            .originalSize = data.size(),
            .crc          = checksum,
            .data         = compressed,
        };
    }
} // namespace dscstools::mdb1new::detail

// implementation
namespace dscstools::mdb1new
{
    using namespace detail;

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
    void ArchiveInfo<MDB>::extract(std::filesystem::path output)
    {
        std::ranges::for_each(entries, [this, output](const auto& key) { extractFile(output, key.first, key.second); });
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

    template<ArchiveType MDB>
    void packArchive(std::filesystem::path source, std::filesystem::path target, CompressMode compress)
    {
        std::vector<std::filesystem::path> files;

        for (auto i : std::filesystem::recursive_directory_iterator(source))
            if (std::filesystem::is_regular_file(i)) files.push_back(i);

        std::ranges::sort(files);

        log("[Pack] Generating File Tree...");
        auto tree = generateTree(files, source);

        // start compressing files
        std::map<std::string, std::promise<CompressionResult>> futureMap;
        // twice the core count to account for blocking threads
        size_t threadCount = std::thread::hardware_concurrency() * 2;
        boost::asio::thread_pool pool(threadCount);
        log(std::format("[Pack] Start compressing files with {} threads...", threadCount));

        for (const auto& file : tree)
        {
            if (file.compareBit == std::numeric_limits<decltype(file.compareBit)>::max()) continue;

            futureMap[file.name.name] = std::promise<CompressionResult>();

            boost::asio::post(pool,
                              [&]
                              {
                                  futureMap[file.name.name].set_value(
                                      getFileData<typename MDB::Compressor>(file.name.path, compress));
                              });
        }

        std::vector<typename MDB::TreeEntry> treeEntries;
        std::vector<typename MDB::NameEntry> nameEntries;
        std::vector<typename MDB::DataEntry> dataEntries;

        const auto fileCount     = files.size();
        const auto headerSize    = sizeof(typename MDB::Header);
        const auto treeEntrySize = sizeof(typename MDB::TreeEntry) * (fileCount + 1);
        const auto nameEntrySize = sizeof(typename MDB::NameEntry) * (fileCount + 1);
        const auto dataEntrySize = sizeof(typename MDB::DataEntry) * (fileCount);
        const auto dataStart     = headerSize + treeEntrySize + nameEntrySize + dataEntrySize;

        treeEntries.push_back({
            .compareBit = std::numeric_limits<decltype(MDB::TreeEntry::compareBit)>::max(),
            .dataId     = std::numeric_limits<decltype(MDB::TreeEntry::dataId)>::max(),
            .left       = 0,
            .right      = 1,
        });
        nameEntries.push_back({});

        auto fileId = 0;
        std::map<uint32_t, size_t> dataMap;
        size_t offset = 0;
        std::ofstream output(target);

        for (const auto& file : tree)
        {
            if (file.compareBit == std::numeric_limits<decltype(file.compareBit)>::max()) continue;

            if (fileId++ % 200 == 0) log(std::format("[Pack] Writing File {} of {}", fileId, fileCount));

            auto data         = futureMap[file.name.name].get_future().get();
            auto existingData = compress == CompressMode::ADVANCED ? dataMap.find(data.crc) : dataMap.end();
            auto dataId       = existingData == dataMap.end() ? dataEntries.size() : existingData->second;

            treeEntries.push_back({
                .compareBit = static_cast<decltype(MDB::TreeEntry::compareBit)>(file.compareBit),
                .dataId     = static_cast<decltype(MDB::TreeEntry::dataId)>(dataId),
                .left       = static_cast<decltype(MDB::TreeEntry::left)>(file.left),
                .right      = static_cast<decltype(MDB::TreeEntry::right)>(file.right),
            });
            nameEntries.emplace_back(file.name.name);
            if (existingData == dataMap.end())
            {
                dataMap[data.crc] = dataId;
                dataEntries.push_back({
                    .offset         = static_cast<decltype(MDB::DataEntry::offset)>(offset),
                    .fullSize       = static_cast<decltype(MDB::DataEntry::fullSize)>(data.originalSize),
                    .compressedSize = static_cast<decltype(MDB::DataEntry::compressedSize)>(data.data.size()),
                });

                output.seekp(dataStart + offset);
                output.write(data.data.data(), data.data.size());
                offset += data.data.size();
            }
        }

        output.seekp(0);
        typename MDB::Header header = {
            .fileEntryCount = static_cast<decltype(MDB::Header::fileEntryCount)>(treeEntries.size()),
            .fileNameCount  = static_cast<decltype(MDB::Header::fileNameCount)>(nameEntries.size()),
            .dataEntryCount = static_cast<decltype(MDB::Header::dataEntryCount)>(dataEntries.size()),
            .dataStart      = static_cast<decltype(MDB::Header::dataStart)>(dataStart),
            .totalSize      = static_cast<decltype(MDB::Header::totalSize)>(dataStart + offset),
        };

        output.write(reinterpret_cast<char*>(&header), headerSize);
        output.write(reinterpret_cast<char*>(treeEntries.data()), treeEntries.size() * sizeof(typename MDB::TreeEntry));
        output.write(reinterpret_cast<char*>(nameEntries.data()), nameEntries.size() * sizeof(typename MDB::NameEntry));
        output.write(reinterpret_cast<char*>(dataEntries.data()), dataEntries.size() * sizeof(typename MDB::DataEntry));
    }
} // namespace dscstools::mdb1new