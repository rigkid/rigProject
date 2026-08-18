#include "rigProject.h"
#include <spdlog/spdlog.h>
#include "CPage.h"
#include "CProject.h"
#include "ComponentSerializers.h"
#include "core/RigKitEngine.h"
#include "core/pack/PackRegistry.h"
#include "ecs/MEcs.h"
#include "ecs/SystemRegistry.h"

namespace rigkit {
namespace {

bool hasFileExtension(const std::string& path) {
	const auto slash = path.find_last_of("/\\");
	const auto dot = path.find_last_of('.');
	if (dot == std::string::npos) {
		return false;
	}
	// Ignore leading-dot names (".rig") and directory segments.
	if (slash != std::string::npos && dot < slash) {
		return false;
	}
	return dot + 1 < path.size();
}

} // namespace

rigProject::rigProject() : IPack("rigProject") {}

bool rigProject::init() {
	spdlog::info("[rigProject] init (ext='{}')", m_fileExtension);
	return true;
}

void rigProject::setFileExtension(std::string ext) {
	if (ext.empty()) {
		m_fileExtension = ".rig";
		return;
	}
	if (ext.front() != '.') {
		ext.insert(ext.begin(), '.');
	}
	m_fileExtension = std::move(ext);
}

std::string rigProject::documentPath(const std::string& stemOrPath) const {
	return normalizePath(stemOrPath);
}

std::string rigProject::normalizePath(const std::string& path) const {
	if (path.empty()) {
		return {};
	}
	if (hasFileExtension(path)) {
		return path;
	}
	return path + m_fileExtension;
}

void rigProject::setup() {
	auto* engine = getEngine();
	if (!engine) {
		return;
	}
	auto* ecs = engine->getECSManager();
	if (!ecs) {
		return;
	}

	ecs->registerComponent<ecs::CProject>("Project", true);
	ecs->registerComponent<ecs::CPage>("Page", true);
	project::registerPageSerializers(m_serializer.registry());
	// Codecs for rigComponent's PODs live with the registry, so a data-only app
	// (rigComponent without this pack) links clean.
	project::registerComponentSerializers(m_serializer.registry());

	ecs->registerSystem("ProjectLoadSave", SystemPhase::Update, [this](MEcs& e) {
		if (m_loadRequested) {
			const std::string path = normalizePath(m_pendingLoadPath);
			if (path.empty()) {
				spdlog::error("[rigProject] load requested with empty path");
			} else if (!m_serializer.load(e, path)) {
				spdlog::error("[rigProject] load failed path='{}'", path);
			}
			m_loadRequested = false;
		}
		if (m_saveRequested) {
			std::string path = normalizePath(m_pendingSavePath);
			if (path.empty()) {
				auto view = e.view<ecs::CProject>();
				for (auto entity : view) {
					path = normalizePath(view.get<ecs::CProject>(entity).path);
					break;
				}
			}
			if (path.empty()) {
				spdlog::error("[rigProject] save requested with empty path");
			} else if (!m_serializer.save(e, path)) {
				spdlog::error("[rigProject] save failed path='{}'", path);
			}
			m_saveRequested = false;
		}
	});

	spdlog::info("[rigProject] registered Project/Page + load/save system (ext='{}')",
				 m_fileExtension);
}

void rigProject::requestSave(const std::string& path) {
	m_pendingSavePath = path;
	m_saveRequested = true;
}

void rigProject::requestLoad(const std::string& path) {
	m_pendingLoadPath = path;
	m_loadRequested = true;
}

void rigProject::registerSerializer(project::ComponentSerializer serializer) {
	m_serializer.registry().registerSerializer(std::move(serializer));
}

void rigProject::setRootExtensionWriter(project::ProjectSerializer::RootExtensionWriter writer) {
	m_serializer.setRootExtensionWriter(std::move(writer));
}

void rigProject::setRootExtensionReader(project::ProjectSerializer::RootExtensionReader reader) {
	m_serializer.setRootExtensionReader(std::move(reader));
}

} // namespace rigkit

namespace {
struct rigProjectRegistrar {
	rigProjectRegistrar() {
		rigkit::PackRegistry::instance().addFactory("rigProject", []() {
			return std::shared_ptr<rigkit::IPack>(std::make_shared<rigkit::rigProject>());
		});
	}
};
static rigProjectRegistrar rigProject_auto_reg;
} // namespace
