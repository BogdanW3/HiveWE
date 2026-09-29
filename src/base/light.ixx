export module Light;

import std;
import BinaryReader;
import BinaryWriter;
import <glm/glm.hpp>;

/// A light attached to a placed doodad or unit (war3map.doo/war3mapUnits.doo version >= 13)
export struct Light {
	uint32_t index = 0;
	bool shadow_casting = false;
	glm::u8vec4 color = glm::u8vec4(255);
	float intensity = 1.f;
	float shadow_casting_start = 0.f;
	float shadow_casting_end = 0.f;
	float quadratic_falloff = 0.f;
	float linear_falloff = 0.f;
	float damping = 0.f;

	static Light read(BinaryReader& reader) {
		Light light;
		light.index = reader.read<uint32_t>();
		light.shadow_casting = reader.read<uint32_t>();
		light.color = reader.read<glm::u8vec4>();
		light.intensity = reader.read<float>();
		light.shadow_casting_start = reader.read<float>();
		light.shadow_casting_end = reader.read<float>();
		light.quadratic_falloff = reader.read<float>();
		light.linear_falloff = reader.read<float>();
		light.damping = reader.read<float>();
		return light;
	}

	void write(BinaryWriter& writer) const {
		writer.write<uint32_t>(index);
		writer.write<uint32_t>(shadow_casting);
		writer.write<glm::u8vec4>(color);
		writer.write<float>(intensity);
		writer.write<float>(shadow_casting_start);
		writer.write<float>(shadow_casting_end);
		writer.write<float>(quadratic_falloff);
		writer.write<float>(linear_falloff);
		writer.write<float>(damping);
	}
};
