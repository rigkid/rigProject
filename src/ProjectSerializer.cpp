#include "ProjectSerializer.h"

#include "ecs/MEcs.h"
#include "CProject.h"
#include "CTransient.h"
#include "EntityIdRemap.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>

namespace rigkit {
namespace project {
namespace {

/// Entity name the project entity's extra components are saved under.
constexpr const char* kProjectEntityName = "project";

std::string iso8601Now() {
	using clock = std::chrono::system_clock;
	const auto now = clock::now();
	const std::time_t t = clock::to_time_t(now);
	std::tm tm{};
#if defined(_WIN32)
	gmtime_s(&tm, &t);
#else
	gmtime_r(&t, &tm);
#endif
	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
	return oss.str();
}

void syncEnvelopeFromProject(const ecs::CProject& doc, ordered_json& envelope) {
	envelope["title"] = doc.title;
	envelope["author"] = doc.author;
	envelope["createdAt"] = doc.createdAt;
	envelope["modifiedAt"] = doc.modifiedAt;
	// Stamp the format this writer emits, not the one the document was read
	// with - a loaded 1.0 file is saved back as the current format.
	envelope["format_major"] = kFormatMajor;
	envelope["format_minor"] = kFormatMinor;
	envelope["activePageIndex"] = doc.activePageIndex;
	envelope["defaultUnit"] = doc.defaultUnit;
	envelope["colorSpace"] = doc.colorSpace.empty() ? "srgb" : doc.colorSpace;
	if (!doc.language.empty()) {
		envelope["language"] = doc.language;
	}
	if (!doc.pdfX.empty()) {
		envelope["pdfX"] = doc.pdfX;
	}
	if (!doc.outputCondition.empty()) {
		envelope["outputCondition"] = doc.outputCondition;
	}
	if (!doc.outputRegistry.empty()) {
		envelope["outputRegistry"] = doc.outputRegistry;
	}
	if (!doc.outputProfile.empty()) {
		envelope["outputProfile"] = doc.outputProfile;
	}
	if (!doc.trapped.empty() && doc.trapped != "unknown") {
		envelope["trapped"] = doc.trapped;
	}
	if (!doc.untaggedRgbProfile.empty() && doc.untaggedRgbProfile != "srgb") {
		envelope["untaggedRgbProfile"] = doc.untaggedRgbProfile;
	}
	if (!doc.renderingIntent.empty() && doc.renderingIntent != "relative-colorimetric") {
		envelope["renderingIntent"] = doc.renderingIntent;
	}
	if (!doc.path.empty()) {
		envelope["path"] = doc.path;
	}
}

void applyEnvelopeToProject(ecs::CProject& doc, const ordered_json& envelope) {
	doc.title = envelope.value("title", doc.title);
	doc.author = envelope.value("author", doc.author);
	doc.createdAt = envelope.value("createdAt", doc.createdAt);
	doc.modifiedAt = envelope.value("modifiedAt", doc.modifiedAt);
	doc.formatMajor = envelope.value("format_major", doc.formatMajor);
	doc.formatMinor = envelope.value("format_minor", doc.formatMinor);
	doc.activePageIndex = envelope.value("activePageIndex", doc.activePageIndex);
	doc.defaultUnit = envelope.value("defaultUnit", doc.defaultUnit);
	doc.colorSpace = envelope.value("colorSpace", doc.colorSpace);
	doc.language = envelope.value("language", doc.language);
	doc.pdfX = envelope.value("pdfX", doc.pdfX);
	doc.outputCondition = envelope.value("outputCondition", doc.outputCondition);
	doc.outputRegistry = envelope.value("outputRegistry", doc.outputRegistry);
	doc.outputProfile = envelope.value("outputProfile", doc.outputProfile);
	doc.trapped = envelope.value("trapped", doc.trapped);
	doc.untaggedRgbProfile = envelope.value("untaggedRgbProfile", doc.untaggedRgbProfile);
	doc.renderingIntent = envelope.value("renderingIntent", doc.renderingIntent);
	if (envelope.contains("path")) {
		doc.path = envelope.value("path", doc.path);
	}
}

} // namespace

bool ProjectSerializer::isProjectMetadataEntity(entt::registry& reg, entt::entity e) {
	return reg.all_of<ecs::CProject>(e);
}

bool ProjectSerializer::shouldSkipEntity(entt::registry& reg, entt::entity e) {
	// Session scaffolding (preview scenes, caches) never belongs in a document.
	return reg.all_of<ecs::CTransient>(e);
}

bool ProjectSerializer::save(MEcs& ecs, const std::string& path) const {
	auto& reg = ecs.registry();

	ecs::CProject* docPtr = nullptr;
	for (auto entity : reg.view<ecs::CProject>()) {
		docPtr = &reg.get<ecs::CProject>(entity);
		break;
	}
	if (!docPtr) {
		spdlog::error("[rigProject] save: no CProject entity");
		return false;
	}

	if (docPtr->createdAt.empty()) {
		docPtr->createdAt = iso8601Now();
	}
	docPtr->modifiedAt = iso8601Now();

	ordered_json root;
	// Version first: it is what tells any reader this is a Rig document at all.
	root["rig"] = kContractVersion;
	syncEnvelopeFromProject(*docPtr, root["document"]);

	ordered_json entities = ordered_json::array();
	for (const entt::entity entity : m_registry.collectEntities(reg)) {
		if (shouldSkipEntity(reg, entity)) {
			continue;
		}
		// The project entity's CProject lives in the `document` envelope. Any other
		// component composed on it (for example rig.layout.chrome) rides a record
		// named kProjectEntityName so load can fold it back onto the same entity.
		const bool projectEntity = isProjectMetadataEntity(reg, entity);

		ordered_json components = ordered_json::object();
		const std::string name =
			projectEntity ? std::string(kProjectEntityName) : ecs.entityName(entity);
		if (!name.empty()) {
			components["rig.meta.named"] = ordered_json{{"name", name}};
		}

		bool wroteComponent = false;
		for (const auto& ser : m_registry.serializers()) {
			if (!ser.hasComponent || !ser.hasComponent(reg, entity)) {
				continue;
			}
			// A component with no fields is still present: write {}, never null.
			ordered_json compJson = ordered_json::object();
			if (ser.serialize && ser.serialize(reg, entity, compJson)) {
				components[ser.schemaId] = std::move(compJson);
				wroteComponent = true;
			}
		}

		if (components.empty() || (projectEntity && !wroteComponent)) {
			continue;
		}
		ordered_json entityObj;
		entityObj["id"] = entityIdString(entity);
		entityObj["components"] = std::move(components);
		entities.push_back(std::move(entityObj));
	}
	root["entities"] = std::move(entities);

	if (m_writeRootExtension) {
		m_writeRootExtension(ecs, root);
	}

	if (!savePrettyOrderedJson(path, root)) {
		return false;
	}

	docPtr->path = path;
	docPtr->dirty = false;
	spdlog::info("[rigProject] saved '{}'", path);
	return true;
}

bool ProjectSerializer::load(MEcs& ecs, const std::string& path) {
	ordered_json root = loadOrderedJson(path);
	if (root.is_null() || root.empty()) {
		return false;
	}
	if (!root.contains("rig")) {
		spdlog::error("[rigProject] '{}' is not a Rig document: missing the \"rig\" version "
					  "field. Files written before RigKit spoke the Contract cannot be read.",
					  path);
		return false;
	}
	if (!root.contains("entities")) {
		spdlog::error("[rigProject] invalid project file: missing entities");
		return false;
	}
	if (!root.contains("document")) {
		spdlog::error("[rigProject] invalid project file: missing document envelope");
		return false;
	}

	const auto& docJson = root["document"];
	const int formatMajor = docJson.value("format_major", 1);
	if (formatMajor > 1) {
		spdlog::error("[rigProject] unsupported project major version {}", formatMajor);
		return false;
	}

	ecs.clear();
	// Registry ctx survives clear(); wipe staged layout/story refs before
	// deserialize so a prior incomplete load cannot leak into this one.
	m_registry.clearContractEntityStaging(ecs.registry());

	auto docEntity = ecs.createEntity("project");
	ecs::CProject doc;
	applyEnvelopeToProject(doc, docJson);
	doc.path = path;
	doc.dirty = false;
	ecs.addComponent<ecs::CProject>(docEntity, doc);

	EntityIdMap idMap;
	bool projectFolded = false;
	const auto& entities = root["entities"];
	// Forward order matches save/collectEntities creation order so layered 2D
	// present (shadow under plate under art) survives round-trip. Parents are
	// remapped after all entities exist - creation order does not matter for that.
	for (const ordered_json& entityObj : entities) {
		if (!entityObj.contains("id") || !entityObj.contains("components")) {
			continue;
		}
		// Serializer-written ids look like "e42" and join the remap table so
		// entity references survive. Hand-authored documents may use readable
		// ids ("pixelplotter-scene") — those entities must still load; they
		// just cannot be the target of a numeric reference remap.
		const entt::entity savedEntity = entityIdFromString(entityObj["id"].get<std::string>());
		const auto& components = entityObj["components"];

		std::string name;
		if (components.contains("rig.meta.named")) {
			name = components["rig.meta.named"].value("name", "");
		}
		// The record save wrote for the project entity's extra components folds
		// back onto the project entity instead of making a second entity.
		const bool foldIntoProject = !projectFolded && name == kProjectEntityName;
		projectFolded = projectFolded || foldIntoProject;
		const entt::entity entity = foldIntoProject ? docEntity : ecs.createEntity(name);
		if (savedEntity != entt::null) {
			idMap[static_cast<std::uint32_t>(savedEntity)] = entity;
		}

		auto& reg = ecs.registry();
		for (const auto& ser : m_registry.serializers()) {
			if (!components.contains(ser.schemaId)) {
				continue;
			}
			if (ser.deserialize) {
				// A field-less component written as null (older saves) reads as empty.
				static const ordered_json kEmpty = ordered_json::object();
				const auto& blob = components[ser.schemaId];
				ser.deserialize(reg, entity, blob.is_null() ? kEmpty : blob);
			}
		}
	}

	remapEntityReferences(ecs.registry(), idMap);
	for (const auto& ser : m_registry.serializers()) {
		if (ser.remapReferences) {
			ser.remapReferences(ecs.registry(), idMap);
		}
	}
	// Layout/story codecs stage eN refs into pending during deserialize;
	// resolve them the same way Contract import does.
	{
		std::unordered_map<std::string, entt::entity> stringMap;
		for (const auto& [saved, live] : idMap) {
			stringMap[entityIdString(static_cast<entt::entity>(saved))] = live;
		}
		m_registry.applyContractEntityResolutions(ecs.registry(), stringMap);
	}

	if (m_readRootExtension) {
		m_readRootExtension(ecs, root);
	}

	spdlog::info("[rigProject] loaded '{}' (entities={})", path, ecs.getEntityCount());
	return true;
}

} // namespace project
} // namespace rigkit
