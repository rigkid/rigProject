#pragma once

#include <cstdint>
#include <entt/entt.hpp>
#include <string>
#include <unordered_map>

namespace rigkit {
namespace project {

using EntityIdMap = std::unordered_map<std::uint32_t, entt::entity>;

/**
 * @brief Document id for an entity, as the Contract's `id` string.
 * @details Derived from the handle so a codec can name a parent without
 * consulting the document. Names are not used: they may repeat or be empty,
 * and an id has to be unique. The readable name travels in `rig.meta.named`.
 */
std::string entityIdString(entt::entity entity);

/** @brief Handle back out of an id written by `entityIdString`, or null. */
entt::entity entityIdFromString(const std::string& id);

/// Rewrite entity references (e.g. CRelationship::parent) after load.
void remapEntityReferences(entt::registry& reg, const EntityIdMap& idMap);

} // namespace project
} // namespace rigkit
