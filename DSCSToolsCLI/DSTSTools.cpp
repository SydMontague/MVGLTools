#include "AFS2.h"
#include "Compressors.h"
#include "EXPAnew.h"
#include "MDB1new.h"
#include "SaveFile.h"
#include "boost/program_options/options_description.hpp"
#include "boost/program_options/variables_map.hpp"

#include <Helpers.h>
#include <boost/program_options.hpp>
#include <boost/program_options/errors.hpp>

#include <cctype>
#include <exception>
#include <expected>
#include <filesystem>
#include <iostream>
#include <ranges>

namespace
{
    enum class GameMode
    {
        DSCS,
        DSCS_CONSOLE,
        DSTS,
        HLTLA,

        INVALID,
    };

    enum class Mode
    {
        PACK_MVGL,
        UNPACK_MVGL,
        UNPACK_MVGL_FILE,

        PACK_MBE,
        PACK_MBE_DIR,
        UNPACK_MBE,
        UNPACK_MBE_DIR,
        DUMP_MBE_STRUCTURES,

        DECRYPT_SAVE,
        ENCRYPT_SAVE,

        PACK_AFS2,
        UNPACK_AFS2,

        ENCRYPT_FILE,
        DECRYPT_FILE,

        INVALID,
    };

    template<typename T>
    concept AFS2Module = requires(std::filesystem::path source, std::filesystem::path target) {
        { T::pack(source, target) } -> std::same_as<std::expected<void, std::string>>;
        { T::unpack(source, target) } -> std::same_as<std::expected<void, std::string>>;
    };

    template<typename T>
    concept FileCryptModule = requires(std::filesystem::path source, std::filesystem::path target) {
        { T::encrypt(source, target) } -> std::same_as<std::expected<void, std::string>>;
        { T::decrypt(source, target) } -> std::same_as<std::expected<void, std::string>>;
    };

    template<typename T>
    concept SaveCryptModule = requires(std::filesystem::path source, std::filesystem::path target) {
        { T::encrypt(source, target) } -> std::same_as<std::expected<void, std::string>>;
        { T::decrypt(source, target) } -> std::same_as<std::expected<void, std::string>>;
    };

    template<typename T>
    concept GameModules =
        requires() {
            typename T::MDB1Module;
            typename T::EXPAModule;
            typename T::CryptModule;
            typename T::SaveCryptModule;
            typename T::AFS2Module;
        } && mdb1new::ArchiveType<typename T::MDB1Module> && expa::EXPA<typename T::EXPAModule> &&
        FileCryptModule<typename T::CryptModule> && SaveCryptModule<typename T::SaveCryptModule> &&
        AFS2Module<typename T::AFS2Module>;

    struct DummySaveCryptor
    {
        static std::expected<void, std::string> encrypt(std::filesystem::path source, std::filesystem::path target)
        {
            return std::unexpected("Not supported");
        }

        static std::expected<void, std::string> decrypt(std::filesystem::path source, std::filesystem::path target)
        {
            return std::unexpected("Not supported");
        }
    };

    struct DSCSSaveCryptor
    {
        static std::expected<void, std::string> encrypt(std::filesystem::path source, std::filesystem::path target)
        {
            try
            {
                savefile::encryptSaveFile(source, target);
                return {};
            }
            catch (std::exception& ex)
            {
                return std::unexpected(ex.what());
            }
        }

        static std::expected<void, std::string> decrypt(std::filesystem::path source, std::filesystem::path target)
        {
            try
            {
                savefile::decryptSaveFile(source, target);
                return {};
            }
            catch (std::exception& ex)
            {
                return std::unexpected(ex.what());
            }
        }
    };

    struct DummyAFS2Packer
    {
        static std::expected<void, std::string> unpack(std::filesystem::path source, std::filesystem::path target)
        {
            return std::unexpected("Not supported");
        }

        static std::expected<void, std::string> pack(std::filesystem::path source, std::filesystem::path target)
        {
            return std::unexpected("Not supported");
        }
    };

    struct DSCSAFS2Packer
    {
        static std::expected<void, std::string> unpack(std::filesystem::path source, std::filesystem::path target)
        {
            try
            {
                afs2::extractAFS2(source, target);
                return {};
            }
            catch (std::exception& ex)
            {
                return std::unexpected(ex.what());
            }
        }

        static std::expected<void, std::string> pack(std::filesystem::path source, std::filesystem::path target)
        {
            try
            {
                afs2::packAFS2(source, target);
                return {};
            }
            catch (std::exception& ex)
            {
                return std::unexpected(ex.what());
            }
        }
    };

    struct DummyFileCryptor
    {
        static std::expected<void, std::string> encrypt(std::filesystem::path source, std::filesystem::path target)
        {
            return std::unexpected("Not supported");
        }

        static std::expected<void, std::string> decrypt(std::filesystem::path source, std::filesystem::path target)
        {
            return std::unexpected("Not supported");
        }
    };

    struct DSCSFileCryptor
    {
        static std::expected<void, std::string> encrypt(std::filesystem::path source, std::filesystem::path target)
        {
            if (!std::filesystem::is_regular_file(source)) return std::unexpected("Input path is not a file.");
            if (std::filesystem::exists(target) && !std::filesystem::is_regular_file(target))
                return std::unexpected("Output path exists and is not a file.");
            if (file_equivalent(source, target)) return std::unexpected("Input and output file must be different.");

            mdb1new::DSCS::InputStream input(source, std::ios::binary | std::ios::in);
            mdb1new::DSCS::OutputStream output(target, std::ios::binary | std::ios::out);

            std::streamsize offset = 0;
            std::array<char, 0x2000> inArr;

            while (!input.eof())
            {
                input.read(inArr.data(), 0x2000);
                std::streamsize count = input.gcount();
                output.write(inArr.data(), count);
                offset += count;
            }

            return {};
        }

        static std::expected<void, std::string> decrypt(std::filesystem::path source, std::filesystem::path target)
        {
            return encrypt(source, target);
        }
    };

    struct DSTSModule
    {
        using MDB1Module      = dscstools::mdb1new::DSTS;
        using EXPAModule      = dscstools::expa::EXPA64;
        using CryptModule     = DummyFileCryptor;
        using SaveCryptModule = DummySaveCryptor;
        using AFS2Module      = DummyAFS2Packer;
    };

    struct HLTLAModule
    {
        using MDB1Module      = dscstools::mdb1new::HLTLDA;
        using EXPAModule      = dscstools::expa::EXPA64;
        using CryptModule     = DummyFileCryptor;
        using SaveCryptModule = DummySaveCryptor;
        using AFS2Module      = DummyAFS2Packer;
    };

    struct DSCSModule
    {
        using MDB1Module      = dscstools::mdb1new::DSCS;
        using EXPAModule      = dscstools::expa::EXPA32;
        using CryptModule     = DSCSFileCryptor;
        using SaveCryptModule = DSCSSaveCryptor;
        using AFS2Module      = DSCSAFS2Packer;
    };

    struct DSCSConsoleModule
    {
        using MDB1Module      = dscstools::mdb1new::DSCSNoCrypt;
        using EXPAModule      = dscstools::expa::EXPA32;
        using CryptModule     = DSCSFileCryptor;
        using SaveCryptModule = DSCSSaveCryptor;
        using AFS2Module      = DSCSAFS2Packer;
    };

    template<GameModules T>
    struct GameCLI
    {
        static void packMVGL(std::filesystem::path source, std::filesystem::path target, CompressMode compress)
        {
            auto result = dscstools::mdb1new::packArchive<typename T::MDB1Module>(source, target, compress);
            if (!result) std::cout << result.error() << "\n";
        }
        static void unpackMVGL(std::filesystem::path source, std::filesystem::path target)
        {
            dscstools::mdb1new::ArchiveInfo<typename T::MDB1Module> archive(source);
            auto result = archive.extract(target);
            if (!result) std::cout << result.error() << "\n";
        }
        static void unpackMVGLFile(std::filesystem::path source, std::filesystem::path target, std::string file)
        {
            dscstools::mdb1new::ArchiveInfo<typename T::MDB1Module> archive(source);
            auto result = archive.extractSingleFile(target, file);
            if (!result) std::cout << result.error() << "\n";
        }

        static void unpackMBE(std::filesystem::path source, std::filesystem::path target)
        {
            std::cout << source << "\n";
            auto result = dscstools::expa::readEXPA<typename T::EXPAModule>(source);
            if (!result)
            {
                std::cout << result.error() << "\n";
                return;
            }

            auto result2 = dscstools::expa::exportCSV(result.value(), target / source.filename());
            if (!result2) std::cout << result2.error() << "\n";
        }

        static void packMBE(std::filesystem::path source, std::filesystem::path target)
        {
            std::cout << source << "\n";
            auto result = dscstools::expa::importCSV(source);
            if (!result)
            {
                std::cout << result.error() << "\n";
                return;
            }

            auto result2 = dscstools::expa::writeEXPA<typename T::EXPAModule>(result.value(), target);
            if (!result2) std::cout << result2.error() << "\n";
        }

        static void unpackMBEDir(std::filesystem::path source, std::filesystem::path target)
        {
            if (!std::filesystem::exists(source) || !std::filesystem::is_directory(source)) return;
            if (std::filesystem::exists(target) && !std::filesystem::is_directory(target)) return;

            std::filesystem::create_directories(target);

            std::filesystem::directory_iterator itr(source);

            for (auto file : itr)
                if (file.is_regular_file()) unpackMBE(file, target);
        }

        static void packMBEDir(std::filesystem::path source, std::filesystem::path target)
        {
            if (!std::filesystem::exists(source) || !std::filesystem::is_directory(source)) return;
            if (std::filesystem::exists(target) && !std::filesystem::is_directory(target)) return;

            std::filesystem::create_directories(target);

            std::filesystem::directory_iterator itr(source);

            for (auto file : itr)
                if (file.is_directory()) packMBE(file.path(), target / file.path().filename());
        }

        static void dumpMBEStructures(std::filesystem::path source, std::filesystem::path target)
        {
            // TODO implement
            std::cout << "No implemented yet.\n";
        }

        static void packAFS2(std::filesystem::path source, std::filesystem::path target)
        {
            auto result = T::AFS2Module::pack(source, target);
            if (!result) std::cout << result.error() << "\n";
        }

        static void unpackAFS2(std::filesystem::path source, std::filesystem::path target)
        {
            auto result = T::AFS2Module::unpack(source, target);
            if (!result) std::cout << result.error() << "\n";
        }

        static void encryptSave(std::filesystem::path source, std::filesystem::path target)
        {
            auto result = T::SaveCryptModule::encrypt(source, target);
            if (!result) std::cout << result.error() << "\n";
        }

        static void decryptSave(std::filesystem::path source, std::filesystem::path target)
        {
            auto result = T::SaveCryptModule::decrypt(source, target);
            if (!result) std::cout << result.error() << "\n";
        }

        static void encryptFile(std::filesystem::path source, std::filesystem::path target)
        {
            auto result = T::CryptModule::encrypt(source, target);
            if (!result) std::cout << result.error() << "\n";
        }

        static void decryptFile(std::filesystem::path source, std::filesystem::path target)
        {
            auto result = T::CryptModule::decrypt(source, target);
            if (!result) std::cout << result.error() << "\n";
        }

        static void doAction(Mode mode, const boost::program_options::variables_map vm)
        {
            std::filesystem::path source = vm["input"].as<std::string>();
            std::filesystem::path target = vm["output"].as<std::string>();

            switch (mode)
            {
                case Mode::PACK_MVGL:
                {
                    CompressMode compress = vm["compress"].as<CompressMode>();
                    packMVGL(source, target, compress);
                    break;
                }
                case Mode::UNPACK_MVGL: unpackMVGL(source, target); break;
                case Mode::UNPACK_MVGL_FILE:
                {
                    auto file = vm["file"].as<std::string>();
                    unpackMVGLFile(source, target, file);
                    break;
                }
                case Mode::UNPACK_MBE: unpackMBE(source, target); break;
                case Mode::UNPACK_MBE_DIR: unpackMBEDir(source, target); break;
                case Mode::PACK_MBE: packMBE(source, target); break;
                case Mode::PACK_MBE_DIR: packMBEDir(source, target); break;
                case Mode::ENCRYPT_FILE: encryptFile(source, target); break;
                case Mode::DECRYPT_FILE: decryptFile(source, target); break;
                case Mode::ENCRYPT_SAVE: encryptSave(source, target); break;
                case Mode::DECRYPT_SAVE: decryptSave(source, target); break;
                case Mode::PACK_AFS2: packAFS2(source, target); break;
                case Mode::UNPACK_AFS2: unpackAFS2(source, target); break;
                case Mode::DUMP_MBE_STRUCTURES: dumpMBEStructures(source, target); break;
                case Mode::INVALID: std::cout << "Invalid mode!\n"; break;
            }

            std::cout << "Done\n";
        }
    };

    std::map<std::string, Mode> getModeMap()
    {
        std::map<std::string, Mode> map;
        map["pack"]      = Mode::PACK_MVGL;
        map["packmvgl"]  = Mode::PACK_MVGL;
        map["pack-mvgl"] = Mode::PACK_MVGL;

        map["unpack"]       = Mode::UNPACK_MVGL;
        map["unpackmvgl"]   = Mode::UNPACK_MVGL;
        map["unpack-mvgl"]  = Mode::UNPACK_MVGL;
        map["extract"]      = Mode::UNPACK_MVGL;
        map["extractmvgl"]  = Mode::UNPACK_MVGL;
        map["extract-mvgl"] = Mode::UNPACK_MVGL;

        map["unpackfile"]        = Mode::UNPACK_MVGL_FILE;
        map["unpackmvglfile"]    = Mode::UNPACK_MVGL_FILE;
        map["unpack-mvgl-file"]  = Mode::UNPACK_MVGL_FILE;
        map["extractfile"]       = Mode::UNPACK_MVGL_FILE;
        map["extractmvglfile"]   = Mode::UNPACK_MVGL_FILE;
        map["extract-mvgl-file"] = Mode::UNPACK_MVGL_FILE;

        map["packmbe"]  = Mode::PACK_MBE;
        map["pack-mbe"] = Mode::PACK_MBE;

        map["unpackmbe"]   = Mode::UNPACK_MBE;
        map["unpack-mbe"]  = Mode::UNPACK_MBE;
        map["extractmbe"]  = Mode::UNPACK_MBE;
        map["extract-mbe"] = Mode::UNPACK_MBE;

        map["packmbedir"]   = Mode::PACK_MBE_DIR;
        map["pack-mbe-dir"] = Mode::PACK_MBE_DIR;

        map["unpackmbedir"]    = Mode::UNPACK_MBE_DIR;
        map["unpack-mbe-dir"]  = Mode::UNPACK_MBE_DIR;
        map["extractmbedir"]   = Mode::UNPACK_MBE_DIR;
        map["extract-mbe-dir"] = Mode::UNPACK_MBE_DIR;

        map["packafs2"]  = Mode::PACK_AFS2;
        map["pack-afs2"] = Mode::PACK_AFS2;

        map["unpackafs2"]   = Mode::UNPACK_AFS2;
        map["unpack-afs2"]  = Mode::UNPACK_AFS2;
        map["extractafs2"]  = Mode::UNPACK_AFS2;
        map["extract-afs2"] = Mode::UNPACK_AFS2;

        map["crypt"]        = Mode::ENCRYPT_FILE;
        map["encrypt"]      = Mode::ENCRYPT_FILE;
        map["encrypt-file"] = Mode::ENCRYPT_FILE;

        map["decrypt"]      = Mode::DECRYPT_FILE;
        map["decrypt-file"] = Mode::DECRYPT_FILE;

        map["decryptsave"]  = Mode::DECRYPT_SAVE;
        map["decrypt-save"] = Mode::DECRYPT_SAVE;
        map["encryptsave"]  = Mode::ENCRYPT_SAVE;
        map["encrypt-save"] = Mode::ENCRYPT_SAVE;

        return map;
    }

    std::map<std::string, GameMode> getGameMap()
    {
        std::map<std::string, GameMode> map;
        map["dscs"]         = GameMode::DSCS;
        map["cs"]           = GameMode::DSCS;
        map["cyber-sleuth"] = GameMode::DSCS;

        map["dscs-console"]         = GameMode::DSCS_CONSOLE;
        map["cs-console"]           = GameMode::DSCS_CONSOLE;
        map["cyber-sleuth-console"] = GameMode::DSCS_CONSOLE;

        map["dsts"]          = GameMode::DSTS;
        map["ts"]            = GameMode::DSTS;
        map["time-stranger"] = GameMode::DSTS;

        map["hltla"]        = GameMode::HLTLA;
        map["hundred-line"] = GameMode::HLTLA;
        map["hl"]           = GameMode::HLTLA;
        map["tla"]          = GameMode::HLTLA;
        return map;
    }

    std::map<std::string, CompressMode> getCompressionMap()
    {
        std::map<std::string, CompressMode> map;
        map["normal"]   = CompressMode::NORMAL;
        map["none"]     = CompressMode::NONE;
        map["advanced"] = CompressMode::ADVANCED;
        return map;
    }

    template<typename T>
    void validate_helper(boost::any& value, const std::vector<std::string>& values, const std::map<std::string, T> map)
    {
        boost::program_options::validators::check_first_occurrence(value);
        const std::string& string = boost::program_options::validators::get_single_string(values);
        auto lower =
            string | std::views::transform([](auto a) { return std::tolower(a); }) | std::ranges::to<std::string>();
        auto entry = map.find(lower);

        if (entry != map.end())
            value = entry->second;
        else
            throw boost::program_options::validation_error(
                boost::program_options::validation_error::invalid_option_value);
    }

    void validate(boost::any& value, const std::vector<std::string>& values, Mode* target_type, int)
    {
        static std::map<std::string, Mode> map = getModeMap();
        validate_helper(value, values, map);
    }

    void validate(boost::any& value, const std::vector<std::string>& values, GameMode* target_type, int)
    {
        static std::map<std::string, GameMode> map = getGameMap();
        validate_helper(value, values, map);
    }

} // namespace

namespace dscstools
{
    void validate(boost::any& value, const std::vector<std::string>& values, CompressMode* target_type, int)
    {
        static std::map<std::string, CompressMode> map = getCompressionMap();
        validate_helper(value, values, map);
    }
} // namespace dscstools

int main(int argc, char** argv)
{
    namespace po = boost::program_options;
    po::variables_map vm;
    po::positional_options_description pos;
    po::options_description desc("Usage: DSTSToolsCLI --game=<game> --mode=<mode> <source> <target> [mode options]",
                                 120);

    auto base_options = desc.add_options();
    base_options("help,h", "This text.");
    base_options("game,g", po::value<GameMode>()->required(), "Valid: dscs, dsts, hltla, dscs-console");
    base_options("mode,m",
                 po::value<Mode>()->required(),
                 "pack-mvgl        -> folder in, file out\n"
                 "unpack-mvgl      -> file in, folder out\n"
                 "unpack-mvgl-file -> file in, file out\n"
                 "pack-mbe         -> folder in, file out\n"
                 "unpack-mbe       -> file in, folder out\n"
                 "pack-mbe-dir     -> folder in, folder out\n"
                 "unpack-mbe-dir   -> folder in, folder out\n"
                 "pack-afs2        -> folder in, file out\n"
                 "unpack-afs2      -> file in, folder out\n"
                 "file-encrypt, file-decrypt, save-encrypt, save-decrypt\n"
                 "                 -> file in, file out\n"
                 "Some mods only applies to certain games.");
    base_options("input,i",
                 po::value<std::string>()->required(),
                 "the input path, must point to file or folder, depending on the mode");
    base_options(
        "output,o",
        po::value<std::string>()->required(),
        "the output path, must point to file or folder, depending on the mode.\nWill be created if it doesn't exist.");

    pos.add("input", 1);
    pos.add("output", 1);

    po::options_description pack_desc(
        "MVGL Pack Options\n  Input: Root folder to pack\n  Output: Path of the packed file",
        120);
    auto pack_options = pack_desc.add_options();
    pack_options("compress",
                 po::value<CompressMode>()->default_value(CompressMode::NORMAL, "normal"),
                 "normal   -> use regular compression, as in vanilla files\n"
                 "none     -> use no compression\n"
                 "advanced -> improve compression by deduplicating, slower");

    po::options_description unpack_desc("MVGL Unpack Options", 120);
    auto unpack_options = unpack_desc.add_options();
    unpack_options("file",
                   po::value<std::string>(),
                   "for unpack-mvgl-file, specifies the file to unpack within the MVGL archive");

    desc.add(pack_desc).add(unpack_desc);

    try
    {
        po::store(po::command_line_parser(argc, argv).options(desc).positional(pos).run(), vm);
        po::notify(vm);

        if (vm.count("help")) std::cout << desc;

        auto game = vm["game"].as<GameMode>();
        auto mode = vm["mode"].as<Mode>();

        switch (game)
        {
            case GameMode::DSCS: GameCLI<DSCSModule>::doAction(mode, vm); break;
            case GameMode::DSCS_CONSOLE: GameCLI<DSCSConsoleModule>::doAction(mode, vm); break;
            case GameMode::DSTS: GameCLI<DSTSModule>::doAction(mode, vm); break;
            case GameMode::HLTLA: GameCLI<HLTLAModule>::doAction(mode, vm); break;
            case GameMode::INVALID: std::cout << "Invalid Game\n"; break;
        }
    }
    catch (std::exception& ex)
    {
        if (vm.count("help"))
            std::cout << desc;
        else
            std::cout << ex.what() << std::endl;
    }

    return 0;
}
