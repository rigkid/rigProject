#include "ComponentSerializerRegistry.h"
#include <cstdint>
#include <unordered_set>
#include <spdlog/spdlog.h>

namespace rigkit {
namespace project {

void ComponentSerializerRegistry::registerSerializer(ComponentSerializer serializer) {
	for (const auto& existing : m_serializers) {
		if (existing.name == serializer.name) {
			return;
		}
	}
	// Refuse rather than invent one: a guessed id would write documents that
	// look valid and mean nothing to any other host.
	if (serializer.schemaId.empty()) {
		spdlog::error("[rigProject] codec '{}' has no schemaId — not registered", serializer.name);
		return;
	}
	for (const auto& existing : m_serializers) {
		if (existing.schemaId == serializer.schemaId) {
			spdlog::error("[rigProject] codec '{}' reuses schemaId '{}' from '{}' — not registered",
						  serializer.name, serializer.schemaId, existing.name);
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

int ComponentSerializerRegistry::applyDeserializers(
	entt::registry& reg, entt::entity entity, const ordered_json& components,
	const std::vector<std::string>& skipSchemaIds) const {
	std::unordered_set<std::string> skip(skipSchemaIds.begin(), skipSchemaIds.end());
	int applied = 0;
	for (const auto& ser : m_serializers) {
		if (skip.count(ser.schemaId) || !components.contains(ser.schemaId) || !ser.deserialize) {
			continue;
		}
		if (ser.deserialize(reg, entity, components[ser.schemaId])) {
			++applied;
		}
	}
	return applied;
}

} // namespace project
} // namespace rigkit
