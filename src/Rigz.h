#pragma once

/**
 * @file
 * @brief Open a `.rig` or `.rigz` package and resolve the asset paths inside it.
 * @details A `.rigz` unzips to a temp directory, so pair every `openPackage` with
 * `releasePackage`.
 */

#include <string>
#include <vector>

namespace rigkit {
class MEcs;

namespace project {

/**
 * @brief Result of opening a `.rig` or `.rigz` package for import.
 * @details Packet root is the directory that contains the `.rig` and (when
 * present) a sibling `data/` folder. For `.rigz`, that is a temp extract  -
 * call @ref releasePackage when done. Sidecar `asset_ref.path` values resolve
 * relative to `data/` - see RigWorks interchange.
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
 * @details `.rig` to packet root = parent directory. `.rigz` to unzip to a temp
 * dir (one root `.rig` + optional `data/`). Other extensions fail.
 */
PackageOpen openPackage(const std::string& path);

/** @brief Delete a temporary extract from @ref openPackage (no-op if not temp). */
void releasePackage(PackageOpen& opened);

/**
 * @brief Write a `.rigz` with exactly one `.rig` at the archive root.
 * @details Optional sidecars under @p packetRoot (`data/`, `assets/`) are stored
 * with those prefixes. `data/` may contain further `.rig` files. @p rigPath is
 * the JSON document embedded at the archive root.
 */
bool writeRigz(const std::string& destPath, const std::string& rigPath,
			   const std::string& packetRoot, std::string* error = nullptr);

/**
 * @brief Copy linked media and fonts into the package and point each ref at the copy.
 * @details Walks every `CAssetRef` of kind Image, Audio, Video, Model, or Font. Media
 * lands in `data/links/`; fonts land in `data/fonts/`. Paths become `links/<name>` or
 * `fonts/<name>` (relative to `data/`). Files already under `data/` or `assets/` are
 * rewritten relative without a copy. Same source is copied once; name clashes get
 * `-N`. Missing files are left untouched. Refs rewrite only after every copy succeeds.
 * Documents and presets are not collected. Call before @ref rigkit::project::writeRigz / `.rig` save.
 * @return false (with @p error set) when @p packetRoot is empty or a copy fails.
 */
bool collectLinkedAssets(MEcs& ecs, const std::string& packetRoot, std::string* error = nullptr);

/**
 * @brief Resolve a sidecar or host-root `asset_ref.path` against search roots.
 * @details Absolute paths and paths that already exist pass through. Otherwise
 * tries each root as `root/data/<path>` then `root/<path>` (Contract package
 * layout, then loose / legacy). Empty roots are skipped.
 */
std::string resolvePacketAssetPath(const std::string& packetRoot, const std::string& assetPath);

/**
 * @brief Same as @ref resolvePacketAssetPath with an ordered list of roots
 * (packet extract dir, document directory, …).
 */
std::string resolvePacketAssetPath(const std::vector<std::string>& roots,
								   const std::string& assetPath);

} // namespace project
} // namespace rigkit
