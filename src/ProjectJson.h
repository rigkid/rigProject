#pragma once

/**
 * @file
 * @brief Ordered JSON read and write for Rig documents, plus the Contract version stamp.
 * @details Both helpers log and return empty on failure rather than throwing.
 */

#include <filesystem>
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
inline constexpr const char* kContractVersion = "0.26.0";

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
	// Write beside the target and rename over it only after the write finished.
	// Opening the target directly truncates it first, so a crash or full disk
	// mid-write destroys the previous good document.
	const std::string tmp = path + ".tmp";
	std::error_code ec;
	{
		std::ofstream out(tmp, std::ios::binary);
		if (!out) {
			spdlog::error("[rigProject] failed to open '{}' for write", tmp);
			return false;
		}
		try {
			out << json.dump(4);
		} catch (const std::exception& e) {
			spdlog::error("[rigProject] failed to save '{}': {}", path, e.what());
			std::filesystem::remove(tmp, ec);
			return false;
		} catch (...) {
			spdlog::error("[rigProject] failed to save '{}'", path);
			std::filesystem::remove(tmp, ec);
			return false;
		}
		out.flush();
		if (!out.good()) {
			spdlog::error("[rigProject] short write on '{}'", tmp);
			out.close();
			std::filesystem::remove(tmp, ec);
			return false;
		}
	}
	std::filesystem::rename(tmp, path, ec);
	if (ec) {
		spdlog::error("[rigProject] failed to replace '{}': {}", path, ec.message());
		std::filesystem::remove(tmp, ec);
		return false;
	}
	return true;
}

} // namespace project
} // namespace rigkit
