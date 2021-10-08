#include "GameCameras.h"

#include <iostream>
#include "HiveWE.h"

void GameCameras::load() {
	BinaryReader reader = hierarchy.map_file_read("war3map.w3c");

	int version = reader.read<uint32_t>();
	if (version != 0) {
		std::cout << "Unknown war3map.w3c version: " << version << " Attempting to load but may crash\n";
	}

	cameras.resize(reader.read<uint32_t>());
	for (auto& i : cameras) {
		i.target_x = reader.read<float>();
		i.target_y = reader.read<float>();
		i.z_offset = reader.read<float>();
		i.rotation = reader.read<float>();
		i.angle_of_attack = reader.read<float>();
		i.distance = reader.read<float>();
		i.roll = reader.read<float>();
		i.fov = reader.read<float>();
		i.far_z = reader.read<float>();
		i.near_z = reader.read<float>();

		if (map->info.game_version_major * 100 + map->info.game_version_minor >= 131) {
			i.local_pitch = reader.read<float>();
			i.local_yaw = reader.read<float>();
			i.local_roll = reader.read<float>();
		}
		i.name = reader.read_c_string();
	}	
}

void GameCameras::save() {
	BinaryWriter writer;
	writer.write<uint32_t>(0);

	writer.write<uint32_t>(cameras.size());
	for (auto& i : cameras) {
		writer.write<float>(i.target_x);
		writer.write<float>(i.target_y);
		writer.write<float>(i.z_offset);
		writer.write<float>(i.rotation);
		writer.write<float>(i.angle_of_attack);
		writer.write<float>(i.distance);
		writer.write<float>(i.roll);
		writer.write<float>(i.fov);
		writer.write<float>(i.far_z);
		writer.write<float>(i.near_z);
		writer.write<float>(i.local_pitch);
		writer.write<float>(i.local_yaw);
		writer.write<float>(i.local_roll);
		writer.write_c_string(i.name);

	}
	hierarchy.map_file_write("war3map.w3c", writer.buffer);
}