#include "Texture.h"

#include <SOIL2/SOIL2.h>

#include "BinaryReader.h"
#include "Hierarchy.h"
#include "BLP.h"

Texture::Texture(const fs::path& path) {
	fs::path new_path = path;

	if (!hierarchy.file_exists(new_path)) {
		new_path = path;
		new_path.replace_extension(".dds");
		if (!hierarchy.file_exists(new_path)) {
			new_path.replace_extension(".blp");
			if (!hierarchy.file_exists(new_path)) {
				new_path.replace_extension(".dds");
			}
		}
	}

	BinaryReader reader = hierarchy.open_file(new_path);
	uint8_t* image_data;

	if (new_path.extension() == ".blp" || new_path.extension() == ".BLP") {
		image_data = blp::load(reader, width, height, channels);
	} else {
		image_data = SOIL_load_image_from_memory(reader.buffer.data(), static_cast<int>(reader.buffer.size()), &width, &height, &channels, SOIL_LOAD_AUTO);
	}
	data = std::vector<uint8_t>(image_data, image_data + width * height * channels);
	delete image_data;
}