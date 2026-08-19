#include "ContractImport.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <spdlog/spdlog.h>

#include "CArc.h"
#include "CCadBoolean.h"
#include "CCadBox.h"
#include "CCadChamfer.h"
#include "CCadCylinder.h"
#include "CCadExtrude.h"
#include "CCadFillet.h"
#include "CCadRevolve.h"
#include "CCadSphere.h"
#include "CCamera.h"
#include "CCode.h"
#include "CDrawStyle.h"
#include "CEllipse.h"
#include "CLight.h"
#include "CLine.h"
#include "CMesh.h"
#include "CModBinding.h"
#include "CModLfo.h"
#include "CMusicClock.h"
#include "CMusicTransport.h"
#include "CNurbsSurface.h"
#include "CPage.h"
#include "CPalette.h"
#include "CPath.h"
#include "CPolygon.h"
#include "CRectangle.h"
#include "CRegularPolygon.h"
#include "CRelationship.h"
#include "CRing.h"
#include "CSelectable.h"
#include "CSpline.h"
#include "CSpline3d.h"
#include "CStar.h"
#include "CTransform.h"
#include "CTween.h"
#include "EntityProperty.h"
#include "MeshFaces.h"
#include "PrimitiveBounds.h"
#include "ProjectJson.h"
#include "core/TypeJson.h"
#include "core/json.h"
#include "ecs/MEcs.h"
#include "ecs/PropertyReflection.h"
#include "rig/create.h"

namespace rigkit {
namespace project {

namespace {

using json = nlohmann::json;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

json loadFile(const std::string& path, std::string& error) {
	std::ifstream in(path);
	if (!in) {
		error = "could not open file";
		return {};
	}
	try {
		json j;
		in >> j;
		return j;
	} catch (const std::exception& e) {
		error = e.what();
		return {};
	}
}

rigkit::ecs::CDrawStyle styleFrom(const json& comps) {
	rigkit::ecs::CDrawStyle style;
	style.hasFill = false;
	style.hasStroke = false;
	style.strokeWidth = 1.f;
	if (!comps.contains("rig.paint.fill_stroke")) {
		return style;
	}
	const auto& p = comps["rig.paint.fill_stroke"];
	const bool hasFillField = p.contains("fillRgba");
	const bool hasStrokeField = p.contains("strokeRgba");
	style.hasFill = p.value("hasFill", hasFillField);
	style.hasStroke = p.value("hasStroke", hasStrokeField);
	if (hasFillField) {
		const glm::vec4 fill = rgbaFromJson(p["fillRgba"]);
		style.fillR = fill.r;
		style.fillG = fill.g;
		style.fillB = fill.b;
		style.fillA = fill.a;
	}
	if (hasStrokeField) {
		const glm::vec4 stroke = rgbaFromJson(p["strokeRgba"]);
		style.strokeR = stroke.r;
		style.strokeG = stroke.g;
		style.strokeB = stroke.b;
		style.strokeA = stroke.a;
	}
	style.strokeWidth = p.value("strokeWidth", 1.f);
	return style;
}

void applyTransform(rigkit::ecs::CTransform& t, const json& comps) {
	if (!comps.contains("rig.spatial.transform")) {
		return;
	}
	const auto& tr = comps["rig.spatial.transform"];
	if (tr.contains("position")) {
		t.position = vec3FromJson(tr["position"]);
	}
	if (tr.contains("scale")) {
		t.scale = vec3FromJson(tr["scale"], {1.f, 1.f, 1.f});
	}
	if (tr.contains("rotation")) {
		t.setRotationQuat(quatFromJson(tr["rotation"]));
	}
}

rigkit::ecs::CMesh meshFromPoints2(const std::vector<glm::vec2>& pts, bool closed) {
	rigkit::ecs::CMesh mesh;
	if (pts.size() < 2) {
		return mesh;
	}
	if (!closed || pts.size() < 3) {
		mesh.mode = rigkit::ecs::CMesh::Mode::LineStrip;
		for (const auto& p : pts) {
			mesh.positions.emplace_back(p.x, p.y, 0.f);
		}
		return mesh;
	}
	// Fan triangulation around centroid for filled stills.
	mesh.mode = rigkit::ecs::CMesh::Mode::Triangles;
	glm::vec2 c{0.f, 0.f};
	for (const auto& p : pts) {
		c += p;
	}
	c /= static_cast<float>(pts.size());
	const uint32_t center = 0;
	mesh.positions.emplace_back(c.x, c.y, 0.f);
	for (const auto& p : pts) {
		mesh.positions.emplace_back(p.x, p.y, 0.f);
	}
	const uint32_t n = static_cast<uint32_t>(pts.size());
	for (uint32_t i = 0; i < n; ++i) {
		const uint32_t a = 1 + i;
		const uint32_t b = 1 + ((i + 1) % n);
		mesh.indices.push_back(center);
		mesh.indices.push_back(a);
		mesh.indices.push_back(b);
	}
	return mesh;
}

rigkit::ecs::CMesh meshFromContract(const json& meshJson) {
	rigkit::ecs::CMesh mesh;
	const std::string mode = meshJson.value("mode", "triangles");
	if (mode == "lines") {
		mesh.mode = rigkit::ecs::CMesh::Mode::Lines;
	} else if (mode == "line-strip") {
		mesh.mode = rigkit::ecs::CMesh::Mode::LineStrip;
	} else {
		mesh.mode = rigkit::ecs::CMesh::Mode::Triangles;
	}

	const auto& positions = meshJson["positions"];
	if (positions.is_array() && !positions.empty()) {
		if (positions[0].is_number()) {
			for (size_t i = 0; i + 2 < positions.size(); i += 3) {
				mesh.positions.emplace_back(positions[i].get<float>(),
											positions[i + 1].get<float>(),
											positions[i + 2].get<float>());
			}
		} else {
			for (const auto& v : positions) {
				mesh.positions.push_back(vec3FromJson(v));
			}
		}
	}
	if (meshJson.contains("indices") && meshJson["indices"].is_array()) {
		for (const auto& i : meshJson["indices"]) {
			mesh.indices.push_back(i.get<uint32_t>());
		}
	}
	if (meshJson.contains("loops") && meshJson["loops"].is_array()) {
		for (const auto& i : meshJson["loops"]) {
			mesh.loops.push_back(i.get<uint32_t>());
		}
	}
	if (meshJson.contains("loopSizes") && meshJson["loopSizes"].is_array()) {
		for (const auto& i : meshJson["loopSizes"]) {
			mesh.loopSizes.push_back(i.get<uint32_t>());
		}
	}
	if (meshJson.contains("faceColors") && meshJson["faceColors"].is_array()) {
		for (const auto& c : meshJson["faceColors"]) {
			if (c.is_array() && c.size() >= 3) {
				mesh.faceColors.push_back(rgbaFromJson(c));
			}
		}
	}
	if (!mesh.loopSizes.empty()) {
		rigkit::ecs::meshTriangulate(mesh);
	} else {
		rigkit::ecs::meshFinishFaces(mesh);
	}
	return mesh;
}

const std::unordered_set<std::string> kKnown = {
	"rig.meta.named",
	"rig.spatial.transform",
	"rig.spatial.relationship",
	"rig.spatial.anchor",
	"rig.spatial.camera",
	"rig.spatial.group",
	"rig.spatial.layer",
	"rig.render.visibility",
	"rig.interact.selectable",
	"rig.paint.fill_stroke",
	"rig.geometry.rectangle",
	"rig.geometry.ellipse",
	"rig.geometry.line",
	"rig.geometry.polygon",
	"rig.geometry.regular_polygon",
	"rig.geometry.star",
	"rig.geometry.arc",
	"rig.geometry.spline",
	"rig.geometry.spline3d",
	"rig.geometry.nurbs_surface",
	"rig.geometry.ring",
	"rig.geometry.path",
	"rig.geometry.mesh",
	"rig.cad.box",
	"rig.cad.cylinder",
	"rig.cad.sphere",
	"rig.cad.extrude",
	"rig.cad.revolve",
	"rig.cad.boolean",
	"rig.cad.fillet",
	"rig.cad.chamfer",
	"rig.mod.lfo",
	"rig.mod.binding",
	"rig.anim.tween",
	"rig.music.clock",
	"rig.music.transport",
	"rig.paint.solid",
	"rig.ui.panel",
	"rig.ui.group",
	"rig.ui.control",
	"rig.ui.action",
	"rig.render.material",
	"rig.render.light",
	"rig.media.code",
	"rig.layout.page",
	"rig.pixel.palette",
	"rig.media.text",
	"rig.media.asset_ref",
	"rig.anim.curve",
	"x.rigkit.layer_visible",
	"x.rigkit.stroke_style",
	"x.rigkit.light_shading",
	"x.rigkit.palette_shade",
};

/// Any geometry schema at all — the paint-only fallback must not fire when the
/// entity already carries something drawable.
bool hasGeometry(const json& comps) {
	for (auto it = comps.begin(); it != comps.end(); ++it) {
		if (it.key().rfind("rig.geometry.", 0) == 0 || it.key().rfind("rig.cad.", 0) == 0) {
			return true;
		}
	}
	return false;
}

void readContractEdges(const json& j, std::vector<rigkit::ecs::MeshEdge>& edges) {
	edges.clear();
	if (!j.contains("edges") || !j["edges"].is_array()) {
		return;
	}
	for (const auto& item : j["edges"]) {
		if (item.is_object()) {
			edges.push_back(rigkit::ecs::meshEdge(item.value("a", 0u), item.value("b", 0u)));
		}
	}
}

bool importCadAndNurbs(rigkit::MEcs& ecs, entt::entity entity, const json& comps) {
	bool wrote = false;
	if (comps.contains("rig.geometry.spline3d")) {
		const auto& sp = comps["rig.geometry.spline3d"];
		rigkit::ecs::CSpline3d shape;
		shape.degree = sp.value("degree", 3);
		shape.closed = sp.value("closed", false);
		if (sp.contains("controlPoints") && sp["controlPoints"].is_array()) {
			for (const auto& p : sp["controlPoints"]) {
				shape.controlPoints.push_back(vec3FromJson(p));
			}
		}
		if (sp.contains("knots") && sp["knots"].is_array()) {
			for (const auto& k : sp["knots"]) {
				shape.knots.push_back(k.get<float>());
			}
		}
		if (sp.contains("weights") && sp["weights"].is_array()) {
			for (const auto& w : sp["weights"]) {
				shape.weights.push_back(w.get<float>());
			}
		}
		if (sp.contains("fitPoints") && sp["fitPoints"].is_array()) {
			for (const auto& p : sp["fitPoints"]) {
				shape.fitPoints.push_back(vec3FromJson(p));
			}
		}
		ecs.addComponent(entity, std::move(shape));
		wrote = true;
	}
	if (comps.contains("rig.geometry.nurbs_surface")) {
		const auto& n = comps["rig.geometry.nurbs_surface"];
		rigkit::ecs::CNurbsSurface s;
		s.degreeU = n.value("degreeU", 3);
		s.degreeV = n.value("degreeV", 3);
		s.countU = n.value("countU", 0);
		s.countV = n.value("countV", 0);
		s.closedU = n.value("closedU", false);
		s.closedV = n.value("closedV", false);
		if (n.contains("controlPoints") && n["controlPoints"].is_array()) {
			for (const auto& p : n["controlPoints"]) {
				s.controlPoints.push_back(vec3FromJson(p));
			}
		}
		if (n.contains("knotsU") && n["knotsU"].is_array()) {
			for (const auto& k : n["knotsU"]) {
				s.knotsU.push_back(k.get<float>());
			}
		}
		if (n.contains("knotsV") && n["knotsV"].is_array()) {
			for (const auto& k : n["knotsV"]) {
				s.knotsV.push_back(k.get<float>());
			}
		}
		if (n.contains("weights") && n["weights"].is_array()) {
			for (const auto& w : n["weights"]) {
				s.weights.push_back(w.get<float>());
			}
		}
		ecs.addComponent(entity, std::move(s));
		wrote = true;
	}
	if (comps.contains("rig.cad.box")) {
		const auto& b = comps["rig.cad.box"];
		rigkit::ecs::CCadBox s;
		s.sizeX = b.value("sizeX", 1.f);
		s.sizeY = b.value("sizeY", 1.f);
		s.sizeZ = b.value("sizeZ", 1.f);
		s.center = b.value("center", true);
		ecs.addComponent(entity, s);
		wrote = true;
	}
	if (comps.contains("rig.cad.cylinder")) {
		const auto& c = comps["rig.cad.cylinder"];
		rigkit::ecs::CCadCylinder s;
		s.radius = c.value("radius", 1.f);
		s.height = c.value("height", 1.f);
		s.circularSegments = c.value("circularSegments", 0);
		s.center = c.value("center", true);
		ecs.addComponent(entity, s);
		wrote = true;
	}
	if (comps.contains("rig.cad.sphere")) {
		const auto& c = comps["rig.cad.sphere"];
		rigkit::ecs::CCadSphere s;
		s.radius = c.value("radius", 1.f);
		s.circularSegments = c.value("circularSegments", 0);
		ecs.addComponent(entity, s);
		wrote = true;
	}
	if (comps.contains("rig.cad.extrude")) {
		const auto& x = comps["rig.cad.extrude"];
		rigkit::ecs::CCadExtrude s;
		if (x.contains("profile") && x["profile"].is_string()) {
			s.profile = x["profile"].get<std::string>();
		}
		s.height = x.value("height", 1.f);
		s.nDivisions = x.value("nDivisions", 0);
		s.twistDegrees = x.value("twistDegrees", 0.f);
		s.scaleTop = x.value("scaleTop", 1.f);
		ecs.addComponent(entity, s);
		wrote = true;
	}
	if (comps.contains("rig.cad.revolve")) {
		const auto& r = comps["rig.cad.revolve"];
		rigkit::ecs::CCadRevolve s;
		if (r.contains("profile") && r["profile"].is_string()) {
			s.profile = r["profile"].get<std::string>();
		}
		s.revolveDegrees = r.value("revolveDegrees", 360.f);
		s.circularSegments = r.value("circularSegments", 0);
		ecs.addComponent(entity, s);
		wrote = true;
	}
	if (comps.contains("rig.cad.boolean")) {
		const auto& b = comps["rig.cad.boolean"];
		rigkit::ecs::CCadBoolean s;
		const std::string op = b.value("op", std::string("difference"));
		if (op == "union") {
			s.op = rigkit::ecs::CCadBoolean::Op::Union;
		} else if (op == "intersection") {
			s.op = rigkit::ecs::CCadBoolean::Op::Intersection;
		} else {
			s.op = rigkit::ecs::CCadBoolean::Op::Difference;
		}
		if (b.contains("operands") && b["operands"].is_array()) {
			for (const auto& id : b["operands"]) {
				if (id.is_string()) {
					s.operands.push_back(id.get<std::string>());
				}
			}
		}
		ecs.addComponent(entity, std::move(s));
		wrote = true;
	}
	if (comps.contains("rig.cad.fillet")) {
		const auto& f = comps["rig.cad.fillet"];
		rigkit::ecs::CCadFillet s;
		s.radius = f.value("radius", 1.f);
		s.allEdges = f.value("allEdges", false);
		if (!s.allEdges) {
			readContractEdges(f, s.edges);
		}
		ecs.addComponent(entity, std::move(s));
	}
	if (comps.contains("rig.cad.chamfer")) {
		const auto& c = comps["rig.cad.chamfer"];
		rigkit::ecs::CCadChamfer s;
		s.distance = c.value("distance", 1.f);
		s.allEdges = c.value("allEdges", false);
		if (!s.allEdges) {
			readContractEdges(c, s.edges);
		}
		ecs.addComponent(entity, std::move(s));
	}
	return wrote;
}

void aimDirectionalAtOrigin(rigkit::ecs::CTransform& transform) {
	const float len = glm::length(transform.position);
	if (len < 1e-4f) {
		return;
	}
	const glm::vec3 zAxis = transform.position / len;
	glm::vec3 up(0.f, 1.f, 0.f);
	if (std::fabs(glm::dot(zAxis, up)) > 0.95f) {
		up = glm::vec3(1.f, 0.f, 0.f);
	}
	const glm::vec3 xAxis = glm::normalize(glm::cross(up, zAxis));
	const glm::vec3 yAxis = glm::cross(zAxis, xAxis);
	transform.rotation = glm::normalize(glm::quat_cast(glm::mat3(xAxis, yAxis, zAxis)));
}

/// Nine Contract cells onto `CPage::originAnchor` (same order as page codecs).
int originAnchorFromId(const std::string& id) {
	static const char* const kIds[] = {"top-left",	  "top-center",	   "top-right",
									   "middle-left", "center",		   "middle-right",
									   "bottom-left", "bottom-center", "bottom-right"};
	for (int i = 0; i < 9; ++i) {
		if (id == kIds[i]) {
			return i;
		}
	}
	return 0;
}

void readPageEdges(const json& j, const char* key, float& top, float& right, float& bottom,
				   float& left) {
	if (!j.contains(key) || !j[key].is_array() || j[key].size() < 4) {
		return;
	}
	top = j[key][0].get<float>();
	right = j[key][1].get<float>();
	bottom = j[key][2].get<float>();
	left = j[key][3].get<float>();
}

void applyMaterialAlbedo(rigkit::ecs::CDrawStyle& style, const json& comps) {
	if (!comps.contains("rig.render.material")) {
		return;
	}
	const auto& m = comps["rig.render.material"];
	if (!m.contains("albedoRgb") || !m["albedoRgb"].is_array() || m["albedoRgb"].size() < 3) {
		return;
	}
	const glm::vec3 albedo = rgbFromJson(m["albedoRgb"]);
	style.hasFill = true;
	style.fillR = albedo.r;
	style.fillG = albedo.g;
	style.fillB = albedo.b;
	style.fillA = 1.f;
	if (m.contains("emissive")) {
		const glm::vec3 em = rgbFromJson(m["emissive"]);
		style.fillR = std::min(1.f, style.fillR + em.r);
		style.fillG = std::min(1.f, style.fillG + em.g);
		style.fillB = std::min(1.f, style.fillB + em.b);
	}
	style.hasStroke = false;
}

entt::entity resolveEntity(const ContractImportResult& doc, const std::string& entityId) {
	auto it = doc.entities.find(entityId);
	if (it == doc.entities.end()) {
		return entt::null;
	}
	return it->second;
}

ContractImportResult importContractJsonImpl(rigkit::MEcs& ecs, const std::string& jsonText,
											const std::string& sourceLabel,
											const ComponentSerializerRegistry* codecs) {
	ContractImportResult result;
	json root;
	try {
		root = json::parse(jsonText);
	} catch (const std::exception& e) {
		result.error = e.what();
		return result;
	}

	if (!root.contains("rig")) {
		result.error = "missing required \"rig\" version field";
		return result;
	}

	ecs.clear();
	result.title = root.value("document", json::object())
					   .value("title", sourceLabel.empty() ? "Rig document" : sourceLabel);

	if (!root.contains("entities") || !root["entities"].is_array()) {
		result.ok = true;
		return result;
	}

	std::unordered_map<std::string, entt::entity> idMap;
	std::vector<std::pair<entt::entity, std::string>> pendingParents;

	for (const auto& ent : root["entities"]) {
		const std::string id = ent.value("id", "");
		const json comps = ent.value("components", json::object());

		for (auto it = comps.begin(); it != comps.end(); ++it) {
			if (!kKnown.count(it.key())) {
				result.skipped.push_back(it.key());
			}
		}

		if (comps.contains("rig.render.visibility") &&
			comps["rig.render.visibility"].value("visible", true) == false) {
			continue;
		}

		std::string name = id;
		if (comps.contains("rig.meta.named")) {
			name = comps["rig.meta.named"].value("name", id);
		}

		auto entity = ecs.createEntity(name);
		if (!id.empty()) {
			idMap[id] = entity;
			result.entities[id] = entity;
		}
		++result.entityCount;

		if (!codecs) {
			if (comps.contains("rig.mod.lfo")) {
				const auto& lfo = comps["rig.mod.lfo"];
				rigkit::ecs::CModLfo mod;
				mod.waveform = lfo.value("waveform", "sine");
				mod.frequency = lfo.value("frequency", 0.f);
				mod.amplitude = lfo.value("amplitude", 1.f);
				mod.offset = lfo.value("offset", 0.f);
				mod.phase = lfo.value("phase", 0.f);
				ecs.addComponent(entity, mod);
			}
			if (comps.contains("rig.mod.binding")) {
				const auto& b = comps["rig.mod.binding"];
				rigkit::ecs::CModBinding bind;
				bind.source = b.value("source", "");
				bind.target = b.value("target", "");
				bind.propertyKey = b.value("propertyKey", "");
				bind.depth = b.value("depth", 1.f);
				bind.additive = b.value("additive", false);
				if (b.contains("min")) {
					bind.hasMin = true;
					bind.min = b["min"].get<float>();
				}
				if (b.contains("max")) {
					bind.hasMax = true;
					bind.max = b["max"].get<float>();
				}
				ecs.addComponent(entity, bind);
			}
			if (comps.contains("rig.anim.tween")) {
				const auto& t = comps["rig.anim.tween"];
				rigkit::ecs::CTween tw;
				tw.target = t.value("target", "");
				tw.propertyKey = t.value("propertyKey", "");
				tw.from = t.value("from", 0.f);
				tw.to = t.value("to", 1.f);
				tw.duration = t.value("duration", 1.f);
				tw.elapsed = t.value("elapsed", 0.f);
				tw.easing = t.value("easing", "linear");
				tw.loop = t.value("loop", false);
				tw.playing = t.value("playing", true);
				ecs.addComponent(entity, tw);
			}
			if (comps.contains("rig.music.clock")) {
				const auto& c = comps["rig.music.clock"];
				rigkit::ecs::CMusicClock clock;
				clock.ticksPerQuarter = c.value("ticksPerQuarter", clock.ticksPerQuarter);
				clock.phaseTicks = c.value("phaseTicks", clock.phaseTicks);
				clock.swingAmount = c.value("swingAmount", clock.swingAmount);
				clock.swingSubdiv = c.value("swingSubdiv", clock.swingSubdiv);
				clock.externalSync = c.value("externalSync", clock.externalSync);
				clock.syncBeat = c.value("syncBeat", clock.syncBeat);
				clock.syncPhase = c.value("syncPhase", clock.syncPhase);
				clock.syncPeriodBars = c.value("syncPeriodBars", clock.syncPeriodBars);
				ecs.addComponent(entity, clock);
			}
			if (comps.contains("rig.music.transport")) {
				const auto& t = comps["rig.music.transport"];
				rigkit::ecs::CMusicTransport tr;
				tr.playing = t.value("playing", tr.playing);
				tr.bpm = t.value("bpm", tr.bpm);
				tr.timeSigNum = t.value("timeSigNum", tr.timeSigNum);
				tr.timeSigDen = t.value("timeSigDen", tr.timeSigDen);
				tr.positionBeats = t.value("positionBeats", tr.positionBeats);
				tr.loop = t.value("loop", tr.loop);
				tr.loopStartBeats = t.value("loopStartBeats", tr.loopStartBeats);
				tr.loopEndBeats = t.value("loopEndBeats", tr.loopEndBeats);
				ecs.addComponent(entity, tr);
			}

			if (comps.contains("rig.layout.page")) {
				const auto& p = comps["rig.layout.page"];
				rigkit::ecs::CPage page;
				page.name = name;
				page.index = p.value("index", page.index);
				page.unit = p.value("unit", page.unit);
				page.width = p.value("width", page.width);
				page.height = p.value("height", page.height);
				readPageEdges(p, "margins", page.marginTop, page.marginRight, page.marginBottom,
							  page.marginLeft);
				readPageEdges(p, "bleed", page.bleedTop, page.bleedRight, page.bleedBottom,
							  page.bleedLeft);
				readPageEdges(p, "slug", page.slugTop, page.slugRight, page.slugBottom,
							  page.slugLeft);
				if (comps.contains("rig.spatial.anchor")) {
					const auto& anchor = comps["rig.spatial.anchor"];
					if (anchor.contains("point") && anchor["point"].is_string()) {
						page.originAnchor = originAnchorFromId(anchor["point"].get<std::string>());
					}
				}
				ecs.addComponent(entity, page);
			}

			if (comps.contains("rig.pixel.palette")) {
				const auto& pal = comps["rig.pixel.palette"];
				rigkit::ecs::CPalette palette = rigkit::ecs::CPalette::default16();
				if (pal.contains("colors") && pal["colors"].is_array()) {
					const size_t n = std::min(pal["colors"].size(),
											  static_cast<size_t>(rigkit::ecs::CPalette::kCount));
					for (size_t i = 0; i < n; ++i) {
						const glm::vec4 c = rgbaFromJson(pal["colors"][i]);
						palette.colors[i] = c;
					}
				}
				ecs.addComponent(entity, palette);
			}
		} // !codecs — POD blobs come from applyDeserializers when a registry is passed

		if (comps.contains("rig.ui.panel")) {
			const auto& p = comps["rig.ui.panel"];
			ContractImportResult::Panel panel;
			panel.id = id;
			panel.name = name;
			panel.role = p.value("role", "");
			panel.order = p.value("order", 0);
			panel.visible = p.value("visible", true);
			panel.preferredWidth = p.value("preferredWidth", 320.f);
			panel.preferredHeight = p.value("preferredHeight", 240.f);
			result.panels.push_back(panel);
		}
		if (comps.contains("rig.ui.group")) {
			const auto& g = comps["rig.ui.group"];
			ContractImportResult::Group group;
			group.id = id;
			group.name = name;
			group.panel = g.value("panel", "");
			if (g.contains("parent") && g["parent"].is_string()) {
				group.parent = g["parent"].get<std::string>();
			}
			group.order = g.value("order", 0);
			group.orientation = g.value("orientation", "vertical");
			group.collapsed = g.value("collapsed", false);
			result.groups.push_back(group);
		}
		if (comps.contains("rig.ui.control")) {
			const auto& c = comps["rig.ui.control"];
			ContractImportResult::Control ctrl;
			ctrl.id = id;
			ctrl.name = name;
			ctrl.panel = c.value("panel", "");
			if (c.contains("group") && c["group"].is_string()) {
				ctrl.group = c["group"].get<std::string>();
			}
			ctrl.order = c.value("order", 0);
			ctrl.target = c.value("target", "");
			ctrl.propertyKey = c.value("propertyKey", "");
			ctrl.type = c.value("type", "float");
			if (c.contains("min")) {
				ctrl.min = c["min"].get<float>();
			}
			if (c.contains("max")) {
				ctrl.max = c["max"].get<float>();
			}
			if (c.contains("step")) {
				ctrl.step = c["step"].get<float>();
			}
			ctrl.enabled = c.value("enabled", true);
			ctrl.readOnly = c.value("readOnly", false);
			ctrl.widget = c.value("widget", "auto");
			if (c.contains("options") && c["options"].is_array()) {
				for (const auto& opt : c["options"]) {
					if (opt.is_string()) {
						ctrl.options.push_back(opt.get<std::string>());
					}
				}
			}
			result.controls.push_back(ctrl);
		}
		if (comps.contains("rig.ui.action")) {
			const auto& a = comps["rig.ui.action"];
			ContractImportResult::Action act;
			act.id = id;
			act.name = name;
			act.panel = a.value("panel", "");
			if (a.contains("group") && a["group"].is_string()) {
				act.group = a["group"].get<std::string>();
			}
			act.order = a.value("order", 0);
			act.actionId = a.value("actionId", "");
			act.enabled = a.value("enabled", true);
			result.actions.push_back(act);
		}

		if (codecs) {
			ordered_json blob = comps;
			codecs->applyDeserializers(ecs.registry(), entity, blob, {"rig.spatial.relationship"});
			if (ecs.hasComponent<rigkit::ecs::CPage>(entity)) {
				ecs.getComponent<rigkit::ecs::CPage>(entity).name = name;
			}
			if (!ecs.hasComponent<rigkit::ecs::CTransform>(entity)) {
				rigkit::ecs::CTransform transform;
				applyTransform(transform, comps);
				ecs.addComponent(entity, transform);
			}
			if (comps.contains("rig.spatial.relationship")) {
				const auto& rel = comps["rig.spatial.relationship"];
				if (rel.contains("parent") && rel["parent"].is_string()) {
					const auto parentId = rel["parent"].get<std::string>();
					if (!parentId.empty()) {
						pendingParents.emplace_back(entity, parentId);
					}
				}
			}
			if (comps.contains("rig.media.code") && !ecs.hasComponent<rigkit::ecs::CCode>(entity)) {
				const auto& code = comps["rig.media.code"];
				rigkit::ecs::CCode buffer;
				buffer.name = name;
				buffer.text = code.value("text", "");
				buffer.language = code.value("language", "");
				buffer.readOnly = code.value("readOnly", false);
				ecs.addComponent(entity, buffer);
			}

			auto style = styleFrom(comps);
			applyMaterialAlbedo(style, comps);
			if (comps.contains("rig.paint.solid") && !hasGeometry(comps)) {
				const auto& solid = comps["rig.paint.solid"];
				rigkit::ecs::CEllipse shape;
				shape.rx = 80.f;
				shape.ry = 80.f;
				ecs.addComponent(entity, shape);
				if (solid.contains("rgba") && solid["rgba"].is_array() &&
					solid["rgba"].size() >= 3) {
					const glm::vec4 rgba = rgbaFromJson(solid["rgba"]);
					style.hasFill = true;
					style.fillR = rgba.r;
					style.fillG = rgba.g;
					style.fillB = rgba.b;
					style.fillA = rgba.a;
					style.hasStroke = false;
				}
				if (!comps.contains("rig.spatial.transform")) {
					auto& xf = ecs.getComponent<rigkit::ecs::CTransform>(entity);
					xf.position = {320.f + static_cast<float>(result.geometryCount) * 200.f, 280.f,
								   0.f};
				}
				ecs.addComponent(entity, style);
				++result.geometryCount;
				result.notes.push_back(id + ": paint.solid presented as LED ellipse");
			} else if (hasGeometry(comps)) {
				if (comps.contains("rig.render.material")) {
					auto& d = ecs.registry().get_or_emplace<rigkit::ecs::CDrawStyle>(entity);
					d = style;
				}
				++result.geometryCount;
			}
			if (ecs.hasComponent<rigkit::ecs::CLight>(entity)) {
				auto& L = ecs.getComponent<rigkit::ecs::CLight>(entity);
				if (L.type == rigkit::ecs::CLight::Type::Directional) {
					auto& xf = ecs.getComponent<rigkit::ecs::CTransform>(entity);
					aimDirectionalAtOrigin(xf);
				}
			}
			continue;
		}

		rigkit::ecs::CTransform transform;
		applyTransform(transform, comps);
		ecs.addComponent(entity, transform);

		if (comps.contains("rig.spatial.relationship")) {
			const auto parentId = comps["rig.spatial.relationship"].value("parent", "");
			if (!parentId.empty()) {
				pendingParents.emplace_back(entity, parentId);
			}
		}

		if (comps.contains("rig.interact.selectable")) {
			rigkit::ecs::CSelectable sel;
			sel.enabled = comps["rig.interact.selectable"].value("enabled", true);
			ecs.addComponent(entity, sel);
		}

		if (comps.contains("rig.spatial.camera")) {
			const auto& cam = comps["rig.spatial.camera"];
			rigkit::ecs::CCamera camera;
			camera.active = cam.value("active", true);
			const std::string proj = cam.value("projection", "perspective");
			camera.projection = (proj == "orthographic")
									? rigkit::ecs::CCamera::Projection::Orthographic
									: rigkit::ecs::CCamera::Projection::Perspective;
			camera.fovYDegrees = cam.value("fovYDegrees", 60.f);
			camera.orthoHeight = cam.value("orthoHeight", 10.f);
			camera.nearClip = cam.value("nearClip", 0.1f);
			camera.farClip = cam.value("farClip", 1000.f);
			camera.aspect = cam.value("aspect", 0.f);
			ecs.addComponent(entity, camera);
		}

		if (comps.contains("rig.media.code")) {
			const auto& code = comps["rig.media.code"];
			rigkit::ecs::CCode buffer;
			buffer.name = name;
			buffer.text = code.value("text", "");
			buffer.language = code.value("language", "");
			buffer.readOnly = code.value("readOnly", false);
			ecs.addComponent(entity, buffer);
		}

		auto style = styleFrom(comps);
		applyMaterialAlbedo(style, comps);
		bool wroteGeometry = false;

		if (comps.contains("rig.render.light")) {
			const auto& light = comps["rig.render.light"];
			rigkit::ecs::CLight L;
			L.enabled = light.value("enabled", true);
			const std::string type = light.value("type", "directional");
			L.type = (type == "point") ? rigkit::ecs::CLight::Type::Point
									   : rigkit::ecs::CLight::Type::Directional;
			if (light.contains("rgb")) {
				const glm::vec3 rgb = rgbFromJson(light["rgb"], {L.colorR, L.colorG, L.colorB});
				L.colorR = rgb.r;
				L.colorG = rgb.g;
				L.colorB = rgb.b;
			}
			L.intensity = light.value("intensity", 1.f);
			L.ambient = light.value("ambient", 0.35f);
			// Contract / web path is smooth shade; Kit default banded is for palette look.
			L.banded = light.value("banded", false);
			L.dither = light.value("dither", false);
			if (L.type == rigkit::ecs::CLight::Type::Directional) {
				aimDirectionalAtOrigin(transform);
				ecs.getComponent<rigkit::ecs::CTransform>(entity) = transform;
			}
			ecs.addComponent(entity, L);
		}

		// Paint-only docs: synthesize an LED ellipse so the desktop window isn't blank.
		if (comps.contains("rig.paint.solid") && !hasGeometry(comps)) {
			const auto& solid = comps["rig.paint.solid"];
			rigkit::ecs::CEllipse shape;
			shape.rx = 80.f;
			shape.ry = 80.f;
			ecs.addComponent(entity, shape);
			if (solid.contains("rgba") && solid["rgba"].is_array() && solid["rgba"].size() >= 3) {
				const glm::vec4 rgba = rgbaFromJson(solid["rgba"]);
				style.hasFill = true;
				style.fillR = rgba.r;
				style.fillG = rgba.g;
				style.fillB = rgba.b;
				style.fillA = rgba.a;
				style.hasStroke = false;
			}
			if (!comps.contains("rig.spatial.transform")) {
				transform.position = {320.f + static_cast<float>(result.geometryCount) * 200.f,
									  280.f, 0.f};
				ecs.getComponent<rigkit::ecs::CTransform>(entity) = transform;
			}
			wroteGeometry = true;
			result.notes.push_back(id + ": paint.solid presented as LED ellipse");
		} else if (comps.contains("rig.geometry.rectangle")) {
			const auto& r = comps["rig.geometry.rectangle"];
			rigkit::ecs::CRectangle shape;
			shape.x = r.value("x", 0.f);
			shape.y = r.value("y", 0.f);
			shape.width = r.value("width", 0.f);
			shape.height = r.value("height", 0.f);
			shape.cornerRadius = r.value("cornerRadius", 0.f);
			ecs.addComponent(entity, shape);
			wroteGeometry = true;
		} else if (comps.contains("rig.geometry.ellipse")) {
			const auto& el = comps["rig.geometry.ellipse"];
			rigkit::ecs::CEllipse shape;
			shape.cx = el.value("cx", 0.f);
			shape.cy = el.value("cy", 0.f);
			shape.rx = el.value("rx", 0.f);
			shape.ry = el.value("ry", 0.f);
			ecs.addComponent(entity, shape);
			wroteGeometry = true;
		} else if (comps.contains("rig.geometry.line")) {
			const auto& ln = comps["rig.geometry.line"];
			rigkit::ecs::CLine shape;
			shape.x1 = ln.value("x1", 0.f);
			shape.y1 = ln.value("y1", 0.f);
			shape.x2 = ln.value("x2", 0.f);
			shape.y2 = ln.value("y2", 0.f);
			ecs.addComponent(entity, shape);
			wroteGeometry = true;
		} else if (comps.contains("rig.geometry.regular_polygon")) {
			const auto& ngon = comps["rig.geometry.regular_polygon"];
			rigkit::ecs::CRegularPolygon shape;
			shape.cx = ngon.value("cx", 0.f);
			shape.cy = ngon.value("cy", 0.f);
			shape.radius = ngon.value("radius", 1.f);
			shape.sides = ngon.value("sides", 3);
			shape.rotationDegrees = ngon.value("rotationDegrees", 0.f);
			ecs.addComponent(entity, shape);
			wroteGeometry = true;
		} else if (comps.contains("rig.geometry.star")) {
			const auto& star = comps["rig.geometry.star"];
			rigkit::ecs::CStar shape;
			shape.cx = star.value("cx", 0.f);
			shape.cy = star.value("cy", 0.f);
			shape.radius = star.value("radius", 1.f);
			shape.innerRadius = star.value("innerRadius", shape.radius * 0.5f);
			shape.points = star.value("points", 5);
			shape.rotationDegrees = star.value("rotationDegrees", 0.f);
			ecs.addComponent(entity, shape);
			wroteGeometry = true;
		} else if (comps.contains("rig.geometry.polygon")) {
			const auto& poly = comps["rig.geometry.polygon"];
			rigkit::ecs::CPolygon shape;
			if (poly.contains("points") && poly["points"].is_array()) {
				for (const auto& p : poly["points"]) {
					if (p.is_array() && p.size() >= 2) {
						shape.points.push_back(vec2FromJson(p));
					}
				}
			}
			shape.closed = poly.value("closed", true);
			ecs.addComponent(entity, std::move(shape));
			wroteGeometry = true;
		} else if (comps.contains("rig.geometry.arc")) {
			const auto& arc = comps["rig.geometry.arc"];
			rigkit::ecs::CArc shape;
			shape.cx = arc.value("cx", 0.f);
			shape.cy = arc.value("cy", 0.f);
			if (arc.contains("rx") || arc.contains("ry")) {
				shape.rx = arc.value("rx", arc.value("radius", 1.f));
				shape.ry = arc.value("ry", arc.value("radius", 1.f));
			} else {
				shape.setRadius(arc.value("radius", 1.f));
			}
			shape.startAngleDegrees = arc.value("startAngleDegrees", 0.f);
			shape.endAngleDegrees = arc.value("endAngleDegrees", 90.f);
			shape.pie = arc.value("pie", false);
			ecs.addComponent(entity, shape);
			wroteGeometry = true;
		} else if (comps.contains("rig.geometry.spline")) {
			const auto& sp = comps["rig.geometry.spline"];
			rigkit::ecs::CSpline shape;
			shape.degree = sp.value("degree", 3);
			shape.closed = sp.value("closed", false);
			if (sp.contains("controlPoints") && sp["controlPoints"].is_array()) {
				for (const auto& p : sp["controlPoints"]) {
					if (p.is_array() && p.size() >= 2) {
						shape.controlPoints.push_back(vec2FromJson(p));
					}
				}
			}
			if (sp.contains("knots") && sp["knots"].is_array()) {
				for (const auto& k : sp["knots"]) {
					shape.knots.push_back(k.get<float>());
				}
			}
			if (sp.contains("weights") && sp["weights"].is_array()) {
				for (const auto& w : sp["weights"]) {
					shape.weights.push_back(w.get<float>());
				}
			}
			if (sp.contains("fitPoints") && sp["fitPoints"].is_array()) {
				for (const auto& p : sp["fitPoints"]) {
					if (p.is_array() && p.size() >= 2) {
						shape.fitPoints.push_back(vec2FromJson(p));
					}
				}
			}
			ecs.addComponent(entity, std::move(shape));
			wroteGeometry = true;
		} else if (comps.contains("rig.geometry.ring")) {
			const auto& ring = comps["rig.geometry.ring"];
			rigkit::ecs::CRing shape;
			shape.cx = ring.value("cx", 0.f);
			shape.cy = ring.value("cy", 0.f);
			shape.outerRadius = ring.value("outerRadius", 1.f);
			shape.innerRadius = ring.value("innerRadius", 0.5f);
			ecs.addComponent(entity, shape);
			wroteGeometry = true;
		} else if (comps.contains("rig.geometry.path")) {
			const auto& path = comps["rig.geometry.path"];
			std::vector<glm::vec2> pts;
			float cx = 0.f;
			float cy = 0.f;
			bool closed = false;
			if (path.contains("commands") && path["commands"].is_array()) {
				for (const auto& cmd : path["commands"]) {
					const std::string type = cmd.value("type", "");
					if (type == "move-to" && cmd.contains("point")) {
						const glm::vec2 p = vec2FromJson(cmd["point"], {cx, cy});
						cx = p.x;
						cy = p.y;
						pts.push_back(p);
					} else if (type == "line-to" && cmd.contains("point")) {
						const glm::vec2 p = vec2FromJson(cmd["point"], {cx, cy});
						cx = p.x;
						cy = p.y;
						pts.push_back(p);
					} else if (type == "cubic-to") {
						const glm::vec2 c1 = cmd.contains("control1")
												 ? vec2FromJson(cmd["control1"], {cx, cy})
												 : glm::vec2{cx, cy};
						const glm::vec2 c2 = cmd.contains("control2")
												 ? vec2FromJson(cmd["control2"], {cx, cy})
												 : glm::vec2{cx, cy};
						const glm::vec2 p = cmd.contains("point")
												? vec2FromJson(cmd["point"], {cx, cy})
												: glm::vec2{cx, cy};
						const float x0 = cx;
						const float y0 = cy;
						for (int i = 1; i <= 12; ++i) {
							const float t = static_cast<float>(i) / 12.f;
							const float u = 1.f - t;
							const float x = u * u * u * x0 + 3 * u * u * t * c1.x +
											3 * u * t * t * c2.x + t * t * t * p.x;
							const float y = u * u * u * y0 + 3 * u * u * t * c1.y +
											3 * u * t * t * c2.y + t * t * t * p.y;
							pts.emplace_back(x, y);
						}
						cx = p.x;
						cy = p.y;
					} else if (type == "quad-to") {
						const glm::vec2 c1 = cmd.contains("control1")
												 ? vec2FromJson(cmd["control1"], {cx, cy})
												 : glm::vec2{cx, cy};
						const glm::vec2 p = cmd.contains("point")
												? vec2FromJson(cmd["point"], {cx, cy})
												: glm::vec2{cx, cy};
						const float x0 = cx;
						const float y0 = cy;
						for (int i = 1; i <= 10; ++i) {
							const float t = static_cast<float>(i) / 10.f;
							const float u = 1.f - t;
							const float x = u * u * x0 + 2 * u * t * c1.x + t * t * p.x;
							const float y = u * u * y0 + 2 * u * t * c1.y + t * t * p.y;
							pts.emplace_back(x, y);
						}
						cx = p.x;
						cy = p.y;
					} else if (type == "close") {
						closed = true;
					}
				}
			}
			ecs.addComponent(entity, meshFromPoints2(pts, closed));
			result.notes.push_back(id + ": path tessellated to mesh");
			wroteGeometry = true;
		} else if (comps.contains("rig.geometry.mesh")) {
			ecs.addComponent(entity, meshFromContract(comps["rig.geometry.mesh"]));
			wroteGeometry = true;
		}

		if (importCadAndNurbs(ecs, entity, comps)) {
			wroteGeometry = true;
		}

		if (wroteGeometry) {
			ecs.addComponent(entity, style);
			++result.geometryCount;
		}
	}

	for (const auto& [entity, parentId] : pendingParents) {
		auto it = idMap.find(parentId);
		if (it == idMap.end()) {
			continue;
		}
		rigkit::ecs::CRelationship rel;
		rel.parent = it->second;
		ecs.addComponent(entity, rel);
	}

	// Remap binding/tween entity ids to live entity names.
	auto remapEntityKey = [&](std::string& key) {
		auto it = idMap.find(key);
		if (it == idMap.end()) {
			return;
		}
		const std::string n = ecs.entityName(it->second);
		if (!n.empty()) {
			key = n;
		}
	};
	for (auto e : ecs.view<rigkit::ecs::CModBinding>()) {
		auto& b = ecs.getComponent<rigkit::ecs::CModBinding>(e);
		remapEntityKey(b.source);
		remapEntityKey(b.target);
	}
	for (auto e : ecs.view<rigkit::ecs::CTween>()) {
		remapEntityKey(ecs.getComponent<rigkit::ecs::CTween>(e).target);
	}
	for (auto e : ecs.view<rigkit::ecs::CCadBoolean>()) {
		for (auto& id : ecs.getComponent<rigkit::ecs::CCadBoolean>(e).operands) {
			remapEntityKey(id);
		}
	}
	for (auto e : ecs.view<rigkit::ecs::CCadExtrude>()) {
		remapEntityKey(ecs.getComponent<rigkit::ecs::CCadExtrude>(e).profile);
	}
	for (auto e : ecs.view<rigkit::ecs::CCadRevolve>()) {
		remapEntityKey(ecs.getComponent<rigkit::ecs::CCadRevolve>(e).profile);
	}

	// Contract docs often leave camera rotation as identity; host view is local -Z.
	// Web always lookAt(scene). Aim identity cameras at the imported geometry AABB.
	{
		glm::vec3 bmin(0.f);
		glm::vec3 bmax(0.f);
		bool any = false;
		auto expand = [&](const glm::vec3& p) {
			if (!any) {
				bmin = bmax = p;
				any = true;
				return;
			}
			bmin = glm::min(bmin, p);
			bmax = glm::max(bmax, p);
		};
		for (auto entity : ecs.view<rigkit::ecs::CTransform, rigkit::ecs::CMesh>()) {
			const auto& xf = ecs.getComponent<rigkit::ecs::CTransform>(entity);
			const auto& mesh = ecs.getComponent<rigkit::ecs::CMesh>(entity);
			if (mesh.positions.empty()) {
				expand(xf.position);
				continue;
			}
			for (const auto& p : mesh.positions) {
				expand(xf.position + glm::vec3(p) * xf.scale);
			}
		}
		for (auto entity : ecs.view<rigkit::ecs::CTransform>()) {
			const auto bounds = rigkit::ecs::shapeBounds2D(ecs, entity);
			if (!bounds.valid) {
				continue;
			}
			const auto& xf = ecs.getComponent<rigkit::ecs::CTransform>(entity);
			expand(xf.position + glm::vec3(bounds.min, 0.f) * xf.scale);
			expand(xf.position + glm::vec3(bounds.max, 0.f) * xf.scale);
		}
		const glm::vec3 center = any ? (bmin + bmax) * 0.5f : glm::vec3(0.f);

		for (auto entity : ecs.view<rigkit::ecs::CTransform, rigkit::ecs::CCamera>()) {
			auto& cam = ecs.getComponent<rigkit::ecs::CCamera>(entity);
			if (!cam.active) {
				continue;
			}
			auto& xf = ecs.getComponent<rigkit::ecs::CTransform>(entity);
			// Near-identity quat (Contract default) → aim at scene like the web viewer.
			if (std::fabs(xf.rotation.w) < 0.999f) {
				continue;
			}
			glm::vec3 target = center;
			if (cam.projection == rigkit::ecs::CCamera::Projection::Orthographic) {
				target = {xf.position.x, xf.position.y, center.z};
			}
			if (glm::length(xf.position - target) < 1e-3f) {
				target = xf.position + glm::vec3(0.f, 0.f, -1.f);
			}
			rig::lookAt(xf, xf.position, target);
			result.notes.push_back("camera aimed at scene (identity rotation → lookAt)");
		}
	}

	// Dedupe skipped keys
	std::sort(result.skipped.begin(), result.skipped.end());
	result.skipped.erase(std::unique(result.skipped.begin(), result.skipped.end()),
						 result.skipped.end());

	result.ok = true;
	spdlog::info("[ContractImport] {} — {} entities, {} geometry (from {})", result.title,
				 result.entityCount, result.geometryCount,
				 sourceLabel.empty() ? "memory" : sourceLabel);
	for (const auto& note : result.notes) {
		spdlog::info("[ContractImport] note: {}", note);
	}
	if (!result.skipped.empty()) {
		spdlog::info("[ContractImport] skipped {} unknown component key(s)", result.skipped.size());
	}
	{
		size_t lfoN = 0;
		size_t bindN = 0;
		for (auto e : ecs.view<rigkit::ecs::CModLfo>()) {
			(void)e;
			++lfoN;
		}
		for (auto e : ecs.view<rigkit::ecs::CModBinding>()) {
			(void)e;
			++bindN;
		}
		if (lfoN > 0 || bindN > 0) {
			spdlog::info("[ContractImport] modulators: {} LFO(s), {} binding(s) -> ECS", lfoN,
						 bindN);
		}
	}
	if (!result.panels.empty()) {
		spdlog::info("[ContractImport] ui: {} panel(s), {} control(s), {} action(s)",
					 result.panels.size(), result.controls.size(), result.actions.size());
	}
	return result;
}

} // namespace

ContractImportResult importContractJson(rigkit::MEcs& ecs, const std::string& jsonText,
										const std::string& sourceLabel) {
	return importContractJsonImpl(ecs, jsonText, sourceLabel, nullptr);
}

ContractImportResult importContractJson(rigkit::MEcs& ecs, const std::string& jsonText,
										const std::string& sourceLabel,
										const ComponentSerializerRegistry& codecs) {
	return importContractJsonImpl(ecs, jsonText, sourceLabel, &codecs);
}

ContractImportResult importContractFile(rigkit::MEcs& ecs, const std::string& path,
										const ComponentSerializerRegistry& codecs) {
	ContractImportResult result;
	std::string error;
	json root = loadFile(path, error);
	if (!error.empty() || root.is_null() || root.empty()) {
		result.error = error.empty() ? "empty or invalid JSON" : error;
		return result;
	}
	return importContractJson(ecs, root.dump(), path, codecs);
}

std::optional<float> contractGetFloat(rigkit::MEcs& ecs, const ContractImportResult& doc,
									  const std::string& entityId, const std::string& propertyKey) {
	const entt::entity e = resolveEntity(doc, entityId);
	if (e == entt::null) {
		return std::nullopt;
	}
	return rigkit::ecs::readEntityProperty(ecs, e, propertyKey);
}

std::optional<std::string> contractGetString(rigkit::MEcs& ecs, const ContractImportResult& doc,
											 const std::string& entityId,
											 const std::string& propertyKey) {
	const entt::entity e = resolveEntity(doc, entityId);
	if (e == entt::null) {
		return std::nullopt;
	}
	if (ecs.hasComponent<rigkit::ecs::CModLfo>(e) && propertyKey == "waveform") {
		return ecs.getComponent<rigkit::ecs::CModLfo>(e).waveform;
	}
	for (const auto& typeInfo : ecs.componentTypes()) {
		if (!ecs.hasRegisteredComponent(typeInfo, e)) {
			continue;
		}
		for (auto& prop : ecs.registeredProperties(typeInfo, e)) {
			if (prop.name == propertyKey && prop.type == EPT_STRING) {
				return *static_cast<std::string*>(prop.data);
			}
		}
	}
	return std::nullopt;
}

std::optional<std::array<float, 4>> contractGetRgba(rigkit::MEcs& ecs,
													const ContractImportResult& doc,
													const std::string& entityId,
													const std::string& propertyKey) {
	(void)propertyKey;
	const entt::entity e = resolveEntity(doc, entityId);
	if (e == entt::null || !ecs.hasComponent<rigkit::ecs::CDrawStyle>(e)) {
		return std::nullopt;
	}
	const auto& s = ecs.getComponent<rigkit::ecs::CDrawStyle>(e);
	return std::array<float, 4>{s.fillR, s.fillG, s.fillB, s.fillA};
}

bool contractSetFloat(rigkit::MEcs& ecs, const ContractImportResult& doc,
					  const std::string& entityId, const std::string& propertyKey, float value) {
	const entt::entity e = resolveEntity(doc, entityId);
	if (e == entt::null) {
		return false;
	}
	return rigkit::ecs::writeEntityProperty(ecs, e, propertyKey, value);
}

bool contractSetString(rigkit::MEcs& ecs, const ContractImportResult& doc,
					   const std::string& entityId, const std::string& propertyKey,
					   const std::string& value) {
	const entt::entity e = resolveEntity(doc, entityId);
	if (e == entt::null) {
		return false;
	}
	if (ecs.hasComponent<rigkit::ecs::CModLfo>(e) && propertyKey == "waveform") {
		ecs.getComponent<rigkit::ecs::CModLfo>(e).waveform = value;
		return true;
	}
	for (const auto& typeInfo : ecs.componentTypes()) {
		if (!ecs.hasRegisteredComponent(typeInfo, e)) {
			continue;
		}
		for (auto& prop : ecs.registeredProperties(typeInfo, e)) {
			if (prop.name == propertyKey && prop.type == EPT_STRING) {
				*static_cast<std::string*>(prop.data) = value;
				return true;
			}
		}
	}
	return false;
}

bool contractSetRgba(rigkit::MEcs& ecs, const ContractImportResult& doc,
					 const std::string& entityId, const std::string& propertyKey,
					 const std::array<float, 4>& rgba) {
	(void)propertyKey;
	const entt::entity e = resolveEntity(doc, entityId);
	if (e == entt::null) {
		return false;
	}
	if (!ecs.hasComponent<rigkit::ecs::CDrawStyle>(e)) {
		rigkit::ecs::CDrawStyle style;
		style.hasFill = true;
		style.fillR = rgba[0];
		style.fillG = rgba[1];
		style.fillB = rgba[2];
		style.fillA = rgba[3];
		ecs.addComponent(e, style);
		return true;
	}
	auto& s = ecs.getComponent<rigkit::ecs::CDrawStyle>(e);
	s.hasFill = true;
	s.fillR = rgba[0];
	s.fillG = rgba[1];
	s.fillB = rgba[2];
	s.fillA = rgba[3];
	return true;
}

bool contractRunAction(rigkit::MEcs& ecs, const ContractImportResult& doc,
					   const std::string& actionId) {
	(void)doc;
	if (actionId == "lfo.resetPhase") {
		for (auto e : ecs.view<rigkit::ecs::CModLfo>()) {
			ecs.getComponent<rigkit::ecs::CModLfo>(e).phase = 0.f;
		}
		return true;
	}
	return false;
}

ContractImportResult importContractFile(rigkit::MEcs& ecs, const std::string& path) {
	ContractImportResult result;
	std::string error;
	json root = loadFile(path, error);
	if (!error.empty() || root.is_null() || root.empty()) {
		result.error = error.empty() ? "empty or invalid JSON" : error;
		return result;
	}
	return importContractJson(ecs, root.dump(), path);
}

} // namespace project
} // namespace rigkit
