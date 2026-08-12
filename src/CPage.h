#pragma once

#include <string>
#include <vector>
#include "ecs/PropertyReflection.h"

namespace rigkit {
namespace ecs {

/**
 * @brief Page record — data only. Speaks `rig.layout.page`.
 * @details Empty `unit` inherits `CProject::defaultUnit`. Margins / bleed /
 * slug are scalar channel packing of the schema's number[4] arrays (CSS order).
 * `name` is editor-only; the document names the entity with `rig.meta.named`.
 *
 * `originAnchor` (0 topLeft, 1 topRight, 2 bottomLeft, 3 bottomRight, 4 center)
 * is which corner of the trim is page-local (0,0). It rides the page struct for
 * host convenience but travels as its own component, `rig.spatial.anchor` —
 * the page schema does not carry an anchor field. Top-left is the Contract
 * default, so that value writes no component at all.
 */
struct CPage {
	std::string name = "Page";
	int index = 0;
	std::string unit; // empty → inherit defaultUnit
	float width = 1920.0f;
	float height = 1080.0f;
	float marginTop = 0.0f;
	float marginRight = 0.0f;
	float marginBottom = 0.0f;
	float marginLeft = 0.0f;
	float bleedTop = 0.0f;
	float bleedRight = 0.0f;
	float bleedBottom = 0.0f;
	float bleedLeft = 0.0f;
	float slugTop = 0.0f;
	float slugRight = 0.0f;
	float slugBottom = 0.0f;
	float slugLeft = 0.0f;
	int originAnchor = 0;

	std::vector<sProp> GetProperties() {
		static const char* const kOrigin[] = {"Top Left", "Top Right", "Bottom Left",
											  "Bottom Right", "Center"};
		return {{0, "Name", EPT_STRING, &name},
				{1, "Index", EPT_INT, &index},
				{2, "Unit", EPT_STRING, &unit},
				{3, "Width", EPT_FLOAT, &width},
				{4, "Height", EPT_FLOAT, &height},
				{5, "Margin Top", EPT_FLOAT, &marginTop},
				{6, "Margin Right", EPT_FLOAT, &marginRight},
				{7, "Margin Bottom", EPT_FLOAT, &marginBottom},
				{8, "Margin Left", EPT_FLOAT, &marginLeft},
				{9, "Origin", EPT_INT, &originAnchor, kOrigin, 5}};
	}
};

} // namespace ecs
} // namespace rigkit
