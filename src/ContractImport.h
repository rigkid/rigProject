#pragma once

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt/entt.hpp>

#include "core/U_core.h"
#include "ComponentSerializerRegistry.h"

namespace rigkit {
namespace project {

/**
 * @brief Contract JSON (`rig.*` keys) → host POD import report + UI layout rows.
 * @details Clears the ECS and writes `rigComponent` PODs including `CModLfo` /
 * `CModBinding`. Property values live on entities (GetProperties); UI panel /
 * control / action rows here are layout only — never a second property store.
 * Modulators advance via `SModulators` (Update), not a free-function tick.
 */
struct ContractImportResult {
	bool ok = false;
	std::string title;
	std::string error;
	int entityCount = 0;
	int geometryCount = 0;
	std::vector<std::string> skipped;
	std::vector<std::string> notes;

	struct Panel {
		std::string id;
		std::string name;
		std::string role;
		int order = 0;
		bool visible = true;
		float preferredWidth = 320.f;
		float preferredHeight = 240.f;
	};
	struct Group {
		std::string id;
		std::string name;
		std::string panel;
		std::string parent;
		int order = 0;
		std::string orientation = "vertical";
		bool collapsed = false;
	};
	struct Control {
		std::string id;
		std::string name;
		std::string panel;
		std::string group;
		int order = 0;
		std::string target;
		std::string propertyKey;
		std::string type = "float";
		std::optional<float> min;
		std::optional<float> max;
		std::optional<float> step;
		bool enabled = true;
		bool readOnly = false;
		std::vector<std::string> options;
		std::string widget = "auto";
	};
	struct Action {
		std::string id;
		std::string name;
		std::string panel;
		std::string group;
		int order = 0;
		std::string actionId;
		bool enabled = true;
	};

	std::vector<Panel> panels;
	std::vector<Group> groups;
	std::vector<Control> controls;
	std::vector<Action> actions;
	/// Contract entity id → live entt handle (for UI target lookup).
	std::unordered_map<std::string, entt::entity> entities;
};

/** @brief Read a float from the live entity via GetProperties (not a sidecar). */
std::optional<float> contractGetFloat(MEcs& ecs, const ContractImportResult& doc,
									  const std::string& entityId, const std::string& propertyKey);
std::optional<std::string> contractGetString(MEcs& ecs, const ContractImportResult& doc,
											 const std::string& entityId,
											 const std::string& propertyKey);
std::optional<std::array<float, 4>> contractGetRgba(MEcs& ecs, const ContractImportResult& doc,
													const std::string& entityId,
													const std::string& propertyKey);

bool contractSetFloat(MEcs& ecs, const ContractImportResult& doc, const std::string& entityId,
					  const std::string& propertyKey, float value);
bool contractSetString(MEcs& ecs, const ContractImportResult& doc, const std::string& entityId,
					   const std::string& propertyKey, const std::string& value);
bool contractSetRgba(MEcs& ecs, const ContractImportResult& doc, const std::string& entityId,
					 const std::string& propertyKey, const std::array<float, 4>& rgba);

bool contractRunAction(MEcs& ecs, const ContractImportResult& doc, const std::string& actionId);

ContractImportResult importContractFile(MEcs& ecs, const std::string& path);
ContractImportResult importContractFile(MEcs& ecs, const std::string& path,
										const ComponentSerializerRegistry& codecs);

/** @brief Import without a codec table (hand-written fallback). Prefer the codecs overload. */
ContractImportResult importContractJson(MEcs& ecs, const std::string& jsonText,
										const std::string& sourceLabel = {});

/**
 * @brief Import using registered document codecs for component blobs.
 * @details Relationship still remaps by Contract document id (not `eN` handles).
 * UI layout rows, paint.solid present fallback, and material→fill stay special.
 */
ContractImportResult importContractJson(MEcs& ecs, const std::string& jsonText,
										const std::string& sourceLabel,
										const ComponentSerializerRegistry& codecs);

} // namespace project
} // namespace rigkit
