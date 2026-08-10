#pragma once

#include <fstream>
#include <spdlog/spdlog.h>
#include <string>
#include "core/json.h"

namespace rigkit {
namespace project {

/// Preserves key order in `.rigdoc` files.
using ordered_json = nlohmann::ordered_json;

/**
 * @brief Contract version stamped into the `rig` field of every document we write.
 * @details Tracks the vendored RigWorks in `docs/contract/RigWorks/VERSION`. Bump
 * both together, and only after the writer actually emits what that version says.
 */
inline constexpr const char* kContractVersion = "0.10.0";

inline ordered_json loadOrderedJson(const std::string& path) {
	ordered_json json;
	std::ifstream in(path);
	if (!in) {
		spdlog::error("[rigProject] missing document '{}'", path);
		return json;
	}
	try {
		in >> json;
	} catch (const std::exception& e) {
		spdlog::error("[rigProject] failed to parse '{}': {}", path, e.what());
		json = ordered_json::object();
	} catch (...) {
		spdlog::error("[rigProject] failed to parse '{}'", path);
		json = ordered_json::object();
	}
	return json;
}

inline bool savePrettyOrderedJson(const std::string& path, const ordered_json& json) {
	std::ofstream out(path);
	if (!out) {
		spdlog::error("[rigProject] failed to open '{}' for write", path);
		return false;
	}
	try {
		out << json.dump(4);
	} catch (const std::exception& e) {
		spdlog::error("[rigProject] failed to save '{}': {}", path, e.what());
		return false;
	} catch (...) {
		spdlog::error("[rigProject] failed to save '{}'", path);
		return false;
	}
	return true;
}

} // namespace project
} // namespace rigkit
