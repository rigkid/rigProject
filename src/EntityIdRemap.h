#pragma once

#include <cstdint>
#include <entt/entt.hpp>
#include <unordered_map>

namespace rigkit {
namespace project {

using EntityIdMap = std::unordered_map<std::uint32_t, entt::entity>;

/// Rewrite entity references (e.g. CRelationship::parent) after load.
void remapEntityReferences(entt::registry& reg, const EntityIdMap& idMap);

} // namespace project
} // namespace rigkit
