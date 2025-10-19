#pragma once
#include "Helpers.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <optional>
#include <ranges>
#include <string>
#include <variant>
#include <vector>

namespace dscstools::expa
{
    /**
     * Represents the value of an EXPA entry.
     */
    using EntryValue =
        std::variant<bool, int8_t, int16_t, int32_t, float, std::string, std::vector<int32_t>, std::nullopt_t>;

    /**
     * Represents the available Entry types for EXPA tables
     */
    enum class EntryType : uint32_t
    {
        INT_ARRAY = 0,
        UNK1      = 1,
        INT32     = 2,
        INT16     = 3,
        INT8      = 4,
        FLOAT     = 5,
        STRING3   = 6, // ?
        STRING    = 7, // ?
        STRING2   = 8, // ?
        BOOL      = 9,
        EMPTY     = 10,
    };

    /**
     * Represents a CHNKEntry for an EXPA file.
     */
    struct CHNKEntry
    {
        uint32_t offset;
        std::vector<char> value;

        CHNKEntry(uint32_t offset, const std::string& data);
        CHNKEntry(uint32_t offset, const std::vector<int32_t>& data);
    };

    /**
     * Represents an entry in an EXPA table, containing the binary representation of the data as well as any potentially
     * associated CHNKEntries
     */
    struct EXPAEntry
    {
        std::vector<char> data;
        std::vector<CHNKEntry> chunk;
    };

    /**
     * Represents an entry in a structure, consisting of name and type
     */
    struct StructureEntry
    {
        std::string name;
        EntryType type;
    };

    /**
     * Represents the structure of a data table.
     */
    class Structure
    {
    private:
        std::vector<StructureEntry> structure;

    public:
        explicit Structure(std::vector<StructureEntry> structure);

        /**
         * Get the structure entries.
         */
        [[nodiscard]] auto getStructure() const -> std::vector<StructureEntry>;

        /**
         * Returns the number of entries the structure has.
         */
        [[nodiscard]] auto getEntryCount() const -> size_t;

        /**
         * Convert a vector of entry values, representing a row of this structure, into an EXPAEntry.
         */
        [[nodiscard]] auto writeEXPA(const std::vector<EntryValue>& entries) const -> EXPAEntry;

        /**
         * Read a row of entry values from a raw buffer. The caller must make sure there is enough data to read.
         */
        [[nodiscard]] auto readEXPA(const char* data) const -> std::vector<EntryValue>;

        /**
         * Gets the size of an entry of this structure when written in the EXPA format.
         */
        [[nodiscard]] auto getEXPASize() const -> uint32_t;

        /**
         * Get the CSV header row of this structure.
         */
        [[nodiscard]] auto getCSVHeader() const -> std::string;

        /**
         * Convert a vector of entry values, representing a row of this structure, into a CSV compatible string.
         */
        [[nodiscard]] auto writeCSV(const std::vector<EntryValue>& entries) const -> std::string;

        /**
         * Convert a vector of strings into a vector of entry values, representing a row of this structure.
         */
        [[nodiscard]] auto readCSV(const std::vector<std::string>& data) const -> std::vector<EntryValue>;
    };

    /**
     * Represents a structured data table, which contains a set of entries.
     */
    struct Table
    {
        std::string name;
        Structure structure;
        std::vector<std::vector<EntryValue>> entries;
    };

    /**
     * Represents a file of multiple tables.
     */
    struct TableFile
    {
        std::vector<Table> tables;
    };

    /**
     * Represents an EXPA implementation, detailing all the static functions and data needed to use this module.
     */
    template<typename T>
    concept EXPA = requires(std::ifstream& stream, const std::filesystem::path& path, const std::string& tableName) {
        /**
         * The alignment size of the EXPA.
         */
        { T::ALIGN_STEP } -> std::convertible_to<size_t>;
        /**
         * Whether the EXPA contains a structure section.
         */
        { T::HAS_STRUCTURE_SECTION } -> std::convertible_to<bool>;
        /**
         * Read the structure from the stream and lookup the structure file and return it or an empty structure if
         * nothing was found.
         */
        { T::getStructure(stream, path, tableName) } -> std::same_as<Structure>;
    };

    // See EXPA concept for details
    struct EXPA32
    {
        static constexpr auto ALIGN_STEP            = 4;
        static constexpr auto HAS_STRUCTURE_SECTION = false;

        static auto getStructure(std::ifstream& stream,
                                 const std::filesystem::path& filePath,
                                 const std::string& tableName) -> Structure;
    };

    // See EXPA concept for details
    struct EXPA64
    {
        static constexpr auto ALIGN_STEP            = 8;
        static constexpr auto HAS_STRUCTURE_SECTION = true;

        static auto getStructure(std::ifstream& stream,
                                 const std::filesystem::path& filePath,
                                 const std::string& tableName) -> Structure;
    };

    /**
     * Write a table file as EXPA into the given path
     *
     * @param file the table file to write
     * @param path the path to write to
     * @return void if successful, an error string otherwise
     */
    template<EXPA expa>
    auto writeEXPA(const TableFile& file, const std::filesystem::path& path) -> std::expected<void, std::string>;

    /**
     * Reads an EXPA file into a table file.
     *
     * @param path the path to read from
     * @return the table file if successful, an error string otherwise
     */
    template<EXPA expa>
    auto readEXPA(const std::filesystem::path& path) -> std::expected<TableFile, std::string>;

    /**
     * Write a table file as CSV into the given path
     *
     * @param file the table file to write
     * @param path the path to write to
     * @return void if successful, an error string otherwise
     */
    auto exportCSV(const TableFile& file, const std::filesystem::path& target) -> std::expected<void, std::string>;

    /**
     * Reads an CSV folder into a table file.
     *
     * @param path the path to read from
     * @return the table file if successful, an error string otherwise
     */
    auto importCSV(const std::filesystem::path& source) -> std::expected<TableFile, std::string>;
} // namespace dscstools::expa

namespace dscstools::expa::detail
{
    constexpr auto EXPA_MAGIC = 0x41505845;
    constexpr auto CHNK_MAGIC = 0x4B4E4843;

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
} // namespace dscstools::expa::detail

// implementation
namespace dscstools::expa
{
    using namespace detail;

    template<EXPA expa>
    auto writeEXPA(const TableFile& file, const std::filesystem::path& path) -> std::expected<void, std::string>
    {
        if (std::filesystem::exists(path) && !std::filesystem::is_regular_file(path))
            return std::unexpected("Target path already exists and is not a file.");
        if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());

        std::ofstream stream(path, std::ios::out | std::ios::binary);

        if (!stream) return std::unexpected("Failed to write target file.");

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
                for (const auto& entry : structure.getStructure())
                    write(stream, entry.type);
            }

            write(stream, structureSize);
            write(stream, static_cast<uint32_t>(table.entries.size()));

            stream.seekp(ceilInteger<8>(stream.tellp()), std::ios::beg);

            for (const auto& entry : table.entries)
            {
                auto start  = stream.tellp();
                auto result = structure.writeEXPA(entry);
                write(stream, result.data);

                auto lambda = [=](CHNKEntry& val)
                {
                    val.offset += static_cast<uint32_t>(start);
                    return val;
                };

                chnk.append_range(std::ranges::views::transform(result.chunk, lambda));
            }
        }

        write(stream, CHNK_MAGIC);
        write(stream, static_cast<uint32_t>(chnk.size()));
        for (const auto& entry : chnk)
        {
            write(stream, entry.offset);
            write(stream, static_cast<uint32_t>(entry.value.size()));
            write(stream, entry.value);
        }

        return {};
    }

    template<EXPA expa>
    auto readEXPA(const std::filesystem::path& path) -> std::expected<TableFile, std::string>
    {
        struct TableEntry
        {
            std::string name;
            size_t dataOffset{};
            uint32_t entryCount{};
            uint32_t entrySize{};
            Structure structure;
        };

        if (!std::filesystem::exists(path)) return std::unexpected("Source path does not exist.");
        if (!std::filesystem::is_regular_file(path)) return std::unexpected("Source path does not lead to a file.");

        std::ifstream stream(path, std::ios::in | std::ios::binary);

        if (!stream) return std::unexpected("Failed to read source file.");

        std::vector<char> content(std::filesystem::file_size(path));
        stream.read(content.data(), content.size());
        stream.seekg(std::ios::beg);

        const auto header = read<EXPAHeader>(stream);
        if (header.magic != EXPA_MAGIC) return std::unexpected("Source file lacks EXPA header.");

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
            if (structureSize != ceilInteger<8>(entrySize))
            {
                return std::unexpected(
                    std::format("Structure size {} doesn't match entry size {}.", structureSize, entrySize));
            }
        }

        alignStream<expa::ALIGN_STEP>(stream);

        const auto chunkHeader = read<CHNKHeader>(stream);
        if (chunkHeader.magic != CHNK_MAGIC) return std::unexpected("Source file lacks CHNK header.");

        for (uint32_t i = 0; i < chunkHeader.numEntry; i++)
        {
            auto offset = read<uint32_t>(stream);
            auto size   = read<uint32_t>(stream);
            auto ptr    = reinterpret_cast<uint64_t>(content.data() + stream.tellg());
            *reinterpret_cast<uint64_t*>(content.data() + offset) = ptr;
            stream.seekg(size, std::ios::cur);
        }

        std::vector<Table> finalTable;
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

        return TableFile{finalTable};
    }
} // namespace dscstools::expa