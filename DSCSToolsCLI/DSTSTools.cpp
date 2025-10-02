#include "Compressors.h"
#include "EXPAnew.h"
#include "MDB1new.h"

#include <cstring>
#include <filesystem>
#include <iostream>

void printUse()
{
    // clang-format off
    std::cout << "DSTSTools v2.0.0-alpha1 by SydMontague | https://github.com/SydMontague/DSCSTools/" << std::endl;
    std::cout << "Modes:" << std::endl;
    std::cout << "	--extract <sourceFile> <targetFolder>" << std::endl;
    std::cout << "		Extracts the given MDB1 into a folder." << std::endl;
    std::cout << "	--pack <sourceFolder> <targetFile> [--disable-compression|--advanced-compression]" << std::endl;
    std::cout << "		Repacks the given folder into an MDB1." << std::endl;
    std::cout << "		Optional: --disable-compression" << std::endl;
    std::cout << "		Optional: --advanced-compression. Doesn't store duplicate data." << std::endl;
    std::cout << "	--mbeextract <source> <targetFolder>" << std::endl;
    std::cout << "		Extracts a .mbe file or a directory of them into CSV. " << std::endl;
    std::cout << "	--mbepack <sourceFolder> <targetFile>" << std::endl;
    std::cout << "		Repacks an .mbe folder containing CSV files back into a .mbe file." << std::endl;
    // clang-format on
}

void extractMBE(std::filesystem::path source, std::filesystem::path target)
{
    auto result = dscstools::expa::readEXPA<expa::EXPA64>(source);
    if (!result)
        std::cout << result.error() << "\n";
    else
    {
        auto result2 = dscstools::expa::exportCSV(result.value(), target);
        if (!result2) std::cout << result.error() << "\n";
    }
}

void packMBE(std::filesystem::path source, std::filesystem::path target)
{
    auto result = dscstools::expa::importCSV(source);
    if (!result)
        std::cout << result.error() << "\n";
    else
    {
        auto result2 = dscstools::expa::writeEXPA<expa::EXPA64>(result.value(), target);
        if (!result2) std::cout << result2.error() << "\n";
    }
}

void extractMBEDir(std::filesystem::path source, std::filesystem::path target)
{
    if (!std::filesystem::exists(source) || !std::filesystem::is_directory(source)) return;
    if (std::filesystem::exists(target) && !std::filesystem::is_directory(target)) return;

    std::filesystem::create_directories(target);

    std::filesystem::directory_iterator itr(source);

    for (auto file : itr)
        if (file.is_regular_file()) extractMBE(file, target / file.path().filename());
}

void packMBEDir(std::filesystem::path source, std::filesystem::path target)
{
    if (!std::filesystem::exists(source) || !std::filesystem::is_directory(source)) return;
    if (std::filesystem::exists(target) && !std::filesystem::is_directory(target)) return;

    std::filesystem::create_directories(target);

    std::filesystem::directory_iterator itr(source);

    for (auto file : itr)
        if (file.is_directory()) packMBE(file.path(), target / file.path().filename());
}

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        printUse();
        return 0;
    }
    std::filesystem::path source =
        std::filesystem::exists(argv[2]) ? argv[2] : std::filesystem::current_path().append(argv[2]);
    std::filesystem::path target = argv[3];

    if (!target.has_root_directory()) target = std::filesystem::current_path().append(argv[3]);

    try
    {
        if (strncmp("--extract", argv[1], 10) == 0)
        {
            bool decompress = argc < 5 || strncmp("--compressed", argv[4], 13) != 0;

            dscstools::mdb1new::ArchiveInfo<dscstools::mdb1new::DSTS> archive(source);
            auto result = archive.extract(target);

            std::cout << "Done" << std::endl;
        }
        else if (strncmp("--pack", argv[1], 7) == 0)
        {
            dscstools::CompressMode mode = dscstools::CompressMode::NORMAL;

            if (argc >= 5)
            {
                if (strncmp("--disable-compression", argv[4], 22) == 0)
                    mode = dscstools::CompressMode::NONE;
                else if (strncmp("--advanced-compression", argv[4], 23) == 0)
                    mode = dscstools::CompressMode::ADVANCED;
            }

            auto result = dscstools::mdb1new::packArchive<mdb1new::DSTS>(source, target, mode);
            std::cout << "Done" << std::endl;
        }
        else if (strncmp("--mbeextract", argv[1], 13) == 0)
        {
            extractMBE(source, target);
            std::cout << "Done" << std::endl;
        }
        else if (strncmp("--mbepack", argv[1], 10) == 0)
        {
            packMBE(source, target);
            std::cout << "Done" << std::endl;
        }
        else if (strncmp("--mbeextractdir", argv[1], 13) == 0)
        {
            extractMBEDir(source, target);
            std::cout << "Done" << std::endl;
        }
        else if (strncmp("--mbepackdir", argv[1], 10) == 0)
        {
            packMBEDir(source, target);
            std::cout << "Done" << std::endl;
        }
        else { printUse(); }
    }
    catch (std::exception& ex)
    {
        std::cout << ex.what() << std::endl;
    }

    return 0;
}
