#pragma once

#include <string>
#include <vector>

#define GLM_FORCE_CXX17
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include "BinaryReader.h"

enum class UIRace {
	human,
	orc,
	undead,
	night_elf
};

struct ButtonData {
	bool visible;
	std::string chapter_name;
	std::string button_name;
	std::string map;
};
struct MapData {
	std::string unknown;
	std::string map;
};

class CampaignInfo {
  public:
	int campaign_version;
	int editor_version;
	std::string name;
	std::string difficulty;
	std::string author;
	std::string description;

	bool variable_difficulty;
	bool expansion;

	int background_screen_number;
	std::string background_screen_model;
	std::string campaign_image_path;

	int custom_sound_number;
	std::string custom_sound_path;
	int fog_style;
	float fog_start_z_height;
	float fog_end_z_height;
	float fog_density;
	glm::u8vec4 fog_color;
	UIRace race;

	std::vector<ButtonData> buttons;
	std::vector<MapData> map_transitions;

	void load();
	void save() const;
};