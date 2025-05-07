
#include "include/EXPA.h"
#include <stdint.h>
#include <fstream>
#include <iostream>

#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <format>
#include <sstream>
#include <memory>
#include <boost/algorithm/string/replace.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>

#include "../libs/csv-parser/parser.hpp"
#include "boost/property_tree/ptree_fwd.hpp"

#define PADDING_BYTE (unsigned char) 0xCC

namespace dscstools {
	namespace mbe {
		struct EXPAHeader {
			uint32_t magicValue;
			uint32_t numTables;
		};

		struct EXPATable {
			char* tablePtr;

			uint32_t nameSize() { return *reinterpret_cast<uint32_t*>(tablePtr); }
			char* name() { return reinterpret_cast<char*>(tablePtr + 4); }
			boost::property_tree::ptree getElements() {
				boost::property_tree::ptree tree;
				
				for(int32_t i = 0; i < elementCount(); i++) {
					uint32_t type = *reinterpret_cast<uint32_t*>(tablePtr + nameSize() + 8 + i * 4);
					switch (type) {
						case 2:
							tree.add("int " + std::to_string(i), "int"); break; 
						case 9:
							tree.add("int2 " + std::to_string(i), "int"); break; 
						case 4:
							tree.add("byte " + std::to_string(i), "byte"); break; 
						case 5:
							tree.add("float " + std::to_string(i), "float"); break; 
						case 7:
							tree.add("string " + std::to_string(i), "string"); break;
						case 8:
							tree.add("string2 " + std::to_string(i), "string"); break;
						default:
							std::cout << type << "\n";
					
					}
				}
				return tree;
			}
			uint32_t elementCount() { return *reinterpret_cast<uint32_t*>(tablePtr + nameSize() + 4);}
			uint32_t entrySize() { return *reinterpret_cast<uint32_t*>(tablePtr + nameSize() + 8 + elementCount() * 4); }
			uint32_t entryCount() { return *reinterpret_cast<uint32_t*>(tablePtr + nameSize() + 12 + elementCount() * 4); }
			uint32_t totalSize() { return nameSize() + 16 + elementCount() * 4; }
			
		};

		struct EXPATableHeader {
			uint32_t stringSize;
			char* name;
			uint32_t entrySize;
			uint32_t entryCount;
		};

		struct CHNKHeader {
			uint32_t magicValue;
			uint32_t numEntry;
		};

		uint32_t align(uint64_t offset, uint64_t value)
		{
			return (value - (offset % value)) % value;
		}

		std::string wrapRegex(const std::string& in)
		{
			return "^" + in + "$";
		}

		inline boost::property_tree::ptree matchStructureName(const boost::property_tree::ptree& format, const std::string& structureName, const std::string& sourceName)
		{
			auto formatValue = format.get_child_optional(structureName);
			if (!formatValue) {
				// Scan all table definitions to find a matching regex expression, if any
				for (auto& kv : format) {
					if (boost::regex_search(structureName, boost::regex{ wrapRegex(kv.first) })) {
						formatValue = kv.second;
						break;
					}
				}
				if (!formatValue)
					throw std::runtime_error("Error: no definition for table " + std::string(structureName) + " found. " + sourceName);
			}

			return formatValue.get();
		}

		uint32_t getEntrySize(const std::string &type, uint32_t currentSize) {
			if (type == "byte")
				return 1;
			if (type == "short")
				return 2 + align(currentSize, 2);
			if (type.starts_with("int"))
				return 4 + align(currentSize, 4);
			if (type == "float")
				return 4 + align(currentSize, 4);
			else if (type.starts_with("string"))
				return 8 + align(currentSize, 8);
			else if (type == "int array")
				return 16 + align(currentSize, 8);

			return 0;
		}

		void writeEXPAEntry(std::ofstream &output, char* &ptr, const std::string &type) {
			// TODO remove boilerplate
			if (type == "int") {
				ptr = ptr + align((std::size_t)ptr, 4);
				output << *reinterpret_cast<int32_t*>(ptr);
				ptr += 4;
			}
			else if (type == "float") {
				ptr = ptr + align((std::size_t)ptr, 4);
				output << *reinterpret_cast<float*>(ptr);
				ptr += 4;
			}
			else if (type == "string") {
				ptr = ptr + align((std::size_t)ptr, 8);
				char* strPtr = *reinterpret_cast<char**>(ptr);
				output << std::quoted(strPtr == nullptr ? "" : std::string(strPtr + 8), '\"', '\"');
				ptr += 8;
			}
			else if (type == "byte") {
				ptr = ptr;
				output << (int32_t) *reinterpret_cast<int8_t*>(ptr);
				ptr += 1;
			}
			else if (type == "short") {
				ptr = ptr + align((std::size_t)ptr, 2);
				output << *reinterpret_cast<int16_t*>(ptr);
				ptr += 2;
			}
			else if (type == "int array") {
				ptr = ptr + align((std::size_t)ptr, 8);
				uint32_t elemCount = *reinterpret_cast<uint32_t*>(ptr);
				ptr += 4;
				ptr = ptr + align((std::size_t)ptr, 8);
				int32_t* arrPtr = *reinterpret_cast<int32_t**>(ptr);

				for (uint32_t i = 0; i < elemCount; i++)
					output << arrPtr[i + 2] << ((i != (elemCount - 1)) ? " " : "");
				ptr += 8;
			}
		}

		boost::property_tree::ptree getStructureFile(std::filesystem::path source) {
			boost::property_tree::ptree format;
			boost::property_tree::ptree structure;
			boost::property_tree::read_json(std::string("structures/structure.json"), structure);

			std::string formatFile;
			for (auto var : structure) {
				if (boost::regex_search(source.string(), boost::regex{ var.first })) {
					formatFile = var.second.data();
					break;
				}
			}

			if (formatFile.empty())
				throw std::runtime_error("Error: No fitting structure file found for " + source.string());

			boost::property_tree::read_json("structures/" + formatFile, format);

			return format;
		}


		std::string conversionErrorMessage(const std::string& in, const std::string& type, const std::string& mbe, const std::string& filename, const std::string& colName, size_t colCount, size_t rowCount)
		{

			std::string type_name;
			return "Error packing " + mbe + "/" + filename + ": Value \'" + in + "\' cannot be converted to \'" + type + "\' at Row " + std::to_string(rowCount) + ", Column " + std::to_string(colCount) + " \'" + colName + "\'.";
		}

		void extractMBEFile(std::filesystem::path source, std::filesystem::path target) {
			if (!std::filesystem::exists(target)) {
				if (target.has_parent_path())
					std::filesystem::create_directories(target);
			}
			else if (!std::filesystem::is_directory(target))
				throw std::invalid_argument("Error: target path is not a directory.");

			std::ifstream input(source, std::ios::in | std::ios::binary);
			input.seekg(0, std::ios::end);
			std::streamoff length = input.tellg();
			input.seekg(0, std::ios::beg);

			std::unique_ptr<char[]> data = std::make_unique<char[]>(length);
			input.read(data.get(), length);

			EXPAHeader* header = reinterpret_cast<EXPAHeader*>(data.get());

			if (header->magicValue != 0x41505845) // "EXPA"
				throw std::invalid_argument("Error: source file " + source.filename().string() + " is not in EXPA format.");

			std::vector<EXPATable> tables;
			uint32_t offset = 8;

			for (uint32_t i = 0; i < header->numTables; i++) {

				offset += align(offset, 8);
				EXPATable table = { data.get() + offset };
				tables.push_back(table);

				offset += table.totalSize();

			//if (table.nameSize() % 8 == 0)
			//		offset += 4;

				offset += table.entryCount() * (table.entrySize() + align(table.entrySize(), 8));
			}

			offset += align(offset, 8);

			CHNKHeader* chunkHeader = reinterpret_cast<CHNKHeader*>(data.get() + offset);
			offset += 8;

			for (uint32_t i = 0; i < chunkHeader->numEntry; i++) {
				uint32_t dataOffset = *reinterpret_cast<uint32_t*>(data.get() + offset);
				uint32_t size = *reinterpret_cast<uint32_t*>(data.get() + offset + 4);
				uint64_t ptr = reinterpret_cast<uint64_t>(data.get() + offset);

				memcpy(data.get() + dataOffset, &ptr, 8);
				offset += (size + 8);
			}

			//boost::property_tree::ptree format = getStructureFile(source);
			auto filename = source.filename().string();

			int32_t index = 0;
			for (auto table : tables) {

				uint32_t tableHeaderSize = table.totalSize();
				const auto& formatValue = table.getElements(); //matchStructureName(format, table.name(), filename);

				std::filesystem::path outputPath = target / source.filename() / std::format("{:02d}_{}.csv", index++, table.name());
				if (outputPath.has_parent_path())
					std::filesystem::create_directories(outputPath.parent_path());
				std::ofstream output(outputPath, std::ios::out);

				// write header
				bool first = true;
				for (auto var : formatValue) {
					if (first)
						first = false;
					else
						output << ",";

					output << var.first;
				}
				output << std::endl;

				// write data
				for (uint32_t i = 0; i < table.entryCount(); i++) {
					bool first = true;
					char* localOffset = table.tablePtr + i * (table.entrySize() + align(table.entrySize(), 8)) + tableHeaderSize;

					localOffset += align((uint64_t)localOffset, 8);

					for (auto var : formatValue) {
						if (first)
							first = false;
						else
							output << ",";

						writeEXPAEntry(output, localOffset, var.second.data());
					}
					output << std::endl;
				}
			}
		}

		void extractMBE(std::filesystem::path source, std::filesystem::path target) {
			if (!std::filesystem::exists(source))
				throw std::invalid_argument("Error: input path does not exist.");
			if (std::filesystem::equivalent(source, target))
				throw std::invalid_argument("Error: input and output path must be different!");

			if (std::filesystem::is_directory(source))
				for (auto file : std::filesystem::directory_iterator(source))
					extractMBEFile(file, target);
			else if (std::filesystem::is_regular_file(source))
				extractMBEFile(source, target);
			else
				throw std::invalid_argument("Error: input is neither directory nor file.");
		}

		uint32_t convertTypeMapping(std::string& type) {
			if(type == "int") return 2;
			if(type == "int2") return 9;
			if(type == "byte") return 4;
			if(type == "float") return 5;
			if(type == "string") return 7;
			if(type == "string2") return 8;

			return 0xFFFFFFFF;
		}

		// folder input, file output
		void packMBE(std::filesystem::path source, std::filesystem::path target) {
			if (!std::filesystem::exists(source))
				throw std::invalid_argument("Error: input path does not exist.");
			if (std::filesystem::equivalent(source, target))
				throw std::invalid_argument("Error: input and output path must be different!");
			if (!std::filesystem::is_directory(source))
				throw std::invalid_argument("Error: input path is not a directory.");
			if (!std::filesystem::exists(target)) {
				if(target.has_parent_path())
					std::filesystem::create_directories(target.parent_path());
			}
			else if (!std::filesystem::is_regular_file(target))
				throw std::invalid_argument("Error: target path already exists and is not a file.");

			std::ofstream output(target, std::ios::out | std::ios::binary);

			//boost::property_tree::ptree format = getStructureFile(source);


			// write EXPA Header
			output.write("EXPA", 4);
			std::size_t numTables = 0;
			output.seekp(4, std::ios::cur); // Skip numTables output, we'll add it at the end

			struct CHNKData {
				std::string type;
				std::string data;
				uint32_t offset;
				std::string error_msg;
			};
			std::vector<CHNKData> chnkData;

			// Find all files in the directory, and assign each to the table definition they will be
			// built to
			std::vector<std::filesystem::path> sortedFiles;
			//sortedFiles.resize(format.size());
			for (auto& dir_entry : std::filesystem::directory_iterator(source)) {
				auto file = dir_entry.path();
				auto filename = file.filename().stem().string();
				size_t i = 0;
				//for (auto table : format) {
				//	if (boost::regex_search(filename, boost::regex{ wrapRegex(table.first) })) {
						sortedFiles.push_back(file);
				//	}
					++i;
				//}
			}

			// Sort each list of files alphanumerically
			// Might not be necessary, and it would be much nicer to sort naturally rather than alphanumerically
			//for (auto& filelist : sortedFiles)
			//{
				std::sort(sortedFiles.begin(), sortedFiles.end());
			//}

			size_t tableIterationCounter = 0;

			for (auto& file : sortedFiles) {
				++numTables;
				auto filename = file.filename().stem().string().substr(3);
				// write EXPA Table header
				std::ifstream countInput(file, std::ios::in);
				aria::csv::CsvParser countParser(countInput);
				
				std::vector<std::string> header;
				for (auto& row : countParser) {
					for(auto& col : row) 
						header.push_back(col.substr(0, col.rfind(" ")));
					
					break;
				}

				uint32_t entrySize = 0;
				for (auto entry : header)
					entrySize += getEntrySize(entry, entrySize);

				entrySize += align(entrySize, 8);
				uint32_t count = (uint32_t)std::distance(countParser.begin(), countParser.end());

				uint32_t nameSize = (uint32_t)(filename.size() + 4) / 4 * 4;
				std::vector<char> name(nameSize);
				std::copy(filename.begin(), filename.end(), name.begin());
				std::vector<char> padding(align(0x0CLL + nameSize, 8), 0);
				auto headerSize = header.size();

				output.write(reinterpret_cast<char*>(&nameSize), 4);
				output.write(name.data(), nameSize);
				output.write(reinterpret_cast<char*>(&headerSize), 4);
				for(auto& val : header) {
					auto typeMapping = convertTypeMapping(val);
					output.write(reinterpret_cast<char*>(&typeMapping), 4);
				}

				output.write(reinterpret_cast<char*>(&entrySize), 4);
				output.write(reinterpret_cast<char*>(&count), 4);
				//output.write(padding.data(), align(0x0CLL + nameSize, 8));

				output.write("\0\0\0\0\0\0\0\0\0\0\0\0", align(nameSize + 24 + 4 * headerSize, 8));
				

				// write EXPA data, cache CHNK data
				std::ifstream input(file, std::ios::in);
				aria::csv::CsvParser parser(input);

				bool first = true;

				int32_t row_counter = -1;
				for (auto& row : parser) {
					++row_counter;

					/*if (localFormat.size() != row.size()) {
						std::stringstream sstream;
						sstream << "Error: structure element count differs from input element count. The wrong structure might be used?" << std::endl;
						sstream << "Expected: " << localFormat.size() << " | Found: " << row.size() << std::endl;
						throw std::runtime_error(sstream.str());
					}*/

					if (first) {
						first = false;
						continue;
					}

					//auto itr = localFormat.begin();
					uint32_t entrySize = 0;

					int32_t col_counter = -1;
					for (auto& col : row) {
						++col_counter;
						//const auto& structEntry = (*itr++);
						//std::string colName = structEntry.first.data();
						//std::string type = structEntry.second.data();
						std::string type = header[col_counter];
						std::string colName = type;

						try
						{
							// TODO remove boilerplate
							if (type == "byte") {
								int8_t value = std::stoi(col);
								output.write(reinterpret_cast<char*>(&value), 1);
								entrySize += 1;
							}
							else if (type == "short") {
								uint32_t paddingSize = align(entrySize, 2);
								std::vector<char> padding(paddingSize, PADDING_BYTE);
								output.write(padding.data(), paddingSize);

								int16_t value = std::stoi(col);
								output.write(reinterpret_cast<char*>(&value), 2);
								entrySize += 2 + paddingSize;
							}
							else if (type.starts_with("int")) {
								uint32_t paddingSize = align(entrySize, 4);
								std::vector<char> padding(paddingSize, PADDING_BYTE);
								output.write(padding.data(), paddingSize);

								int32_t value = std::stoi(col);
								output.write(reinterpret_cast<char*>(&value), 4);
								entrySize += 4 + paddingSize;
							}
							else if (type == "float") {
								uint32_t paddingSize = align(entrySize, 4);
								std::vector<char> padding(paddingSize, PADDING_BYTE);
								output.write(padding.data(), paddingSize);

								float value = std::stof(col);
								output.write(reinterpret_cast<char*>(&value), 4);
								entrySize += 4 + paddingSize;
							}
							else if (type.starts_with("string")) {
								if (!col.empty())
									chnkData.push_back({ type, col, (uint32_t)output.tellp() + align(entrySize, 8), "" });

								uint32_t paddingSize = align(entrySize, 8);
								std::vector<char> padding(paddingSize, PADDING_BYTE);
								output.write(padding.data(), paddingSize);

								output.write("\0\0\0\0\0\0\0\0", 8);
								entrySize += 8 + paddingSize;
							}
							else if (type == "int array") {
								if (!col.empty())
									chnkData.push_back({ type, col, (uint32_t)output.tellp() + 8 + align(entrySize, 8), conversionErrorMessage(col, type, source.filename().string(), filename, colName, col_counter, row_counter) });

								uint32_t paddingSize = align(entrySize, 8);
								std::vector<char> padding(8, PADDING_BYTE);
								output.write(padding.data(), paddingSize);

								uint32_t arraySize = (uint32_t)std::count(col.begin(), col.end(), ' ') + 1;
								if (col.empty())
									arraySize = 0;

								output.write(reinterpret_cast<char*>(&arraySize), 4);
								output.write(padding.data(), 4);
								output.write("\0\0\0\0\0\0\0\0", 8);

								entrySize += 16 + paddingSize;
							}
						}
						catch (const std::exception& ex)
						{
							throw std::invalid_argument(conversionErrorMessage(col, type, source.filename().string(), filename, colName, col_counter, row_counter));
						}
					}

					if (align(entrySize, 8) != 0) {
						std::vector<char> padding(align(entrySize, 8), PADDING_BYTE);
						output.write(padding.data(), align(entrySize, 8));
						entrySize += align(entrySize, 8);
					}
				}
			}
			


			std::size_t chunkCount = chnkData.size();
			output.write("CHNK", 4);
			output.write(reinterpret_cast<char*>(&chunkCount), 4);

			for (auto entry : chnkData) {
				// TODO remove boilerplate
				if (entry.type.starts_with("string")) {
					uint32_t stringSize = (uint32_t)((entry.data.size() + 5) / 4) * 4;
					std::vector<char> data(stringSize);
					std::copy(entry.data.begin(), entry.data.end(), data.begin());

					output.write(reinterpret_cast<char*>(&entry.offset), 4);
					output.write(reinterpret_cast<char*>(&stringSize), 4);
					output.write(data.data(), stringSize);
				}
				else if (entry.type == "int array") {
					std::vector<std::string> numbers;
					boost::split(numbers, entry.data, boost::is_any_of(" "));

					uint32_t size = (uint32_t)numbers.size() * 4;
					output.write(reinterpret_cast<char*>(&entry.offset), 4);
					output.write(reinterpret_cast<char*>(&size), 4);

					try
					{
						for (auto number : numbers) {
							int32_t val = std::stoi(number);
							output.write(reinterpret_cast<char*>(&val), 4);
						}
					}
					catch (const std::exception& ex)
					{
						throw std::invalid_argument(entry.error_msg);
					}
				}
			}

			// Seek back to the 4th byte and write out the number of tables that were found
			output.seekp(4, std::ios::beg);
			output.write(reinterpret_cast<char*>(&numTables), 4);
		}
	}
}