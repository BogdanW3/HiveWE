module;

#include <QProcess>
#include <QDir>

export module Triggers;

import std;
import Hierarchy;
import Utilities;
import Globals;
import <glm/glm.hpp>;
import Units;
import Doodads;
import Regions;
import GameCameras;
import Sounds;
import Terrain;
import MapInfo;
import BinaryReader;
import BinaryWriter;
import INI;

namespace fs = std::filesystem;
using namespace std::literals::string_literals;

enum class Classifier {
	map = 1,
	library = 2,
	category = 4,
	gui = 8,
	comment = 16,
	script = 32,
	variable = 64
};

struct TriggerCategory {
	Classifier classifier;
	int id;
	std::string name;
	bool open_state = true;
	bool is_comment = false;
	int parent_id;
};

struct TriggerParameter;

struct TriggerSubParameter {
	enum class Type {
		events,
		conditions,
		actions,
		calls
	};
	Type type;
	std::string name;
	bool begin_parameters;
	std::vector<TriggerParameter> parameters;
};

struct TriggerParameter {
	enum class Type {
		invalid = -1,
		preset,
		variable,
		function,
		string
	};
	Type type;
	int unknown;
	std::string value;
	bool has_sub_parameter;
	TriggerSubParameter sub_parameter;
	bool is_array = false;
	std::vector<TriggerParameter> parameters; // There is really only one so unique_ptr I guess
};

struct ECA {
	enum class Type {
		event,
		condition,
		action
	};

	Type type;
	int group;
	std::string name;
	bool enabled;
	std::vector<TriggerParameter> parameters;
	std::vector<ECA> ecas;
};

export struct Trigger {
	Classifier classifier;
	int id;
	int parent_id = 0;
	std::string name;
	std::string description;
	std::string custom_text;
	bool is_comment = false;
	bool is_enabled = true;
	bool is_script = false;
	bool initially_on = true;
	bool run_on_initialization = false;
	std::vector<ECA> ecas;

	static inline int next_id = 0;
};

struct TriggerVariable {
	std::string name;
	std::string type;
	uint32_t unknown;
	bool is_array;
	int array_size = 0;
	bool is_initialized;
	std::string initial_value;
	int id;
	int parent_id;
};

/// A minimal utility wrapper around a std::string that manages newlines, indentation and closing braces
struct MapScriptWriter {
	std::string script;
	size_t current_indentation = 0;

	enum class Mode {
		lua,
		jass
	};
	Mode mode = Mode::lua;

	bool is_empty() {
		return script.empty();
	}

	void merge(const MapScriptWriter& writer) {
		script += writer.script;
	}

	void raw_write_to_log(std::string_view users_fmt, std::format_args&& args) {
		std::vformat_to(std::back_inserter(script), users_fmt, args);
	}

	constexpr void local(std::string_view type, std::string_view name, std::string_view value) {
		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}

		if (mode == Mode::lua) {
			std::format_to(std::back_inserter(script), "local {} = {}\n", name, value);
		} else {
			std::format_to(std::back_inserter(script), "local {} {} = {}\n", type, name, value);
		}
	}

	constexpr void global(std::string_view type, std::string_view name, std::string_view value) {
		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}

		if (mode == Mode::lua) {
			std::format_to(std::back_inserter(script), "{} = {}\n", name, value);
		} else {
			std::format_to(std::back_inserter(script), "{} {} = {}\n", type, name, value);
		}
	}

	constexpr void set_variable(std::string_view name, std::string_view value) {
		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}

		if (mode == Mode::lua) {
			std::format_to(std::back_inserter(script), "{} = {}\n", name, value);
		} else {
			std::format_to(std::back_inserter(script), "set {} = {}\n", name, value);
		}
	}

	template <typename... Args>
	constexpr void inline_call(std::string_view name, Args&&... args) {
		std::string work = "{}(";

		for (size_t i = 0; i < sizeof...(args); i++) {
			work += "{}";
			if (i < sizeof...(args) - 1) {
				work += ", ";
			}
		}
		work += ")";
		// Reduce binary code size by having only one instantiation
		raw_write_to_log(work, std::make_format_args(name, args...));
	}

	template <typename... Args>
	constexpr void call(std::string_view name, Args&&... args) {
		std::string work;

		if (mode == Mode::jass) {
			work = "call {}(";
		} else {
			work = "{}(";
		}

		for (size_t i = 0; i < sizeof...(args); i++) {
			work += "{}";
			if (i < sizeof...(args) - 1) {
				work += ", ";
			}
		}
		work += ")\n";

		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}
		// Reduce binary code size by having only one instantiation
		raw_write_to_log(work, std::make_format_args(name, args...));
	}

	void write(std::string_view string) {
		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}

		script += string;
	}

	template <typename... Args>
	constexpr void write_ln(Args&&... args) {
		std::string work = std::string(current_indentation, '\t');
		for (size_t i = 0; i < sizeof...(args); i++) {
			work += "{}";
		}
		work.push_back('\n');

		// Reduce binary code size by having only one instantiation
		raw_write_to_log(work, std::make_format_args(args...));
	}

	template <typename T>
	constexpr void forloop(size_t start, size_t end, T callback) {
		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}
		std::format_to(std::back_inserter(script), "for i={},{} do\n", start, end);

		current_indentation += 1;
		callback();
		current_indentation -= 1;
		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}
		script += "end\n";
	}

	template <typename T>
	void function(std::string_view name, T callback) {
		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}

		if (mode == Mode::lua) {
			std::format_to(std::back_inserter(script), "function {}()\n", name);
		} else {
			std::format_to(std::back_inserter(script), "function {} takes nothing returns nothing\n", name);
		}

		current_indentation += 1;
		callback();
		current_indentation -= 1;
		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}

		if (mode == Mode::lua) {
			script += "end\n";
		} else {
			script += "endfunction\n";
		}
	}

	template <typename T>
	void if_statement(std::string_view condition, T callback) {
		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}

		std::format_to(std::back_inserter(script), "if ({}) then\n", condition);

		current_indentation += 1;
		callback();
		current_indentation -= 1;
		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}

		if (mode == Mode::lua) {
			script += "end\n";
		} else {
			script += "endif\n";
		}
	}

	 template <typename T>
	 void global_variable(std::string_view name, T value) {
		for (size_t i = 0; i < current_indentation; i++) {
			script += '\t';
		}
		std::format_to(std::back_inserter(script), "udg_{} = {}", name, value);
	 }

	 /// The ID should not be quoted
	 std::string four_cc(std::string_view id) {
		if (mode == Mode::lua) {
			return std::format("FourCC(\"{}\")", id);
		} else {
			return std::format("\"{}\"", id);;
		}
	 }

	 std::string null() {
		if (mode == Mode::lua) {
			return "nil";
		} else {
			return "null";
		}
	 }
};

export class Triggers {
	std::unordered_map<std::string, int> argument_counts;
	const std::string separator = "//===========================================================================\n";

	static constexpr int write_version = 0x80000004;
	static constexpr int write_sub_version = 7;
	static constexpr int write_string_version = 1;

	int unknown1 = 0;
	int unknown2 = 0;
	int trig_def_ver = 2;

public:
	ini::INI trigger_strings;
	ini::INI trigger_data;

	std::string global_jass_comment;
	std::string global_jass;

	std::vector<TriggerCategory> categories;
	std::vector<TriggerVariable> variables;
	std::vector<Trigger> triggers;

private:

	void parse_parameter_structure(BinaryReader& reader, TriggerParameter& parameter, uint32_t version) {
		parameter.type = static_cast<TriggerParameter::Type>(reader.read<uint32_t>());
		parameter.value = reader.read_c_string();
		parameter.has_sub_parameter = reader.read<uint32_t>();
		if (parameter.has_sub_parameter) {
			parameter.sub_parameter.type = static_cast<TriggerSubParameter::Type>(reader.read<uint32_t>());
			parameter.sub_parameter.name = reader.read_c_string();
			parameter.sub_parameter.begin_parameters = reader.read<uint32_t>();
			if (parameter.sub_parameter.begin_parameters) {
				parameter.sub_parameter.parameters.resize(argument_counts[parameter.sub_parameter.name]);
				for (auto&& i : parameter.sub_parameter.parameters) {
					parse_parameter_structure(reader, i, version);
				}
			}
		}
		if (version == 4) {
			if (parameter.type == TriggerParameter::Type::function) {
				reader.advance(4); // Unknown always 0
			} else {
				parameter.is_array = reader.read<uint32_t>();
			}
		} else {
			if (parameter.has_sub_parameter) {
				parameter.unknown = reader.read<uint32_t>(); // Unknown always 0
			}
			parameter.is_array = reader.read<uint32_t>();
		}
		if (parameter.is_array) {
			parameter.parameters.resize(1);
			parse_parameter_structure(reader, parameter.parameters.front(), version);
		}
	}

	void parse_eca_structure(BinaryReader& reader, ECA& eca, bool is_child, uint32_t version) {
		eca.type = static_cast<ECA::Type>(reader.read<uint32_t>());
		if (is_child) {
			eca.group = reader.read<uint32_t>();
		}
		eca.name = reader.read_c_string();
		eca.enabled = reader.read<uint32_t>();
		eca.parameters.resize(argument_counts[eca.name]);
		for (auto&& i : eca.parameters) {
			parse_parameter_structure(reader, i, version);
		}
		if (version == 7) {
			eca.ecas.resize(reader.read<uint32_t>());
			for (auto&& i : eca.ecas) {
				parse_eca_structure(reader, i, true, version);
			}
		}
	}

	void print_parameter_structure(BinaryWriter& writer, const TriggerParameter& parameter) const {
		writer.write<uint32_t>(static_cast<int>(parameter.type));
		writer.write_c_string(parameter.value);
		writer.write<uint32_t>(parameter.has_sub_parameter);

		if (parameter.has_sub_parameter) {
			writer.write<uint32_t>(static_cast<int>(parameter.sub_parameter.type));
			writer.write_c_string(parameter.sub_parameter.name);
			writer.write<uint32_t>(parameter.sub_parameter.begin_parameters);
			if (parameter.sub_parameter.begin_parameters) {
				for (const auto& i : parameter.sub_parameter.parameters) {
					print_parameter_structure(writer, i);
				}
			}

			writer.write<uint32_t>(parameter.unknown);
		}
		writer.write<uint32_t>(parameter.is_array);
		if (parameter.is_array) {
			print_parameter_structure(writer, parameter.parameters.front());
		}
	}

	void print_eca_structure(BinaryWriter& writer, const ECA& eca, bool is_child) const {
		writer.write<uint32_t>(static_cast<int>(eca.type));
		if (is_child) {
			writer.write<uint32_t>(eca.group);
		}

		writer.write_c_string(eca.name);
		writer.write<uint32_t>(eca.enabled);
		for (const auto& i : eca.parameters) {
			print_parameter_structure(writer, i);
		}

		writer.write<uint32_t>(eca.ecas.size());
		for (const auto& i : eca.ecas) {
			print_eca_structure(writer, i, true);
		}
	}

	std::string convert_eca_to_jass(const ECA& eca, std::string& pre_actions, const std::string& trigger_name, bool nested) const {
		std::string output;

		if (!eca.enabled) {
			return "";
		}

		if (eca.name == "WaitForCondition") {
			output += "while (true) do\n";
			output += std::format("if (({})) then break end\n", resolve_parameter(eca.parameters[0], trigger_name, pre_actions, get_type(eca.name, 0)));
			output += "TriggerSleepAction(RMaxBJ(bj_WAIT_FOR_COND_MIN_INTERVAL, " + resolve_parameter(eca.parameters[1], trigger_name, pre_actions, get_type(eca.name, 1)) + "))\n";
			output += "end\n";

			return output;
		}

		if (eca.name == "ForLoopAMultiple" || eca.name == "ForLoopBMultiple") {
			std::string loop_index = eca.name == "ForLoopAMultiple" ? "bj_forLoopAIndex" : "bj_forLoopBIndex";
			std::string loop_index_end = eca.name == "ForLoopAMultiple" ? "bj_forLoopAIndexEnd" : "bj_forLoopBIndexEnd";

			output += loop_index + "=" + resolve_parameter(eca.parameters[0], trigger_name, pre_actions, get_type(eca.name, 0)) + "\n";
			output += loop_index_end + "=" + resolve_parameter(eca.parameters[1], trigger_name, pre_actions, get_type(eca.name, 1)) + "\n";
			output += "while (true) do\n";
			output += std::format("if (({} > {})) then break end\n", loop_index, loop_index_end);
			for (const auto& i : eca.ecas) {
				output += "" + convert_eca_to_jass(i, pre_actions, trigger_name, false) + "\n";
			}
			output += loop_index + " = " + loop_index + " + 1\n";
			output += "end\n";

			return output;
		}

		if (eca.name == "ForLoopVarMultiple") {
			std::string variable = resolve_parameter(eca.parameters[0], trigger_name, pre_actions, "integer");

			output += variable + " = " + resolve_parameter(eca.parameters[1], trigger_name, pre_actions, get_type(eca.name, 1)) + "\n";
			output += "while (true) do\n";
			output += std::format("if (({} > {})) then break end\n", variable, resolve_parameter(eca.parameters[2], trigger_name, pre_actions, get_type(eca.name, 2)));
			for (const auto& i : eca.ecas) {
				output += convert_eca_to_jass(i, pre_actions, trigger_name, false) + "\n";
			}
			output += variable + " = " + variable + " + 1\n";
			output += "end\n";

			return output;
		}

		if (eca.name == "IfThenElseMultiple") {
			std::string iftext;
			std::string thentext;
			std::string elsetext;

			std::string function_name = generate_function_name(trigger_name);
			iftext += "function " + function_name + "()\n"; // returns boolean

			for (const auto& i : eca.ecas) {
				if (i.type == ECA::Type::condition) {
					iftext += "if (not (" + convert_eca_to_jass(i, pre_actions, trigger_name, true) + ")) then\n";
					iftext += "return false\n";
					iftext += "end\n";
				} else if (i.type == ECA::Type::action) {
					if (i.group == 1) {
						thentext += convert_eca_to_jass(i, pre_actions, trigger_name, false) + "\n";
					} else {
						elsetext += convert_eca_to_jass(i, pre_actions, trigger_name, false) + "\n";
					}
				}
			}
			iftext += "return true\n";
			iftext += "end\n";
			pre_actions += iftext;

			return "if (" + function_name + "()) then\n" + thentext + "else\n" + elsetext + "end";
		}

		if (eca.name == "ForForceMultiple" || eca.name == "ForGroupMultiple" || eca.name == "EnumDestructablesInRectAllMultiple" || eca.name == "EnumDestructablesInCircleBJMultiple") {
			std::string script_name = trigger_data.data("TriggerActions", "_" + eca.name + "_ScriptName");

			const std::string function_name = generate_function_name(trigger_name);

			if (eca.name == "EnumDestructablesInCircleBJMultiple") {
				output += script_name + "(" + resolve_parameter(eca.parameters[0], trigger_name, pre_actions, get_type(eca.name, 0)) + ", " +
						  resolve_parameter(eca.parameters[1], trigger_name, pre_actions, get_type(eca.name, 1)) + ", function " + function_name + ")\n";
			} else {
				output += script_name + "(" + resolve_parameter(eca.parameters[0], trigger_name, pre_actions, get_type(eca.name, 0)) + ", function " + function_name + ")\n";
			}

			std::string toto;
			for (const auto& i : eca.ecas) {
				toto += "" + convert_eca_to_jass(i, pre_actions, trigger_name, false) + "\n";
			}
			pre_actions += "function " + function_name + "()\n"; // returns nothing
			pre_actions += toto;
			pre_actions += "\nend\n";

			return output;
		}

		if (eca.name == "AndMultiple") {
			const std::string function_name = generate_function_name(trigger_name);

			std::string iftext = "function " + function_name + "()\n"; // returns boolean
			for (const auto& i : eca.ecas) {
				iftext += "if (not (" + convert_eca_to_jass(i, pre_actions, trigger_name, true) + ")) then\n";
				iftext += "return false\n";
				iftext += "end\n";
			}
			iftext += "return true\n";
			iftext += "end\n";
			pre_actions += iftext;

			return function_name + "()";
		}

		if (eca.name == "OrMultiple") {
			const std::string function_name = generate_function_name(trigger_name);

			std::string iftext = "function " + function_name + "()\n"; // returns boolean
			for (const auto& i : eca.ecas) {
				iftext += "if (" + convert_eca_to_jass(i, pre_actions, trigger_name, true) + ") then\n";
				iftext += "return true\n";
				iftext += "end\n";
			}
			iftext += "return false\n";
			iftext += "end\n";
			pre_actions += iftext;

			return function_name + "()";
		}

		return testt(trigger_name, eca.name, eca.parameters, pre_actions, !nested);
	}

	std::string testt(const std::string& trigger_name, const std::string& parent_name, const std::vector<TriggerParameter>& parameters, std::string& pre_actions, bool add_call) const {
		std::string output;

		std::string script_name = trigger_data.data("TriggerActions", "_" + parent_name + "_ScriptName");

		if (parent_name == "SetVariable") {
			const std::string& type = (*find_if(variables.begin(), variables.end(),
												[parameters](const TriggerVariable& var) {
													return var.name == parameters[0].value;
												}))
										  .type;
			const std::string first = resolve_parameter(parameters[0], trigger_name, pre_actions, "");
			const std::string second = resolve_parameter(parameters[1], trigger_name, pre_actions, type);

			return first + " = " + second;
		}

		if (parent_name == "CommentString") {
			return "//" + resolve_parameter(parameters[0], trigger_name, pre_actions, "");
		}

		if (parent_name == "CustomScriptCode") {
			return resolve_parameter(parameters[0], trigger_name, pre_actions, "");
		}

		if (parent_name.substr(0, 15) == "OperatorCompare") {
			output += resolve_parameter(parameters[0], trigger_name, pre_actions, get_type(parent_name, 0));

			auto result_operator = resolve_parameter(parameters[1], trigger_name, pre_actions, get_type(parent_name, 1));
			if (result_operator == "!=") {
				result_operator = "~=";
			}

			output += " " + result_operator + " ";
			output += resolve_parameter(parameters[2], trigger_name, pre_actions, get_type(parent_name, 2));
			return output;
		}

		if (parent_name == "OperatorString") {
			output += "(" + resolve_parameter(parameters[0], trigger_name, pre_actions, get_type(parent_name, 0));
			output += " .. ";
			output += resolve_parameter(parameters[1], trigger_name, pre_actions, get_type(parent_name, 1)) + ")";
			return output;
		}

		if (parent_name == "ForLoopVar") {
			std::string variable = resolve_parameter(parameters[0], trigger_name, pre_actions, "integer");

			output += variable + " = ";
			output += resolve_parameter(parameters[1], trigger_name, pre_actions, get_type(parent_name, 1)) + "\n";

			output += "while (true) do\n";
			output += std::format("if (({} > {})) then break end\n", variable, resolve_parameter(parameters[2], trigger_name, pre_actions, get_type(parent_name, 2)));
			output += resolve_parameter(parameters[3], trigger_name, pre_actions, get_type(parent_name, 3), true) + "\n";
			output += variable + " = " + variable + " + 1\n";
			output += "end\n";

			return output;
		}

		if (parent_name == "IfThenElse") {
			std::string thentext;
			std::string elsetext;

			std::string function_name = generate_function_name(trigger_name);
			std::string tttt = resolve_parameter(parameters[0], trigger_name, pre_actions, get_type(parent_name, 0));

			output += "if (" + function_name + "())\n";
			output += resolve_parameter(parameters[1], trigger_name, pre_actions, get_type(parent_name, 1), true) + "\n";
			output += "else\n";
			output += resolve_parameter(parameters[2], trigger_name, pre_actions, get_type(parent_name, 2), true) + "\n";
			output += "end";

			pre_actions += "function " + function_name + "()\n"; // returns boolean
			pre_actions += "return " + tttt + "\n";
			pre_actions += "end\n";
			return output;
		}

		if (parent_name == "ForForce" || parent_name == "ForGroup") {
			std::string function_name = generate_function_name(trigger_name);

			std::string tttt = resolve_parameter(parameters[1], trigger_name, pre_actions, get_type(parent_name, 1));

			output += parent_name + "(";
			output += resolve_parameter(parameters[0], trigger_name, pre_actions, get_type(parent_name, 0));
			output += ", " + function_name;
			output += ")";

			pre_actions += "function " + function_name + "()\n"; // returns nothing
			pre_actions += tttt + "\n";
			pre_actions += "end\n\n";
			return /*(add_call ? "call " : "") +*/ output;
		}

		if (parent_name == "GetBooleanAnd") {
			std::string first_parameter = resolve_parameter(parameters[0], trigger_name, pre_actions, get_type(parent_name, 0));
			std::string second_parameter = resolve_parameter(parameters[1], trigger_name, pre_actions, get_type(parent_name, 1));

			std::string function_name = generate_function_name(trigger_name);
			output += "GetBooleanAnd(" + function_name + "(), ";
			pre_actions += "function " + function_name + "()\n"; // returns boolean
			pre_actions += "return ( " + first_parameter + ")\n";
			pre_actions += "end\n\n";

			function_name = generate_function_name(trigger_name);
			output += function_name + "())";
			pre_actions += "function " + function_name + "()\n"; // returns boolean
			pre_actions += "return ( " + second_parameter + ")\n";
			pre_actions += "end\n\n";

			return /*(add_call ? "call " : "") +*/ output;
		}

		if (parent_name == "GetBooleanOr") {
			std::string first_parameter = resolve_parameter(parameters[0], trigger_name, pre_actions, get_type(parent_name, 0));
			std::string second_parameter = resolve_parameter(parameters[1], trigger_name, pre_actions, get_type(parent_name, 1));

			std::string function_name = generate_function_name(trigger_name);
			output += "GetBooleanOr(" + function_name + "(), ";
			pre_actions += "function " + function_name + "()\n"; // returns boolean
			pre_actions += "return ( " + first_parameter + ")\n";
			pre_actions += "end\n\n";

			function_name = generate_function_name(trigger_name);
			output += function_name + "())";
			pre_actions += "function " + function_name + "()\n"; // returns boolean
			pre_actions += "return ( " + second_parameter + ")\n";
			pre_actions += "end\n\n";

			return /*(add_call ? "call " : "") +*/ output;
		}

		if (parent_name == "OperatorInt" || parent_name == "OperatorReal") {
			output += "(" + resolve_parameter(parameters[0], trigger_name, pre_actions, get_type(parent_name, 0));

			auto result_operator = resolve_parameter(parameters[1], trigger_name, pre_actions, get_type(parent_name, 1));
			if (result_operator == "!=") {
				result_operator = "~=";
			}

			output += " " + result_operator + " ";
			output += resolve_parameter(parameters[2], trigger_name, pre_actions, get_type(parent_name, 2)) + ")";
			return output;
		}

		if (parent_name == "AddTriggerEvent") {
			std::string first_parameter = resolve_parameter(parameters[0], trigger_name, pre_actions, get_type(parent_name, 0));
			std::string second_parameter = resolve_parameter(parameters[1], trigger_name, pre_actions, get_type(parent_name, 1));
			output += second_parameter.insert(second_parameter.find_first_of('(') + 1, first_parameter + ", ");
			return /*(add_call ? "call " : "") + */ output;
		}

		for (size_t k = 0; k < parameters.size(); k++) {
			const auto& i = parameters[k];

			const std::string type = get_type(parent_name, k);

			if (type == "boolexpr") {
				const std::string function_name = generate_function_name(trigger_name);

				std::string tttt = resolve_parameter(parameters[k], trigger_name, pre_actions, type);

				pre_actions += "function " + function_name + "()\n"; // returns boolean
				pre_actions += "return " + tttt + "\n";
				pre_actions += "end\n\n";

				output += function_name;
			} else if (type == "code") {
				const std::string function_name = generate_function_name(trigger_name);

				std::string tttt = resolve_parameter(parameters[k], trigger_name, pre_actions, type);

				pre_actions += "function " + function_name + "()\n"; // returns nothing
				pre_actions += tttt + "\n";
				pre_actions += "end\n\n";

				output += function_name;
			} else {
				output += resolve_parameter(i, trigger_name, pre_actions, type);
			}

			if (k < parameters.size() - 1) {
				output += ", ";
			}
		}

		return /*(add_call ? "call " : "") + */ (script_name.empty() ? parent_name : script_name) + "(" + output + ")";
	}

	std::string resolve_parameter(const TriggerParameter& parameter, const std::string& trigger_name, std::string& pre_actions, const std::string& type, bool add_call = false) const {
		if (parameter.has_sub_parameter) {
			return testt(trigger_name, parameter.sub_parameter.name, parameter.sub_parameter.parameters, pre_actions, add_call);
		} else {
			switch (parameter.type) {
				case TriggerParameter::Type::invalid:
					std::print("Invalid parameter type\n");
					return "";
				case TriggerParameter::Type::preset: {
					const std::string preset_type = trigger_data.data("TriggerParams", parameter.value, 1);

					if (get_base_type(preset_type) == "string") {
						return string_replaced(trigger_data.data("TriggerParams", parameter.value, 2), "`", "\"");
					}

					if (preset_type == "timedlifebuffcode" // ToDo this seems like a hack?
						|| type == "abilcode" || type == "buffcode" || type == "destructablecode" || type == "itemcode" || type == "ordercode" || type == "techcode" || type == "unitcode" || type == "heroskillcode" || type == "weathereffectcode" || type == "timedlifebuffcode" || type == "doodadcode" || type == "timedlifebuffcode" || type == "terraintype") {
						return "FourCC(" + trigger_data.data("TriggerParams", parameter.value, 2) + ")";
					}

					return trigger_data.data("TriggerParams", parameter.value, 2);
				}
				case TriggerParameter::Type::function:
					return parameter.value + "()";
				case TriggerParameter::Type::variable: {
					std::string output = parameter.value;

					if (!output.starts_with("gg_")) {
						output = "udg_" + output;
					}

					if (parameter.is_array) {
						output += "[" + resolve_parameter(parameter.parameters[0], trigger_name, pre_actions, "integer") + "]";
					}
					return output;
				}
				case TriggerParameter::Type::string:
					std::string import_type = trigger_data.data("TriggerTypes", type, 5);

					if (!import_type.empty()) {
						return "\"" + string_replaced(parameter.value, "\\", "\\\\") + "\"";
					} else if (get_base_type(type) == "string") {
						return "\"" + parameter.value + "\"";
					} else if (type == "abilcode" // ToDo this seems like a hack?
							   || type == "buffcode" || type == "destructablecode" || type == "itemcode" || type == "ordercode" || type == "techcode" || type == "unitcode" || type == "heroskillcode" || type == "weathereffectcode" || type == "timedlifebuffcode" || type == "doodadcode" || type == "timedlifebuffcode" || type == "terraintype") {
						return "FourCC('" + parameter.value + "')";
					} else {
						return parameter.value;
					}
			}
		}
		std::print("Unable to resolve parameter for trigger: {} and parameter value {}\n", trigger_name, parameter.value);
		return "";
	}

	std::string get_base_type(const std::string& type) const {
		std::string base_type = trigger_data.data("TriggerTypes", type, 4);

		if (base_type.empty()) {
			return type;
		}

		return base_type;
	}

	std::string get_type(const std::string& function_name, int parameter) const {
		std::string type;

		if (trigger_data.key_exists("TriggerActions", function_name)) {
			type = trigger_data.data("TriggerActions", function_name, 1 + parameter);
		} else if (trigger_data.key_exists("TriggerCalls", function_name)) {
			type = trigger_data.data("TriggerCalls", function_name, 3 + parameter);
		} else if (trigger_data.key_exists("TriggerEvents", function_name)) {
			type = trigger_data.data("TriggerEvents", function_name, 1 + parameter);
		} else if (trigger_data.key_exists("TriggerConditions", function_name)) {
			type = trigger_data.data("TriggerConditions", function_name, 1 + parameter);
		}
		return type;
	}

	std::string generate_function_name(const std::string& trigger_name) const {
		auto time = std::chrono::high_resolution_clock::now().time_since_epoch().count();
		return "Trig_" + trigger_name + "_" + std::to_string(time & 0xFFFFFFFF);
	}

	std::string convert_gui_to_jass(const Trigger& trigger, std::vector<std::string>& map_initializations) const {
		std::string trigger_name = trigger.name;
		trim(trigger_name);
		std::replace(trigger_name.begin(), trigger_name.end(), ' ', '_');

		const std::string trigger_variable_name = "gg_trg_" + trigger_name;
		const std::string trigger_action_name = "Trig_" + trigger_name + "_Actions";
		const std::string trigger_conditions_name = "Trig_" + trigger_name + "_Conditions";

		MapScriptWriter events;
		MapScriptWriter conditions;

		std::string pre_actions;
		MapScriptWriter actions;

		for (const auto& i : trigger.ecas) {
			if (!i.enabled) {
				continue;
			}

			switch (i.type) {
				case ECA::Type::event: {
					if (i.name == "MapInitializationEvent") {
						map_initializations.push_back(trigger_variable_name);
						continue;
					}

					std::string arguments;

					for (size_t k = 0; k < i.parameters.size(); k++) {
						const auto& p = i.parameters[k];

						if (get_type(i.name, k) == "VarAsString_Real") {
							arguments += "\"" + resolve_parameter(p, trigger_name, pre_actions, get_type(i.name, k)) + "\"";
						} else {
							arguments += resolve_parameter(p, trigger_name, pre_actions, get_type(i.name, k));
						}

						if (k < i.parameters.size() - 1) {
							arguments += ", ";
						}
					}

					events.call(i.name, trigger_variable_name, arguments);

					break;
				}
				case ECA::Type::condition:
					conditions.if_statement(std::format("not ({})", convert_eca_to_jass(i, pre_actions, trigger_name, true)), [&] {
						conditions.write_ln("return false");
					});
					break;
				case ECA::Type::action:
					actions.write_ln(convert_eca_to_jass(i, pre_actions, trigger_name, false));
					break;
			}
		}

		MapScriptWriter final_trigger;

		if (!conditions.is_empty()) {
			final_trigger.function(trigger_conditions_name, [&] {
			    final_trigger.merge(conditions);
				final_trigger.write_ln("return true");
			});
		}

		final_trigger.function(trigger_action_name, [&] {
			final_trigger.merge(actions);
		});

		final_trigger.function("InitTrig_" + trigger_name, [&] {
			final_trigger.set_variable(trigger_variable_name, "CreateTrigger()");
			final_trigger.merge(events);

			if (!conditions.is_empty()) {
				final_trigger.call("TriggerAddCondition", trigger_variable_name, trigger_conditions_name);
			}
			if (!trigger.initially_on) {
				final_trigger.call("DisableTrigger", trigger_variable_name);
			}
			final_trigger.call("TriggerAddAction", trigger_variable_name, trigger_action_name);
		});

		return final_trigger.script;

		// return pre_actions + conditions + actions + events;
	}

	void generate_global_variables(MapScriptWriter& script, std::unordered_map<std::string, std::string>& unit_variables, std::unordered_map<std::string, std::string>& destructable_variables, const Regions& regions, const GameCameras& cameras, const Sounds& sounds) {
		if (script.mode == MapScriptWriter::Mode::jass) {
			script.write_ln("globals");
		}

		if (script.mode == MapScriptWriter::Mode::jass) {
			for (const auto& variable : variables) {
				const std::string base_type = get_base_type(variable.type);
				if (variable.is_array) {
					//writer.write_ln(base_type, " array udg_", variable.name);
					script.global(base_type, "array udg_" + variable.name, script.null());
				} else {
					std::string default_value = trigger_data.data("TriggerTypeDefaults", base_type);

					if (default_value.empty()) { // handle?
						default_value = "null";
					}

					// writer.write_string("\t" + base_type + " udg_" + variable.name + " = " + default_value + "\n");
					script.global(base_type, "udg_" + variable.name, default_value);
				}
			}
		} else {
			for (const auto& variable : variables) {
				if (variable.is_array) {
					script.global(variable.type, "udg_" + variable.name, "__jarray(\"\")");
				} else {
					script.global(variable.type, "udg_" + variable.name, script.null());
				}
			}
		}

		for (const auto& i : regions.regions) {
			std::string region_name = i.name;
			trim(region_name);
			std::replace(region_name.begin(), region_name.end(), ' ', '_');
			script.global("rect", "gg_rct_" + region_name, script.null());
		}

		for (const auto& i : cameras.cameras) {
			std::string camera_name = i.name;
			trim(camera_name);
			std::replace(camera_name.begin(), camera_name.end(), ' ', '_');
			script.global("camerasetup", "gg_cam_" + camera_name, script.null());
		}

		for (const auto& i : sounds.sounds) {
			std::string sound_name = i.name;
			trim(sound_name);
			std::replace(sound_name.begin(), sound_name.end(), ' ', '_');
			script.global("sound", sound_name, script.null());
		}

		for (const auto& i : triggers) {
			if (i.is_comment || !i.is_enabled) {
				continue;
			}

			std::string trigger_name = i.name;
			trim(trigger_name);
			std::replace(trigger_name.begin(), trigger_name.end(), ' ', '_');
			script.global("trigger", "gg_trg_" + trigger_name, script.null());
		}

		for (const auto& [creation_number, type] : unit_variables) {
			script.global("unit", "gg_unit_" + type + "_" + creation_number, script.null());
		}

		for (const auto& [creation_number, type] : destructable_variables) {
			script.global("destructable", "gg_dest_" + type + "_" + creation_number, script.null());
		}

		if (script.mode == MapScriptWriter::Mode::jass) {
			script.write_ln("endglobals");
		}
	}

	void generate_init_global_variables(MapScriptWriter& script) {
		script.function("InitGlobals", [&]() {
			for (const auto& variable : variables) {
				const std::string base_type = trigger_data.data("TriggerTypes", variable.type, 4);
				const std::string type = base_type.empty() ? variable.type : base_type;
				const std::string default_value = trigger_data.data("TriggerTypeDefaults", type);

				if (!variable.is_initialized && default_value.empty()) {
					continue;
				}

				if (variable.is_array) {
					script.forloop(0, variable.array_size, [&]() {
						if (variable.is_initialized) {
							if (type == "string" && variable.initial_value.empty()) {
								script.set_variable("udg_" + variable.name + "[i]", "\"\"");
							} else {
								script.set_variable("udg_" + variable.name + "[i]", "\"" + variable.initial_value + "\"");
							}
						} else {
							if (type == "string") {
								script.set_variable("udg_" + variable.name + "[i]", "\"\"");
							} else {
								script.set_variable("udg_" + variable.name + "[i]", default_value);
							}
						}
					});
				} else if (type == "string") {
					if (variable.is_initialized) {
						script.set_variable("udg_" + variable.name, "\"" + variable.initial_value + "\"");
					} else {
						script.set_variable("udg_" + variable.name, "\"\"");
					}
				} else {
					if (variable.is_initialized) {
						const std::string converted_value = trigger_data.data("TriggerParams", variable.initial_value, 2);

						if (converted_value.empty()) {
							script.set_variable("udg_" + variable.name, variable.initial_value);
						} else {
							script.set_variable("udg_" + variable.name, converted_value);
						}
					} else {
						script.set_variable("udg_" + variable.name, default_value);
					}
				}
			}
		});
	}

	void generate_units(MapScriptWriter& script, std::unordered_map<std::string, std::string>& unit_variables, const Terrain& terrain, const Units& units) {
		script.function("CreateAllUnits", [&]() {
			script.local("unit", "u", script.null());
			script.local("integer", "unitID", "0");
			script.local("trigger", "t", script.null());
			script.local("real", "life", "0");

			for (const auto& i : units.units) {
				if (i.id == "sloc") {
					continue;
				}

				std::string unit_reference = "u";
				if (unit_variables.contains(std::format("{:0>4}", i.creation_number))) {
					unit_reference = std::format("gg_unit_{}_{:0>4}", i.id, i.creation_number);
				}

				script.set_variable(unit_reference, std::format("BlzCreateUnitWithSkin(Player({}), {}, {:.4f}, {:.4f}, {:.4f}, {})",
																i.player,
																script.four_cc(i.id),
																i.position.x * 128.f + terrain.offset.x,
																i.position.y * 128.f + terrain.offset.y,
																glm::degrees(i.angle),
																script.four_cc(i.skin_id)));

				if (i.health != -1) {
					script.set_variable("life", std::format("GetUnitState({}, {})", unit_reference, "UNIT_STATE_LIFE"));
					script.call("SetUnitState", unit_reference, "UNIT_STATE_LIFE", std::to_string(i.health / 100.f) + " * life");
				}

				if (i.mana != -1) {
					script.call("SetUnitState", unit_reference, "UNIT_STATE_MANA", i.mana);
				}
				if (i.level != 1) {
					script.call("SetHeroLevel", unit_reference, i.level, "false");
				}

				if (i.strength != 0) {
					script.call("SetHeroStr", unit_reference, i.strength, "true");
				}

				if (i.agility != 0) {
					script.call("SetHeroAgi", unit_reference, i.agility, "true");
				}

				if (i.intelligence != 0) {
					script.call("SetHeroInt", unit_reference, i.intelligence, "true");
				}

				float range;
				if (i.target_acquisition != -1.f) {
					if (i.target_acquisition == -2.f) {
						range = 200.f;
					} else {
						range = i.target_acquisition;
					}
					script.call("SetUnitAcquireRange", unit_reference, range);
				}

				for (const auto& j : i.abilities) {
					for (size_t k = 0; k < std::get<2>(j); k++) {
						script.call("SelectHeroSkill", unit_reference, script.four_cc(std::get<0>(j)));
					}

					if (std::get<1>(j)) {
						std::string order_on = abilities_slk.data("orderon", std::get<0>(j));
						if (order_on.empty()) {
							order_on = abilities_slk.data("order", std::get<0>(j));
						}
						script.call("IssueImmediateOrder", unit_reference, "\"" + order_on + "\"");

					} else {
						std::string order_off = abilities_slk.data("orderoff", std::get<0>(j));
						if (!order_off.empty()) {
							script.call("IssueImmediateOrder", unit_reference, "\"" + order_off + "\"");
						}
					}
				}

				for (const auto& j : i.items) {
					script.call("UnitAddItemToSlotById", unit_reference, script.four_cc(j.second), j.first);
				}

				if (i.item_sets.size()) {
					script.set_variable("t", "CreateTrigger()");
					script.call("TriggerRegisterUnitEvent", "t", unit_reference, "EVENT_UNIT_DEATH");
					script.call("TriggerRegisterUnitEvent", "t", unit_reference, "EVENT_UNIT_CHANGE_OWNER");
					script.call("TriggerAddAction", "t", "UnitItemDrops_" + std::to_string(i.creation_number));
				}
			}
		});
	}

	void generate_items(MapScriptWriter& script, const Terrain& terrain, const Units& units) {
		script.function("CreateAllItems", [&]() {
			for (const auto& i : units.items) {
				script.call("BlzCreateItemWithSkin", script.four_cc(i.id), i.position.x * 128.f + terrain.offset.x, i.position.y * 128.f + terrain.offset.y, script.four_cc(i.id));
			}
		});
	}

	void generate_destructables(MapScriptWriter& script, std::unordered_map<std::string, std::string>& destructable_variables, const Terrain& terrain, const Doodads& doodads) {
		script.function("CreateAllDestructables", [&]() {
			script.local("destructable", "d", script.null());
			script.local("trigger", "t", script.null());
			script.local("real", "life", "0");

			for (const auto& i : doodads.doodads) {
				std::string id = "d";

				if (destructable_variables.contains(std::to_string(i.creation_number))) {
					id = "gg_dest_" + i.id + "_" + std::to_string(i.creation_number);
				}

				if (id == "d" && i.item_sets.empty() && i.item_table_pointer == -1) {
					continue;
				}

				script.set_variable(id, std::format("BlzCreateDestructableZWithSkin({}, {:.4f}, {:.4f}, {:.4f}, {}, {}, {}, {})",
													script.four_cc(i.id),
													i.position.x * 128.f + terrain.offset.x,
													i.position.y * 128.f + terrain.offset.y,
													i.position.z * 128.f,
													glm::degrees(i.angle),
													i.scale.x,
													i.variation,
													script.four_cc(i.skin_id)));

				if (i.life != 100) {
					script.set_variable("life", "GetDestructableLife(" + id + ")");
					script.call("SetDestructableLife", id, std::to_string(i.life / 100.f) + " * life");
				}

				if (!i.item_sets.empty()) {
					script.set_variable("t", "CreateTrigger()");
					script.call("TriggerRegisterDeathEvent", "t", id);
					script.call("TriggerAddAction", "t", "SaveDyingWidget");
					script.call("TriggerAddAction", "t", "DoodadItemDrops_" + std::to_string(i.creation_number));
				} else if (i.item_table_pointer != -1) {
					script.set_variable("t", "CreateTrigger()");
					script.call("TriggerRegisterDeathEvent", "t", id);
					script.call("TriggerAddAction", "t", "SaveDyingWidget");
					script.call("TriggerAddAction", "t", "ItemTable_" + std::to_string(i.item_table_pointer));
				}
			}
		});
	}

	void generate_regions(MapScriptWriter& script, const Regions& regions) {
		script.function("CreateRegions", [&]() {
			script.local("weathereffect", "we", script.null());
			for (const auto& i : regions.regions) {
				std::string region_name = "gg_rct_" + i.name;
				trim(region_name);
				std::replace(region_name.begin(), region_name.end(), ' ', '_');

				script.set_variable(region_name, std::format("Rect({}, {}, {}, {})", std::min(i.left, i.right), std::min(i.bottom, i.top), std::max(i.left, i.right), std::max(i.bottom, i.top)));

				if (!i.weather_id.empty()) {
					script.set_variable("we", std::format("AddWeatherEffect({}, {})", region_name, script.four_cc(i.weather_id)));
					script.call("EnableWeatherEffect", "we", true);
				}
			}
		});
	}

	void generate_cameras(MapScriptWriter& script, const GameCameras& cameras) {
		script.function("CreateCameras", [&]() {
			for (const auto& i : cameras.cameras) {
				std::string camera_name = "gg_cam_" + i.name;
				trim(camera_name);
				std::replace(camera_name.begin(), camera_name.end(), ' ', '_');

				script.set_variable(camera_name, "CreateCameraSetup()");
				script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_ZOFFSET", i.z_offset, 0.0);
				script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_ROTATION", i.rotation, 0.0);
				script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_ANGLE_OF_ATTACK", i.angle_of_attack, 0.0);
				script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_TARGET_DISTANCE", i.distance, 0.0);
				script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_ROLL", i.roll, 0.0);
				script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_FIELD_OF_VIEW", i.fov, 0.0);
				script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_FARZ", i.far_z, 0.0);
				script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_NEARZ", i.near_z, 0.0);
				script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_LOCAL_PITCH", i.local_pitch, 0.0);
				script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_LOCAL_YAW", i.local_yaw, 0.0);
				script.call("CameraSetupSetField", camera_name, "CAMERA_FIELD_LOCAL_ROLL", i.local_roll, 0.0);
				script.call("CameraSetupSetDestPosition", camera_name, i.target_x, i.target_y, 0.0);
			}
		});
	}

	// Todo, missing fields, soundduration also wrong
	void generate_sounds(MapScriptWriter& script, const Sounds& sounds) {
		script.function("InitSounds", [&]() {
			for (const auto& i : sounds.sounds) {
				std::string sound_name = i.name;
				trim(sound_name);
				std::replace(sound_name.begin(), sound_name.end(), ' ', '_');

				script.set_variable(sound_name, std::format("CreateSound(\"{}\", {}, {}, {}, {}, {}, \"{}\")",
										 string_replaced(i.file, "\\", "\\\\"),
										 i.looping ? "true" : "false",
										 i.is_3d ? "true" : "false",
										 i.stop_out_of_range ? "true" : "false",
										 i.fade_in_rate,
										 i.fade_out_rate,
										 string_replaced(i.eax_effect, "\\", "\\\\")));

				script.call("SetSoundDuration", sound_name, i.fade_in_rate);
				script.call("SetSoundChannel", sound_name, i.channel);
				script.call("SetSoundVolume", sound_name, i.volume);
				script.call("SetSoundPitch", sound_name, i.pitch);
			}
		});
	}

	void generate_trigger_initialization(MapScriptWriter& script, std::vector<std::string> initialization_triggers) {
		script.function("InitCustomTriggers", [&]() {
			for (const auto& i : triggers) {
				if (i.is_comment || !i.is_enabled) {
					continue;
				}
				std::string trigger_name = i.name;
				trim(trigger_name);
				std::replace(trigger_name.begin(), trigger_name.end(), ' ', '_');

				script.call("InitTrig_" + trigger_name);
			}
		});

		script.function("RunInitializationTriggers", [&]() {
			for (const auto& i : initialization_triggers) {
				script.call("ConditionalTriggerExecute", i);
			}
		});
	}

	void generate_players(MapScriptWriter& script, const MapInfo& map_info) {
		script.function("InitCustomPlayerSlots", [&]() {
			const std::vector<std::string> players = { "MAP_CONTROL_USER", "MAP_CONTROL_COMPUTER", "MAP_CONTROL_NEUTRAL", "MAP_CONTROL_RESCUABLE" };
			const std::vector<std::string> races = { "RACE_PREF_RANDOM", "RACE_PREF_HUMAN", "RACE_PREF_ORC", "RACE_PREF_UNDEAD", "RACE_PREF_NIGHTELF" };

			size_t index = 0;
			for (const auto& i : map_info.players) {
				std::string player = "Player(" + std::to_string(i.internal_number) + ")";

				script.call("SetPlayerStartLocation", player, index);
				if (i.fixed_start_position || i.race == PlayerRace::selectable) {
					script.call("ForcePlayerStartLocation", player, index);
				}

				script.call("SetPlayerColor", player, "ConvertPlayerColor(" + std::to_string(i.internal_number) + ")");
				script.call("SetPlayerRacePreference", player, races[static_cast<int>(i.race)]);
				script.call("SetPlayerRaceSelectable", player, true);
				script.call("SetPlayerController", player, players[static_cast<int>(i.type)]);

				if (i.type == PlayerType::rescuable) {
					for (const auto& j : map_info.players) {
						if (j.type == PlayerType::human) {
							script.call("SetPlayerAlliance", player, "Player(" + std::to_string(j.internal_number) + ")", "ALLIANCE_RESCUABLE", true);
						}
					}
				}

				script.write("\n");
				index++;
			}
		});
	}

	void generate_custom_teams(MapScriptWriter& script, const MapInfo& map_info) {
		script.function("InitCustomTeams", [&]() {
			int current_force = 0;
			for (const auto& i : map_info.forces) {
				for (const auto& j : map_info.players) {
					if (i.player_masks & (1 << j.internal_number)) {
						script.call("SetPlayerTeam", "Player(" + std::to_string(j.internal_number) + ")", current_force);

						if (i.allied_victory) {
							script.call("SetPlayerState", "Player(" + std::to_string(j.internal_number) + ")", "PLAYER_STATE_ALLIED_VICTORY", 1);
						}
					}
				}

				for (const auto& j : map_info.players) {
					if (i.player_masks & (1 << j.internal_number)) {
						for (const auto& k : map_info.players) {
							if (i.player_masks & (1 << k.internal_number) && j.internal_number != k.internal_number) {
								if (i.allied) {
									script.call("SetPlayerAllianceStateAllyBJ", "Player(" + std::to_string(j.internal_number) + ")", "Player(" + std::to_string(k.internal_number) + ")", true);
								}
								if (i.share_vision) {
									script.call("SetPlayerAllianceStateVisionBJ", "Player(" + std::to_string(j.internal_number) + ")", "Player(" + std::to_string(k.internal_number) + ")", true);
								}
								if (i.share_unit_control) {
									script.call("SetPlayerAllianceStateControlBJ", "Player(" + std::to_string(j.internal_number) + ")", "Player(" + std::to_string(k.internal_number) + ")", true);
								}
								if (i.share_advanced_unit_control) {
									script.call("SetPlayerAllianceStateFullControlBJ", "Player(" + std::to_string(j.internal_number) + ")", "Player(" + std::to_string(k.internal_number) + ")", true);
								}
							}
						}
					}
				}
				current_force++;
			}
		});
	}

	void generate_ally_priorities(MapScriptWriter& script, const MapInfo& map_info) {
		script.function("InitAllyPriorities", [&]() {
			std::unordered_map<int, int> player_to_startloc;

			int current_player = 0;
			for (const auto& i : map_info.players) {
				player_to_startloc[i.internal_number] = current_player;
				current_player++;
			}

			current_player = 0;
			for (const auto& i : map_info.players) {
				size_t count = 0;
				for (const auto& j : map_info.players) {
					if (i.ally_low_priorities_flags & (1 << j.internal_number) && i.internal_number != j.internal_number) {
						count++;
					} else if (i.ally_high_priorities_flags & (1 << j.internal_number) && i.internal_number != j.internal_number) {
						count++;
					}
				}

				script.call("SetStartLocPrioCount", current_player, count);

				size_t current_index = 0;
				for (const auto& j : map_info.players) {
					if (i.ally_low_priorities_flags & (1 << j.internal_number) && i.internal_number != j.internal_number) {
						script.call("SetStartLocPrio", current_player, current_index, player_to_startloc[j.internal_number], "MAP_LOC_PRIO_LOW");
						current_index++;
					} else if (i.ally_high_priorities_flags & (1 << j.internal_number) && i.internal_number != j.internal_number) {
						script.call("SetStartLocPrio", current_player, current_index, player_to_startloc[j.internal_number], "MAP_LOC_PRIO_HIGH");
						current_index++;
					}
				}

				current_player++;
			}
		});
	}

	void generate_main(MapScriptWriter& script, const Terrain& terrain, const MapInfo& map_info) {
		script.function("main", [&]() {
			script.call("SetCameraBounds",
						std::to_string(map_info.camera_left_bottom.x - 512.f) + " + GetCameraMargin(CAMERA_MARGIN_LEFT)",
						std::to_string(map_info.camera_left_bottom.y - 256.f) + " + GetCameraMargin(CAMERA_MARGIN_BOTTOM)",

						std::to_string(map_info.camera_right_top.x + 512.f) + " - GetCameraMargin(CAMERA_MARGIN_RIGHT)",
						std::to_string(map_info.camera_right_top.y + 256.f) + " - GetCameraMargin(CAMERA_MARGIN_TOP)",

						std::to_string(map_info.camera_left_top.x - 512.f) + " + GetCameraMargin(CAMERA_MARGIN_LEFT)",
						std::to_string(map_info.camera_left_top.y + 256.f) + " - GetCameraMargin(CAMERA_MARGIN_TOP)",

						std::to_string(map_info.camera_right_bottom.x + 512.f) + " - GetCameraMargin(CAMERA_MARGIN_RIGHT)",
						std::to_string(map_info.camera_right_bottom.y - 256.f) + " + GetCameraMargin(CAMERA_MARGIN_BOTTOM)");

			const std::string terrain_lights = string_replaced(world_edit_data.data("TerrainLights", ""s + terrain.tileset), "\\", "/");
			const std::string unit_lights = string_replaced(world_edit_data.data("TerrainLights", ""s + terrain.tileset), "\\", "/");
			script.call("SetDayNightModels", "\"" + terrain_lights + "\"", "\"" + unit_lights + "\"");

			const std::string sound_environment = string_replaced(world_edit_data.data("SoundEnvironment", ""s + terrain.tileset), "\\", "/");
			script.call("NewSoundEnvironment", "\"" + sound_environment + "\"");

			const std::string ambient_day = string_replaced(world_edit_data.data("DayAmbience", ""s + terrain.tileset), "\\", "/");
			script.call("SetAmbientDaySound", "\"" + ambient_day + "\"");

			const std::string ambient_night = string_replaced(world_edit_data.data("NightAmbience", ""s + terrain.tileset), "\\", "/");
			script.call("SetAmbientNightSound", "\"" + ambient_night + "\"");

			script.call("SetMapMusic", "\"Music\"", true, 0);
			script.call("InitSounds");
			script.call("CreateRegions");
			script.call("CreateCameras");
			script.call("CreateDestructables");
			script.call("CreateItems");
			script.call("CreateUnits");
			script.call("InitBlizzard");
			script.call("InitGlobals");
			script.call("InitCustomTriggers");
			script.call("RunInitializationTriggers");
		});
	}

	void generate_map_configuration(MapScriptWriter& script, const Terrain& terrain, const Units& units, const MapInfo& map_info) {
		script.function("config", [&]() {
			script.call("SetMapName", "\"" + map_info.name + "\"");
			script.call("SetMapDescription", "\"" + map_info.description + "\"");
			script.call("SetPlayers", map_info.players.size());
			script.call("SetTeams", map_info.forces.size());
			script.call("SetGamePlacement", "MAP_PLACEMENT_USE_MAP_SETTINGS");

			script.write("\n");

			for (const auto& i : units.units) {
				if (i.id == "sloc") {
					script.call("DefineStartLocation", i.player, i.position.x * 128.f + terrain.offset.x, i.position.y * 128.f + terrain.offset.y);
				}
			}

			script.write("\n");

			script.call("InitCustomPlayerSlots");
			if (map_info.custom_forces) {
				script.call("InitCustomTeams");
			} else {
				for (const auto& i : map_info.players) {
					script.call("SetPlayerSlotAvailable", "Player(" + std::to_string(i.internal_number) + ")", "MAP_CONTROL_USER");
				}

				script.call("InitGenericPlayerSlots");
			}
			script.call("InitAllyPriorities");
		});
	}

public:
	void load() {
		BinaryReader reader = hierarchy.map_file_read("war3map.wtg");

		trigger_strings.load("UI/TriggerStrings.txt");
		trigger_data.load("UI/TriggerData.txt");
		trigger_data.substitute(world_edit_strings, "WorldEditStrings");

		// Manual fixes
		trigger_data.set_whole_data("TriggerTypeDefaults", "string", "\"\"");

		for (auto&& section : { "TriggerActions"s, "TriggerEvents"s, "TriggerConditions"s, "TriggerCalls"s }) {
			for (const auto& [key, value] : trigger_data.section(section)) {
				if (key.front() == '_') {
					continue;
				}

				int arguments = 0;
				for (const auto& j : value) {
					arguments += !j.empty() && !is_number(j) && j != "nothing";
				}

				if (section == "TriggerCalls") {
					--arguments;
				}

				argument_counts[key] = arguments;
			}
		}

		Trigger::next_id = 0;

		std::string magic_number = reader.read_string(4);
		if (magic_number != "WTG!") {
			std::print("Unknown magic number for war3map.wtg {}\n", magic_number);
			return;
		}

		uint32_t version = reader.read<uint32_t>();
		if (version == 0x80000004)
			load_version_31(reader, version);
		else if (version == 4 || version == 7)
			load_version_pre31(reader, version);
		else {
			std::print("Unknown WTG format! Trying 1.31 loader\n");
			load_version_31(reader, version);
		}
	}


	void load_version_31(BinaryReader& reader, uint32_t version) {
		uint32_t sub_version = reader.read<uint32_t>();
		if (sub_version != 7 && sub_version != 4) {
			std::print("Unknown 1.31 WTG subformat! Trying anyway.\n");
		}

		reader.advance(4);							 // map_count
		reader.advance(4 * reader.read<uint32_t>()); // map ids of deleted maps

		reader.advance(4);							 // library_count
		reader.advance(4 * reader.read<uint32_t>()); // library ids of deleted libraries

		reader.advance(4);							 // category_count
		reader.advance(4 * reader.read<uint32_t>()); // category ids of deleted categories

		reader.advance(4);							 // trigger_count
		reader.advance(4 * reader.read<uint32_t>()); // trigger ids of deleted triggers

		reader.advance(4);							 // comment_count
		reader.advance(4 * reader.read<uint32_t>()); // comment ids of deleted comments

		reader.advance(4);							 // script_count
		reader.advance(4 * reader.read<uint32_t>()); // script ids of deleted scripts

		reader.advance(4);							 // variable_count
		reader.advance(4 * reader.read<uint32_t>()); // variable ids of deleted variables

		unknown1 = reader.read<uint32_t>();
		unknown2 = reader.read<uint32_t>();
		trig_def_ver = reader.read<uint32_t>();

		uint32_t variable_count = reader.read<uint32_t>();
		for (uint32_t i = 0; i < variable_count; i++) {
			TriggerVariable variable;
			variable.name = reader.read_c_string();
			variable.type = reader.read_c_string();
			variable.unknown = reader.read<uint32_t>();
			variable.is_array = reader.read<uint32_t>();
			if (sub_version == 7) {
				variable.array_size = reader.read<uint32_t>();
			}
			variable.is_initialized = reader.read<uint32_t>();
			variable.initial_value = reader.read_c_string();
			variable.id = reader.read<uint32_t>();
			variable.parent_id = reader.read<uint32_t>();
			variables.push_back(variable);

			Trigger::next_id = std::max(Trigger::next_id, variable.id + 1);
		}

		uint32_t element_count = reader.read<uint32_t>();

		for (uint32_t i = 0; i < element_count; i++) {
			Classifier classifier = static_cast<Classifier>(reader.read<uint32_t>());
			switch (classifier) {
				case Classifier::map:
				case Classifier::library:
				case Classifier::category: {
					TriggerCategory cat;
					cat.classifier = classifier;
					cat.id = reader.read<uint32_t>();
					cat.name = reader.read_c_string();
					if (sub_version == 7) {
						cat.is_comment = reader.read<uint32_t>();
					}
					cat.open_state = reader.read<uint32_t>();
					cat.parent_id = reader.read<uint32_t>();
					categories.push_back(cat);

					Trigger::next_id = std::max(Trigger::next_id, cat.id + 1);
					break;
				}
				case Classifier::gui:
				case Classifier::comment:
				case Classifier::script: {
					Trigger trigger;
					trigger.classifier = classifier;
					trigger.name = reader.read_c_string();
					trigger.description = reader.read_c_string();
					if (sub_version == 7) {
						trigger.is_comment = reader.read<uint32_t>();
					}
					trigger.id = reader.read<uint32_t>();
					trigger.is_enabled = reader.read<uint32_t>();
					trigger.is_script = reader.read<uint32_t>();
					trigger.initially_on = !reader.read<uint32_t>();
					trigger.run_on_initialization = reader.read<uint32_t>();
					trigger.parent_id = reader.read<uint32_t>();
					trigger.ecas.resize(reader.read<uint32_t>());
					for (auto& j : trigger.ecas) {
						parse_eca_structure(reader, j, false, sub_version);
					}

					triggers.push_back(trigger);

					Trigger::next_id = std::max(Trigger::next_id, trigger.id + 1);
					break;
				}
				case Classifier::variable: {
					reader.advance(4);		   // id
					reader.advance_c_string(); // name
					reader.advance(4);		   // parentid
					break;
				}
			}
		}
	}

	void load_version_pre31(BinaryReader& reader, uint32_t version) {
		std::print("Importing pre-1.31 trigger format\n");

		categories.resize(reader.read<uint32_t>());
		for (auto& i : categories) {
			i.classifier = Classifier::category;
			i.id = reader.read<uint32_t>();
			i.name = reader.read_c_string();
			i.parent_id = 0;
			if (version == 7) {
				i.is_comment = reader.read<uint32_t>();
			}

			Trigger::next_id = std::max(Trigger::next_id, i.id + 1);
			if (i.id == 0) {
				i.id = -2;
			}
		}

		reader.advance(4); // dunno

		int variable_category = Trigger::next_id++;
		categories.insert(categories.begin(), { Classifier::map, 0, "Map Header", true, false, -1 });
		categories.insert(categories.begin(), { Classifier::category, variable_category, "Variables", true, false, 0 });

		variables.resize(reader.read<uint32_t>());
		for (auto& i : variables) {
			i.name = reader.read_c_string();
			i.type = reader.read_c_string();
			i.unknown = reader.read<uint32_t>();
			i.id = Trigger::next_id++;

			i.is_array = reader.read<uint32_t>();
			if (version == 7) {
				i.array_size = reader.read<uint32_t>();
			}
			i.is_initialized = reader.read<uint32_t>();
			i.initial_value = reader.read_c_string();
			i.parent_id = variable_category;
		}

		triggers.resize(reader.read<uint32_t>());
		for (auto& i : triggers) {
			i.name = reader.read_c_string();
			i.description = reader.read_c_string();
			if (version == 7) {
				i.is_comment = reader.read<uint32_t>();
			}
			i.is_enabled = reader.read<uint32_t>();
			i.is_script = reader.read<uint32_t>();
			i.initially_on = !reader.read<uint32_t>();
			i.run_on_initialization = reader.read<uint32_t>();

			i.id = Trigger::next_id++;

			if (i.run_on_initialization && i.is_script) {
				i.classifier = Classifier::gui;
			} else if (i.is_comment) {
				i.classifier = Classifier::comment;
			} else if (i.is_script) {
				i.classifier = Classifier::script;
			} else {
				i.classifier = Classifier::gui;
			}

			i.parent_id = reader.read<uint32_t>();
			if (i.parent_id == 0) {
				i.parent_id = -2;
			}
			i.ecas.resize(reader.read<uint32_t>());
			for (auto& j : i.ecas) {
				parse_eca_structure(reader, j, false, version);
			}
		}
	}

	void load_jass() {
		BinaryReader reader = hierarchy.map_file_read("war3map.wct");

		const uint32_t version = reader.read<uint32_t>();
		if (version != 0x80000004) {
			if (version == 1 || version == 0) {
				if (version == 1) {
					global_jass_comment = reader.read_c_string();
					global_jass = reader.read_string(reader.read<uint32_t>());
				}
				reader.advance(4);
				for (auto&& i : triggers) {
					const uint32_t size = reader.read<uint32_t>();
					if (size > 0) {
						i.custom_text = reader.read_string(size);
					}
				}
				return;
			} else {
				std::print("Probably invalid WCT format\n");
			}
		}

		const int sub_version = reader.read<uint32_t>();
		if (sub_version != 1 && sub_version != 0) {
			std::print("Unknown WCT 1.31 subformat\n");
		}

		if (sub_version == 1) {
			global_jass_comment = reader.read_c_string();
			int size = reader.read<uint32_t>();
			if (size > 0) {
				global_jass = reader.read_string(size);
			}
		}

		for (auto& i : triggers) {
			if (!i.is_comment) {
				int size = reader.read<uint32_t>();
				if (size > 0) {
					i.custom_text = reader.read_string(size);
				}
			}
		}
	}

	void save() const {
		BinaryWriter writer;
		writer.write_string("WTG!");
		writer.write<uint32_t>(write_version);
		writer.write<uint32_t>(write_sub_version);

		writer.write<uint32_t>(0);
		writer.write<uint32_t>(0);

		writer.write<uint32_t>(0);
		writer.write<uint32_t>(0);

		writer.write<uint32_t>(0);
		writer.write<uint32_t>(0);

		writer.write<uint32_t>(0);
		writer.write<uint32_t>(0);

		writer.write<uint32_t>(0);
		writer.write<uint32_t>(0);

		writer.write<uint32_t>(0);
		writer.write<uint32_t>(0);

		writer.write<uint32_t>(0);
		writer.write<uint32_t>(0);

		writer.write<uint32_t>(unknown1);
		writer.write<uint32_t>(unknown2);
		writer.write<uint32_t>(trig_def_ver);
		writer.write<uint32_t>(variables.size());

		for (const auto& i : variables) {
			writer.write_c_string(i.name);
			writer.write_c_string(i.type);
			writer.write<uint32_t>(i.unknown);
			writer.write<uint32_t>(i.is_array);
			writer.write<uint32_t>(i.array_size);
			writer.write<uint32_t>(i.is_initialized);
			writer.write_c_string(i.initial_value);
			writer.write<uint32_t>(i.id);
			writer.write<uint32_t>(i.parent_id);
		}

		writer.write<uint32_t>(categories.size() + triggers.size() + variables.size());

		for (const auto& i : categories) {
			writer.write<uint32_t>(static_cast<int>(i.classifier));
			writer.write<uint32_t>(i.id);
			writer.write_c_string(i.name);
			writer.write<uint32_t>(i.is_comment);
			writer.write<uint32_t>(i.open_state);
			writer.write<uint32_t>(i.parent_id);
		}

		for (const auto& i : triggers) {
			writer.write<uint32_t>(static_cast<int>(i.classifier));
			writer.write_c_string(i.name);
			writer.write_c_string(i.description);

			writer.write<uint32_t>(i.is_comment);
			writer.write<uint32_t>(i.id);
			writer.write<uint32_t>(i.is_enabled);
			writer.write<uint32_t>(i.is_script);
			writer.write<uint32_t>(!i.initially_on);
			writer.write<uint32_t>(i.run_on_initialization);
			writer.write<uint32_t>(i.parent_id);
			writer.write<uint32_t>(i.ecas.size());
			for (const auto& eca : i.ecas) {
				print_eca_structure(writer, eca, false);
			}
		}

		for (const auto& i : variables) {
			writer.write<uint32_t>(static_cast<int>(Classifier::variable));
			writer.write<uint32_t>(i.id);
			writer.write_c_string(i.name);
			writer.write<uint32_t>(i.parent_id);
		}

		hierarchy.map_file_write("war3map.wtg", writer.buffer);
	}

	void save_jass() const {
		BinaryWriter writer;

		writer.write<uint32_t>(write_version);
		writer.write<uint32_t>(1);

		writer.write_c_string(global_jass_comment);
		if (global_jass.size() == 0) {
			writer.write<uint32_t>(0);
		} else {
			writer.write<uint32_t>(global_jass.size() + (global_jass.back() == '\0' ? 0 : 1));
			writer.write_c_string(global_jass);
		}

		// Custom text (jass) needs to be saved in the order they appear in the hierarchy
		for (const auto& j : categories) {
			for (const auto& i : triggers) {
				if (i.parent_id == j.id) {
					if (!i.is_comment) {
						if (i.custom_text.size() == 0) {
							writer.write<uint32_t>(0);
						} else {
							writer.write<uint32_t>(i.custom_text.size() + (i.custom_text.back() == '\0' ? 0 : 1));
							writer.write_c_string(i.custom_text);
						}
					}
				}
			}
		}

		hierarchy.map_file_write("war3map.wct", writer.buffer);
	}

	template <typename T>
	void generate_item_tables(MapScriptWriter& script, const std::string& table_name_prefix, std::vector<T> table_holders) {
		for (const auto& i : table_holders) {
			if (i.item_sets.empty()) {
				continue;
			}

			script.function(table_name_prefix + std::to_string(i.creation_number), [&]() {
				script.local("widget", "trigWidget", script.null());
				script.local("unit", "trigUnit", script.null());
				script.local("integer", "itemID", "0");
				script.local("boolean", "canDrop", "true");

				script.set_variable("trigWidget", "bj_lastDyingWidget");

				script.if_statement("trigWidget == " + script.null(), [&]() {
					script.set_variable("trigUnit", "GetTriggerUnit()");
				});

				script.if_statement("trigUnit ~= " + script.null(), [&]() {
					script.set_variable("canDrop", "not IsUnitHidden(trigUnit)");
					script.if_statement("canDrop and GetChangingUnit() ~= " + script.null(), [&]() {
						script.set_variable("canDrop", "(GetChangingUnitPrevOwner() == Player(PLAYER_NEUTRAL_AGGRESSIVE))");
					});
				});

				script.if_statement("canDrop", [&]() {
					for (const auto& j : i.item_sets) {
						script.call("RandomDistReset");
						for (const auto& [chance, id] : j.items) {
							Triggers::write_item_table_entry(script, chance, id);
						}
						script.set_variable("itemID", "RandomDistChoose()");
						script.if_statement("trigUnit ~= " + script.null(), [&]() {
							script.call("UnitDropitem", "trigUnit", "itemID"); // Todo fourcc?
						});
						script.if_statement("trigUnit == " + script.null(), [&]() {
							script.call("WidgetDropItem", "trigWidget", "itemID"); // Todo fourcc?
						});
					}
				});

				script.set_variable("bj_lastDyingWidget", script.null());
				script.call("DestroyTrigger", "GetTriggeringTrigger()");
			});
		}
	}

	static void write_item_table_entry(MapScriptWriter& script, int chance, const std::string& id) {
		if (id == "") {
			script.call("RandomDistAddItem", -1, chance);
		} else if (id[0] == 'Y' && id[2] == 'I' &&
				   ((id[1] >= 'i' && id[1] <= 'o') || id[1] == 'Y')) { // Random items

			std::string item_type;
			switch (id[1]) {
				case 'i':
					item_type = "PERMANENT";
					break;
				case 'j':
					item_type = "CHARGED";
					break;
				case 'k':
					item_type = "POWERUP";
					break;
				case 'l':
					item_type = "ARTIFACT";
					break;
				case 'm':
					item_type = "PURCHASABLE";
					break;
				case 'n':
					item_type = "CAMPAIGN";
					break;
				case 'o':
					item_type = "MISCELLANEOUS";
					break;
				case 'Y':
					item_type = "ANY";
					break;
			}

			const std::string random_item = std::format("ChooseRandomItemEx(ITEM_TYPE_{}, {})", item_type, (id[3] == '/') ? "-1" : std::string(1, id[3]));
			script.call("RandomDistAddItem", random_item, chance);
				   } else {
				   	script.call("RandomDistAddItem", script.four_cc(id), chance);
				   }
	}

	// Returns compile output which could contain errors or general information
	std::string generate_map_script(const Terrain& terrain, const Units& units, const Doodads& doodads, const MapInfo& map_info, const Sounds& sounds, const Regions& regions, const GameCameras& cameras) {
		std::unordered_map<std::string, std::string> unit_variables;		 // creation_number, unit_id
		std::unordered_map<std::string, std::string> destructable_variables; // creation_number, destructable_id
		std::vector<std::string> initialization_triggers;

		std::string trigger_script;
		for (const auto& i : triggers) {
			if (i.is_comment || !i.is_enabled) {
				continue;
			}
			if (!i.custom_text.empty()) {
				trigger_script += i.custom_text + "\n";
			} else {
				trigger_script += convert_gui_to_jass(i, initialization_triggers);
			}
		}

		// Search the trigger script for global unit/destructible definitions
		size_t pos = trigger_script.find("gg_unit", 0);
		while (pos != std::string::npos) {
			std::string type = trigger_script.substr(pos + 8, 4);
			std::string creation_number = trigger_script.substr(pos + 13, 4);
			unit_variables[creation_number] = type;
			pos = trigger_script.find("gg_unit", pos + 17);
		}

		pos = trigger_script.find("gg_dest", 0);
		while (pos != std::string::npos) {
			std::string type = trigger_script.substr(pos + 8, 4);
			std::string creation_number = trigger_script.substr(pos + 13, trigger_script.find_first_not_of("0123456789", pos + 13) - pos - 13);
			destructable_variables[creation_number] = type;
			pos = trigger_script.find("gg_dest", pos + 17);
		}

		MapScriptWriter script_writer;

		generate_global_variables(script_writer, unit_variables, destructable_variables, regions, cameras, sounds);
		generate_init_global_variables(script_writer);
		generate_item_tables(script_writer, "ItemTable_", map_info.random_item_tables);
		generate_item_tables(script_writer, "UnitItemDrops_", units.units);
		generate_item_tables(script_writer, "DoodadItemDrops_", doodads.doodads);
		generate_sounds(script_writer, sounds);

		generate_destructables(script_writer, destructable_variables, terrain, doodads);
		generate_items(script_writer, terrain, units);
		generate_units(script_writer, unit_variables, terrain, units);
		generate_regions(script_writer, regions);
		generate_cameras(script_writer, cameras);

		// Write the results to a buffer
		BinaryWriter writer;
		writer.write_string(global_jass); // ToDo, this isn't written to anything

		script_writer.write(trigger_script);

		generate_trigger_initialization(script_writer, initialization_triggers);
		generate_players(script_writer, map_info);
		generate_custom_teams(script_writer, map_info);
		generate_ally_priorities(script_writer, map_info);
		generate_main(script_writer, terrain, map_info);
		generate_map_configuration(script_writer, terrain, units, map_info);

		fs::path path = QDir::tempPath().toStdString() + "/input.lua";
		std::ofstream output(path, std::ios::binary);
		output.write((char*)script_writer.script.data(), script_writer.script.size());
		output.close();

		hierarchy.map_file_add(path, "war3map.lua");

		/*QProcess* proc = new QProcess();
		proc->setWorkingDirectory("data/tools");
		proc->start("data/tools/clijasshelper.exe", { "--scriptonly", "common.j", "blizzard.j", QString::fromStdString(path.string()), "war3map.j" });
		proc->waitForFinished();
		QString result = proc->readAllStandardOutput();

		if (result.contains("Compile error")) {
			QMessageBox::information(nullptr, "vJass output", "There were compilation errors. See the output tab for more information\n" + result.mid(result.indexOf("Compile error")), QMessageBox::StandardButton::Ok);
			return result.mid(result.indexOf("Compile error"));
		} else if (result.contains("compile errors")) {
			QMessageBox::information(nullptr, "vJass output", "There were compilation errors. See the output tab for more information" + result.mid(result.indexOf("compile errors")), QMessageBox::StandardButton::Ok);
			return result.mid(result.indexOf("compile errors."));
		} else {
			hierarchy.map_file_add("data/tools/war3map.j", "war3map.j");
			return "Compilation successful";
		}*/
		return "Compilation successful";
	}
};