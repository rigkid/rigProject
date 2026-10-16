#pragma once

#include <string>
#include <vector>
#include "ecs/PropertyReflection.h"

namespace rigkit {
namespace ecs {

/**
 * @brief Page record - data only. Speaks `rig.layout.page`.
 * @details Empty `unit` inherits `CProject::defaultUnit`. Margins / bleed /
 * slug are face insets on the local AABB (CSS XY order + optional Z min/max),
 * same cuboid as `rig.spatial.anchor`. Margins inset inward; bleed/slug
 * outward. `name` is editor-only; the document names the entity with
 * `rig.meta.named`.
 *
 * Local origin is sibling `CAnchor` (`rig.spatial.anchor`) - the page schema
 * does not carry an anchor field. Absent anchor = top-left / min Z.
 */
struct CPage {
	std::string name = "Page";
	int index = 0;
	std::string unit; // empty to inherit defaultUnit
	float width = 1920.0f;
	float height = 1080.0f;
	float marginTop = 0.0f;
	float marginRight = 0.0f;
	float marginBottom = 0.0f;
	float marginLeft = 0.0f;
	float marginFloor = 0.0f;	 ///< min-Z face
	float marginCeiling = 0.0f; ///< max-Z face
	float bleedTop = 0.0f;
	float bleedRight = 0.0f;
	float bleedBottom = 0.0f;
	float bleedLeft = 0.0f;
	float bleedFloor = 0.0f;
	float bleedCeiling = 0.0f;
	float slugTop = 0.0f;
	float slugRight = 0.0f;
	float slugBottom = 0.0f;
	float slugLeft = 0.0f;
	float slugFloor = 0.0f;
	float slugCeiling = 0.0f;

	std::vector<sProp> GetProperties() {
		return {{0, "Name", EPT_STRING, &name},
				{1, "Index", EPT_INT, &index},
				{2, "Unit", EPT_STRING, &unit},
				{3, "Width", EPT_FLOAT, &width},
				{4, "Height", EPT_FLOAT, &height},
				{5, "Margin Top", EPT_FLOAT, &marginTop},
				{6, "Margin Right", EPT_FLOAT, &marginRight},
				{7, "Margin Bottom", EPT_FLOAT, &marginBottom},
				{8, "Margin Left", EPT_FLOAT, &marginLeft},
				{9, "Margin Floor", EPT_FLOAT, &marginFloor},
				{10, "Margin Ceiling", EPT_FLOAT, &marginCeiling}};
	}
};

} // namespace ecs
} // namespace rigkit
