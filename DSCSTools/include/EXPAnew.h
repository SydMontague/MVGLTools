#pragma once
#include "Helpers.h"

#include <expected>
#include <filesystem>
#include <fstream>
#include <variant>

namespace dscstools::expa
{
    // forward declarations
    struct CHNKEntry;

    using EntryValue =
        std::variant<bool, int8_t, int16_t, int32_t, float, std::string, std::vector<int32_t>, std::nullopt_t>;

    enum class EntryType : uint32_t
    {
        UNK0    = 0,
        UNK1    = 1,
        INT32   = 2,
        INT16   = 3,
        INT8    = 4,
        FLOAT   = 5,
        STRING3 = 6, // ?
        STRING  = 7, // ?
        STRING2 = 8, // ?
        BOOL    = 9,
        EMPTY   = 10,

        // int array, not present in DSTS?
        INT_ARRAY = 100,
    };

    struct StructureEntry
    {
        std::string name;
        EntryType type;
    };

    struct Structure
    {
        std::vector<StructureEntry> structure;

        std::vector<CHNKEntry>
        writeEXPA(uint32_t base_offset, char* data, const std::vector<EntryValue>& entries) const;

        std::vector<EntryValue> readEXPA(const char* data) const;

        uint32_t getEXPASize() const;

        size_t getEntryCount() const;
    };

    struct FinalTable
    {
        std::string name;
        Structure structure;
        std::vector<std::vector<EntryValue>> entries;
    };

    struct FinalFile
    {
        std::vector<FinalTable> tables;
    };

    constexpr auto EXPA_MAGIC = 0x41505845;
    constexpr auto CHNK_MAGIC = 0x4B4E4843;

    struct CHNKEntry
    {
        uint32_t offset;
        std::vector<char> value;

        CHNKEntry(uint32_t offset, const std::string& data);
        CHNKEntry(uint32_t offset, const std::vector<int32_t>& data);
    };

    struct EXPAHeader
    {
        uint32_t magic{EXPA_MAGIC};
        int32_t tableCount{0};
    };

    struct CHNKHeader
    {
        uint32_t magic{CHNK_MAGIC};
        uint32_t numEntry{0};
    };

    template<typename T>
    concept EXPA = requires(std::ifstream& stream, std::filesystem::path filePath, std::string tableName) {
        { T::ALIGN_STEP } -> std::convertible_to<size_t>;
        { T::HAS_STRUCTURE_SECTION } -> std::convertible_to<bool>;
        { T::getStructure(stream, filePath, tableName) } -> std::same_as<Structure>;
    };

    struct EXPA32
    {
        static constexpr auto ALIGN_STEP            = 4;
        static constexpr auto HAS_STRUCTURE_SECTION = false;

        static Structure getStructure(std::ifstream& stream, std::filesystem::path filePath, std::string tableName);
    };

    struct EXPA64
    {
        static constexpr auto ALIGN_STEP            = 8;
        static constexpr auto HAS_STRUCTURE_SECTION = true;

        static Structure getStructure(std::ifstream& stream, std::filesystem::path filePath, std::string tableName);
    };

    template<EXPA expa>
    void writeEXPA(const FinalFile& file, std::filesystem::path path);

    template<EXPA expa>
    std::expected<FinalFile, std::string> readEXPA(std::filesystem::path path);
} // namespace dscstools::expa


// implementation
namespace dscstools::expa
{
    template<EXPA expa>
    void writeEXPA(const FinalFile& file, std::filesystem::path path)
    {
        std::ofstream stream(path, std::ios::binary);
        std::vector<CHNKEntry> chnk;

        write(stream, EXPA_MAGIC);
        write(stream, static_cast<uint32_t>(file.tables.size()));

        for (const auto& table : file.tables)
        {
            const auto& structure    = table.structure;
            const auto nameSize      = ceilInteger<4>(table.name.size() + 1);
            auto structureSize       = structure.getEXPASize();
            auto actualStructureSize = ceilInteger<8>(structureSize);
            write(stream, nameSize);
            write(stream, table.name, nameSize);

            if constexpr (expa::HAS_STRUCTURE_SECTION)
            {
                write(stream, static_cast<uint32_t>(structure.getEntryCount()));
                for (const auto& entry : structure.structure)
                    write(stream, entry.type);
            }

            write(stream, structureSize);
            write(stream, static_cast<uint32_t>(table.entries.size()));

            stream.seekp(ceilInteger<8>(stream.tellp()), std::ios::beg);

            for (const auto& entry : table.entries)
            {
                std::vector<char> data(actualStructureSize, 0xCC);
                chnk.append_range(structure.writeEXPA(static_cast<uint32_t>(stream.tellp()), data.data(), entry));
                stream.write(data.data(), data.size());
            }
        }

        write(stream, CHNK_MAGIC);
        write(stream, static_cast<uint32_t>(chnk.size()));
        for (const auto& entry : chnk)
        {
            write(stream, entry.offset);
            write(stream, static_cast<uint32_t>(entry.value.size()));
            write(stream, entry.value.data(), entry.value.size());
        }
    }

    template<EXPA expa>
    std::expected<FinalFile, std::string> readEXPA(std::filesystem::path path)
    {
        struct TableEntry
        {
            std::string name;
            size_t dataOffset;
            uint32_t entryCount;
            uint32_t entrySize;
            Structure structure;
        };

        std::ifstream stream(path);

        std::vector<char> content(std::filesystem::file_size(path));
        stream.read(content.data(), content.size());
        stream.seekg(std::ios::beg);

        const auto header = read<EXPAHeader>(stream);
        if (header.magic != EXPA_MAGIC) return std::unexpected("Given file lacks EXPA header.");

        std::vector<TableEntry> tables;

        for (int32_t i = 0; i < header.tableCount; i++)
        {
            alignStream<expa::ALIGN_STEP>(stream);

            auto nameLength = read<uint32_t>(stream);
            std::vector<char> nameData(nameLength);
            stream.read(nameData.data(), nameData.size());
            std::string name(nameData.data());

            Structure structure = expa::getStructure(stream, path, name);
            auto entrySize      = read<uint32_t>(stream);
            auto entryCount     = read<uint32_t>(stream);

            alignStream<8>(stream);
            tables.emplace_back(name, stream.tellg(), entryCount, entrySize, structure);
            stream.seekg(entryCount * ceilInteger<8>(entrySize), std::ios::cur);

            auto structureSize = structure.getEXPASize();
            if (structureSize != entrySize) return std::unexpected("Structure size doesn't match entry size.");
        }

        alignStream<expa::ALIGN_STEP>(stream);

        const auto chunkHeader = read<CHNKHeader>(stream);
        if (chunkHeader.magic != CHNK_MAGIC) return std::unexpected("Given file lacks CHNK header.");

        for (uint32_t i = 0; i < chunkHeader.numEntry; i++)
        {
            uint32_t offset = read<uint32_t>(stream);
            uint32_t size   = read<uint32_t>(stream);
            uint64_t ptr    = reinterpret_cast<uint64_t>(content.data() + stream.tellg());
            *reinterpret_cast<uint64_t*>(content.data() + offset) = ptr;
            stream.seekg(size, std::ios::cur);
        }

        std::vector<FinalTable> finalTable;
        for (const auto& table : tables)
        {
            const auto increase = ceilInteger<8>(table.entrySize);
            auto offset         = table.dataOffset;
            std::vector<std::vector<EntryValue>> values;

            for (uint32_t i = 0; i < table.entryCount; i++)
            {
                values.push_back(table.structure.readEXPA(content.data() + offset));
                offset += increase;
            }

            finalTable.emplace_back(table.name, table.structure, values);
        }

        return FinalFile{finalTable};
    }
} // namespace dscstools::expa