#pragma once

#include <entt/entt.hpp>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "EntityIdRemap.h"
#include "ProjectJson.h"

namespace rigkit {
namespace project {

struct ComponentSerializer {
	/// Registry-local name, also what the inspector shows.
	std::string name;
	/**
	 * @brief Wire key: a Contract schema id, or an x\.\<vendor\>\.\<name\> extension.
	 * @details Required - registration refuses a codec without one, because a
	 * component with no id cannot be written into a document. Use a @c rig\.* id
	 * only where the Contract defines that schema; anything host-specific takes
	 * x\.rigkit\.\<name\>. Claiming a @c rig\.* id the Contract does not have makes
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
	/**
	 * @brief Optional Contract-import rewrite of entity references stored as names.
	 * @details The map is document entity id to live entity name. A codec replaces
	 * a stored string only when it is a key in the map; values that already hold
	 * entity names pass through. Used for name-reference PODs (mod bindings, CAD
	 * operands) whose wire form names document entities, not live handles.
	 */
	std::function<void(entt::registry&, const std::unordered_map<std::string, std::string>&)>
		remapContractReferences;
	/**
	 * @brief Optional Contract-import resolution of entity references stored as handles.
	 * @details The map is document entity id to live entity. A codec whose
	 * deserializer stashed document-id references (because the target entity did
	 * not exist yet) resolves them here, once every entity exists. Used for
	 * handle-reference PODs (layout masters, story flows).
	 */
	std::function<void(entt::registry&, const std::unordered_map<std::string, entt::entity>&)>
		resolveEntityReferences;
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

	/**
	 * @brief Run every codec's Contract reference fixup once all entities exist.
	 * @param nameByDocumentId Document entity id to live entity name.
	 * @return How many codecs ran a fixup.
	 */
	int applyContractRemaps(entt::registry& reg,
							const std::unordered_map<std::string, std::string>& nameByDocumentId) const;

	/**
	 * @brief Run every codec's entity-reference resolution once all entities exist.
	 * @param entityByDocumentId Document entity id to live entity.
	 * @return How many codecs ran a resolution.
	 */
	int applyContractEntityResolutions(
		entt::registry& reg,
		const std::unordered_map<std::string, entt::entity>& entityByDocumentId) const;

  private:
	std::vector<ComponentSerializer> m_serializers;
};

} // namespace project
} // namespace rigkit
