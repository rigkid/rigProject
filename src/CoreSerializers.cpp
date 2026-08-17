#include <string>

#include "AddSerializer.h"
#include "CPage.h"
#include "ProjectSerializer.h"

namespace rigkit {
namespace project {
namespace {

/// `rig.spatial.anchor` ids for `CPage::originAnchor` — full 3×3 face.
constexpr const char* kOriginAnchorIds[] = {
	"topLeft",	   "topCenter",	  "topRight",	 "middleLeft",	 "center",
	"middleRight", "bottomLeft", "bottomCenter", "bottomRight"};
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

bool edgesAllZero(float a, float b, float c, float d) {
	return a == 0.f && b == 0.f && c == 0.f && d == 0.f;
}

void writeEdges(ordered_json& j, const char* key, float top, float right, float bottom, float left) {
	if (edgesAllZero(top, right, bottom, left)) {
		return;
	}
	j[key] = ordered_json::array({top, right, bottom, left});
}

void readEdges(const ordered_json& j, const char* key, float& top, float& right, float& bottom,
			   float& left) {
	if (!j.contains(key) || !j[key].is_array() || j[key].size() < 4) {
		return;
	}
	top = j[key][0].get<float>();
	right = j[key][1].get<float>();
	bottom = j[key][2].get<float>();
	left = j[key][3].get<float>();
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
	writeEdges(j, "margins", p.marginTop, p.marginRight, p.marginBottom, p.marginLeft);
	writeEdges(j, "bleed", p.bleedTop, p.bleedRight, p.bleedBottom, p.bleedLeft);
	writeEdges(j, "slug", p.slugTop, p.slugRight, p.slugBottom, p.slugLeft);
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
	readEdges(j, "margins", p.marginTop, p.marginRight, p.marginBottom, p.marginLeft);
	readEdges(j, "bleed", p.bleedTop, p.bleedRight, p.bleedBottom, p.bleedLeft);
	readEdges(j, "slug", p.slugTop, p.slugRight, p.slugBottom, p.slugLeft);
	// Documents written before the anchor moved to its own component still name
	// it here; the page schema has no such field, so this only reads.
	if (j.contains("originAnchor") && j["originAnchor"].is_string()) {
		p.originAnchor = originAnchorFromId(j["originAnchor"].get<std::string>());
	}
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
