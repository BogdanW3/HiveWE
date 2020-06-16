#include "CampaignInfo.h"

#include <iostream>

#include "BinaryWriter.h"

#include "Hierarchy.h"

void CampaignInfo::load() {
	BinaryReader reader = hierarchy.map_file_read("war3campaign.w3f");

	const int version = reader.read<uint32_t>();

	if (version > 1) {
		std::cout << "Unknown war3campaign.w3f version\n";
	}

	campaign_version = reader.read<uint32_t>();
	editor_version = reader.read<uint32_t>();
	name = reader.read_c_string();
	difficulty = reader.read_c_string();
	author = reader.read_c_string();
	description = reader.read_c_string();
	
	const int flags = reader.read<uint32_t>();
	variable_difficulty = flags & 0x0001;
	expansion = flags & 0x0002;

	background_screen_number = reader.read<uint32_t>();
	background_screen_model = reader.read_c_string();
	campaign_image_path = reader.read_c_string();

	custom_sound_number = reader.read<uint32_t>();
	custom_sound_path = reader.read_c_string();

	if (version > 0) {
		fog_style = reader.read<uint32_t>();
		fog_start_z_height = reader.read<float>();
		fog_end_z_height = reader.read<float>();
		fog_density = reader.read<float>();
		fog_color = reader.read<glm::u8vec4>();

		race = static_cast<UIRace>(reader.read<uint32_t>());
	} else {
		fog_style = -1;
		race = 0;
	}

	buttons.resize(reader.read<uint32_t>());
	for (auto&& i : buttons) {
		i.visible = reader.read<uint32_t>();
		i.chapter_name = reader.read_c_string();
		i.button_name = reader.read_c_string();
		i.map = reader.read_c_string();
	}
	map_transitions.resize(reader.read<uint32_t>());
	for (auto&& i : maps) {
		i.unknown = reader.read<uint8_t>();
		i.map = reader.read_c_string();
	}
}