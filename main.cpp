#include <iostream>
#include <set>
#include <boost/filesystem.hpp>
#include "genie/dat/DatFile.h"
#include "wololo/fix.h"
#include "wololo/Drs.h"
#include "fixes/berbersutfix.h"
#include "fixes/demoshipfix.h"
#include "fixes/portuguesefix.h"


using namespace std;

string const version = "1.0-beta2";

void fileCopy(string const src, string const dst) {
	ifstream srcStream(src, std::ios::binary);
	ofstream dstStream(dst, std::ios::binary);
	dstStream << srcStream.rdbuf();
}

void listAssetFiles(string const path, vector<string> *listOfSlpFiles, std::vector<string> *listOfWavFiles) {
	const set<string> exclude = {
		// Exclude Forgotten Empires leftovers
		"50163", // Forgotten Empires loading screen
		"50189", // Forgotten Empires main menu
		"53207", // Forgotten Empires in-game logo
		"53208", // Forgotten Empires objective window
		"53209" // ???
	};
	for (boost::filesystem::directory_iterator end, it(path); it != end; it++) {
		string fileName = it->path().stem().string();
		if (exclude.find(fileName) == exclude.end()) {
			string extension = it->path().extension().string();
			if (extension == ".slp") {
				listOfSlpFiles->push_back(fileName);
			}
			else if (extension == ".wav") {
				listOfWavFiles->push_back(fileName);
			}
		}
	}
}

void convertLanguageFile(std::ifstream *in, std::ofstream *out) {
	string line;
	while (getline(*in, line)) {
		int spaceIdx = line.find(' ');
		string number = line.substr(0, spaceIdx);
		try {
			stoi(number);
		}
		catch (invalid_argument const & e){
			continue;
		}
		*out << number << '=' << line.substr(spaceIdx+2, line.size() - spaceIdx - 3) << endl;
	}
}

void makeDrs(string const inputDir ,std::ofstream *out) {
	const int numberOfTables = 2; // slp and wav

	vector<string> slpFilesNames;
	vector<string> wavFilesNames;
	listAssetFiles(inputDir, &slpFilesNames, &wavFilesNames);

	int numberOfSlpFiles = slpFilesNames.size();
	int numberOfWavFiles = wavFilesNames.size();
	int offsetOfFirstFile = sizeof (wololo::DrsHeader) +
			sizeof (wololo::DrsTableInfo) * numberOfTables +
			sizeof (wololo::DrsFileInfo) * (numberOfSlpFiles + numberOfWavFiles);
	int offset = offsetOfFirstFile;


	// file infos

	vector<wololo::DrsFileInfo> slpFiles;
	vector<wololo::DrsFileInfo> wavFiles;

	for (vector<string>::iterator it = slpFilesNames.begin(); it != slpFilesNames.end(); it++) {
		wololo::DrsFileInfo slp;
		size_t size = boost::filesystem::file_size(inputDir + "/" + *it + ".slp");
		slp.file_id = stoi(*it);
		slp.file_data_offset = offset;
		slp.file_size = size;
		offset += size;
		slpFiles.push_back(slp);
	}
	for (vector<string>::iterator it = wavFilesNames.begin(); it != wavFilesNames.end(); it++) {
		wololo::DrsFileInfo wav;
		size_t size = boost::filesystem::file_size(inputDir + "/" + *it + ".wav");
		wav.file_id = stoi(*it);
		wav.file_data_offset = offset;
		wav.file_size = size;
		offset += size;
		wavFiles.push_back(wav);
	}

	// header infos

	wololo::DrsHeader const header = {
		{ 'C', 'o', 'p', 'y', 'r',
		  'i', 'g', 'h', 't', ' ',
		  '(', 'c', ')', ' ', '1',
		  '9', '9', '7', ' ', 'E',
		  'n', 's', 'e', 'm', 'b',
		  'l', 'e', ' ', 'S', 't',
		  'u', 'd', 'i', 'o', 's',
		  '.', '\x1a' }, // copyright
		{ '1', '.', '0', '0' }, // version
		{ 't', 'r', 'i', 'b', 'e' }, // ftype
		numberOfTables, // table_count
		offsetOfFirstFile // file_offset
	};

	// table infos

	wololo::DrsTableInfo const slpTableInfo = {
		0x20, // file_type, MAGIC
		{ 'p', 'l', 's' }, // file_extension, "slp" in reverse
		sizeof (wololo::DrsHeader) + sizeof (wololo::DrsFileInfo) * numberOfTables, // file_info_offset
		slpFiles.size() // num_files
	};
	wololo::DrsTableInfo const wavTableInfo = {
		0x20, // file_type, MAGIC
		{ 'v', 'a', 'w' }, // file_extension, "wav" in reverse
		sizeof (wololo::DrsHeader) + sizeof (wololo::DrsFileInfo) * numberOfTables + sizeof (wololo::DrsFileInfo) * slpFiles.size(), // file_info_offset
		wavFiles.size() // num_files
	};


	// now write the actual drs file

	// header
	out->write(header.copyright, sizeof (wololo::DrsHeader::copyright));
	out->write(header.version, sizeof (wololo::DrsHeader::version));
	out->write(header.ftype, sizeof (wololo::DrsHeader::ftype));
	out->write(reinterpret_cast<const char *>(&header.table_count), sizeof (wololo::DrsHeader::table_count));
	out->write(reinterpret_cast<const char *>(&header.file_offset), sizeof (wololo::DrsHeader::file_offset));

	// table infos
	out->write(reinterpret_cast<const char *>(&slpTableInfo.file_type), sizeof (wololo::DrsTableInfo::file_type));
	out->write(slpTableInfo.file_extension, sizeof (wololo::DrsTableInfo::file_extension));
	out->write(reinterpret_cast<const char *>(&slpTableInfo.file_info_offset), sizeof (wololo::DrsTableInfo::file_info_offset));
	out->write(reinterpret_cast<const char *>(&slpTableInfo.num_files), sizeof (wololo::DrsTableInfo::num_files));

	out->write(reinterpret_cast<const char *>(&wavTableInfo.file_type), sizeof (wololo::DrsTableInfo::file_type));
	out->write(wavTableInfo.file_extension, sizeof (wololo::DrsTableInfo::file_extension));
	out->write(reinterpret_cast<const char *>(&wavTableInfo.file_info_offset), sizeof (wololo::DrsTableInfo::file_info_offset));
	out->write(reinterpret_cast<const char *>(&wavTableInfo.num_files), sizeof (wololo::DrsTableInfo::num_files));

	// file infos
	for (vector<wololo::DrsFileInfo>::iterator it = slpFiles.begin(); it != slpFiles.end(); it++) {
		out->write(reinterpret_cast<const char *>(&it->file_id), sizeof (wololo::DrsFileInfo::file_id));
		out->write(reinterpret_cast<const char *>(&it->file_data_offset), sizeof (wololo::DrsFileInfo::file_data_offset));
		out->write(reinterpret_cast<const char *>(&it->file_size), sizeof (wololo::DrsFileInfo::file_size));
	}

	for (vector<wololo::DrsFileInfo>::iterator it = wavFiles.begin(); it != wavFiles.end(); it++) {
		out->write(reinterpret_cast<const char *>(&it->file_id), sizeof (wololo::DrsFileInfo::file_id));
		out->write(reinterpret_cast<const char *>(&it->file_data_offset), sizeof (wololo::DrsFileInfo::file_data_offset));
		out->write(reinterpret_cast<const char *>(&it->file_size), sizeof (wololo::DrsFileInfo::file_size));
	}

	// now write the actual files
	for (vector<string>::iterator it = slpFilesNames.begin(); it != slpFilesNames.end(); it++) {
		ifstream srcStream(inputDir + "/" + *it + ".slp", std::ios::binary);
		*out << srcStream.rdbuf();
	}

	for (vector<string>::iterator it = wavFilesNames.begin(); it != wavFilesNames.end(); it++) {
		ifstream srcStream(inputDir + "/" + *it + ".wav", std::ios::binary);
		*out << srcStream.rdbuf();
	}
}

void uglyHudHack(string const inputDir) {
	/*
	 * copies the Slav hud files for AK civs, the good way of doing this would be to extract
	 * the actual AK civs hud files from
	 * Age2HD\resources\_common\slp\game_b[24-27].slp correctly, but I haven't found a way yet
	 */
	int const slavHudFiles[] = {51123, 51153, 51183};
	for (size_t baseIndex = 0; baseIndex < sizeof slavHudFiles / sizeof (int); baseIndex++) {
		for (size_t i = 1; i <= 4; i++) {
			string dst = inputDir + "/" + to_string(slavHudFiles[baseIndex]+i) + ".slp";
			if (! (boost::filesystem::exists(dst) && boost::filesystem::file_size(dst)) > 0) {
				string src = inputDir + "/" + to_string(slavHudFiles[baseIndex]) + ".slp";
				fileCopy(src, dst);
			}
		}
	}
}

void cleanTheUglyHudHack(string const inputDir) {
	int const slavHudFiles[] = {51123, 51153, 51183};
	for (size_t baseIndex = 0; baseIndex < sizeof slavHudFiles / sizeof (int); baseIndex++) {
		for (size_t i = 1; i <= 4; i++) {
			boost::filesystem::remove(inputDir + "/" + to_string(slavHudFiles[baseIndex]+i) + ".slp");
		}
	}
}

void copyCivIntroSounds(string const inputDir, string const outputDir) {
	string const civs[] = {"italians", "indians", "incas", "magyars", "slavs",
						   "portuguese", "ethiopians", "malians", "berbers"};
	for (size_t i = 0; i < sizeof civs / sizeof (string); i++) {
		fileCopy(inputDir + "/" + civs[i] + ".mp3", outputDir + "/" + civs[i] + ".mp3");
	}
}

void transferHdDatElements(genie::DatFile *hdDat, genie::DatFile *aocDat) {
	aocDat->Sounds = hdDat->Sounds;
	aocDat->GraphicPointers = hdDat->GraphicPointers;
	aocDat->Graphics = hdDat->Graphics;
	aocDat->Techages = hdDat->Techages;
	aocDat->UnitHeaders = hdDat->UnitHeaders;
	aocDat->Civs = hdDat->Civs;
	aocDat->Researchs = hdDat->Researchs;
	aocDat->UnitLines = hdDat->UnitLines;
	aocDat->TechTree = hdDat->TechTree;
}


int main(int argc, char *argv[]) {

	string const aocDatPath = "../resources/_common/dat/empires2_x1_p1.dat";
	string const hdDatPath = "../resources/_common/dat/empires2_x2_p1.dat";
	string const keyValuesStringsPath = "../resources/en/strings/key-value/key-value-strings-utf8.txt"; // TODO pick other languages
	string const languageIniPath = "out/Voobly Mods/AOC/Data Mods/AK/language.ini";
	string const versionIniPath = "out/Voobly Mods/AOC/Data Mods/AK/version.ini";
	string const civIntroSoundsInputPath = "../resources/_common/sound/civ/";
	string const civIntroSoundsOutputPath = "out/Voobly Mods/AOC/Data Mods/AK/Sound/stream/";
	string const xmlPath = "age2_x1.xml";
	string const xmlOutPath = "out/Voobly Mods/AOC/Data Mods/AK/age2_x1.xml";
	string const drsOutPath = "out/Voobly Mods/AOC/Data Mods/AK/Data/gamedata_x1_p1.drs";
	string const assetsPath = "../resources/_common/drs/gamedata_x2/";
	string const outputDatPath = "out/Voobly Mods/AOC/Data Mods/AK/Data/empires2_x1_p1.dat";


	int ret = 0;
	try {
		cout << "WololoKingdoms ver. " << version << endl;

		cout << "Opening the AOC dat file..." << endl << endl;
		genie::DatFile aocDat;
		aocDat.setVerboseMode(true);
		aocDat.setGameVersion(genie::GameVersion::GV_TC);
		aocDat.load(aocDatPath.c_str());

		cout << endl << "Opening the AOE2HD dat file..." << endl << endl;
		genie::DatFile hdDat;
		hdDat.setVerboseMode(true);
		hdDat.setGameVersion(genie::GameVersion::GV_Cysion);
		hdDat.load(hdDatPath.c_str());

		cout << endl << "Converting the language file..." << endl;
		ifstream langIn(keyValuesStringsPath);
		ofstream langOut(languageIniPath);
		convertLanguageFile(&langIn, &langOut);

		cout << "Preparing ressources files..." << endl;
		ofstream versionOut(versionIniPath);
		versionOut << version << endl;
		copyCivIntroSounds(civIntroSoundsInputPath, civIntroSoundsOutputPath);
		fileCopy(xmlPath, xmlOutPath);
		ofstream drsOut(drsOutPath, std::ios::binary);

		cout << "Generating gamedata_x1_p1.drs..." << endl;
		uglyHudHack(assetsPath);
		makeDrs(assetsPath, &drsOut);
		cleanTheUglyHudHack(assetsPath);


		cout << "Generating empires2_x1_p1.dat..." << endl;
		transferHdDatElements(&hdDat, &aocDat);

		wololo::Fix fixes[] = {
			wololo::berbersUTFix,
			wololo::demoShipFix,
			wololo::portugueseFix
		};
		for (size_t i = 0, nbPatches = sizeof fixes / sizeof (wololo::Fix); i < nbPatches; i++) {
			cout << "Applying patch " << i+1 << " of " << nbPatches << " : " << fixes[i].name << endl;
			fixes[i].patch(&aocDat);
		}


		cout << endl;

		aocDat.saveAs(outputDatPath.c_str());

		cout << endl << "Conversion complete, you can put \"out/Voobly Mods/\" into your AOE2 folder" << endl;
	}
	catch (exception const & e) {
		cerr << e.what() << endl;
		ret = 1;
	}
	if (argc == 1) { // any argument = non-blocking install
		cout << "Press enter to exit..." << endl;
		cin.get();
	}
	return ret;
}