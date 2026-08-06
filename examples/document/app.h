#pragma once
#include <entt/entt.hpp>
#include <memory>
#include <string>
#include "core/U_core.h"
#include "packs/rigProject/src/rigProject.h"

class DocumentApp : public rigkit::IApp {
  public:
	DocumentApp() {
		window().width = 1100;
		window().height = 700;
		window().title = "rigProject — document";
	}
	void setup() override;
	void update(float) override;
	void draw() override {}

  private:
	void buildScene();
	void applyActiveSelection();
	void fitBoard();

	std::shared_ptr<rigkit::rigProject> m_document;
	std::string m_docPath;
	entt::entity m_root{entt::null};
	int m_phase = 0; // 0 = wait save, 1 = request load, 2 = after load, 3 = idle
};
