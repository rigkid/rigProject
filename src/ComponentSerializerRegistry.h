#pragma once

#include <entt/entt.hpp>
#include <functional>
#include <string>
#include <vector>
#include "EntityIdRemap.h"
#include "ProjectJson.h"

namespace rigkit {
namespace project {

struct ComponentSerializer {
	/// Registry-local name, also what the inspector shows.
	std::string name;
	/**
	 * @brief Wire key: a Contract schema id, or an `x.<vendor>.<name>` extension.
	 * @details Required — registration refuses a codec without one, because a
	 * component with no id cannot be written into a document. Use a `rig.*` id
	 * only where the Contract defines that schema; anything host-specific takes
	 * `x.rigkit.<name>`. Claiming a `rig.*` id the Contract does not have makes
	 * a document that validates nowhere.
	 */
	std::string schemaId;
	std::function<bool(entt::registry&, entt::entity, ordered_json&)> serialize;
	std::function<bool(entt::registry&, entt::entity, const ordered_json&)> deserialize;
	std::function<bool(entt::registry&, entt::entity)> hasComponent;
	std::function<void(entt::registry&, std::function<void(entt::entity)>)> forEachEntity;
	/**
	 * @brief Optional post-load rewrite of entity ids stored inside this POD.
	 * @details Called once per codec after the core `CRelationship` remap. Packs
	 * use this for domain parent/mask handles (e.g. pixel layers).
	 */
	std::function<void(entt::registry&, const EntityIdMap&)> remapReferences;
};

/**
 * @brief Named component codecs for `.rigdoc` entity dumps.
 * @details Packs register serializers here; ProjectSerializer walks them on
 * save/load. Plot packs add codecs without changing the host.
 */
class ComponentSerializerRegistry {
  public:
	void registerSerializer(ComponentSerializer serializer);
	const std::vector<ComponentSerializer>& serializers() const { return m_serializers; }

	/// Union of entities that carry any registered serializable component.
	std::vector<entt::entity> collectEntities(entt::registry& reg) const;

	/**
	 * @brief Run every registered deserialize whose schemaId is a key in @p components.
	 * @details Used by Contract import and any other path that already has a
	 * components object. Returns how many codecs applied. Skips schema ids in
	 * @p skipSchemaIds (Contract-specific handling, e.g. relationship by doc id).
	 */
	int applyDeserializers(entt::registry& reg, entt::entity entity, const ordered_json& components,
						   const std::vector<std::string>& skipSchemaIds = {}) const;

  private:
	std::vector<ComponentSerializer> m_serializers;
};

} // namespace project
} // namespace rigkit
