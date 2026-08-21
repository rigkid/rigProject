#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "CDrawStyle.h"
#include "CGroup.h"
#include "CLayer.h"
#include "CPage.h"
#include "CProject.h"
#include "CRectangle.h"
#include "CRelationship.h"
#include "CTransform.h"
#include "ecs/MEcs.h"

namespace rigkit {
class MEcs;
}

namespace rig {

rigkit::MEcs* currentEcs();

/**
 * @brief Pointer + view for `pollPageNav` — same space as `CTransform` (design).
 * @details `view*` is the canvas hole. Convert window-client / ImGui pixels
 * with `design / framebuffer` from `getPresentSize`. `blocked` when UI ate the
 * mouse. LMB or MMB pans; wheel zooms toward the cursor; `wheelH` pans X.
 */
struct PageNavInput {
	float viewX = 0.f;
	float viewY = 0.f;
	float viewW = 0.f;
	float viewH = 0.f;
	float mouseX = 0.f;
	float mouseY = 0.f;
	float wheel = 0.f;
	float wheelH = 0.f;
	bool pan = false;
	bool blocked = false;
};

/**
 * @brief Stacked `CPage` board — include this header and call; no extra system.
 * @details First `makePage` writes a `CProject` (or reuses one) and a board
 * group. Later pages sit under the last, centered on the board X. Shapes stay
 * page-local; call `placeOnPage` after `make*`. Board `CTransform` is the view
 * (pan + zoom). PDF emit is **rigPdf**.
 *
 * ```cpp
 * #include "rig/pages.h"
 * auto p = rig::makePage(520.f, 380.f);
 * auto c = rig::makeCircle(260.f, 190.f, 40.f);
 * rig::placeOnPage(c);
 * // update: rig::pollPageNav(nav); // or rig::fitPages();
 * // view: rig::setPageLayout(rig::PageLayout::Single);
 * ```
 */
enum class PageLayout {
	Stack,	///< Pages sit under each other; pan the board to scroll.
	Single, ///< Only the active page is visible.
	Spread, ///< Facing pairs side by side (recto-first: page 0 alone on the right).
};

entt::entity makePage(float w, float h, const char* unit = nullptr, bool plate = true);
entt::entity makePage(rigkit::MEcs& ecs, float w, float h, const char* unit = nullptr,
					  bool plate = true);

/** @brief Board group pages parent to. Created on first `makePage`. */
entt::entity pageBoard();

/** @brief Page `makePage` last wrote, or `entt::null`. */
entt::entity currentPage();

/** @brief Parent @p child to `currentPage` when one exists. */
void placeOnPage(entt::entity child);
void placeOnPage(rigkit::MEcs& ecs, entt::entity child);

int pageCount();
int activePageIndex();
entt::entity pageEntity(int index);
void setPageLayout(PageLayout layout);
PageLayout pageLayout();
void focusPage(int index);
void scrollPages(float dy);
void panPages(float dx, float dy);
void zoomPages(float factor, float pivotX, float pivotY);
void fitPages();
float pageZoom();

/** @brief Wheel over the canvas — pans Y. Prefer `pollPageNav` for zoom + pan. */
void pollPageScroll(float wheel, bool uiWantsMouse);

/** @brief Pan / zoom / first-fit. No-op when @p in.blocked except for storing the view. */
void pollPageNav(const PageNavInput& in);

/** @brief Destroy board / pages / plates this helper created. Keeps a found `CProject`. */
void clearPages();

/** @brief Forget board state after the app already destroyed the entities. */
void resetPages();

namespace pages_detail {

constexpr float kPageGap = 48.f;
constexpr float kWheelPan = 56.f;
constexpr float kZoomMin = 0.12f;
constexpr float kZoomMax = 8.f;
constexpr float kFitPad = 0.90f;
constexpr float kParkY = 1.0e7f;

struct Runtime {
	rigkit::MEcs* ecs = nullptr;
	entt::entity project{entt::null};
	entt::entity board{entt::null};
	entt::entity current{entt::null};
	std::vector<entt::entity> pages;
	std::vector<entt::entity> owned;
	float stackY = 0.f;
	float panX = 0.f;
	float panY = 0.f;
	float zoom = 1.f;
	float viewX = 0.f;
	float viewY = 0.f;
	float viewW = 0.f;
	float viewH = 0.f;
	float lastMouseX = 0.f;
	float lastMouseY = 0.f;
	float fitViewW = 0.f;
	float fitViewH = 0.f;
	bool ownProject = false;
	bool dragging = false;
	bool fitted = false;
	bool userNav = false;
	PageLayout layout = PageLayout::Stack;
};

inline Runtime& rt() {
	static Runtime r;
	return r;
}

inline rigkit::MEcs* ecsOr(rigkit::MEcs* passed) {
	if (passed) {
		rt().ecs = passed;
		return passed;
	}
	if (rt().ecs) {
		return rt().ecs;
	}
	rt().ecs = currentEcs();
	return rt().ecs;
}

inline void own(entt::entity e) {
	if (e != entt::null) {
		rt().owned.push_back(e);
	}
}

inline void writeBoard() {
	auto& s = rt();
	if (!s.ecs || s.board == entt::null || !s.ecs->hasComponent<rigkit::ecs::CTransform>(s.board)) {
		return;
	}
	auto& t = s.ecs->getComponent<rigkit::ecs::CTransform>(s.board);
	t.position.x = s.panX;
	t.position.y = s.panY;
	t.scale = {s.zoom, s.zoom, 1.f};
}

inline void setActive(int index) {
	auto& s = rt();
	if (!s.ecs || s.project == entt::null ||
		!s.ecs->hasComponent<rigkit::ecs::CProject>(s.project)) {
		return;
	}
	s.ecs->getComponent<rigkit::ecs::CProject>(s.project).activePageIndex = index;
}

inline void setParent(rigkit::MEcs& ecs, entt::entity child, entt::entity parent) {
	if (child == entt::null || !ecs.registry().valid(child)) {
		return;
	}
	rigkit::ecs::CRelationship rel;
	rel.parent = parent;
	ecs.registry().emplace_or_replace<rigkit::ecs::CRelationship>(child, rel);
}

inline bool pageExtent(entt::entity e, float& x, float& y, float& w, float& h) {
	auto& s = rt();
	if (!s.ecs || !s.ecs->hasComponent<rigkit::ecs::CPage>(e) ||
		!s.ecs->hasComponent<rigkit::ecs::CTransform>(e)) {
		return false;
	}
	const auto& page = s.ecs->getComponent<rigkit::ecs::CPage>(e);
	const auto& xf = s.ecs->getComponent<rigkit::ecs::CTransform>(e);
	w = page.width;
	h = page.height;
	x = xf.position.x;
	y = xf.position.y;
	return true;
}

inline bool pageVisible(entt::entity e) {
	auto& s = rt();
	if (!s.ecs || !s.ecs->hasComponent<rigkit::ecs::CLayer>(e)) {
		return true;
	}
	return s.ecs->getComponent<rigkit::ecs::CLayer>(e).visible;
}

inline void applyLayout() {
	auto& s = rt();
	if (!s.ecs) {
		return;
	}
	const int active = [&]() {
		if (s.project != entt::null && s.ecs->hasComponent<rigkit::ecs::CProject>(s.project)) {
			return s.ecs->getComponent<rigkit::ecs::CProject>(s.project).activePageIndex;
		}
		return 0;
	}();
	float y = 0.f;

	if (s.layout == PageLayout::Spread) {
		// Recto-first, left binding: page 0 alone on the right; then 1|2, 3|4, …
		int i = 0;
		const int n = static_cast<int>(s.pages.size());
		while (i < n) {
			const bool loneRecto = (i == 0);
			const int leftIdx = loneRecto ? -1 : i;
			const int rightIdx = loneRecto ? 0 : (i + 1 < n ? i + 1 : -1);
			float rowH = 0.f;

			auto place = [&](int idx, bool left) {
				if (idx < 0) {
					return;
				}
				const auto e = s.pages[static_cast<size_t>(idx)];
				if (!s.ecs->registry().valid(e) || !s.ecs->hasComponent<rigkit::ecs::CPage>(e) ||
					!s.ecs->hasComponent<rigkit::ecs::CTransform>(e)) {
					return;
				}
				const auto& page = s.ecs->getComponent<rigkit::ecs::CPage>(e);
				auto& xf = s.ecs->getComponent<rigkit::ecs::CTransform>(e);
				if (!s.ecs->hasComponent<rigkit::ecs::CLayer>(e)) {
					s.ecs->addComponent<rigkit::ecs::CLayer>(e, rigkit::ecs::CLayer{});
				}
				s.ecs->getComponent<rigkit::ecs::CLayer>(e).visible = true;
				rowH = std::max(rowH, page.height);
				if (loneRecto) {
					xf.position.x = kPageGap * 0.5f;
				} else if (left) {
					xf.position.x = -page.width - kPageGap * 0.5f;
				} else {
					xf.position.x = kPageGap * 0.5f;
				}
				xf.position.y = y;
			};

			if (loneRecto) {
				place(0, false);
				i = 1;
			} else {
				place(leftIdx, true);
				place(rightIdx, false);
				i += 2;
			}
			y += rowH + kPageGap;
		}
		// Hide nothing in Spread — all pages stay visible in their pairs.
		s.stackY = y;
		return;
	}

	for (int i = 0; i < static_cast<int>(s.pages.size()); ++i) {
		const auto e = s.pages[static_cast<size_t>(i)];
		if (!s.ecs->registry().valid(e) || !s.ecs->hasComponent<rigkit::ecs::CPage>(e) ||
			!s.ecs->hasComponent<rigkit::ecs::CTransform>(e)) {
			continue;
		}
		const auto& page = s.ecs->getComponent<rigkit::ecs::CPage>(e);
		auto& xf = s.ecs->getComponent<rigkit::ecs::CTransform>(e);
		const bool show = (s.layout == PageLayout::Stack) || (i == active);
		if (!s.ecs->hasComponent<rigkit::ecs::CLayer>(e)) {
			s.ecs->addComponent<rigkit::ecs::CLayer>(e, rigkit::ecs::CLayer{});
		}
		s.ecs->getComponent<rigkit::ecs::CLayer>(e).visible = show;
		xf.position.x = -page.width * 0.5f;
		if (s.layout == PageLayout::Stack) {
			xf.position.y = y;
			y += page.height + kPageGap;
		} else {
			xf.position.y = show ? 0.f : kParkY;
		}
	}
	s.stackY = y;
}

inline bool stackBounds(float& minX, float& minY, float& maxX, float& maxY) {
	auto& s = rt();
	bool any = false;
	for (auto e : s.pages) {
		if (!pageVisible(e)) {
			continue;
		}
		float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
		if (!pageExtent(e, x, y, w, h)) {
			continue;
		}
		if (!any) {
			minX = x;
			minY = y;
			maxX = x + w;
			maxY = y + h;
			any = true;
		} else {
			minX = std::min(minX, x);
			minY = std::min(minY, y);
			maxX = std::max(maxX, x + w);
			maxY = std::max(maxY, y + h);
		}
	}
	return any;
}

inline void applyZoom(float factor, float pivotX, float pivotY) {
	auto& s = rt();
	const float oldZ = s.zoom;
	s.zoom = std::clamp(oldZ * factor, kZoomMin, kZoomMax);
	if (std::fabs(s.zoom - oldZ) < 1e-6f) {
		return;
	}
	const float k = s.zoom / oldZ;
	s.panX = pivotX - (pivotX - s.panX) * k;
	s.panY = pivotY - (pivotY - s.panY) * k;
	s.userNav = true;
	writeBoard();
}

inline void markActiveFromView() {
	auto& s = rt();
	if (!s.ecs || s.pages.empty()) {
		return;
	}
	if (s.layout != PageLayout::Stack && s.layout != PageLayout::Spread) {
		return;
	}
	const float viewMidY = (s.viewH > 1.f) ? (s.viewY + s.viewH * 0.5f) : (-s.panY);
	int best = 0;
	float bestDist = 1e9f;
	for (int i = 0; i < static_cast<int>(s.pages.size()); ++i) {
		float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
		if (!pageExtent(s.pages[static_cast<size_t>(i)], x, y, w, h)) {
			continue;
		}
		const float mid = s.panY + (y + h * 0.5f) * s.zoom;
		const float d = std::fabs(mid - viewMidY);
		if (d < bestDist) {
			bestDist = d;
			best = i;
		}
	}
	setActive(best);
}

inline void ensureProject(rigkit::MEcs& ecs) {
	auto& s = rt();
	if (s.project != entt::null && ecs.registry().valid(s.project)) {
		return;
	}
	auto view = ecs.view<rigkit::ecs::CProject>();
	for (auto e : view) {
		s.project = e;
		s.ownProject = false;
		return;
	}
	s.project = ecs.createEntity("project");
	ecs.addComponent<rigkit::ecs::CProject>(s.project, rigkit::ecs::CProject{});
	s.ownProject = true;
	own(s.project);
}

inline void ensureBoard(rigkit::MEcs& ecs) {
	auto& s = rt();
	if (s.board != entt::null && ecs.registry().valid(s.board)) {
		return;
	}
	s.board = ecs.createEntity("page-board");
	ecs.addComponent<rigkit::ecs::CTransform>(s.board, rigkit::ecs::CTransform{});
	ecs.addComponent<rigkit::ecs::CGroup>(s.board, rigkit::ecs::CGroup{});
	own(s.board);
	writeBoard();
}

inline rigkit::ecs::CDrawStyle paperStyle() {
	rigkit::ecs::CDrawStyle s;
	s.hasFill = true;
	s.hasStroke = true;
	s.setFillColor(0.97f, 0.96f, 0.93f, 1.f);
	s.setStrokeColor(0.20f, 0.22f, 0.26f, 0.45f);
	s.strokeWidth = 1.5f;
	return s;
}

inline void resetState() {
	auto& s = rt();
	s.project = entt::null;
	s.board = entt::null;
	s.current = entt::null;
	s.pages.clear();
	s.owned.clear();
	s.stackY = 0.f;
	s.panX = 0.f;
	s.panY = 0.f;
	s.zoom = 1.f;
	s.lastMouseX = 0.f;
	s.lastMouseY = 0.f;
	s.fitViewW = 0.f;
	s.fitViewH = 0.f;
	s.ownProject = false;
	s.dragging = false;
	s.fitted = false;
	s.userNav = false;
	s.layout = PageLayout::Stack;
}

} // namespace pages_detail

inline entt::entity makePage(float w, float h, const char* unit, bool plate) {
	auto* ecs = pages_detail::ecsOr(nullptr);
	return ecs ? makePage(*ecs, w, h, unit, plate) : entt::null;
}

inline entt::entity makePage(rigkit::MEcs& ecs, float w, float h, const char* unit, bool plate) {
	auto& s = pages_detail::rt();
	s.ecs = &ecs;
	pages_detail::ensureProject(ecs);
	pages_detail::ensureBoard(ecs);

	const int index = static_cast<int>(s.pages.size());
	auto e = ecs.createEntity("page-" + std::to_string(index));
	rigkit::ecs::CPage page;
	page.name = "Page " + std::to_string(index + 1);
	page.index = index;
	page.width = w;
	page.height = h;
	if (unit && unit[0]) {
		page.unit = unit;
	}
	rigkit::ecs::CTransform xf;
	xf.position = {-w * 0.5f, s.stackY, 0.f};
	ecs.addComponent<rigkit::ecs::CPage>(e, page);
	ecs.addComponent<rigkit::ecs::CTransform>(e, xf);
	rigkit::ecs::CLayer layer;
	layer.order = index;
	ecs.addComponent<rigkit::ecs::CLayer>(e, layer);
	pages_detail::setParent(ecs, e, s.board);
	s.pages.push_back(e);
	s.current = e;
	s.stackY += h + pages_detail::kPageGap;
	pages_detail::own(e);
	pages_detail::setActive(index);

	if (plate) {
		auto plateE = ecs.createEntity("page-plate-" + std::to_string(index));
		rigkit::ecs::CRectangle rect;
		rect.width = w;
		rect.height = h;
		ecs.addComponent<rigkit::ecs::CTransform>(plateE, rigkit::ecs::CTransform{});
		ecs.addComponent<rigkit::ecs::CRectangle>(plateE, rect);
		ecs.addComponent<rigkit::ecs::CDrawStyle>(plateE, pages_detail::paperStyle());
		pages_detail::setParent(ecs, plateE, e);
		pages_detail::own(plateE);
	}
	s.fitted = false;
	pages_detail::applyLayout();
	pages_detail::writeBoard();
	return e;
}

inline entt::entity pageBoard() {
	return pages_detail::rt().board;
}

inline entt::entity currentPage() {
	return pages_detail::rt().current;
}

inline void placeOnPage(entt::entity child) {
	auto* ecs = pages_detail::ecsOr(nullptr);
	if (ecs) {
		placeOnPage(*ecs, child);
	}
}

inline void placeOnPage(rigkit::MEcs& ecs, entt::entity child) {
	auto& s = pages_detail::rt();
	if (child == entt::null || s.current == entt::null || !ecs.registry().valid(s.current)) {
		return;
	}
	pages_detail::setParent(ecs, child, s.current);
}

inline int pageCount() {
	return static_cast<int>(pages_detail::rt().pages.size());
}

inline int activePageIndex() {
	auto& s = pages_detail::rt();
	if (!s.ecs || s.project == entt::null ||
		!s.ecs->hasComponent<rigkit::ecs::CProject>(s.project)) {
		return 0;
	}
	return s.ecs->getComponent<rigkit::ecs::CProject>(s.project).activePageIndex;
}

inline entt::entity pageEntity(int index) {
	auto& s = pages_detail::rt();
	if (index < 0 || index >= static_cast<int>(s.pages.size())) {
		return entt::null;
	}
	return s.pages[static_cast<size_t>(index)];
}

inline PageLayout pageLayout() {
	return pages_detail::rt().layout;
}

inline void setPageLayout(PageLayout layout) {
	auto& s = pages_detail::rt();
	s.layout = layout;
	pages_detail::applyLayout();
	s.userNav = false;
	s.fitted = false;
	fitPages();
}

inline float pageZoom() {
	return pages_detail::rt().zoom;
}

inline void panPages(float dx, float dy) {
	auto& s = pages_detail::rt();
	if (!s.ecs || s.pages.empty()) {
		return;
	}
	s.panX += dx;
	s.panY += dy;
	s.userNav = true;
	pages_detail::writeBoard();
	pages_detail::markActiveFromView();
}

inline void zoomPages(float factor, float pivotX, float pivotY) {
	if (!pages_detail::rt().ecs || pages_detail::rt().pages.empty()) {
		return;
	}
	pages_detail::applyZoom(factor, pivotX, pivotY);
	pages_detail::markActiveFromView();
}

inline void fitPages() {
	auto& s = pages_detail::rt();
	if (!s.ecs || s.pages.empty()) {
		return;
	}
	float minX = 0.f, minY = 0.f, maxX = 0.f, maxY = 0.f;
	if (!pages_detail::stackBounds(minX, minY, maxX, maxY)) {
		return;
	}
	const float sw = std::max(1.f, maxX - minX);
	const float sh = std::max(1.f, maxY - minY);
	const float vw = (s.viewW > 1.f) ? s.viewW : sw;
	const float vh = (s.viewH > 1.f) ? s.viewH : sh;
	const float vx = (s.viewW > 1.f) ? s.viewX : 0.f;
	const float vy = (s.viewH > 1.f) ? s.viewY : 0.f;
	s.zoom = std::clamp(std::min(vw / sw, vh / sh) * pages_detail::kFitPad,
						pages_detail::kZoomMin, pages_detail::kZoomMax);
	s.panX = vx + vw * 0.5f - (minX + maxX) * 0.5f * s.zoom;
	s.panY = vy + vh * 0.5f - (minY + maxY) * 0.5f * s.zoom;
	s.fitted = true;
	s.fitViewW = vw;
	s.fitViewH = vh;
	pages_detail::writeBoard();
	pages_detail::markActiveFromView();
}

inline void focusPage(int index) {
	auto& s = pages_detail::rt();
	if (!s.ecs || s.pages.empty()) {
		return;
	}
	const int last = static_cast<int>(s.pages.size()) - 1;
	index = std::clamp(index, 0, last);
	float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
	if (!pages_detail::pageExtent(s.pages[static_cast<size_t>(index)], x, y, w, h)) {
		return;
	}
	const float vw = (s.viewW > 1.f) ? s.viewW : w;
	const float vh = (s.viewH > 1.f) ? s.viewH : h;
	const float vx = (s.viewW > 1.f) ? s.viewX : 0.f;
	const float vy = (s.viewH > 1.f) ? s.viewY : 0.f;
	s.current = s.pages[static_cast<size_t>(index)];
	pages_detail::setActive(index);
	pages_detail::applyLayout();
	if (!pages_detail::pageExtent(s.current, x, y, w, h)) {
		return;
	}
	s.panX = vx + vw * 0.5f - (x + w * 0.5f) * s.zoom;
	s.panY = vy + vh * 0.5f - (y + h * 0.5f) * s.zoom;
	s.userNav = true;
	pages_detail::writeBoard();
}

inline void scrollPages(float dy) {
	panPages(0.f, dy);
}

inline void pollPageScroll(float wheel, bool uiWantsMouse) {
	if (uiWantsMouse || pages_detail::rt().pages.empty() || std::fabs(wheel) < 1e-4f) {
		return;
	}
	scrollPages(wheel * pages_detail::kWheelPan);
}

inline void pollPageNav(const PageNavInput& in) {
	auto& s = pages_detail::rt();
	if (in.viewW > 1.f && in.viewH > 1.f) {
		s.viewX = in.viewX;
		s.viewY = in.viewY;
		s.viewW = in.viewW;
		s.viewH = in.viewH;
	}
	if (s.pages.empty()) {
		s.dragging = false;
		return;
	}
	const bool viewReady = s.viewW > 1.f && s.viewH > 1.f;
	const bool viewMoved =
		viewReady && (std::fabs(s.viewW - s.fitViewW) > 8.f || std::fabs(s.viewH - s.fitViewH) > 8.f);
	if (viewReady && !s.userNav && (!s.fitted || viewMoved) && !s.dragging) {
		fitPages();
	}
	if (in.blocked) {
		s.dragging = false;
		return;
	}
	if (in.pan) {
		if (!s.dragging) {
			s.dragging = true;
			s.lastMouseX = in.mouseX;
			s.lastMouseY = in.mouseY;
		} else {
			panPages(in.mouseX - s.lastMouseX, in.mouseY - s.lastMouseY);
			s.lastMouseX = in.mouseX;
			s.lastMouseY = in.mouseY;
		}
	} else {
		s.dragging = false;
	}
	if (std::fabs(in.wheelH) >= 1e-4f) {
		panPages(in.wheelH * pages_detail::kWheelPan, 0.f);
	}
	if (std::fabs(in.wheel) >= 1e-4f) {
		pages_detail::applyZoom(std::pow(1.12f, in.wheel), in.mouseX, in.mouseY);
		pages_detail::markActiveFromView();
	}
}

inline void clearPages() {
	auto& s = pages_detail::rt();
	rigkit::MEcs* kept = s.ecs;
	if (kept) {
		for (auto e : s.owned) {
			if (kept->registry().valid(e)) {
				kept->destroyEntity(e);
			}
		}
	}
	pages_detail::resetState();
	s.ecs = kept;
}

inline void resetPages() {
	pages_detail::rt().ecs = nullptr;
	pages_detail::resetState();
}

} // namespace rig
