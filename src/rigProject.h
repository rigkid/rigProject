#pragma once

#include <string>
#include "ComponentSerializerRegistry.h"
#include "ContractImport.h"
#include "ProjectSerializer.h"
#include "core/pack/IPack.h"

namespace rigkit {

/**
 * @brief Document/page data + JSON document load/save systems.
 * @details Depends on rigComponent for registration order.
 * Default file extension is `.rig` (override with setFileExtension, e.g. "rigdoc").
 */
class rigProject : public IPack {
  public:
	rigProject();
	bool init() override;
	void setup() override;

	/**
	 * @brief Preferred document file extension (with or without leading dot).
	 * @param ext e.g. "rig", ".rig", "rigdoc". Empty resets to `.rig`.
	 */
	void setFileExtension(std::string ext);

	/** @brief Normalized extension including leading dot (default `.rig`). */
	const std::string& fileExtension() const { return m_fileExtension; }

	/**
	 * @brief Build a path using the preferred extension when @p stemOrPath has none.
	 * @param stemOrPath Absolute/relative path or bare stem (`show` → `show.rig`).
	 */
	std::string documentPath(const std::string& stemOrPath) const;

	/**
	 * @brief Queue a save on the next Update.
	 * @param path Target path; empty uses `CProject::path` when set. Bare stems
	 * get fileExtension() appended.
	 */
	void requestSave(const std::string& path = {});

	/**
	 * @brief Queue a load on the next Update.
	 * @param path Source path (required). Bare stems get fileExtension() appended.
	 */
	void requestLoad(const std::string& path);

	/** @brief Register an extra component codec (plot packs, etc.). */
	void registerSerializer(project::ComponentSerializer serializer);

	void setRootExtensionWriter(project::ProjectSerializer::RootExtensionWriter writer);
	void setRootExtensionReader(project::ProjectSerializer::RootExtensionReader reader);

	project::ProjectSerializer& serializer() { return m_serializer; }
	const project::ProjectSerializer& serializer() const { return m_serializer; }

  private:
	std::string normalizePath(const std::string& path) const;

	project::ProjectSerializer m_serializer;
	std::string m_fileExtension = ".rig";
	std::string m_pendingSavePath;
	std::string m_pendingLoadPath;
	bool m_saveRequested = false;
	bool m_loadRequested = false;
};

} // namespace rigkit
