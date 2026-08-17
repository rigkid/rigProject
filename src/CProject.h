#pragma once

#include <string>
#include <vector>
#include "ecs/PropertyReflection.h"

namespace rigkit {
namespace ecs {

/**
 * @brief Project envelope — data only.
 * @details Singleton metadata entity. Serialized into the root `project`
 * object (and Contract `document` keys), not as a normal entity component blob.
 * `colorSpace` speaks `document.colorSpace` (default `srgb`).
 * `pdfX` / `outputCondition` / `trapped` / `outputProfile` drive PDF/X emit.
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
	std::string colorSpace = "srgb"; ///< Speaks document.colorSpace; rgba/rgb meaning
	std::string pdfX; ///< e.g. PDF/X-4; empty = not PDF/X
	std::string outputCondition; ///< OutputConditionIdentifier (FOGRA39, …)
	std::string outputRegistry = "http://www.color.org";
	std::string outputProfile; ///< Host path to ICC for DestOutputProfile
	std::string trapped = "unknown"; ///< unknown | true | false
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
				{9, "Color Space", EPT_STRING, &colorSpace},
				{10, "PDF/X", EPT_STRING, &pdfX},
				{11, "Output Condition", EPT_STRING, &outputCondition},
				{12, "Output Registry", EPT_STRING, &outputRegistry},
				{13, "Output Profile", EPT_STRING, &outputProfile},
				{14, "Trapped", EPT_STRING, &trapped},
				{15, "Dirty", EPT_BOOL, &dirty}};
	}
};

} // namespace ecs
} // namespace rigkit
