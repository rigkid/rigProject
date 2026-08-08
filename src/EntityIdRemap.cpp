#include "EntityIdRemap.h"
#include <cstdlib>
#include "CRelationship.h"

namespace rigkit {
namespace project {

namespace {
constexpr char kEntityIdPrefix = 'e';
}

std::string entityIdString(entt::entity entity) {
	if (entity == entt::null) {
		return {};
	}
	return kEntityIdPrefix + std::to_string(static_cast<std::uint32_t>(entity));
}

entt::entity entityIdFromString(const std::string& id) {
	if (id.size() < 2 || id[0] != kEntityIdPrefix) {
		return entt::null;
	}
	char* end = nullptr;
	const unsigned long value = std::strtoul(id.c_str() + 1, &end, 10);
	if (end == nullptr || *end != '\0') {
		return entt::null;
	}
	return static_cast<entt::entity>(static_cast<std::uint32_t>(value));
}

void remapEntityReferences(entt::registry& reg, const EntityIdMap& idMap) {
	auto view = reg.view<ecs::CRelationship>();
	for (auto entity : view) {
		auto& rel = view.get<ecs::CRelationship>(entity);
		if (rel.parent == entt::null) {
			continue;
		}
		const auto it = idMap.find(static_cast<std::uint32_t>(rel.parent));
		if (it != idMap.end()) {
			rel.parent = it->second;
		} else {
			rel.parent = entt::null;
		}
	}
}

} // namespace project
} // namespace rigkit
