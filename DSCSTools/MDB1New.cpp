
#include "MDB1new.h"

#include "Compressors.h"
#include "MDB1.h"

#include <cassert>
#include <filesystem>
#include <iostream>

namespace
{
} // namespace

namespace dscstools::mdb1new
{

    void test()
    {
        auto dscsPath   = "/home/syd/Development/MyRepos/DSCSTools/build/DSCSToolsCLI/DSDBP.decrypt.bin";
        auto hltldaPath = "/home/syd/Development/MyRepos/DSCSTools/build/DSCSToolsCLI/app_romA_0.dx11.mvgl";
        auto dstsPath   = "/home/syd/Development/MyRepos/DSCSTools/build/DSCSToolsCLI/app_0.dx11.mvgl";

        ArchiveInfo<DSCS> info(dscsPath);
        ArchiveInfo<HLTLDA> info2(hltldaPath);
        ArchiveInfo<DSTS> info3(dstsPath);

        //info.extract("output/");
        // info2.extract("output2/");
        // info3.extract("DSTS/");

        auto start1 = std::chrono::steady_clock::now();
        packArchive<DSCS>("output/", "DSDBP.new1", CompressMode::NORMAL);
        auto end1 = std::chrono::steady_clock::now();

        auto start2 = std::chrono::steady_clock::now();
        packArchive<DSCS>("output/", "DSDBP.new2", CompressMode::ADVANCED);
        auto end2 = std::chrono::steady_clock::now();

        auto start3 = std::chrono::steady_clock::now();
        dscstools::mdb1::packMDB1("output/", "DSDBP.old1", dscstools::mdb1::CompressMode::normal, false, std::cout);
        auto end3 = std::chrono::steady_clock::now();

        auto start4 = std::chrono::steady_clock::now();
        dscstools::mdb1::packMDB1("output/", "DSDBP.old2", dscstools::mdb1::CompressMode::advanced, false, std::cout);
        auto end4 = std::chrono::steady_clock::now();

        std::cout << "1: " << end1 - start1 << "\n";
        std::cout << "2: " << end2 - start2 << "\n";
        std::cout << "3: " << end3 - start3 << "\n";
        std::cout << "4: " << end4 - start4 << "\n";

        ArchiveInfo<DSCS> new1("DSDBP.new1");
        ArchiveInfo<DSCS> new2("DSDBP.new2");

        new1.extract("DSDBPNew1/");
        new2.extract("DSDBPNew2/");
    }



} // namespace dscstools::mdb1new