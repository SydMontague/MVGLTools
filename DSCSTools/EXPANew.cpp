#include "EXPAnew.h"

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
#include <string>
#include <variant>
#include <vector>

namespace
{
    using namespace dscstools;
    using namespace dscstools::expa;

    constexpr auto STRUCTURE_FOLDER = "structures/";
    constexpr auto STRUCTURE_FILE   = "structures/structure.json";

    constexpr EntryType convertEntryType(std::string val)
    {
        std::map<std::string, EntryType> map;
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
                return CHNKEntry(base_offset + 8, array);
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
} // namespace

namespace dscstools::expa
{

    void bla()
    {
        constexpr auto path1 =
            "/home/syd/Development/MyRepos/DSCSTools/build/DSCSToolsCLI/DSTS/data/battle_formation.mbe";
        constexpr auto path2 =
            "/home/syd/Development/MyRepos/DSCSTools/build/DSCSToolsCLI/DSDBP/data/digimon_common_para.mbe";
        auto result = readEXPA<EXPA64>(path1);

        writeEXPA<EXPA64>(result.value(), "expa64.mbe");
        std::cout << result.error_or("Success") << "\n";
        result = readEXPA<EXPA32>(path2);
        writeEXPA<EXPA32>(result.value(), "expa32.mbe");
        std::cout << result.error_or("Success") << "\n";
    }

    std::vector<CHNKEntry>
    Structure::writeEXPA(uint32_t base_offset, char* data, const std::vector<EntryValue>& entries) const
    {
        auto offset     = 0;
        auto bitCounter = 0;
        std::bitset<32> currentBool;
        std::vector<CHNKEntry> chunkEntries;

        for (const auto& val : std::views::zip(structure, entries))
        {
            auto type  = get<0>(val).type;
            auto entry = get<1>(val);

            if (type != EntryType::BOOL)
            {
                if (bitCounter > 0)
                {
                    *reinterpret_cast<uint32_t*>(data + offset) = currentBool.to_ulong();
                    offset += sizeof(uint32_t);
                    bitCounter  = 0;
                    currentBool = {};
                }
                offset = ceilInteger(offset, getAlignment(type));
            }

            auto result = writeEXPAEntry(base_offset + offset, data + offset, type, entry);
            if (result) chunkEntries.push_back(result.value());

            if (type == EntryType::BOOL)
                currentBool.set(bitCounter++, get<bool>(entry));
            else
                offset += getSize(type);
        }

        if (bitCounter > 0)
        {
            *reinterpret_cast<uint32_t*>(data + offset) = currentBool.to_ulong();
            offset += sizeof(uint32_t);
        }

        return chunkEntries;
    }

    std::vector<EntryValue> Structure::readEXPA(const char* data) const
    {
        if (structure.empty()) return {};

        std::vector<EntryValue> values;
        auto offset     = 0;
        auto bitCounter = 0;

        for (const auto& val : structure)
        {
            if (bitCounter != 0 || val.type != EntryType::BOOL)
            {
                offset     = ceilInteger(offset, getAlignment(val.type));
                bitCounter = 0;
            }

            values.push_back(readEXPAEntry(val.type, data + offset, bitCounter));

            if (bitCounter == 0) offset += getSize(val.type);
            if (val.type == EntryType::BOOL) bitCounter++;
        }

        return values;
    }

    uint32_t Structure::getEXPASize() const
    {
        if (structure.empty()) return 0;

        auto currentSize = 0;
        auto bitCounter  = 0;

        for (const auto& val : structure)
        {
            if (bitCounter != 0 || val.type != EntryType::BOOL)
            {
                currentSize = ceilInteger(currentSize, getAlignment(val.type));
                bitCounter  = 0;
            }

            if (bitCounter == 0) currentSize += getSize(val.type);
            if (val.type == EntryType::BOOL) bitCounter++;
        }

        if ((structure.size() % 2) == 0) currentSize = ceilInteger(currentSize, 8);

        return currentSize;
    }

    size_t Structure::getEntryCount() const
    {
        return structure.size();
    }

    CHNKEntry::CHNKEntry(uint32_t offset, const std::string& data)
        : offset(offset)
    {
        value = std::vector<char>(ceilInteger<4>(data.size() + 1));
        std::copy(data.begin(), data.end(), value.begin());
    }

    CHNKEntry::CHNKEntry(uint32_t offset, const std::vector<int32_t>& data)
        : offset(offset)
    {
        value = std::vector<char>(data.size() * sizeof(int32_t));
        std::copy(data.begin(), data.end(), value.begin());
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
        auto mismatch =
            std::ranges::any_of(std::ranges::views::zip(structure, fromFile),
                                [](const auto& val) { return std::get<0>(val).type != std::get<1>(val).type; });
        if (mismatch) return {structure};

        return {fromFile};
    }
} // namespace dscstools::expa