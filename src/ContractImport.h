#pragma once

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt/entt.hpp>

#include "core/U_core.h"

namespace rigkit {
namespace project {

/**
 * @brief Contract JSON (`rig.*` keys) → host POD import sidecar.
 * @details Parallel to PascalCase `.rig` via `ProjectSerializer`. Clears the ECS
 * and writes `rigComponent` PODs; LFO / UI / paint live on this result for Update
 * and host UI fulfillments.
 */
struct ContractImportResult {
	bool ok = false;
	std::string title;
	std::string error;
	int entityCount = 0;
	int geometryCount = 0;
	std::vector<std::string> skipped;
	std::vector<std::string> notes;

	struct Lfo {
		std::string id;
		std::string waveform = "sine";
		float frequency = 0.f;
		float amplitude = 1.f;
		float offset = 0.f;
		float phase = 0.f;
	};
	struct Binding {
		std::string id;
		std::string source;
		std::string target;
		std::string propertyKey;
		float depth = 1.f;
		bool hasMin = false;
		bool hasMax = false;
		float min = 0.f;
		float max = 0.f;
		bool additive = false;
	};
	struct Paint {
		std::string id;
		std::array<float, 4> rgba{1.f, 1.f, 1.f, 1.f};
	};
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

	std::vector<Lfo> lfos;
	std::vector<Binding> bindings;
	std::vector<Paint> paints;
	std::vector<Panel> panels;
	std::vector<Group> groups;
	std::vector<Control> controls;
	std::vector<Action> actions;
	/// entity id → live entt handle (for Update-side modulators / UI)
	std::unordered_map<std::string, entt::entity> entities;
};

void tickContractModulators(MEcs& ecs, ContractImportResult& doc, float timeSec);

std::optional<float> contractGetFloat(const ContractImportResult& doc, const std::string& entityId,
									  const std::string& propertyKey);
std::optional<std::string> contractGetString(const ContractImportResult& doc,
											 const std::string& entityId,
											 const std::string& propertyKey);
std::optional<std::array<float, 4>> contractGetRgba(const ContractImportResult& doc,
													const std::string& entityId,
													const std::string& propertyKey);

bool contractSetFloat(ContractImportResult& doc, MEcs& ecs, const std::string& entityId,
					  const std::string& propertyKey, float value);
bool contractSetString(ContractImportResult& doc, const std::string& entityId,
					   const std::string& propertyKey, const std::string& value);
bool contractSetRgba(ContractImportResult& doc, MEcs& ecs, const std::string& entityId,
					 const std::string& propertyKey, const std::array<float, 4>& rgba);

bool contractRunAction(ContractImportResult& doc, const std::string& actionId, float timeSec);

ContractImportResult importContractFile(MEcs& ecs, const std::string& path);
ContractImportResult importContractJson(MEcs& ecs, const std::string& jsonText,
										const std::string& sourceLabel = {});

} // namespace project
} // namespace rigkit
