#pragma once
#include "Helpers.h"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/regex.hpp>
#include <boost/regex/v5/regex_fwd.hpp>
#include <boost/regex/v5/regex_search.hpp>
#include <parser.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace mvgltools::expa
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
        INT32_ARRAY = 0,
        UNK1        = 1,
        INT32       = 2,
        INT16       = 3,
        INT8        = 4,
        FLOAT       = 5,
        STRING3     = 6, // ?
        STRING      = 7, // ?
        STRING2     = 8, // ?
        BOOL        = 9,
        EMPTY       = 10,
    };

    /**
     * Represents a CHNKEntry for an EXPA file.
     */
    struct CHNKEntry
    {
        uint32_t offset;         // NOLINT(misc-non-private-member-variables-in-classes)
        std::vector<char> value; // NOLINT(misc-non-private-member-variables-in-classes)

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
     * Represents an EXPA implementation, detailing all the data needed to use this module.
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
         * The path where the structure files for this implementation are located.
         */
        { T::STRUCTURE_FOLDER } -> std::convertible_to<std::string_view>;
    };

    // See EXPA concept for details
    struct DSCS
    {
        static constexpr auto ALIGN_STEP            = 4;
        static constexpr auto HAS_STRUCTURE_SECTION = false;
        static constexpr auto STRUCTURE_FOLDER      = "structures/dscs/";
    };

    // See EXPA concept for details
    struct ChaosRings3
    {
        static constexpr auto ALIGN_STEP            = 4;
        static constexpr auto HAS_STRUCTURE_SECTION = false;
        static constexpr auto STRUCTURE_FOLDER      = "structures/chaos-rings-3/";
    };

    // See EXPA concept for details
    struct DSTS
    {
        static constexpr auto ALIGN_STEP            = 8;
        static constexpr auto HAS_STRUCTURE_SECTION = true;
        static constexpr auto STRUCTURE_FOLDER      = "structures/dsts/";
    };

    // See EXPA concept for details
    struct THL
    {
        static constexpr auto ALIGN_STEP            = 8;
        static constexpr auto HAS_STRUCTURE_SECTION = true;
        static constexpr auto STRUCTURE_FOLDER      = "structures/tlh/";
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
    template<EXPA expa>
    auto importCSV(const std::filesystem::path& source) -> std::expected<TableFile, std::string>;
} // namespace mvgltools::expa

namespace mvgltools::expa::detail
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

    struct CSVFile
    {
    private:
        std::vector<std::string> header;
        std::vector<std::vector<std::string>> rows;

    public:
        explicit CSVFile(const std::filesystem::path& path)
        {
            std::ifstream stream(path, std::ios::in);
            aria::csv::CsvParser parser(stream);

            for (const auto& row : parser)
            {
                std::vector<std::string> data;
                for (const auto& field : row)
                    data.push_back(field);

                if (header.empty())
                    header = data;
                else
                    rows.push_back(data);
            }
        }

        [[nodiscard]] auto getHeader() const -> std::vector<std::string> { return header; }
        [[nodiscard]] auto getRows() const -> std::vector<std::vector<std::string>> { return rows; }
    };

    inline auto getTypeMap() -> std::map<std::string, EntryType>
    {
        std::map<std::string, EntryType> map;
        map["byte"]      = EntryType::INT8;
        map["short"]     = EntryType::INT16;
        map["int"]       = EntryType::INT32;
        map["int array"] = EntryType::INT32_ARRAY;
        map["float"]     = EntryType::FLOAT;

        map["int8"]        = EntryType::INT8;
        map["int16"]       = EntryType::INT16;
        map["int32"]       = EntryType::INT32;
        map["float"]       = EntryType::FLOAT;
        map["bool"]        = EntryType::BOOL;
        map["empty"]       = EntryType::EMPTY;
        map["string"]      = EntryType::STRING;
        map["string2"]     = EntryType::STRING2;
        map["string3"]     = EntryType::STRING3;
        map["int32 array"] = EntryType::INT32_ARRAY;

        return map;
    }

    inline auto convertEntryType(const std::string& val) -> EntryType
    {
        static const std::map<std::string, EntryType> map = getTypeMap();
        return map.contains(val) ? map.at(val) : EntryType::EMPTY;
    }

    inline auto getCSVStructure(const CSVFile& csv) -> std::vector<StructureEntry>
    {
        auto lambda = [](const auto& val)
        { return StructureEntry{val, convertEntryType(val.substr(0, val.find_last_of(" ")))}; };

        return csv.getHeader() | std::views::transform(lambda) | std::ranges::to<std::vector<StructureEntry>>();
    }

    constexpr auto toString(EntryType type) -> std::string
    {
        switch (type)
        {
            case EntryType::UNK1: return "unk1";
            case EntryType::INT32: return "int32";
            case EntryType::INT16: return "int16";
            case EntryType::INT8: return "int8";
            case EntryType::FLOAT: return "float";
            case EntryType::STRING3: return "string3";
            case EntryType::STRING: return "string";
            case EntryType::STRING2: return "string2";
            case EntryType::BOOL: return "bool";
            case EntryType::EMPTY: return "empty";
            case EntryType::INT32_ARRAY: return "int32 array";
            default: return "invalid";
        }
    }

    template<EXPA expa>
    auto getStructureFromFile(const std::filesystem::path& filePath, const std::string& tableName)
        -> std::vector<StructureEntry>
    {
        std::string STRUCTURE_FILE = std::string(expa::STRUCTURE_FOLDER) + "structure.json";

        if (!std::filesystem::is_directory(expa::STRUCTURE_FOLDER)) return {};
        if (!std::filesystem::exists(STRUCTURE_FILE)) return {};

        boost::property_tree::ptree structure;
        boost::property_tree::read_json(STRUCTURE_FILE, structure);

        std::string formatFile;
        for (auto var : structure)
        {
            if (boost::regex_search(filePath.string(), boost::regex{var.first}))
            {
                formatFile = var.second.data();
                break;
            }
        }

        if (formatFile.empty()) return {};

        boost::property_tree::ptree format;
        boost::property_tree::read_json(expa::STRUCTURE_FOLDER + formatFile, format);

        auto formatValue = format.get_child_optional(tableName);
        if (!formatValue)
        {
            // Scan all table definitions to find a matching regex expression, if any
            for (auto& kv : format)
            {
                if (boost::regex_search(tableName, boost::regex{wrapRegex(kv.first)}))
                {
                    formatValue = kv.second;
                    break;
                }
            }
        }
        if (!formatValue) return {};

        std::vector<StructureEntry> entries;
        for (const auto& val : formatValue.get())
            entries.emplace_back(val.first, convertEntryType(val.second.data()));

        return entries;
    }

    template<EXPA expa>
    auto getStructure(std::ifstream& stream, const std::filesystem::path& filePath, const std::string& tableName)
        -> Structure
    {
        auto fromFile = getStructureFromFile<expa>(filePath, tableName);
        if constexpr (!expa::HAS_STRUCTURE_SECTION) return Structure{fromFile};

        std::vector<StructureEntry> structure;
        auto structureCount = read<uint32_t>(stream);
        for (int32_t j = 0; j < structureCount; j++)
        {
            auto type = read<EntryType>(stream);
            structure.emplace_back(std::format("{} {}", toString(type), j), type);
        }

        if (fromFile.empty()) return Structure{structure};
        if (fromFile.size() != structureCount) return Structure{structure};

        auto lambda   = [](const auto& val) { return std::get<0>(val).type != std::get<1>(val).type; };
        auto mismatch = std::ranges::any_of(std::ranges::views::zip(structure, fromFile), lambda);
        if (mismatch) return Structure{structure};

        return Structure{fromFile};
    }

    template<EXPA expa>
    auto getStructureCSV(const CSVFile& csv, const std::filesystem::path& filePath, const std::string& tableName)
        -> Structure
    {
        auto structure = getCSVStructure(csv);

        auto fromFile = getStructureFromFile<expa>(filePath, tableName);
        if (fromFile.empty()) return Structure{structure};
        if (fromFile.size() != structure.size()) return Structure{structure};

        // file has priority over header, as header might resolve to EMPTY
        return Structure{fromFile};
    }

} // namespace mvgltools::expa::detail

// implementation
namespace mvgltools::expa
{
    using namespace detail;

    template<EXPA expa>
    auto importCSV(const std::filesystem::path& source) -> std::expected<TableFile, std::string>
    {
        if (!std::filesystem::exists(source) || !std::filesystem::is_directory(source))
            return std::unexpected("Source path doesn't exist or is not a directory.");

        const std::filesystem::directory_iterator itr(source);
        std::vector<std::filesystem::path> files;
        for (const auto& val : itr)
            if (val.is_regular_file()) files.push_back(val);
        std::ranges::sort(files);

        std::vector<Table> tables;
        for (const auto& file : files)
        {
            const CSVFile csv(file);

            auto name      = file.stem().generic_string().substr(4);
            auto structure = getStructureCSV<expa>(csv, source, name);
            auto entries   = csv.getRows() |
                           std::views::transform([&](const auto& val) { return structure.readCSV(val); }) |
                           std::ranges::to<std::vector<std::vector<EntryValue>>>();

            tables.emplace_back(name, structure, entries);
        }

        return TableFile{tables};
    }

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
            const auto nameSize      = ceilInteger(static_cast<int64_t>(table.name.size() + 1), 4);
            auto structureSize       = structure.getEXPASize();
            auto actualStructureSize = ceilInteger(structureSize, 8);
            write(stream, static_cast<int32_t>(nameSize));
            write(stream, table.name, nameSize);

            if constexpr (expa::HAS_STRUCTURE_SECTION)
            {
                write(stream, static_cast<uint32_t>(structure.getEntryCount()));
                for (const auto& entry : structure.getStructure())
                    write(stream, entry.type);
            }

            write(stream, structureSize);
            write(stream, static_cast<uint32_t>(table.entries.size()));

            stream.seekp(ceilInteger(stream.tellp(), 8), std::ios::beg);

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
        stream.read(content.data(), static_cast<std::streamsize>(content.size()));
        stream.seekg(std::ios::beg);

        const auto header = read<EXPAHeader>(stream);
        if (header.magic != EXPA_MAGIC) return std::unexpected("Source file lacks EXPA header.");

        std::vector<TableEntry> tables;

        for (int32_t i = 0; i < header.tableCount; i++)
        {
            alignStream<expa::ALIGN_STEP>(stream);

            auto nameLength = read<uint32_t>(stream);
            std::vector<char> nameData(nameLength);
            stream.read(nameData.data(), static_cast<std::streamsize>(nameData.size()));
            std::string name(nameData.data());

            Structure structure = getStructure<expa>(stream, path, name);
            auto entrySize      = read<uint32_t>(stream);
            auto entryCount     = read<uint32_t>(stream);

            alignStream<8>(stream);
            tables.emplace_back(name, stream.tellg(), entryCount, entrySize, structure);
            stream.seekg(entryCount * ceilInteger(entrySize, 8), std::ios::cur);

            auto structureSize = structure.getEXPASize();
            if (structureSize != ceilInteger(entrySize, 8))
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
            const auto increase = ceilInteger(table.entrySize, 8);
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
} // namespace mvgltools::expa