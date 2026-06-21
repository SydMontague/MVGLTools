# MVGLTools
This tool provides support to pack, unpack, decrypt and encrypt various file formats found in games using the game engine built and used by Media.Vision, appreviated "MVGL" (the exact meaning is unclear, probably Media.Vision Game Library).

Currently supported games are:
- Digimon Story: Cyber Sleuth (DSCS)
- Digimon Story: Time Stranger (DSTS)
- The Hundred Line -Last Defense Academy- (THL)
- Chaos Rings III (CR3)

Other games *might* work with one of these presets. If you want other games to be added, please open an issue or contact me.

The tool can also be used as library, for your own tools. Your milage might vary, though.

# Current Features
* Unpack MDB1 (.mvgl) archives
* Unpack individual file from MDB1 (.mvgl) archives
* Repack/Create MDB1 (.mvgl) archives
  * archives get recreated from scratch, files can be added, removed and modified at will
  * optional: with advanced compression, storing identical data only once. ~5% size improvement
  * optional: without compressing the file (faster build), final archive must be <= 4 GiB in size
* Unpack and repack MBE files
* Unpack and repack AFS2 archives
  * The resulting files are in the HCA format. You can use [vgmstream](https://github.com/vgmstream/vgmstream) and [VGAudio](https://github.com/Thealexbarney/VGAudio) to convert them.
  * Note: You'll need a newer version of VGAudio that supports encrypting HCA files.
* Decrypt and Encrypt game files, if the game does that
  * Currently only Cyber Sleuth does this. This is not necessary for .mvgl extraction, as it performs this transparently.
* Decrypt and Encrypt PC save files
  * Currently only supported for Cyber Sleuth

# Usage
The tool is command line only. The MBE functionality requires you to have the `structures` folder relative from where you're calling the tool from.
Hence it's recommended to navigate to the folder containing the DSCSTools binary first.

```
MVGLToolsCLI --game=<game> --mode=<mode> <source> <target> [mode options]
```

## --game

Valid values are:
- dscs
  - for Digimon Story: Cyber Sleuth Complete Edition on PC
	- transparently de-/encrypts mvgl files
	- requires structure files for MBE, see MBE section
	- MVGL limited to 4GiB in size
- dscs-console
  - for Digimon Story: Cyber Sleuth Complete Edition on console or with decrypted PC assets
	- requires structure files for MBE, see MBE section
	- MVGL limited to 4GiB in size
- dsts
  - for Digimon Story: Time Stranger
	- optional structure files for MBE
- dsts-switch
  - for Digimon Story: Time Stranger on Nintendo Switch
	- optional structure files for MBE
- thl
  - for The Hundred Line -Last Defense Academy-
	- optional structure files for MBE
- cr3
	- for Chaos Rings III
	- requires structure files for MBE, see MBE section
	  - structure files not included

## --mode

### unpack-mvgl
Unpacks a MVGL file from `source` into the folder given by `target`. If the game uses asset encryption, it will be dealt with transparently.

### pack-mvgl
Packs a MVGL file from a folder `source` and saves it into the file given by `target`. If the game uses asset encryption, it will be encrypted transparently.

You can use the `--compress=<level>` option to specify how the files in the archive will be compressed.

* `normal` - the regular compression, as in vanilla
* `none` - no compression at all (faster builds, very large file sizes)
* `advanced` - improve compression by deduplicating data (slower builds, slightly smaller file sizes)

### unpack-mbe / unpack-mbe-dir
Unpacks a .mbe file/a folder of .mbe files into CSV from `source` into a folder given by `target`.
See the section on structure files.

### pack-mbe / pack-mbe-dir
Packs a .mbe file/a folder of .mbe files into CSV from `source` folder into a file/folder given by `target`.
See the section on structure files.

### dump-structures
Creates a `structure.json` entry for every readable `.mbe` file in a given `source` folder, searching recursively, and stores it in the `target` folder.
These are intended to be used as a base for filling the `structures` folder with meaningful data.

### pack-afs2 / unpack-afs2
Packs/unpacks a AFS2 formatted archive. 

The resulting files are in the HCA format. You can use [vgmstream](https://github.com/vgmstream/vgmstream) and [VGAudio](https://github.com/Thealexbarney/VGAudio) to convert them.

This is only supported by DSCS. Other games don't seem to use this format anymore.

### file-encrypt / file-decrypt
Encrypts/Decrypts an asset file using the game's asset encryption algorithm.

This is only supported by DSCS. Other games don't encrypt their assets.
For DSCS this operation is symetrical (so both operations do exactly the same).

### save-encryt / save-decrypt
Encrypts/Decrypts a save file.

This is currently only supported by DSCS.
To decrypt the saves of other games:
* DSTS
  * `openssl enc -d -aes-128-ecb -K 33393632373736373534353535383833 -in 0004.bin -out decrypted_save.bin -nopad`
* TLA
  * `openssl enc -d -aes-128-ecb -K bb3d99be083b97c62b14f8736eb30e39 -in 0004.bin -out decrypted_save.bin -nopad`

## MBE files
MBE Files contain a number of data tables and get extracted by the tool into CSV files that can be easily modified.
**Do not use Microsoft Excel to modify extracted CSV files, it does *not* create RFC 4180 compliant CSV.** Use LibreOffice/OpenOffice as an alternative.

In order for the MBE functions to work it to know the underlying data structure. For some games (DSCS, CR3) this has to be provided from the outside, while others (DSTS, THL) stored it in the file itself, albeit without names.

So in order to provide the structure and/or names, the `structures` folder and its contents should exist within the current working directory (i.e. the folder your terminal currently is in). For each game there is a subfolder (note: dscs and dscs-console are treated as the same), which each containing a `structure.json`.
That file contains a simple `regexPattern: structureDefinition.json` associations. The tool matches the currently handled file path with the patterns and picks the first match.
As these lookups happen both when unpacking and packing, it's important to maintain folder structures and files names, as otherwise file creation might fail or result in incompatible files.


### structureDefinition.json
A structureDefinition.json contains one or more names tables of `name: fieldType` mappings. 

Currently supported field types are:
* int8
* int16
* int32
* float
* string
* string2
* string3
* bool
* empty

The exact difference between the string types is yet unknown. 

#### Example
```jsonc
{
	# without the leading 000_
	"tableName1": {
		"fieldName1": "int8",
		"fieldName2": "float",
		"fieldNameN": "int32 array"
	},
	"tableName2": {
		"fieldName1": "string",
		"fieldName2": "int32",
		"fieldNameN": "int32"
	},
	"tableNameN": {
		"fieldName1": "bool",
		"fieldName2": "bool",
		"fieldNameN": "int8"
	}
}
```

# Credits
The tool uses:
* the [doboz compression library](https://voxelium.wordpress.com/2011/03/19/doboz-compression-library-with-very-fast-decompression/). [License Notice](https://github.com/SydMontague/DSCSTools/blob/master/libs/doboz/COPYING.txt)
* the [lz4 compression library](https://github.com/lz4/lz4). 
* AriaFallah's [csv-parser](https://github.com/AriaFallah/csv-parser). [License Notice](https://github.com/SydMontague/DSCSTools/blob/master/libs/csv-parser/LICENSE)

# Contact
* Discord: SydMontague#8056, or in either the [Digimon Modding Community](https://discord.gg/cb5AuxU6su) or [Digimon Discord Community](https://discord.gg/0VODO3ww0zghqOCO)
* directly on GitHub
* E-Mail: sydmontague@web.de
* Reddit: [/u/Sydmontague](https://reddit.com/u/sydmontague)

# Other DSCS Modding Projects/Tools
* [SimpleDSCSModManager](https://github.com/Pherakki/SimpleDSCSModManager) by Pherakki
  * uses an older version of this tool under the hood
* [Blender-Tools-for-DSCS](https://github.com/Pherakki/Blender-Tools-for-DSCS/) by Pherakki
  * partial updated fork for DSTS/THL: https://github.com/Romsstar/Blender-Tools-for-DSCS/
* [NutCracker](https://github.com/SydMontague/NutCracker)
  * a decompiler for the game's Squirrel script files
