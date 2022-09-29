module;

#include <vector>
#include <filesystem>
#include <map>
//#include <QSettings>
#include <iostream>
#include <fstream>
#include <fmt/format.h>

export module Hierarchy;

namespace fs = std::filesystem;
using namespace std::literals::string_literals;

import BinaryReader;
import CASC;
import JSON;
import MPQ;
import no_init_allocator;

export class Hierarchy {
  public:
	std::string tex_ext = ".dds";
	char tileset = 'L';
	casc::CASC game_data;
	json::JSON aliases;

	mpq::MPQ war3tileset;
	mpq::MPQ war3Mod;
	mpq::MPQ war3xLocal;
	mpq::MPQ war3x;
	mpq::MPQ war3Local;
	mpq::MPQ war3;
	mpq::MPQ war3ce;
	mpq::MPQ deprecated;

	fs::path map_directory;
	fs::path warcraft_directory;
	fs::path root_directory;

	bool ptr = false;
	bool hd = true;
	bool teen = false;
	bool local_files = true;
	bool w3ce = false;

	bool load(fs::path directory) {
		warcraft_directory = directory;
		/*QSettings settings;
		ptr = settings.value("flavour", "Retail").toString() != "Retail";
		hd = settings.value("hd", "True").toString() != "False";
		teen = settings.value("teen", "False").toString() != "False";
		QSettings war3reg("HKEY_CURRENT_USER\\Software\\Blizzard Entertainment\\Warcraft III", QSettings::NativeFormat);
		local_files = war3reg.value("Allow Local Files", 0).toInt() != 0;*/

		if (w3ce) {
			tex_ext = ".blp";
			std::cout << "Loading MPQ data from: " << warcraft_directory << "\n";
			deprecated.open(warcraft_directory / L"Deprecated.mpq", 0x100);
			war3ce.open(warcraft_directory / L"war3ce.mpq", 0x100);
			war3Mod.open(warcraft_directory / L"War3Mod.mpq", 0x100);
			war3xLocal.open(warcraft_directory / L"War3xlocal.mpq", 0x100);
			war3x.open(warcraft_directory / L"War3x.mpq", 0x100);
			war3Local.open(warcraft_directory / L"War3local.mpq", 0x100);
			if (!war3.open(warcraft_directory / L"War3.mpq", 0x100)) {
				return false;
			}
			reload_tileset();
			return true;
		} else {
			tex_ext = ".dds";
			std::cout << "Loading CASC data from: " << warcraft_directory << "\n";
			bool open = game_data.open(warcraft_directory / (ptr ? ":w3t" : ":w3"));
			root_directory = warcraft_directory / (ptr ? "_ptr_" : "_retail_");

			if (open) {
				aliases.load(open_file("filealiases.json"));
			}
			return open;
		}
	}

	casc::File open_from_casc(const fs::path& path) const {
		casc::File file;

		if (hd && game_data.file_exists("war3.w3mod:_hd.w3mod:_tilesets/"s + tileset + ".w3mod:"s + path.string())) {
			file = game_data.file_open("war3.w3mod:_hd.w3mod:_tilesets/"s + tileset + ".w3mod:"s + path.string());
		} else if (hd && teen && game_data.file_exists("war3.w3mod:_hd.w3mod:_teen.w3mod:"s + path.string())) {
			file = game_data.file_open("war3.w3mod:_hd.w3mod:_teen.w3mod:"s + path.string());
		} else if (hd && game_data.file_exists("war3.w3mod:_hd.w3mod:"s + path.string())) {
			file = game_data.file_open("war3.w3mod:_hd.w3mod:"s + path.string());
		} else if (game_data.file_exists("war3.w3mod:_tilesets/"s + tileset + ".w3mod:"s + path.string())) {
			file = game_data.file_open("war3.w3mod:_tilesets/"s + tileset + ".w3mod:"s + path.string());
		} else if (game_data.file_exists("war3.w3mod:_locales/enus.w3mod:"s + path.string())) {
			file = game_data.file_open("war3.w3mod:_locales/enus.w3mod:"s + path.string());
		} else if (teen && game_data.file_exists("war3.w3mod:_teen.w3mod:"s + path.string())) {
			file = game_data.file_open("war3.w3mod:_teen.w3mod:"s + path.string());
		} else if (game_data.file_exists("war3.w3mod:"s + path.string())) {
			file = game_data.file_open("war3.w3mod:"s + path.string());
		} else if (game_data.file_exists("war3.w3mod:_deprecated.w3mod:"s + path.string())) {
			file = game_data.file_open("war3.w3mod:_deprecated.w3mod:"s + path.string());
		} else {
			//fmt::print("{} could not be found in the hierarchy", path.string());
		}
		return file;
	}

	
	mpq::File open_from_mpq(const fs::path& path) const {
		mpq::File file;

		if (war3Mod.file_exists(path)) {
			file = war3Mod.file_open(path);
		} else if (war3tileset.file_exists(path)) {
			file = war3tileset.file_open(path);
		} else if (war3xLocal.file_exists(path)) {
			file = war3xLocal.file_open(path);
		} else if (war3x.file_exists(path)) {
			file = war3x.file_open(path);
		} else if (war3Local.file_exists(path)) {
			file = war3Local.file_open(path);
		} else if (war3.file_exists(path)) {
			file = war3.file_open(path);
		} else if (deprecated.file_exists(path)) {
			file = deprecated.file_open(path);
		} else {
			fmt::print("{} could not be found in the hierarchy", path.string());
		}
		return file;
	}

	BinaryReader open_file(const fs::path& path) const {
		if (w3ce && war3ce.file_exists(path)) {
			return BinaryReader(war3ce.file_open(path).read());
		} else if (local_files && fs::exists(root_directory / path)) {
			std::ifstream stream(root_directory / path, std::ios::binary);
			return BinaryReader(std::vector<uint8_t, default_init_allocator<uint8_t>>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()));
		} else if (hd && teen && map_file_exists("_hd.w3mod:_teen.w3mod:" + path.string())) {
			return map_file_read("_hd.w3mod:_teen.w3mod:" + path.string());
		} else if (hd && map_file_exists("_hd.w3mod:" + path.string())) {
			return map_file_read("_hd.w3mod:" + path.string());
		} else if (map_file_exists(path)) {
			return map_file_read(path);
		}  
		
		if (w3ce) {
			return BinaryReader(open_from_mpq(path).read());
		}
		else {
			if (file_exists(path)) {
				return BinaryReader(open_from_casc(path).read());
			} else if (aliases.exists(path.string())) {
				return open_file(aliases.alias(path.string()));
			}
			fmt::print("{} could not be found in the hierarchy", path.string());
			return BinaryReader(std::vector<uint8_t, default_init_allocator<uint8_t>>());
	    }

	}

	bool file_exists(const fs::path& path) const {
		if (path.empty()) {
			return false;
		}

		return 
			(local_files && fs::exists(root_directory / path)) ||
			(hd && teen && map_file_exists("_hd.w3mod:_teen.w3mod:" + path.string())) ||
			(hd && map_file_exists("_hd.w3mod:" + path.string())) ||
			map_file_exists(path) ||
			(w3ce && (
			war3ce.file_exists(path) ||
			war3Mod.file_exists(path) ||
			war3tileset.file_exists(path) ||
			war3xLocal.file_exists(path) ||
			war3x.file_exists(path) ||
			war3Local.file_exists(path) ||
			war3.file_exists(path) ||
			deprecated.file_exists(path))) ||
			(!w3ce && (
			(hd && game_data.file_exists("war3.w3mod:_hd.w3mod:_tilesets/"s + tileset + ".w3mod:"s + path.string())) ||
			(hd && teen && game_data.file_exists("war3.w3mod:_hd.w3mod:_teen.w3mod:"s + path.string())) ||
			(hd && game_data.file_exists("war3.w3mod:_hd.w3mod:"s + path.string())) ||
			game_data.file_exists("war3.w3mod:_tilesets/"s + tileset + ".w3mod:"s + path.string()) ||
			game_data.file_exists("war3.w3mod:_locales/enus.w3mod:"s + path.string()) ||
			(teen && game_data.file_exists("war3.w3mod:_teen.w3mod:"s + path.string())) ||
			game_data.file_exists("war3.w3mod:"s + path.string()) ||
			game_data.file_exists("war3.w3mod:_deprecated.w3mod:"s + path.string()) ||
			(aliases.exists(path.string()) ? file_exists(aliases.alias(path.string())) : false)));
	}

	BinaryReader map_file_read(const fs::path& path) const {
		std::ifstream stream(map_directory / path, std::ios::binary);
		return BinaryReader(std::vector<uint8_t, default_init_allocator<uint8_t>>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()));
	}

	/// source somewhere on disk, destination relative to the map
	void map_file_add(const fs::path& source, const fs::path& destination) const {
		fs::copy_file(source, map_directory / destination, fs::copy_options::overwrite_existing);
	}

	void map_file_write(const fs::path& path, const std::vector<uint8_t>& data) const {
		std::ofstream outfile(map_directory / path, std::ios::binary);

		if (!outfile) {
			throw std::runtime_error("Error writing file " + path.string());
		}

		outfile.write(reinterpret_cast<char const*>(data.data()), data.size());
	}

	void map_file_remove(const fs::path& path) const {
		fs::remove(map_directory / path);
	}

	bool map_file_exists(const fs::path& path) const {
		return fs::exists(map_directory / path);
	}

	void map_file_rename(const fs::path& original, const fs::path& renamed) const {
		fs::rename(map_directory / original, map_directory / renamed);
	}

	//Only for pre-CASC
	void reload_tileset() {
		war3tileset.close();
		if (!w3ce) return;
		const std::string file_name = tileset + ".mpq"s;

		mpq::File tileset_mpq = open_from_mpq(file_name);

		war3tileset.open(tileset_mpq, 0x100);
	}
};

export inline Hierarchy hierarchy;