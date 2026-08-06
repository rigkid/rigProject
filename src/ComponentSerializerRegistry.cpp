#include "ComponentSerializerRegistry.h"
#include <cstdint>
#include <unordered_set>

namespace rigkit {
namespace project {

void ComponentSerializerRegistry::registerSerializer(ComponentSerializer serializer) {
	for (const auto& existing : m_serializers) {
		if (existing.name == serializer.name) {
			return;
		}
	}
	m_serializers.push_back(std::move(serializer));
}

std::vector<entt::entity> ComponentSerializerRegistry::collectEntities(entt::registry& reg) const {
	std::unordered_set<uint32_t> seen;
	std::vector<entt::entity> out;
	for (const auto& ser : m_serializers) {
		if (!ser.forEachEntity) {
			continue;
		}
		ser.forEachEntity(reg, [&](entt::entity e) {
			const uint32_t id = static_cast<uint32_t>(e);
			if (seen.insert(id).second) {
				out.push_back(e);
			}
		});
	}
	return out;
}

} // namespace project
} // namespace rigkit
