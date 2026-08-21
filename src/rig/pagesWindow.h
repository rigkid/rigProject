#pragma once

/**
 * @brief Pages panel — include and `createWindow<rig::PagesWindow>()`.
 * @details Needs **rigImGui** (`IWindow`). View options call `setPageLayout` /
 * `focusPage` / `fitPages` on the same helpers as `rig/pages.h`.
 */

#include "CPage.h"
#include "IWindow.h"
#include "core/RigKitEngine.h"
#include "ecs/MEcs.h"
#include "rig/pages.h"

#include <cstdio>
#include <imgui.h>

namespace rig {

class PagesWindow : public rigkit::IWindow {
  public:
	PagesWindow() : rigkit::IWindow("Pages", 0) { setCategory("Project"); }

  protected:
	void renderContents() override {
		const int n = pageCount();
		if (n <= 0) {
			ImGui::TextDisabled("No pages. Call makePage.");
			return;
		}

		int mode = 0;
		if (pageLayout() == PageLayout::Single) {
			mode = 1;
		} else if (pageLayout() == PageLayout::Spread) {
			mode = 2;
		}
		if (ImGui::RadioButton("Scroll", mode == 0)) {
			setPageLayout(PageLayout::Stack);
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("One page", mode == 1)) {
			setPageLayout(PageLayout::Single);
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Spreads", mode == 2)) {
			setPageLayout(PageLayout::Spread);
		}

		ImGui::Text("Page %d / %d   %.0f%%", activePageIndex() + 1, n, pageZoom() * 100.f);
		if (ImGui::Button("<")) {
			const int cur = activePageIndex();
			if (pageLayout() == PageLayout::Spread) {
				if (cur <= 0) {
					focusPage(0);
				} else if (cur == 1) {
					focusPage(0);
				} else {
					focusPage(cur - 2);
				}
			} else {
				focusPage(cur - 1);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button(">")) {
			const int cur = activePageIndex();
			if (pageLayout() == PageLayout::Spread) {
				if (cur <= 0) {
					focusPage(1);
				} else {
					focusPage(cur + 2);
				}
			} else {
				focusPage(cur + 1);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Fit")) {
			fitPages();
		}
		ImGui::TextUnformatted("Drag canvas to pan. Wheel zooms.");

		ImGui::Separator();
		ImGui::TextUnformatted("Pages");
		auto* engine = getEngine();
		auto* ecs = engine ? engine->getECSManager() : nullptr;
		if (ImGui::BeginChild("page-list", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
			for (int i = 0; i < n; ++i) {
				char label[160];
				const auto e = pageEntity(i);
				if (ecs && e != entt::null && ecs->hasComponent<rigkit::ecs::CPage>(e)) {
					const auto& page = ecs->getComponent<rigkit::ecs::CPage>(e);
					std::snprintf(label, sizeof(label), "%s  %.0fx%.0f###p%d", page.name.c_str(),
								  page.width, page.height, i);
				} else {
					std::snprintf(label, sizeof(label), "Page %d###p%d", i + 1, i);
				}
				if (ImGui::Selectable(label, i == activePageIndex())) {
					focusPage(i);
				}
			}
		}
		ImGui::EndChild();
	}
};

} // namespace rig
