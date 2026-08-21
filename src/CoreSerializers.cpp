#include <string>

#include "AddSerializer.h"
#include "CPage.h"
#include "FaceInsets.h"
#include "ProjectSerializer.h"

namespace rigkit {
namespace project {
namespace {

/// `rig.spatial.anchor` ids for `CPage::originAnchor` — full 3×3 face.
constexpr const char* kOriginAnchorIds[] = {"top-left",	   "top-center",	"top-right",
											"middle-left", "center",		"middle-right",
											"bottom-left", "bottom-center", "bottom-right"};
constexpr int kOriginAnchorIdCount = 9;

const char* originAnchorId(int v) {
	if (v >= 0 && v < kOriginAnchorIdCount) {
		return kOriginAnchorIds[v];
	}
	return kOriginAnchorIds[0];
}

/** @brief Read a `rig.spatial.anchor` cell id into the 9-cell page index. */
int originAnchorFromId(const std::string& id) {
	for (int i = 0; i < kOriginAnchorIdCount; ++i) {
		if (id == kOriginAnchorIds[i]) {
			return i;
		}
	}
	return 0;
}

void writePageInsets(ordered_json& j, const char* key, float top, float right, float bottom,
					 float left, float zFloor, float zCeiling) {
	FaceInsets in;
	in.top = top;
	in.right = right;
	in.bottom = bottom;
	in.left = left;
	in.floor = zFloor;
	in.ceiling = zCeiling;
	writeFaceInsets(j, key, in);
}

void readPageInsets(const ordered_json& j, const char* key, float& top, float& right, float& bottom,
					float& left, float& zFloor, float& zCeiling) {
	if (!j.contains(key)) {
		return;
	}
	FaceInsets in;
	if (!expandFaceInsets(j[key], in)) {
		return;
	}
	top = in.top;
	right = in.right;
	bottom = in.bottom;
	left = in.left;
	zFloor = in.floor;
	zCeiling = in.ceiling;
}

bool serializePage(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CPage>(e)) {
		return false;
	}
	const auto& p = reg.get<ecs::CPage>(e);
	j["width"] = p.width;
	j["height"] = p.height;
	if (p.index != 0) {
		j["index"] = p.index;
	}
	if (!p.unit.empty()) {
		j["unit"] = p.unit;
	}
	writePageInsets(j, "margins", p.marginTop, p.marginRight, p.marginBottom, p.marginLeft,
					p.marginFloor, p.marginCeiling);
	writePageInsets(j, "bleed", p.bleedTop, p.bleedRight, p.bleedBottom, p.bleedLeft, p.bleedFloor,
					p.bleedCeiling);
	writePageInsets(j, "slug", p.slugTop, p.slugRight, p.slugBottom, p.slugLeft, p.slugFloor,
					p.slugCeiling);
	return true;
}

bool deserializePage(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CPage p;
	// The anchor is a sibling component and may already have been read; the two
	// codecs write the same struct, so neither may clear the other's field.
	if (reg.all_of<ecs::CPage>(e)) {
		p.originAnchor = reg.get<ecs::CPage>(e).originAnchor;
	}
	p.index = j.value("index", p.index);
	p.unit = j.value("unit", p.unit);
	p.width = j.value("width", p.width);
	p.height = j.value("height", p.height);
	readPageInsets(j, "margins", p.marginTop, p.marginRight, p.marginBottom, p.marginLeft,
				   p.marginFloor, p.marginCeiling);
	readPageInsets(j, "bleed", p.bleedTop, p.bleedRight, p.bleedBottom, p.bleedLeft, p.bleedFloor,
				   p.bleedCeiling);
	readPageInsets(j, "slug", p.slugTop, p.slugRight, p.slugBottom, p.slugLeft, p.slugFloor,
				   p.slugCeiling);
	reg.emplace_or_replace<ecs::CPage>(e, p);
	return true;
}

/**
 * @brief Page anchor as `rig.spatial.anchor`, the component that owns it.
 * @details Written from the page struct rather than a second POD so the host
 * keeps one anchor field. Top-left is what an absent component means, so it
 * writes nothing — a page that never moved its origin stays clean on the wire.
 */
bool serializePageAnchor(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CPage>(e)) {
		return false;
	}
	const auto& p = reg.get<ecs::CPage>(e);
	if (p.originAnchor == 0) {
		return false;
	}
	j["point"] = originAnchorId(p.originAnchor);
	return true;
}

bool deserializePageAnchor(entt::registry& reg, entt::entity e, const ordered_json& j) {
	if (!j.contains("point") || !j["point"].is_string()) {
		return false;
	}
	ecs::CPage p;
	if (reg.all_of<ecs::CPage>(e)) {
		p = reg.get<ecs::CPage>(e);
	}
	p.originAnchor = originAnchorFromId(j["point"].get<std::string>());
	reg.emplace_or_replace<ecs::CPage>(e, p);
	return true;
}

void registerInto(ComponentSerializerRegistry& registry) {
	addSerializer<ecs::CPage>(registry, "Page", "rig.layout.page", serializePage, deserializePage);
	// Same struct, second Contract component: the anchor is not a page field.
	addSerializer<ecs::CPage>(registry, "PageAnchor", "rig.spatial.anchor", serializePageAnchor,
							  deserializePageAnchor);
}

} // namespace

void registerPageSerializers(ComponentSerializerRegistry& registry) {
	registerInto(registry);
}

} // namespace project
} // namespace rigkit
