module;

#include <algorithm>
#include <nlohmann/json.hpp>

export module CollaborationSnapshot;

import std;
import Map;
import Terrain;
import PathingMap;
import Units;
import Doodads;
import Doodad;
import Utilities;
import <glm/glm.hpp>;


export namespace collaboration {

nlohmann::json serialize_map_snapshot(const Map& map);
void deserialize_map_snapshot(const nlohmann::json& message, Map& map);

}

namespace {

using json = nlohmann::json;

json vec2_to_json(const glm::vec2& value) {
	return json::array({value.x, value.y});
}

json vec3_to_json(const glm::vec3& value) {
	return json::array({value.x, value.y, value.z});
}

glm::vec2 vec2_from_json(const json& value) {
	return {value.at(0).get<float>(), value.at(1).get<float>()};
}

glm::vec3 vec3_from_json(const json& value) {
	return {value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>()};
}

json item_sets_to_json(const std::vector<ItemSet>& item_sets) {
	json result = json::array();
	for (const auto& item_set : item_sets) {
		json items = json::array();
		for (const auto& [chance, id] : item_set.items) {
			items.push_back(json{{"chance", chance}, {"id", id}});
		}
		result.push_back(items);
	}
	return result;
}

std::vector<ItemSet> item_sets_from_json(const json& value) {
	std::vector<ItemSet> result;
	result.reserve(value.size());
	for (const auto& item_set_json : value) {
		ItemSet item_set;
		for (const auto& item_json : item_set_json) {
			item_set.items.emplace_back(item_json.at("chance").get<int>(), item_json.at("id").get<std::string>());
		}
		result.push_back(std::move(item_set));
	}
	return result;
}

json terrain_to_json(const Terrain& terrain) {
	json corners = json::array();
	for (size_t i = 0; i < terrain.corner_height.size(); ++i) {
		corners.push_back(json{
			{"height", terrain.corner_height[i]},
			{"water_height", terrain.corner_water_height[i]},
			{"ground_texture", terrain.corner_ground_texture[i]},
			{"ground_variation", terrain.corner_ground_variation[i]},
			{"cliff_variation", terrain.corner_cliff_variation[i]},
			{"cliff_texture", terrain.corner_cliff_texture[i]},
			{"layer_height", terrain.corner_layer_height[i]},
			{"map_edge", terrain.corner_map_edge[i]},
			{"ramp", terrain.corner_ramp[i]},
			{"blight", terrain.corner_blight[i]},
			{"water", terrain.corner_water[i]},
			{"boundary", terrain.corner_boundary[i]},
			{"cliff", terrain.corner_cliff[i]},
			{"romp", terrain.corner_romp[i]},
			{"special_doodad", terrain.corner_special_doodad[i]},
		});
	}

	return json{
		{"tileset", std::string(1, terrain.tileset)},
		{"tileset_ids", terrain.tileset_ids},
		{"cliffset_ids", terrain.cliffset_ids},
		{"width", terrain.width},
		{"height", terrain.height},
		{"offset", vec2_to_json(terrain.offset)},
		{"corners", corners},
	};
}

void terrain_from_json(const json& value, Terrain& terrain) {
	terrain.tileset = value.at("tileset").get<std::string>().front();
	terrain.tileset_ids = value.at("tileset_ids").get<std::vector<std::string>>();
	terrain.cliffset_ids = value.at("cliffset_ids").get<std::vector<std::string>>();
	terrain.width = value.at("width").get<int>();
	terrain.height = value.at("height").get<int>();
	terrain.offset = vec2_from_json(value.at("offset"));

	const auto& corners = value.at("corners");
	terrain.corner_height.resize(corners.size());
	terrain.corner_water_height.resize(corners.size());
	terrain.corner_ground_texture.resize(corners.size());
	terrain.corner_ground_variation.resize(corners.size());
	terrain.corner_cliff_variation.resize(corners.size());
	terrain.corner_cliff_texture.resize(corners.size());
	terrain.corner_layer_height.resize(corners.size());
	terrain.corner_map_edge.resize(corners.size());
	terrain.corner_ramp.resize(corners.size());
	terrain.corner_blight.resize(corners.size());
	terrain.corner_water.resize(corners.size());
	terrain.corner_boundary.resize(corners.size());
	terrain.corner_cliff.resize(corners.size());
	terrain.corner_romp.resize(corners.size());
	terrain.corner_special_doodad.resize(corners.size());

	for (size_t i = 0; i < corners.size(); ++i) {
		const auto& corner = corners.at(i);
		terrain.corner_height[i] = corner.at("height").get<float>();
		terrain.corner_water_height[i] = corner.at("water_height").get<float>();
		terrain.corner_ground_texture[i] = corner.at("ground_texture").get<uint8_t>();
		terrain.corner_ground_variation[i] = corner.at("ground_variation").get<uint8_t>();
		terrain.corner_cliff_variation[i] = corner.at("cliff_variation").get<uint8_t>();
		terrain.corner_cliff_texture[i] = corner.at("cliff_texture").get<uint8_t>();
		terrain.corner_layer_height[i] = corner.at("layer_height").get<uint8_t>();
		terrain.corner_map_edge[i] = corner.at("map_edge").get<uint8_t>();
		terrain.corner_ramp[i] = corner.at("ramp").get<uint8_t>();
		terrain.corner_blight[i] = corner.at("blight").get<uint8_t>();
		terrain.corner_water[i] = corner.at("water").get<uint8_t>();
		terrain.corner_boundary[i] = corner.at("boundary").get<uint8_t>();
		terrain.corner_cliff[i] = corner.at("cliff").get<uint8_t>();
		terrain.corner_romp[i] = corner.at("romp").get<uint8_t>();
		terrain.corner_special_doodad[i] = corner.at("special_doodad").get<uint8_t>();
	}
}

json pathing_map_to_json(const PathingMap& pathing_map) {
	return json{
		{"width", pathing_map.width},
		{"height", pathing_map.height},
		{"static", pathing_map.pathing_cells_static},
	};
}

void pathing_map_from_json(const json& value, PathingMap& pathing_map) {
	pathing_map.width = value.at("width").get<int>();
	pathing_map.height = value.at("height").get<int>();
	pathing_map.pathing_cells_static = value.at("static").get<std::vector<uint8_t>>();
	pathing_map.pathing_cells_dynamic.assign(pathing_map.pathing_cells_static.size(), 0);
}

json unit_to_json(const Unit& unit) {
	json items = json::array();
	for (const auto& [slot, id] : unit.items) {
		items.push_back(json{{"slot", slot}, {"id", id}});
	}

	json abilities = json::array();
	for (const auto& [id, autocast, level] : unit.abilities) {
		abilities.push_back(json{{"id", id}, {"autocast", autocast}, {"level", level}});
	}

	return json{
		{"id", unit.id},
		{"variation", unit.variation},
		{"position", vec3_to_json(unit.position)},
		{"angle", unit.angle},
		{"scale", vec3_to_json(unit.scale)},
		{"skin_id", unit.skin_id},
		{"flags", unit.flags},
		{"player", unit.player},
		{"unknown1", unit.unknown1},
		{"unknown2", unit.unknown2},
		{"health", unit.health},
		{"mana", unit.mana},
		{"item_table_pointer", unit.item_table_pointer},
		{"item_sets", item_sets_to_json(unit.item_sets)},
		{"gold", unit.gold},
		{"target_acquisition", unit.target_acquisition},
		{"level", unit.level},
		{"strength", unit.strength},
		{"agility", unit.agility},
		{"intelligence", unit.intelligence},
		{"items", items},
		{"abilities", abilities},
		{"random_type", unit.random_type},
		{"random", unit.random},
		{"custom_color", unit.custom_color},
		{"waygate", unit.waygate},
		{"creation_number", unit.creation_number},
	};
}

Unit unit_from_json(const json& value) {
	Unit unit;
	unit.id = value.at("id").get<std::string>();
	unit.variation = value.at("variation").get<int>();
	unit.position = vec3_from_json(value.at("position"));
	unit.angle = value.at("angle").get<float>();
	unit.scale = vec3_from_json(value.at("scale"));
	unit.skin_id = value.at("skin_id").get<std::string>();
	unit.flags = value.at("flags").get<uint8_t>();
	unit.player = value.at("player").get<int>();
	unit.unknown1 = value.at("unknown1").get<uint8_t>();
	unit.unknown2 = value.at("unknown2").get<uint8_t>();
	unit.health = value.at("health").get<int>();
	unit.mana = value.at("mana").get<int>();
	unit.item_table_pointer = value.at("item_table_pointer").get<int>();
	unit.item_sets = item_sets_from_json(value.at("item_sets"));
	unit.gold = value.at("gold").get<int>();
	unit.target_acquisition = value.at("target_acquisition").get<float>();
	unit.level = value.at("level").get<int>();
	unit.strength = value.at("strength").get<int>();
	unit.agility = value.at("agility").get<int>();
	unit.intelligence = value.at("intelligence").get<int>();
	for (const auto& item_json : value.at("items")) {
		unit.items.emplace_back(item_json.at("slot").get<uint32_t>(), item_json.at("id").get<std::string>());
	}
	for (const auto& ability_json : value.at("abilities")) {
		unit.abilities.emplace_back(
			ability_json.at("id").get<std::string>(),
			ability_json.at("autocast").get<uint32_t>(),
			ability_json.at("level").get<uint32_t>()
		);
	}
	unit.random_type = value.at("random_type").get<int>();
	unit.random = value.at("random").get<std::vector<uint8_t>>();
	unit.custom_color = value.at("custom_color").get<int>();
	unit.waygate = value.at("waygate").get<int>();
	unit.creation_number = value.at("creation_number").get<int>();
	Unit::auto_increment = std::max(Unit::auto_increment, unit.creation_number);
	return unit;
}

json doodad_to_json(const Doodad& doodad) {
	return json{
		{"id", doodad.id},
		{"skin_id", doodad.skin_id},
		{"variation", doodad.variation},
		{"position", vec3_to_json(doodad.position)},
		{"scale", vec3_to_json(doodad.scale)},
		{"angle", doodad.angle},
		{"state", static_cast<int>(doodad.state)},
		{"life", doodad.life},
		{"item_table_pointer", doodad.item_table_pointer},
		{"item_sets", item_sets_to_json(doodad.item_sets)},
		{"creation_number", doodad.creation_number},
	};
}

Doodad doodad_from_json(const json& value) {
	Doodad doodad;
	doodad.id = value.at("id").get<std::string>();
	doodad.skin_id = value.at("skin_id").get<std::string>();
	doodad.variation = value.at("variation").get<int>();
	doodad.position = vec3_from_json(value.at("position"));
	doodad.scale = vec3_from_json(value.at("scale"));
	doodad.angle = value.at("angle").get<float>();
	doodad.state = static_cast<Doodad::State>(value.at("state").get<int>());
	doodad.life = value.at("life").get<uint8_t>();
	doodad.item_table_pointer = value.at("item_table_pointer").get<int>();
	doodad.item_sets = item_sets_from_json(value.at("item_sets"));
	doodad.creation_number = value.at("creation_number").get<int>();
	Doodad::auto_increment = std::max(Doodad::auto_increment, doodad.creation_number);
	return doodad;
}

json ivec3_to_json(const glm::ivec3& value) {
	return json::array({value.x, value.y, value.z});
}

glm::ivec3 ivec3_from_json(const json& value) {
	return {value.at(0).get<int>(), value.at(1).get<int>(), value.at(2).get<int>()};
}

json special_doodad_to_json(const SpecialDoodad& doodad) {
	return json{
		{"id", doodad.id},
		{"variation", doodad.variation},
		{"position", ivec3_to_json(doodad.position)},
	};
}

SpecialDoodad special_doodad_from_json(const json& value) {
	SpecialDoodad doodad;
	doodad.id = value.at("id").get<std::string>();
	doodad.variation = value.at("variation").get<int>();
	doodad.position = ivec3_from_json(value.at("position"));
	doodad.old_position = doodad.position;
	return doodad;
}

}

namespace collaboration {

nlohmann::json serialize_map_snapshot(const Map& map) {
	nlohmann::json units = nlohmann::json::array();
	for (const auto& unit : map.units.units) {
		units.push_back(unit_to_json(unit));
	}

	nlohmann::json items = nlohmann::json::array();
	for (const auto& unit : map.units.items) {
		items.push_back(unit_to_json(unit));
	}

	nlohmann::json doodads = nlohmann::json::array();
	for (const auto& doodad : map.doodads.doodads) {
		doodads.push_back(doodad_to_json(doodad));
	}

	nlohmann::json special_doodads = nlohmann::json::array();
	for (const auto& doodad : map.doodads.special_doodads) {
		special_doodads.push_back(special_doodad_to_json(doodad));
	}

	return nlohmann::json{
		{"terrain", terrain_to_json(map.terrain)},
		{"pathing", pathing_map_to_json(map.pathing_map)},
		{"units", units},
		{"items", items},
		{"doodads", doodads},
		{"special_doodads", special_doodads},
	};
}

void deserialize_map_snapshot(const nlohmann::json& message, Map& map) {
	terrain_from_json(message.at("terrain"), map.terrain);
	pathing_map_from_json(message.at("pathing"), map.pathing_map);

	map.units.units.clear();
	map.units.items.clear();
	for (const auto& unit : message.at("units")) {
		map.units.units.push_back(unit_from_json(unit));
	}
	for (const auto& unit : message.at("items")) {
		map.units.items.push_back(unit_from_json(unit));
	}

	map.doodads.doodads.clear();
	map.doodads.special_doodads.clear();
	for (const auto& doodad : message.at("doodads")) {
		map.doodads.doodads.push_back(doodad_from_json(doodad));
	}
	for (const auto& doodad : message.at("special_doodads")) {
		map.doodads.special_doodads.push_back(special_doodad_from_json(doodad));
	}

	// Initialize runtime resources for units and doodads so a joined client
	// has the same visible state as the host. This mirrors what happens on
	// a normal map load (see HiveWE load path).
	map.units.create();
	map.pathing_map.upload_static_pathing();
	map.doodads.create(map.terrain, map.pathing_map);

	map.loaded = true;
}

}
