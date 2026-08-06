#include "ProjectSerializer.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include "CProject.h"
#include "EntityIdRemap.h"
#include "ecs/MEcs.h"

namespace rigkit {
namespace project {
namespace {

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
	envelope["format_major"] = doc.formatMajor;
	envelope["format_minor"] = doc.formatMinor;
	envelope["activePageIndex"] = doc.activePageIndex;
	envelope["defaultUnit"] = doc.defaultUnit;
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
	if (envelope.contains("path")) {
		doc.path = envelope.value("path", doc.path);
	}
}

} // namespace

ProjectSerializer::ProjectSerializer() {
	registerCoreSerializers(m_registry);
}

bool ProjectSerializer::isProjectMetadataEntity(entt::registry& reg, entt::entity e) {
	return reg.all_of<ecs::CProject>(e);
}

bool ProjectSerializer::shouldSkipEntity(entt::registry& reg, entt::entity e) {
	(void)reg;
	(void)e;
	return false;
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
	syncEnvelopeFromProject(*docPtr, root["project"]);

	ordered_json entities = ordered_json::array();
	for (const entt::entity entity : m_registry.collectEntities(reg)) {
		if (shouldSkipEntity(reg, entity)) {
			continue;
		}
		if (isProjectMetadataEntity(reg, entity)) {
			continue;
		}

		ordered_json entityObj;
		entityObj["_id"] = static_cast<std::uint32_t>(entity);
		const std::string name = ecs.entityName(entity);
		if (!name.empty()) {
			entityObj["name"] = name;
		}

		for (const auto& ser : m_registry.serializers()) {
			if (!ser.hasComponent || !ser.hasComponent(reg, entity)) {
				continue;
			}
			ordered_json compJson;
			if (ser.serialize && ser.serialize(reg, entity, compJson)) {
				entityObj[ser.name] = std::move(compJson);
			}
		}

		if (entityObj.size() > 1) {
			entities.push_back(std::move(entityObj));
		}
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
	if (!root.contains("entities")) {
		spdlog::error("[rigProject] invalid project file: missing entities");
		return false;
	}
	const ordered_json* envelopePtr = nullptr;
	if (root.contains("project")) {
		envelopePtr = &root["project"];
	} else if (root.contains("document")) {
		envelopePtr = &root["document"];
	} else {
		spdlog::error("[rigProject] invalid project file: missing project/document envelope");
		return false;
	}

	const auto& docJson = *envelopePtr;
	const int formatMajor = docJson.value("format_major", 1);
	if (formatMajor > 1) {
		spdlog::error("[rigProject] unsupported project major version {}", formatMajor);
		return false;
	}

	ecs.clear();

	auto docEntity = ecs.createEntity("project");
	ecs::CProject doc;
	applyEnvelopeToProject(doc, docJson);
	doc.path = path;
	doc.dirty = false;
	ecs.addComponent<ecs::CProject>(docEntity, doc);

	EntityIdMap idMap;
	const auto& entities = root["entities"];
	// Forward order matches save/collectEntities creation order so layered 2D
	// present (shadow under plate under art) survives round-trip. Parents are
	// remapped after all entities exist — creation order does not matter for that.
	for (const ordered_json& entityObj : entities) {
		if (!entityObj.contains("_id")) {
			continue;
		}

		const std::uint32_t savedId = entityObj["_id"].get<std::uint32_t>();
		const std::string name = entityObj.value("name", "");
		const entt::entity entity = ecs.createEntity(name);
		idMap[savedId] = entity;

		auto& reg = ecs.registry();
		for (const auto& ser : m_registry.serializers()) {
			if (!entityObj.contains(ser.name)) {
				continue;
			}
			if (ser.deserialize) {
				ser.deserialize(reg, entity, entityObj[ser.name]);
			}
		}
	}

	remapEntityReferences(ecs.registry(), idMap);

	if (m_readRootExtension) {
		m_readRootExtension(ecs, root);
	}

	spdlog::info("[rigProject] loaded '{}' (entities={})", path, ecs.getEntityCount());
	return true;
}

} // namespace project
} // namespace rigkit
