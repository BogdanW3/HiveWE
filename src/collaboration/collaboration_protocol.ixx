module;

#include <nlohmann/json.hpp>
#include <glm/glm.hpp>

export module CollaborationProtocol;

import std;
import WorldUndoManager;
import TerrainUndo;
import UnitsUndo;
import DoodadsUndo;
import PathingUndo;
import Terrain;
import Units;
import Doodad;
import Doodads;
import Utilities;
import SkeletalModelInstance;
import Rects;
import Map;

export namespace collaboration {

nlohmann::json serialize_world_command(const WorldCommand& command);
std::unique_ptr<WorldCommand> deserialize_world_command(const nlohmann::json& message, WorldEditContext& ctx);

}

namespace {

nlohmann::json vec3_to_json(const glm::vec3& value) {
	return nlohmann::json::array({value.x, value.y, value.z});
}

glm::vec3 vec3_from_json(const nlohmann::json& value) {
	return {value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>()};
}

nlohmann::json rect_to_json(const TerrainRect& rect) {
	return nlohmann::json{{"x", rect.x()}, {"y", rect.y()}, {"w", rect.width()}, {"h", rect.height()}};
}

nlohmann::json rect_to_json(const PathingRect& rect) {
	return nlohmann::json{{"x", rect.x()}, {"y", rect.y()}, {"w", rect.width()}, {"h", rect.height()}};
}

TerrainRect terrain_rect_from_json(const nlohmann::json& value) {
	return TerrainRect(value.at("x").get<int>(), value.at("y").get<int>(), value.at("w").get<int>(), value.at("h").get<int>());
}

PathingRect pathing_rect_from_json(const nlohmann::json& value) {
	return PathingRect(value.at("x").get<int>(), value.at("y").get<int>(), value.at("w").get<int>(), value.at("h").get<int>());
}

nlohmann::json corner_to_json(const Corner& corner) {
	return nlohmann::json{
		{"map_edge", corner.map_edge},
		{"ground_texture", corner.ground_texture},
		{"height", corner.height},
		{"water_height", corner.water_height},
		{"ramp", corner.ramp},
		{"blight", corner.blight},
		{"water", corner.water},
		{"boundary", corner.boundary},
		{"cliff", corner.cliff},
		{"romp", corner.romp},
		{"special_doodad", corner.special_doodad},
		{"ground_variation", corner.ground_variation},
		{"cliff_variation", corner.cliff_variation},
		{"cliff_texture", corner.cliff_texture},
		{"layer_height", corner.layer_height},
	};
}

Corner corner_from_json(const nlohmann::json& value) {
	Corner corner;
	corner.map_edge = value.at("map_edge").get<bool>();
	corner.ground_texture = value.at("ground_texture").get<uint8_t>();
	corner.height = value.at("height").get<float>();
	corner.water_height = value.at("water_height").get<float>();
	corner.ramp = value.at("ramp").get<bool>();
	corner.blight = value.at("blight").get<bool>();
	corner.water = value.at("water").get<bool>();
	corner.boundary = value.at("boundary").get<bool>();
	corner.cliff = value.at("cliff").get<bool>();
	corner.romp = value.at("romp").get<bool>();
	corner.special_doodad = value.at("special_doodad").get<bool>();
	corner.ground_variation = value.at("ground_variation").get<uint8_t>();
	corner.cliff_variation = value.at("cliff_variation").get<uint8_t>();
	corner.cliff_texture = value.at("cliff_texture").get<uint8_t>();
	corner.layer_height = value.at("layer_height").get<uint8_t>();
	return corner;
}

nlohmann::json item_sets_to_json(const std::vector<ItemSet>& item_sets) {
	nlohmann::json result = nlohmann::json::array();
	for (const auto& item_set : item_sets) {
		nlohmann::json items = nlohmann::json::array();
		for (const auto& [chance, id] : item_set.items) {
			items.push_back(nlohmann::json{{"chance", chance}, {"id", id}});
		}
		result.push_back(items);
	}
	return result;
}

std::vector<ItemSet> item_sets_from_json(const nlohmann::json& value) {
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

nlohmann::json unit_to_json(const Unit& unit) {
	nlohmann::json items = nlohmann::json::array();
	for (const auto& [slot, id] : unit.items) {
		items.push_back(nlohmann::json{{"slot", slot}, {"id", id}});
	}

	nlohmann::json abilities = nlohmann::json::array();
	for (const auto& [id, autocast, level] : unit.abilities) {
		abilities.push_back(nlohmann::json{{"id", id}, {"autocast", autocast}, {"level", level}});
	}

	return nlohmann::json{
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

Unit unit_from_json(const nlohmann::json& value, Units& units) {
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
	unit.mesh = units.get_mesh(unit.id);
	unit.skeleton = SkeletalModelInstance(unit.mesh->mdx, Units::get_required_animation_names(unit.id));
	unit.update();
	Unit::auto_increment = std::max(Unit::auto_increment, unit.creation_number);
	return unit;
}

nlohmann::json doodad_to_json(const Doodad& doodad) {
	return nlohmann::json{
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

Doodad doodad_from_json(const nlohmann::json& value, Doodads& doodads, const Terrain& terrain) {
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
	doodad.mesh = doodads.get_mesh(doodad.id, doodad.variation);
	doodad.init(doodad.id, doodad.mesh, terrain);
	doodad.skin_id = value.at("skin_id").get<std::string>();
	doodad.state = static_cast<Doodad::State>(value.at("state").get<int>());
	doodad.life = value.at("life").get<uint8_t>();
	doodad.item_table_pointer = value.at("item_table_pointer").get<int>();
	doodad.item_sets = item_sets_from_json(value.at("item_sets"));
	doodad.creation_number = value.at("creation_number").get<int>();
	Doodad::auto_increment = std::max(Doodad::auto_increment, doodad.creation_number);
	return doodad;
}

WorldEditContext make_context(Map* map) {
	return WorldEditContext {
		.terrain = map->terrain,
		.units = map->units,
		.doodads = map->doodads,
		.brush = map->brush,
		.pathing_map = map->pathing_map,
	};
}

std::unique_ptr<WorldCommand> command_from_json(const nlohmann::json& message, WorldEditContext& ctx) {
	const auto type = message.at("type").get<std::string>();
	if (type == "terrain") {
		auto action = std::make_unique<TerrainGenericAction>();
		action->undo_type = static_cast<TerrainUndoType>(message.at("undo_type").get<int>());
		action->area = terrain_rect_from_json(message.at("area"));
		for (const auto& corner : message.at("old_corners")) {
			action->old_corners.push_back(corner_from_json(corner));
		}
		for (const auto& corner : message.at("new_corners")) {
			action->new_corners.push_back(corner_from_json(corner));
		}
		return action;
	}
	if (type == "pathing") {
		auto action = std::make_unique<PathingMapAction>();
		action->area = pathing_rect_from_json(message.at("area"));
		action->old_pathing = message.at("old_pathing").get<std::vector<uint8_t>>();
		action->new_pathing = message.at("new_pathing").get<std::vector<uint8_t>>();
		return action;
	}
	if (type == "unit_add") {
		auto action = std::make_unique<UnitAddAction>();
		for (const auto& unit_json : message.at("units")) {
			action->units.push_back(unit_from_json(unit_json, ctx.units));
		}
		return action;
	}
	if (type == "unit_delete") {
		auto action = std::make_unique<UnitDeleteAction>();
		for (const auto& unit_json : message.at("units")) {
			action->units.push_back(unit_from_json(unit_json, ctx.units));
		}
		return action;
	}
	if (type == "unit_state") {
		auto action = std::make_unique<UnitStateAction>();
		for (const auto& unit_json : message.at("old_units")) {
			action->old_units.push_back(unit_from_json(unit_json, ctx.units));
		}
		for (const auto& unit_json : message.at("new_units")) {
			action->new_units.push_back(unit_from_json(unit_json, ctx.units));
		}
		return action;
	}
	if (type == "doodad_add") {
		auto action = std::make_unique<DoodadAddAction>();
		for (const auto& doodad_json : message.at("doodads")) {
			action->doodads.push_back(doodad_from_json(doodad_json, ctx.doodads, ctx.terrain));
		}
		return action;
	}
	if (type == "doodad_delete") {
		auto action = std::make_unique<DoodadDeleteAction>();
		for (const auto& doodad_json : message.at("doodads")) {
			action->doodads.push_back(doodad_from_json(doodad_json, ctx.doodads, ctx.terrain));
		}
		return action;
	}
	if (type == "doodad_state") {
		auto action = std::make_unique<DoodadStateAction>();
		for (const auto& doodad_json : message.at("old_doodads")) {
			action->old_doodads.push_back(doodad_from_json(doodad_json, ctx.doodads, ctx.terrain));
		}
		for (const auto& doodad_json : message.at("new_doodads")) {
			action->new_doodads.push_back(doodad_from_json(doodad_json, ctx.doodads, ctx.terrain));
		}
		return action;
	}

	throw std::runtime_error("Unsupported collaboration message type");
}

}

namespace collaboration {

nlohmann::json serialize_world_command(const WorldCommand& command) {
	if (const auto* action = dynamic_cast<const TerrainGenericAction*>(&command)) {
		nlohmann::json old_corners = nlohmann::json::array();
		nlohmann::json new_corners = nlohmann::json::array();
		for (const auto& corner : action->old_corners) {
			old_corners.push_back(corner_to_json(corner));
		}
		for (const auto& corner : action->new_corners) {
			new_corners.push_back(corner_to_json(corner));
		}
		return nlohmann::json{{"type", "terrain"}, {"undo_type", static_cast<int>(action->undo_type)}, {"area", rect_to_json(action->area)}, {"old_corners", old_corners}, {"new_corners", new_corners}};
	}
	if (const auto* action = dynamic_cast<const PathingMapAction*>(&command)) {
		return nlohmann::json{{"type", "pathing"}, {"area", rect_to_json(action->area)}, {"old_pathing", action->old_pathing}, {"new_pathing", action->new_pathing}};
	}
	if (const auto* action = dynamic_cast<const UnitAddAction*>(&command)) {
		nlohmann::json units = nlohmann::json::array();
		for (const auto& unit : action->units) {
			units.push_back(unit_to_json(unit));
		}
		return nlohmann::json{{"type", "unit_add"}, {"units", units}};
	}
	if (const auto* action = dynamic_cast<const UnitDeleteAction*>(&command)) {
		nlohmann::json units = nlohmann::json::array();
		for (const auto& unit : action->units) {
			units.push_back(unit_to_json(unit));
		}
		return nlohmann::json{{"type", "unit_delete"}, {"units", units}};
	}
	if (const auto* action = dynamic_cast<const UnitStateAction*>(&command)) {
		nlohmann::json old_units = nlohmann::json::array();
		nlohmann::json new_units = nlohmann::json::array();
		for (const auto& unit : action->old_units) {
			old_units.push_back(unit_to_json(unit));
		}
		for (const auto& unit : action->new_units) {
			new_units.push_back(unit_to_json(unit));
		}
		return nlohmann::json{{"type", "unit_state"}, {"old_units", old_units}, {"new_units", new_units}};
	}
	if (const auto* action = dynamic_cast<const DoodadAddAction*>(&command)) {
		nlohmann::json doodads = nlohmann::json::array();
		for (const auto& doodad : action->doodads) {
			doodads.push_back(doodad_to_json(doodad));
		}
		return nlohmann::json{{"type", "doodad_add"}, {"doodads", doodads}};
	}
	if (const auto* action = dynamic_cast<const DoodadDeleteAction*>(&command)) {
		nlohmann::json doodads = nlohmann::json::array();
		for (const auto& doodad : action->doodads) {
			doodads.push_back(doodad_to_json(doodad));
		}
		return nlohmann::json{{"type", "doodad_delete"}, {"doodads", doodads}};
	}
	if (const auto* action = dynamic_cast<const DoodadStateAction*>(&command)) {
		nlohmann::json old_doodads = nlohmann::json::array();
		nlohmann::json new_doodads = nlohmann::json::array();
		for (const auto& doodad : action->old_doodads) {
			old_doodads.push_back(doodad_to_json(doodad));
		}
		for (const auto& doodad : action->new_doodads) {
			new_doodads.push_back(doodad_to_json(doodad));
		}
		return nlohmann::json{{"type", "doodad_state"}, {"old_doodads", old_doodads}, {"new_doodads", new_doodads}};
	}

	throw std::runtime_error("Unsupported world command type");
}

std::unique_ptr<WorldCommand> deserialize_world_command(const nlohmann::json& message, WorldEditContext& ctx) {
	return command_from_json(message, ctx);
}

}
