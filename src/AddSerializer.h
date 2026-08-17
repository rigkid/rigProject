#pragma once

#include <functional>
#include <string>
#include "ComponentSerializerRegistry.h"

namespace rigkit {
namespace project {

/**
 * @brief Register a typed component codec on a document registry.
 * @details Packs that own portable PODs call this from setup() (via
 * `rigProject::registerSerializer` / the registry). One schema id per call —
 * the same C++ type may register twice when the Contract splits one POD across
 * two keys (e.g. page + anchor).
 */
template <typename T>
void addSerializer(
	ComponentSerializerRegistry& registry, const char* key, const char* schemaId,
	std::function<bool(entt::registry&, entt::entity, ordered_json&)> serializeFn,
	std::function<bool(entt::registry&, entt::entity, const ordered_json&)> deserializeFn,
	std::function<void(entt::registry&, const EntityIdMap&)> remapReferences = {}) {
	ComponentSerializer ser;
	ser.name = key;
	ser.schemaId = schemaId;
	ser.hasComponent = [](entt::registry& reg, entt::entity e) { return reg.all_of<T>(e); };
	ser.forEachEntity = [](entt::registry& reg, std::function<void(entt::entity)> fn) {
		for (auto entity : reg.view<T>()) {
			fn(entity);
		}
	};
	ser.serialize = std::move(serializeFn);
	ser.deserialize = std::move(deserializeFn);
	ser.remapReferences = std::move(remapReferences);
	registry.registerSerializer(std::move(ser));
}

/**
 * @brief Single bool field under a Contract / extension key (e.g. selectable.enabled).
 */
template <typename T, bool T::*Member>
void addBoolMemberSerializer(ComponentSerializerRegistry& registry, const char* key,
							 const char* schemaId, const char* fieldName) {
	const std::string field = fieldName;
	addSerializer<T>(
		registry, key, schemaId,
		[field](entt::registry& reg, entt::entity e, ordered_json& j) -> bool {
			if (!reg.all_of<T>(e)) {
				return false;
			}
			j[field] = reg.get<T>(e).*Member;
			return true;
		},
		[field](entt::registry& reg, entt::entity e, const ordered_json& j) -> bool {
			T c{};
			if (reg.all_of<T>(e)) {
				c = reg.get<T>(e);
			}
			c.*Member = j.value(field, c.*Member);
			reg.emplace_or_replace<T>(e, c);
			return true;
		});
}

/**
 * @brief Empty marker component — wire value is `{}`.
 */
template <typename T>
void addMarkerSerializer(ComponentSerializerRegistry& registry, const char* key,
						 const char* schemaId) {
	addSerializer<T>(
		registry, key, schemaId,
		[](entt::registry& reg, entt::entity e, ordered_json& j) -> bool {
			if (!reg.all_of<T>(e)) {
				return false;
			}
			(void)j;
			return true;
		},
		[](entt::registry& reg, entt::entity e, const ordered_json&) -> bool {
			reg.emplace_or_replace<T>(e, T{});
			return true;
		});
}

} // namespace project
} // namespace rigkit
