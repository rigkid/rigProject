#pragma once

#include <string>
#include <vector>
#include "ecs/PropertyReflection.h"

namespace rigkit {
namespace ecs {

/**
 * @brief Project envelope — data only.
 * @details Singleton metadata entity. Serialized into the root `project`
 * object, not as a normal entity component blob.
 */
struct CProject {
	std::string title = "Untitled";
	std::string path;
	std::string author;
	std::string createdAt;
	std::string modifiedAt;
	int formatMajor = 1;
	int formatMinor = 0;
	int activePageIndex = 0;
	std::string defaultUnit = "px"; // px | mm | in
	bool dirty = false;

	std::vector<sProp> GetProperties() {
		return {{0, "Title", EPT_STRING, &title},
				{1, "Path", EPT_STRING, &path},
				{2, "Author", EPT_STRING, &author},
				{3, "Created", EPT_STRING, &createdAt},
				{4, "Modified", EPT_STRING, &modifiedAt},
				{5, "Format Major", EPT_INT, &formatMajor},
				{6, "Format Minor", EPT_INT, &formatMinor},
				{7, "Active Page", EPT_INT, &activePageIndex},
				{8, "Default Unit", EPT_STRING, &defaultUnit},
				{9, "Dirty", EPT_BOOL, &dirty}};
	}
};

} // namespace ecs
} // namespace rigkit
