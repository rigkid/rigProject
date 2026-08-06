#include "EntityIdRemap.h"
#include "CRelationship.h"

namespace rigkit {
namespace project {

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
