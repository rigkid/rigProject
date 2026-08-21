#pragma once

#include <string>

namespace rigkit {
namespace project {

/**
 * @brief Result of opening a `.rig` or `.rigz` package for import.
 * @details Packet root is the directory that contains the `.rig` and (when
 * present) a sibling `data/` folder. For `.rigz`, that is a temp extract —
 * call @ref releasePackage when done. Sidecar `asset_ref.path` values resolve
 * relative to `data/` — see RigWorks interchange.
 */
struct PackageOpen {
	bool ok = false;
	std::string error;
	std::string displayPath; ///< Path the user picked (`.rig` or `.rigz`).
	std::string rigPath;	 ///< Filesystem path to the JSON `.rig`.
	std::string packetRoot;	 ///< Directory containing the `.rig` (+ `data/`).
	bool temporary = false;	 ///< True when @p packetRoot is an extract to delete.
};

/** @return true when @p path ends with `.rigz` (case-insensitive). */
bool isRigzPath(const std::string& path);

/**
 * @brief Prepare a document path for `importContractFile`.
 * @details `.rig` → packet root = parent directory. `.rigz` → unzip to a temp
 * dir (one root `.rig` + optional `data/`). Other extensions fail.
 */
PackageOpen openPackage(const std::string& path);

/** @brief Delete a temporary extract from @ref openPackage (no-op if not temp). */
void releasePackage(PackageOpen& opened);

/**
 * @brief Resolve a sidecar or host-root `asset_ref.path` against a packet root.
 * @details Tries `packetRoot/data/<path>` first (Contract), then
 * `packetRoot/<path>` (loose / legacy `data/...` prefixes). Absolute paths
 * pass through unchanged.
 */
std::string resolvePacketAssetPath(const std::string& packetRoot, const std::string& assetPath);

} // namespace project
} // namespace rigkit
