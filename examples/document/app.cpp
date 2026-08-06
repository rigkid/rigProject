#include "app.h"

#include <algorithm>
#include <string>

#include "core/RigKitEngine.h"
#include "core/pack/MPack.h"
#include "core/util/AppPaths.h"
#include "packs/rigComponent/src/CTransform.h"
#include "packs/rigComponent/src/rig.h"
#include "packs/rigComponent/src/rigComponent.h"
#include "packs/rigProject/src/CPage.h"
#include "packs/rigProject/src/CProject.h"
#include "packs/rigSystems/src/rigSystems.h"

namespace {

using rigkit::ecs::CDrawStyle;

/// Local design width of the storyboard. Update fits board-root to the window.
constexpr float kBoardSpan = 960.f;

constexpr float kCardW = 260.f;
constexpr float kCardH = 146.f; // ~16:9
constexpr float kCardGap = 36.f;

CDrawStyle ink(float alpha = 1.f) {
	return rig::fill(0.11f, 0.13f, 0.18f, alpha);
}

CDrawStyle inkLine(float alpha, float width) {
	return rig::stroke(0.11f, 0.13f, 0.18f, alpha, width);
}

CDrawStyle paper() {
	return rig::fill(0.98f, 0.97f, 0.94f);
}

CDrawStyle shadow() {
	return rig::fill(0.11f, 0.13f, 0.18f, 0.12f);
}

CDrawStyle accent() {
	return rig::fill(0.84f, 0.29f, 0.20f);
}

CDrawStyle warmLight() {
	return rig::fill(0.95f, 0.72f, 0.38f, 0.85f);
}

CDrawStyle curtain() {
	return rig::fill(0.22f, 0.28f, 0.42f);
}

CDrawStyle shutter() {
	return rig::fill(0.18f, 0.20f, 0.24f);
}

/// Keep the child's local TRS (makeRect already placed it in board space).
void attach(entt::entity child, entt::entity parent) {
	if (child == entt::null) {
		return;
	}
	rig::parentTo(child, parent);
}

/**
 * One page card: shadow, paper plate (selection target), ink frame, cue art.
 * Plate is named `card-<name>` so activePageIndex can reseed CSelection after load.
 */
void makeCard(entt::entity root, float cx, float cy, const char* name, int kind) {
	const float left = cx - kCardW * 0.5f;
	const float top = cy - kCardH * 0.5f;
	const std::string id = name;

	auto shade = rig::makeRect(left + 6.f, top + 8.f, kCardW, kCardH, shadow(), "shadow-" + id);
	attach(shade, root);

	auto plate = rig::makeRect(left, top, kCardW, kCardH, paper(), "card-" + id);
	attach(plate, root);

	auto frame = rig::makeRect(left, top, kCardW, kCardH, inkLine(0.55f, 2.f), "frame-" + id);
	attach(frame, root);

	const float midX = cx;
	const float midY = cy;

	if (kind == 0) {
		// Open — parting curtains + warm gap light.
		auto leftCurtain =
			rig::makeRect(left + 18.f, top + 16.f, 72.f, kCardH - 32.f, curtain(), "open-L-" + id);
		attach(leftCurtain, root);
		auto rightCurtain = rig::makeRect(left + kCardW - 90.f, top + 16.f, 72.f, kCardH - 32.f,
										  curtain(), "open-R-" + id);
		attach(rightCurtain, root);
		auto glow = rig::makeEllipse(midX, midY, 36.f, 70.f, warmLight(), "open-glow-" + id);
		attach(glow, root);
	} else if (kind == 1) {
		// Look — concentric iris + coral pupil.
		auto outer = rig::makeCircle(midX, midY, 42.f, inkLine(0.45f, 2.5f), "look-ring-" + id);
		attach(outer, root);
		auto mid = rig::makeCircle(midX, midY, 28.f, ink(0.08f), "look-iris-" + id);
		attach(mid, root);
		auto pupil = rig::makeCircle(midX, midY, 12.f, accent(), "look-pupil-" + id);
		attach(pupil, root);
	} else {
		// Close — horizontal shutter bars.
		const float barH = 14.f;
		const float barW = kCardW - 48.f;
		const float barX = left + 24.f;
		for (int b = 0; b < 4; ++b) {
			const float by = top + 28.f + static_cast<float>(b) * (barH + 10.f);
			auto bar = rig::makeRect(barX, by, barW, barH, shutter(),
									 "close-bar-" + id + "-" + std::to_string(b));
			attach(bar, root);
		}
	}

	// Tab chip under the card — colour cue for the page name without text.
	const CDrawStyle tabs[] = {rig::fill(0.30f, 0.45f, 0.70f), accent(),
							   rig::fill(0.28f, 0.55f, 0.40f)};
	auto tab = rig::makeRect(cx - 18.f, top + kCardH + 14.f, 36.f, 8.f, tabs[kind], "tab-" + id);
	attach(tab, root);
}

} // namespace

void DocumentApp::setup() {
	spdlog::info("document — Install Cue storyboard (.rig round-trip)");
	m_engine->setClearColor(0.94f, 0.93f, 0.90f, 1.0f);

	auto* packs = m_engine->getPackManager();
	if (!packs) {
		return;
	}
	packs->registerPack<rigkit::rigComponent>();
	packs->registerPack<rigkit::rigSystems>();
	// Default is ".rig". Use setFileExtension("rigdoc") for the longer form.
	packs->registerPack<rigkit::rigProject>();
	packs->initAll();
	packs->setupAll();

	m_document = packs->getPack<rigkit::rigProject>();
	if (!m_document) {
		return;
	}

	m_docPath = m_document->documentPath(AppPaths::getDataDir() + "/show");
	buildScene();
	m_document->requestSave(m_docPath);
	m_phase = 0;
}

void DocumentApp::buildScene() {
	auto* ecs = m_engine->getECSManager();
	if (!ecs) {
		return;
	}

	auto docEntity = ecs->createEntity("show-doc");
	rigkit::ecs::CProject doc;
	doc.title = "Install Cue";
	doc.path = m_docPath.empty() ? "data/show.rig" : m_docPath;
	doc.activePageIndex = 1;
	doc.defaultUnit = "px";
	doc.dirty = true;
	ecs->addComponent<rigkit::ecs::CProject>(docEntity, doc);

	m_root = rig::makeGroup({0.f, 0.f, 0.f}, "board-root");

	// Header rule + accent mark (no text present without rigVarFont).
	auto rule = rig::makeLine(-420.f, -210.f, 420.f, -210.f, inkLine(0.35f, 2.f), "header-rule");
	attach(rule, m_root);
	auto mark = rig::makeRect(-420.f, -222.f, 28.f, 8.f, accent(), "header-mark");
	attach(mark, m_root);

	const char* names[] = {"Open", "Look", "Close"};
	const float totalW = 3.f * kCardW + 2.f * kCardGap;
	const float startX = -totalW * 0.5f + kCardW * 0.5f;
	const float cardY = -20.f;

	for (int i = 0; i < 3; ++i) {
		rigkit::ecs::CPage page;
		page.name = names[i];
		page.index = i;
		page.width = 1920.f;
		page.height = 1080.f;
		auto pageEntity = ecs->createEntity(std::string("page-") + names[i]);
		ecs->addComponent<rigkit::ecs::CPage>(pageEntity, page);

		const float cx = startX + static_cast<float>(i) * (kCardW + kCardGap);
		makeCard(m_root, cx, cardY, names[i], i);
	}

	applyActiveSelection();
	fitBoard();
}

void DocumentApp::applyActiveSelection() {
	auto* ecs = m_engine->getECSManager();
	if (!ecs) {
		return;
	}

	int active = 0;
	auto docView = ecs->view<rigkit::ecs::CProject>();
	for (auto entity : docView) {
		active = docView.get<rigkit::ecs::CProject>(entity).activePageIndex;
		break;
	}

	const char* names[] = {"Open", "Look", "Close"};
	for (int i = 0; i < 3; ++i) {
		auto card = ecs->findEntity(std::string("card-") + names[i]);
		if (card == entt::null) {
			continue;
		}
		rig::select(card, i == active);
	}
}

void DocumentApp::fitBoard() {
	auto* ecs = m_engine->getECSManager();
	if (!ecs) {
		return;
	}
	if (m_root == entt::null || !ecs->hasComponent<rigkit::ecs::CTransform>(m_root)) {
		m_root = ecs->findEntity("board-root");
	}
	if (m_root == entt::null || !ecs->hasComponent<rigkit::ecs::CTransform>(m_root)) {
		return;
	}

	int designW = 0, designH = 0, fbW = 0, fbH = 0;
	m_engine->getPresentSize(designW, designH, fbW, fbH);
	auto& root = ecs->getComponent<rigkit::ecs::CTransform>(m_root);
	root.position = {designW * 0.5f, designH * 0.5f, 0.f};
	const float fit = float(std::min(designW, designH)) / kBoardSpan;
	root.scale = {fit, fit, 1.f};
}

void DocumentApp::update(float) {
	fitBoard();

	if (!m_document) {
		return;
	}
	if (m_phase == 0) {
		// Save runs in ProjectLoadSave this frame (after app update).
		m_phase = 1;
		return;
	}
	if (m_phase == 1) {
		m_document->requestLoad(m_docPath);
		m_phase = 2;
		return;
	}
	if (m_phase == 2) {
		// Load ran last frame; entity ids are new — refresh root + session selection.
		m_root = entt::null;
		fitBoard();
		applyActiveSelection();
		auto* ecs = m_engine->getECSManager();
		spdlog::info("document — round-trip done entities={} path='{}'",
					 ecs ? ecs->getEntityCount() : 0, m_docPath);
		m_phase = 3;
	}
}
