#include <iostream>
#include <cstring>

#include "MDB1.h"
#include "SaveFile.h"
#include "EXPA.h"
#include "AFS2.h"

void printUse() {
	std::cout << "DSCSTools v1.1.0 by SydMontague | https://github.com/SydMontague/DSCSTools/" << std::endl;
	std::cout << "Modes:" << std::endl;
	std::cout << "	--extract <sourceFile> <targetFolder> [--compressed]" << std::endl;
	std::cout << "		Extracts the given MDB1 into a folder." << std::endl;
	std::cout << "		Optional: --compressed | don't decompress files" << std::endl;
	std::cout << "	--extractFile <sourceFile> <targetFolder> <filePath> [--compressed]" << std::endl;
	std::cout << "		Extracts a file from a given MDB1 into a folder." << std::endl;
	std::cout << "		Optional: --compressed | don't decompress file" << std::endl;
	std::cout << "	--pack <sourceFolder> <targetFile> [--disable-compression|--advanced-compression] [--disable-crypt]" << std::endl;
	std::cout << "		Repacks the given folder into an MDB1 (encrypted by default)." << std::endl;
	std::cout << "		Auto - encryption disabled for.orbis.mvgl, .nx64.mvgl, and .psp2.mvgl archives." << std::endl;
	std::cout << "		Optional: --disable-compression. Don't use for archives >= 4GiB!" << std::endl;
	std::cout << "		Optional: --advanced-compression. Doesn't store duplicate data." << std::endl;
	std::cout << "		Optional: --disable-crypt. Packs into an unencrypted MDB1." << std::endl;
	std::cout << "	--mbeextract <source> <targetFolder>" << std::endl;
	std::cout << "		Extracts a .mbe file or a directory of them into CSV, " << std::endl;
	std::cout << "		as long as it's structure is defined in the structure.json file." << std::endl;
	std::cout << "	--mbepack <sourceFolder> <targetFile>" << std::endl;
	std::cout << "		Repacks an .mbe folder containing CSV files back into a .mbe file " << std::endl;
	std::cout << "		as long as it's structure is found and defined in the structure.json file." << std::endl;
	std::cout << "	--afs2extract <source> <targetFolder>" << std::endl;
	std::cout << "		Extracts the given AFS2 formatted archive info a folder." << std::endl;
	std::cout << "	--afs2pack <sourceFolder> <targetFile>" << std::endl;
	std::cout << "		Repacks the given folder info the AFS2 format." << std::endl;
	std::cout << "		File order is alphabetical and relevant, filenames themselves are lost." << std::endl;
	std::cout << "	--crypt <sourceFile> <targetFile>" << std::endl;
	std::cout << "		De- and Encrypts a mvgl file using the keys from DSCS." << std::endl;
	std::cout << "	--savedecrypt <sourceFile> <targetFile>" << std::endl;
	std::cout << "		Decrypts a savefile (system_data.bin, 000X.bin, slot_000X.bin)." << std::endl;
	std::cout << "	--saveencrypt <sourceFile> <targetFile>" << std::endl;
	std::cout << "		Encrypts a savefile (system_data.bin, 000X.bin, slot_000X.bin)." << std::endl;
	std::cout << "	--compress <sourceFile> <targetFile>" << std::endl;
	std::cout << "		Compresses a file using the doboz library." << std::endl;
	std::cout << "	--decompress <sourceFile> <targetFile>" << std::endl;
	std::cout << "		Decompresses a file previously compressed by the doboz library." << std::endl;
}

int main(int argc, char** argv) {
	if (argc < 4) {
		printUse();
		return 0;
	}
	std::filesystem::path source = std::filesystem::exists(argv[2]) ? argv[2] : std::filesystem::current_path().append(argv[2]);
	std::filesystem::path target = argv[3];
	
	if(!target.has_root_directory())
		target = std::filesystem::current_path().append(argv[3]);
	
	try {
		if (strncmp("--extractFile", argv[1], 14) == 0 && argc >= 5) {
			bool decompress = argc < 6 || strncmp("--compressed", argv[5], 13) != 0;

			dscstools::mdb1::extractMDB1File(source, target, argv[4], decompress);
			std::cout << "Done" << std::endl;
		}
		else if (strncmp("--extract", argv[1], 10) == 0) {
			bool decompress = argc < 5 || strncmp("--compressed", argv[4], 13) != 0;

			dscstools::mdb1::extractMDB1(source, target, decompress);
			std::cout << "Done" << std::endl;
		}
		else if (strncmp("--pack", argv[1], 7) == 0) {
			dscstools::mdb1::CompressMode mode = dscstools::mdb1::CompressMode::normal;
			bool doCrypt = false;

			if (argc >= 5) {
				if (strncmp("--disable-compression", argv[4], 22) == 0)
					mode = dscstools::mdb1::CompressMode::none;
				else if (strncmp("--advanced-compression", argv[4], 23) == 0)
					mode = dscstools::mdb1::CompressMode::advanced;
				else if (strncmp("--disable-crypt", argv[4], 15) == 0)
					doCrypt = false;
			}

			if (argc >= 6) {
				if (strncmp("--disable-crypt", argv[5], 15) == 0)
					doCrypt = false;
			}

			std::string filename = target.filename().string();
			if ((filename.substr(filename.size() - 10) == "orbis.mvgl") ||
				(filename.substr(filename.size() -  9) == "psp2.mvgl")  ||
				(filename.substr(filename.size() -  9) == "nx64.mvgl"))
				doCrypt = false;

			if (doCrypt)
				std::cout << "Will encrypt." << std::endl;
			else
				std::cout << "Will not encrypt." << std::endl;
			dscstools::mdb1::packMDB1(source, target, mode, doCrypt, std::cout);
			std::cout << "Done" << std::endl;
		}
		else if (strncmp("--crypt", argv[1], 8) == 0) {
			dscstools::mdb1::cryptFile(source, target);
			std::cout << "Done" << std::endl;
		}
		else if (strncmp("--savedecrypt", argv[1], 14) == 0) {
			dscstools::savefile::decryptSaveFile(source, target);
			std::cout << "Done" << std::endl;
		}
		else if (strncmp("--saveencrypt", argv[1], 14) == 0) {
			dscstools::savefile::encryptSaveFile(source, target);
			std::cout << "Done" << std::endl;
		}
		else if (strncmp("--mbeextract", argv[1], 13) == 0) {
			dscstools::mbe::extractMBE(source, target);
			std::cout << "Done" << std::endl;
		}
		else if (strncmp("--mbepack", argv[1], 10) == 0) {
			dscstools::mbe::packMBE(source, target);
			std::cout << "Done" << std::endl;
		}
		else if (strncmp("--afs2extract", argv[1], 14) == 0) {
			dscstools::afs2::extractAFS2(source, target);
			std::cout << "Done" << std::endl;
		}
		else if (strncmp("--afs2pack", argv[1], 11) == 0) {
			dscstools::afs2::packAFS2(source, target);
			std::cout << "Done" << std::endl;
		}
		else if (strncmp("--compress", argv[1], 11) == 0) {
			dscstools::mdb1::dobozCompress(source, target);
			std::cout << "Done" << std::endl;
		}
		else if (strncmp("--decompress", argv[1], 11) == 0) {
			dscstools::mdb1::dobozDecompress(source, target);
			std::cout << "Done" << std::endl;
		}
		else {
			printUse();
		}
	}
	catch (std::exception& ex) {
		std::cout << ex.what() << std::endl;
	}

	return 0;
}
