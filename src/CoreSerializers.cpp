#include "core/TypeJson.h"
#include "AddSerializer.h"
#include "CAnchor.h"
#include "CPage.h"
#include "FaceInsets.h"
#include "ProjectSerializer.h"

namespace rigkit {
namespace project {
namespace {

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
 * @brief `rig.spatial.anchor` from `CAnchor`.
 * @details Top-left / min / zero offset is what an absent component means, so
 * that value writes nothing. `height` and `offset` travel only when they leave
 * those defaults.
 */
bool serializeAnchor(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CAnchor>(e)) {
		return false;
	}
	const auto& a = reg.get<ecs::CAnchor>(e);
	if (a.isDefault()) {
		return false;
	}
	j["point"] = ecs::CAnchor::pointToId(a.point);
	if (a.height != ecs::CAnchor::kHeightMin) {
		j["height"] = ecs::CAnchor::heightToId(a.height);
	}
	if (a.offset.x != 0.f || a.offset.y != 0.f) {
		j["offset"] = vec2ToJson(a.offset);
	}
	return true;
}

bool deserializeAnchor(entt::registry& reg, entt::entity e, const ordered_json& j) {
	if (!j.contains("point") || !j["point"].is_string()) {
		return false;
	}
	ecs::CAnchor a;
	a.point = ecs::CAnchor::pointFromId(j["point"].get<std::string>());
	if (j.contains("height") && j["height"].is_string()) {
		a.height = ecs::CAnchor::heightFromId(j["height"].get<std::string>());
	}
	if (j.contains("offset")) {
		a.offset = vec2FromJson(j["offset"], a.offset);
	}
	reg.emplace_or_replace<ecs::CAnchor>(e, a);
	return true;
}

void registerInto(ComponentSerializerRegistry& registry) {
	addSerializer<ecs::CPage>(registry, "Page", "rig.layout.page", serializePage, deserializePage);
	addSerializer<ecs::CAnchor>(registry, "Anchor", "rig.spatial.anchor", serializeAnchor,
								deserializeAnchor);
}

} // namespace

void registerPageSerializers(ComponentSerializerRegistry& registry) {
	registerInto(registry);
}

} // namespace project
} // namespace rigkit
