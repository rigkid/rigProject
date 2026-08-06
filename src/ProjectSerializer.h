#pragma once

#include <functional>
#include <string>
#include "ComponentSerializerRegistry.h"
#include "ProjectJson.h"

namespace rigkit {

class MEcs;

namespace project {

/**
 * @brief Save/load ECS document content as JSON (`.rig` / `.rigdoc` / custom).
 * @details Root shape: `{ "Project": {...}, "entities": [...] }` plus optional
 * root extensions for packs (e.g. plotter). Not wire-compatible with OF `.ofdoc`.
 * Preferred extension is owned by `rigProject::setFileExtension`.
 */
class ProjectSerializer {
  public:
	using RootExtensionWriter = std::function<void(MEcs& ecs, ordered_json& root)>;
	using RootExtensionReader = std::function<void(MEcs& ecs, const ordered_json& root)>;

	ProjectSerializer();

	ComponentSerializerRegistry& registry() { return m_registry; }
	const ComponentSerializerRegistry& registry() const { return m_registry; }

	void setRootExtensionWriter(RootExtensionWriter writer) {
		m_writeRootExtension = std::move(writer);
	}
	void setRootExtensionReader(RootExtensionReader reader) {
		m_readRootExtension = std::move(reader);
	}

	bool save(MEcs& ecs, const std::string& path) const;
	bool load(MEcs& ecs, const std::string& path);

  private:
	static bool shouldSkipEntity(entt::registry& reg, entt::entity e);
	static bool isProjectMetadataEntity(entt::registry& reg, entt::entity e);

	ComponentSerializerRegistry m_registry;
	RootExtensionWriter m_writeRootExtension;
	RootExtensionReader m_readRootExtension;
};

void registerCoreSerializers(ComponentSerializerRegistry& registry);

} // namespace project
} // namespace rigkit
