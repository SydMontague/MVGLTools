#include "EXPAnew.h"

#include "../libs/csv-parser/parser.hpp"

#include "Helpers.h"
#include "boost/property_tree/json_parser.hpp"

#include <boost/regex.hpp>

#include <algorithm>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace
{
    using namespace dscstools;
    using namespace dscstools::expa;

    constexpr auto STRUCTURE_FOLDER = "structures/";
    constexpr auto STRUCTURE_FILE   = "structures/structure.json";

    struct CSVFile
    {
        std::vector<std::string> header;
        std::vector<std::vector<std::string>> rows;

        CSVFile(std::filesystem::path path)
        {
            std::ifstream stream(path);
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
    };

    constexpr EntryType convertEntryType(std::string val)
    {
        std::map<std::string, EntryType> map;
        map["byte"]      = EntryType::INT8;
        map["short"]     = EntryType::INT16;
        map["int"]       = EntryType::INT32;
        map["int array"] = EntryType::INT_ARRAY;
        map["float"]     = EntryType::FLOAT;

        map["int8"]    = EntryType::INT8;
        map["int16"]   = EntryType::INT16;
        map["int32"]   = EntryType::INT32;
        map["float"]   = EntryType::FLOAT;
        map["bool"]    = EntryType::BOOL;
        map["empty"]   = EntryType::EMPTY;
        map["string"]  = EntryType::STRING;
        map["string2"] = EntryType::STRING2;
        map["string3"] = EntryType::STRING3;

        return map.contains(val) ? map.at(val) : EntryType::EMPTY;
    }

    constexpr std::string toString(EntryType type)
    {
        switch (type)
        {
            case EntryType::UNK0: return "unk0";
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
            case EntryType::INT_ARRAY: return "int array";
            default: return "invalid";
        }
    }

    constexpr uint32_t getAlignment(EntryType type)
    {
        switch (type)
        {
            case EntryType::UNK0: return 0;
            case EntryType::UNK1: return 0;
            case EntryType::INT32: return 4;
            case EntryType::INT16: return 2;
            case EntryType::INT8: return 1;
            case EntryType::FLOAT: return 4;
            case EntryType::STRING3: return 8;
            case EntryType::STRING: return 8;
            case EntryType::STRING2: return 8;
            case EntryType::BOOL: return 4;
            case EntryType::EMPTY: return 0;
            case EntryType::INT_ARRAY: return 8;
            default: return 0;
        }
    }

    constexpr uint32_t getSize(EntryType type)
    {
        switch (type)
        {
            case EntryType::UNK0: return 0;
            case EntryType::UNK1: return 0;
            case EntryType::INT32: return 4;
            case EntryType::INT16: return 2;
            case EntryType::INT8: return 1;
            case EntryType::FLOAT: return 4;
            case EntryType::STRING3: return 8;
            case EntryType::STRING: return 8;
            case EntryType::STRING2: return 8;
            case EntryType::BOOL: return 4;
            case EntryType::EMPTY: return 0;
            case EntryType::INT_ARRAY: return 16;
            default: return 0;
        }
    }

    std::string getCSVString(const EntryType& type, const EntryValue& value)
    {
        switch (type)
        {
            case EntryType::INT32: return std::format("{}", std::get<int32_t>(value));
            case EntryType::INT16: return std::format("{}", std::get<int16_t>(value));
            case EntryType::INT8: return std::format("{}", std::get<int8_t>(value));
            case EntryType::FLOAT: return std::format("{}", std::get<float>(value));
            case EntryType::BOOL: return std::format("{}", std::get<bool>(value));

            case EntryType::STRING3: [[fallthrough]];
            case EntryType::STRING: [[fallthrough]];
            case EntryType::STRING2:
            {
                std::stringstream sstream;
                sstream << std::quoted(std::get<std::string>(value), '\"', '\"');
                return sstream.str();
            }
            case EntryType::INT_ARRAY:
            {
                auto data = std::get<std::vector<int32_t>>(value) |
                            std::views::transform([](auto val) { return std::to_string(val); });
                return std::views::join_with(data, ' ') | std::ranges::to<std::string>();
            }
            case EntryType::EMPTY: [[fallthrough]];
            case EntryType::UNK0: [[fallthrough]];
            case EntryType::UNK1: [[fallthrough]];
            default: return "";
        }
    }

    EntryValue getCSVValue(const EntryType& type, const std::string& value)
    {
        switch (type)
        {
            default:
            case EntryType::UNK0: [[fallthrough]];
            case EntryType::UNK1: [[fallthrough]];
            case EntryType::EMPTY: return std::nullopt;

            case EntryType::INT32: return std::stoi(value);
            case EntryType::INT16: return static_cast<int16_t>(std::stoi(value));
            case EntryType::INT8: return static_cast<int8_t>(std::stoi(value));
            case EntryType::FLOAT: return std::stof(value);

            case EntryType::STRING3: [[fallthrough]];
            case EntryType::STRING: [[fallthrough]];
            case EntryType::STRING2: return value;

            case EntryType::BOOL: return value == "true";
            case EntryType::INT_ARRAY:
                return value | std::views::split(' ') | std::ranges::to<std::vector<std::string>>() |
                       std::views::transform([](const auto& val) { return std::stoi(val); }) |
                       std::ranges::to<std::vector<int32_t>>();
        }
    }

    std::vector<StructureEntry> getStructureFromFile(std::filesystem::path filePath, std::string tableName)
    {
        if (!std::filesystem::is_directory(STRUCTURE_FOLDER)) return {};
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
        boost::property_tree::read_json("structures/" + formatFile, format);

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

    std::optional<CHNKEntry> writeEXPAEntry(size_t base_offset, char* data, EntryType type, const EntryValue& value)
    {
        switch (type)
        {
            case EntryType::INT32: *reinterpret_cast<int32_t*>(data) = get<int32_t>(value); break;
            case EntryType::INT16: *reinterpret_cast<int16_t*>(data) = get<int16_t>(value); break;
            case EntryType::INT8: *reinterpret_cast<int8_t*>(data) = get<int8_t>(value); break;
            case EntryType::FLOAT: *reinterpret_cast<float*>(data) = get<float>(value); break;

            case EntryType::STRING3: [[fallthrough]];
            case EntryType::STRING: [[fallthrough]];
            case EntryType::STRING2:
            {
                *reinterpret_cast<uint64_t*>(data) = 0;
                auto str                           = get<std::string>(value);
                if (!str.empty()) return CHNKEntry(base_offset, str);
                break;
            }
            case EntryType::INT_ARRAY:
            {
                auto array                             = get<std::vector<int32_t>>(value);
                *reinterpret_cast<uint32_t*>(data)     = static_cast<int32_t>(array.size());
                *reinterpret_cast<uint64_t*>(data + 8) = 0;
                if (!array.empty()) return CHNKEntry(base_offset + 8, array);
                break;
            }

            case EntryType::EMPTY: [[fallthrough]];
            case EntryType::BOOL: [[fallthrough]];
            case EntryType::UNK0: [[fallthrough]];
            case EntryType::UNK1: [[fallthrough]];
            default: break;
        }
        return std::nullopt;
    }

    EntryValue readEXPAEntry(EntryType type, const char* data, int32_t bitCounter)
    {
        switch (type)
        {
            default:
            case EntryType::UNK0: [[fallthrough]];
            case EntryType::UNK1: [[fallthrough]];
            case EntryType::EMPTY: return std::nullopt;

            case EntryType::INT32: return *reinterpret_cast<const int32_t*>(data);
            case EntryType::INT16: return *reinterpret_cast<const int16_t*>(data);
            case EntryType::INT8: return *reinterpret_cast<const int8_t*>(data);
            case EntryType::FLOAT: return *reinterpret_cast<const float*>(data);
            case EntryType::STRING3: [[fallthrough]];
            case EntryType::STRING: [[fallthrough]];
            case EntryType::STRING2:
            {
                auto ptr = *reinterpret_cast<char* const*>(data);
                return ptr ? std::string(ptr) : "";
            }
            case EntryType::BOOL: return ((*reinterpret_cast<const int32_t*>(data) >> bitCounter) & 1) == 1;
            case EntryType::INT_ARRAY:
            {
                auto count = *reinterpret_cast<const int32_t*>(data);
                auto ptr   = *reinterpret_cast<int32_t* const*>(data + 8);
                std::vector<int32_t> values;
                for (int32_t i = 0; i < count; i++)
                    values.push_back(ptr[i]);
                return values;
            }
        }
    }

    std::vector<StructureEntry> getCSVStructure(const CSVFile& csv)
    {
        auto lambda = [](const auto& val)
        { return StructureEntry{val, convertEntryType(val.substr(0, val.find_last_of(" ")))}; };

        return csv.header | std::views::transform(lambda) | std::ranges::to<std::vector<StructureEntry>>();
    }

    Structure getStructureCSV(const CSVFile& csv, std::filesystem::path filePath, const std::string& tableName)
    {
        auto structure = getCSVStructure(csv);

        auto fromFile = getStructureFromFile(filePath, tableName);
        if (fromFile.empty()) return {structure};
        if (fromFile.size() != structure.size()) return {structure};

        // file has priority over header, as header might resolve to EMPTY
        return {fromFile};
    }
} // namespace

namespace dscstools::expa
{

    void bla()
    {
        constexpr auto path1 =
            "/home/syd/Development/MyRepos/DSCSTools/build/DSCSToolsCLI/DSTS/data/field_npc_t0303.mbe";
        constexpr auto path2 =
            "/home/syd/Development/MyRepos/DSCSTools/build/DSCSToolsCLI/DSDBP/data/digimon_common_para.mbe";

        // std::filesystem::recursive_directory_iterator itr(
        //     "/home/syd/Development/MyRepos/DSCSTools/build/DSCSToolsCLI/DSDB/data/");
        std::filesystem::recursive_directory_iterator itr(
            "/home/syd/Development/MyRepos/DSCSTools/build/DSCSToolsCLI/DSDB/data/");
        for (const auto& file : itr)
        {
            if (!std::filesystem::is_regular_file(file)) continue;

            std::cout << "Extract " << file.path().filename() << "\n";

            auto result = readEXPA<EXPA32>(file);
            if (result)
            {
                std::ignore = exportCSV(result.value(), std::format("mbe32/{}/", file.path().filename().string()));
            }
            else
                std::cout << result.error() << "\n";
        }

        std::filesystem::directory_iterator itr2("mbe32/");
        for (const auto& file : itr2)
        {
            std::cout << "Rebuild " << file.path().filename() << "\n";

            auto result = importCSV(file);
            if (result) { std::ignore = writeEXPA<EXPA32>(result.value(), "expa32/" / file.path().filename()); }
            else
                std::cout << result.error() << "\n";
        }

        // auto result = readEXPA<EXPA64>(path1);

        // writeEXPA<EXPA64>(result.value(), "expa64.mbe");
        //  std::cout << result.error_or("Success") << "\n";
        //  result = readEXPA<EXPA32>(path2);
        //  writeEXPA<EXPA32>(result.value(), "expa32.mbe");
        //  std::cout << result.error_or("Success") << "\n";
    }

    EXPAEntry Structure::writeEXPA(const std::vector<EntryValue>& entries) const
    {
        auto offset     = 0;
        auto bitCounter = 0;
        std::bitset<32> currentBool;
        std::vector<CHNKEntry> chunkEntries;
        std::vector<char> new_data(getEXPASize(), 0xCC);

        for (const auto& val : std::views::zip(structure, entries))
        {
            auto type  = get<0>(val).type;
            auto entry = get<1>(val);

            if (type != EntryType::BOOL || bitCounter >= 32)
            {
                if (bitCounter > 0)
                {
                    *reinterpret_cast<uint32_t*>(new_data.data() + offset) = currentBool.to_ulong();
                    offset += sizeof(uint32_t);
                    bitCounter  = 0;
                    currentBool = {};
                }
                offset = ceilInteger(offset, getAlignment(type));
            }

            auto result = writeEXPAEntry(offset, new_data.data() + offset, type, entry);
            if (result) chunkEntries.push_back(result.value());

            if (type == EntryType::BOOL)
                currentBool.set(bitCounter++, get<bool>(entry));
            else
                offset += getSize(type);
        }

        if (bitCounter > 0)
        {
            *reinterpret_cast<uint32_t*>(new_data.data() + offset) = currentBool.to_ulong();
            offset += sizeof(uint32_t);
        }

        return {new_data, chunkEntries};
    }

    std::vector<EntryValue> Structure::readEXPA(const char* data) const
    {
        if (structure.empty()) return {};

        std::vector<EntryValue> values;
        auto offset     = 0;
        auto bitCounter = 0;

        for (const auto& val : structure)
        {
            if (val.type != EntryType::BOOL || bitCounter >= 32)
            {
                if (bitCounter > 0) offset += getSize(EntryType::BOOL);

                offset     = ceilInteger(offset, getAlignment(val.type));
                bitCounter = 0;
            }

            values.push_back(readEXPAEntry(val.type, data + offset, bitCounter));

            if (val.type == EntryType::BOOL)
                bitCounter++;
            else
                offset += getSize(val.type);
        }

        return values;
    }

    std::vector<EntryValue> Structure::readCSV(const std::vector<std::string>& data) const
    {
        return std::views::zip_transform([](const auto& val, const auto& val2) { return getCSVValue(val.type, val2); },
                                         structure,
                                         data) |
               std::ranges::to<std::vector<EntryValue>>();
    }

    std::string Structure::getCSVHeader() const
    {
        return structure | std::views::transform([](const auto& val) { return val.name; }) |
               std::views::join_with(',') | std::ranges::to<std::string>();
    }

    std::string Structure::writeCSV(const std::vector<EntryValue>& entries) const
    {
        std::stringstream stream;
        auto result = structure | std::views::transform([](const auto& val) { return val.name; }) |
                      std::views::join_with(',') | std::ranges::to<std::string>();
        stream << result << "\n";

        return std::views::zip_transform([](const auto& val, const auto& val2) { return getCSVString(val.type, val2); },
                                         structure,
                                         entries) |
               std::views::join_with(',') | std::ranges::to<std::string>();
    }

    uint32_t Structure::getEXPASize() const
    {
        if (structure.empty()) return 0;

        auto currentSize = 0;
        auto bitCounter  = 0;

        for (const auto& val : structure)
        {
            if (bitCounter == 0 || bitCounter >= 32 || val.type != EntryType::BOOL)
            {
                currentSize = ceilInteger(currentSize, getAlignment(val.type));
                bitCounter  = 0;
            }

            if (bitCounter == 0) currentSize += getSize(val.type);
            if (val.type == EntryType::BOOL) bitCounter++;
        }

        return ceilInteger(currentSize, 8);
    }

    size_t Structure::getEntryCount() const
    {
        return structure.size();
    }

    CHNKEntry::CHNKEntry(uint32_t offset, const std::string& data)
        : offset(offset)
    {
        value = std::vector<char>(ceilInteger<4>(data.size() + 2));
        std::copy(data.begin(), data.end(), value.begin());
    }

    CHNKEntry::CHNKEntry(uint32_t offset, const std::vector<int32_t>& data)
        : offset(offset)
    {
        value = std::vector<char>(data.size() * sizeof(int32_t));
        std::copy_n(reinterpret_cast<const char*>(data.data()), value.size(), value.begin());
    }

    Structure EXPA32::getStructure(std::ifstream& stream, std::filesystem::path filePath, std::string tableName)
    {
        return {getStructureFromFile(filePath, tableName)};
    }

    Structure EXPA64::getStructure(std::ifstream& stream, std::filesystem::path filePath, std::string tableName)
    {
        std::vector<StructureEntry> structure;
        auto structureCount = read<uint32_t>(stream);
        for (int32_t j = 0; j < structureCount; j++)
        {
            auto type = read<EntryType>(stream);
            structure.emplace_back(std::format("{} {}", toString(type), j), type);
        }

        auto fromFile = getStructureFromFile(filePath, tableName);
        if (fromFile.empty()) return {structure};
        if (fromFile.size() != structureCount) return {structure};

        auto lambda   = [](const auto& val) { return std::get<0>(val).type != std::get<1>(val).type; };
        auto mismatch = std::ranges::any_of(std::ranges::views::zip(structure, fromFile), lambda);
        if (mismatch) return {structure};

        return {fromFile};
    }

    std::expected<void, std::string> exportCSV(const FinalFile& file, std::filesystem::path target)
    {
        if (std::filesystem::exists(target) && !std::filesystem::is_directory(target))
            return std::unexpected("Target path exists and is not a directory.");

        std::filesystem::create_directories(target);

        int32_t table_id = 0;
        for (const auto& table : file.tables)
        {
            auto path = target / std::format("{:03}_{}.csv", table_id++, table.name);
            std::ofstream stream(path);

            if (!stream) return std::unexpected("Failed to write target file.");

            stream << table.structure.getCSVHeader() << "\n";
            std::ranges::for_each(table.entries, [&](auto val) { stream << table.structure.writeCSV(val) << "\n"; });
        }

        return {};
    }

    std::expected<FinalFile, std::string> importCSV(std::filesystem::path source)
    {
        if (!std::filesystem::exists(source) || !std::filesystem::is_directory(source))
            return std::unexpected("Source path doesn't exist or is not a directory.");

        std::filesystem::directory_iterator itr(source);
        std::vector<std::filesystem::path> files;
        for (auto val : itr)
            if (val.is_regular_file()) files.push_back(val);
        std::ranges::sort(files);

        std::vector<FinalTable> tables;

        for (auto file : files)
        {
            CSVFile csv(file);

            auto name      = file.stem().generic_string().substr(4);
            auto structure = getStructureCSV(csv, source, name);
            auto entries   = csv.rows | std::views::transform([&](const auto& val) { return structure.readCSV(val); }) |
                           std::ranges::to<std::vector<std::vector<EntryValue>>>();

            tables.emplace_back(name, structure, entries);
        }

        return FinalFile{tables};
    }
} // namespace dscstools::expa
