#pragma once

#include <entt/entt.hpp>
#include <functional>
#include <string>
#include <vector>
#include "ProjectJson.h"

namespace rigkit {
namespace project {

struct ComponentSerializer {
	std::string name;
	std::function<bool(entt::registry&, entt::entity, ordered_json&)> serialize;
	std::function<bool(entt::registry&, entt::entity, const ordered_json&)> deserialize;
	std::function<bool(entt::registry&, entt::entity)> hasComponent;
	std::function<void(entt::registry&, std::function<void(entt::entity)>)> forEachEntity;
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

  private:
	std::vector<ComponentSerializer> m_serializers;
};

} // namespace project
} // namespace rigkit
