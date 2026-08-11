#include <algorithm>
#include <string>
#include "CCamera.h"
#include "CSelectable.h"
#include "CDrawStyle.h"
#include "CFaceSelection.h"
#include "CGroup.h"
#include "CIndexedAtlas.h"
#include "CLight.h"
#include "CMesh.h"
#include "CModBinding.h"
#include "CModLfo.h"
#include "CMusicClock.h"
#include "CMusicTransport.h"
#include "CTween.h"
#include "CPage.h"
#include "CPalette.h"
#include "CArc.h"
#include "CEllipse.h"
#include "CLine.h"
#include "CPolygon.h"
#include "CRectangle.h"
#include "CRegularPolygon.h"
#include "CRelationship.h"
#include "CRing.h"
#include "CStar.h"
#include "CTransform.h"
#include "EntityIdRemap.h"
#include "ProjectSerializer.h"
#include "ecs/components/CGuide.h"

namespace rigkit {
namespace project {
namespace {

ordered_json vec3ToJson(const glm::vec3& v) {
	return ordered_json::array({v.x, v.y, v.z});
}

glm::vec3 vec3FromJson(const ordered_json& j, const glm::vec3& fallback = {}) {
	if (!j.is_array() || j.size() < 3) {
		return fallback;
	}
	return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

ordered_json vec2ToJson(const glm::vec2& v) {
	return ordered_json::array({v.x, v.y});
}

glm::vec2 vec2FromJson(const ordered_json& j, const glm::vec2& fallback = {}) {
	if (!j.is_array() || j.size() < 2) {
		return fallback;
	}
	return {j[0].get<float>(), j[1].get<float>()};
}

ordered_json vec4ToJson(const glm::vec4& v) {
	return ordered_json::array({v.x, v.y, v.z, v.w});
}

glm::vec4 vec4FromJson(const ordered_json& j, const glm::vec4& fallback = {}) {
	if (!j.is_array() || j.size() < 4) {
		return fallback;
	}
	return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>()};
}

/// Rig quat field order: x, y, z, w.
ordered_json quatToJson(const glm::quat& q) {
	return ordered_json::array({q.x, q.y, q.z, q.w});
}

glm::quat quatFromJson(const ordered_json& j, const glm::quat& fallback = {1.f, 0.f, 0.f, 0.f}) {
	if (!j.is_array() || j.size() < 4) {
		return fallback;
	}
	return glm::normalize(glm::quat(j[3].get<float>(), j[0].get<float>(), j[1].get<float>(),
									j[2].get<float>()));
}

bool serializePage(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CPage>(e)) {
		return false;
	}
	const auto& p = reg.get<ecs::CPage>(e);
	j["name"] = p.name;
	j["index"] = p.index;
	j["unit"] = p.unit;
	j["width"] = p.width;
	j["height"] = p.height;
	j["marginTop"] = p.marginTop;
	j["marginRight"] = p.marginRight;
	j["marginBottom"] = p.marginBottom;
	j["marginLeft"] = p.marginLeft;
	j["bleedTop"] = p.bleedTop;
	j["bleedRight"] = p.bleedRight;
	j["bleedBottom"] = p.bleedBottom;
	j["bleedLeft"] = p.bleedLeft;
	j["slugTop"] = p.slugTop;
	j["slugRight"] = p.slugRight;
	j["slugBottom"] = p.slugBottom;
	j["slugLeft"] = p.slugLeft;
	return true;
}

bool deserializePage(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CPage p;
	p.name = j.value("name", p.name);
	p.index = j.value("index", p.index);
	p.unit = j.value("unit", p.unit);
	p.width = j.value("width", p.width);
	p.height = j.value("height", p.height);
	p.marginTop = j.value("marginTop", p.marginTop);
	p.marginRight = j.value("marginRight", p.marginRight);
	p.marginBottom = j.value("marginBottom", p.marginBottom);
	p.marginLeft = j.value("marginLeft", p.marginLeft);
	p.bleedTop = j.value("bleedTop", p.bleedTop);
	p.bleedRight = j.value("bleedRight", p.bleedRight);
	p.bleedBottom = j.value("bleedBottom", p.bleedBottom);
	p.bleedLeft = j.value("bleedLeft", p.bleedLeft);
	p.slugTop = j.value("slugTop", p.slugTop);
	p.slugRight = j.value("slugRight", p.slugRight);
	p.slugBottom = j.value("slugBottom", p.slugBottom);
	p.slugLeft = j.value("slugLeft", p.slugLeft);
	reg.emplace_or_replace<ecs::CPage>(e, p);
	return true;
}

bool serializeTransform(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CTransform>(e)) {
		return false;
	}
	const auto& t = reg.get<ecs::CTransform>(e);
	j["position"] = vec3ToJson(t.position);
	j["rotation"] = quatToJson(t.rotation);
	j["scale"] = vec3ToJson(t.scale);
	// Omit editor euler and derived world matrix.
	return true;
}

bool deserializeTransform(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CTransform t;
	if (j.contains("position")) {
		t.position = vec3FromJson(j["position"], t.position);
	}
	if (j.contains("scale")) {
		t.scale = vec3FromJson(j["scale"], t.scale);
	}
	if (j.contains("rotation")) {
		t.setRotationQuat(quatFromJson(j["rotation"], t.rotation));
	} else if (j.contains("euler")) {
		// Legacy .rig — euler was the old serialized rotation field.
		t.setEulerRadians(vec3FromJson(j["euler"], t.euler));
	}
	t.world = glm::mat4(1.0f);
	reg.emplace_or_replace<ecs::CTransform>(e, t);
	return true;
}

bool serializeSelectable(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CSelectable>(e)) {
		return false;
	}
	j["enabled"] = reg.get<ecs::CSelectable>(e).enabled;
	return true;
}

bool deserializeSelectable(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CSelectable c;
	c.enabled = j.value("enabled", c.enabled);
	reg.emplace_or_replace<ecs::CSelectable>(e, c);
	return true;
}

bool serializeGroup(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CGroup>(e)) {
		return false;
	}
	(void)j; // marker — empty object
	return true;
}

bool deserializeGroup(entt::registry& reg, entt::entity e, const ordered_json&) {
	reg.emplace_or_replace<ecs::CGroup>(e, ecs::CGroup{});
	return true;
}

// Field names below are the Contract's own, so only the component key still has
// to change when the writer moves to schema ids.

bool serializeRectangle(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CRectangle>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CRectangle>(e);
	j["x"] = s.x;
	j["y"] = s.y;
	j["width"] = s.width;
	j["height"] = s.height;
	j["cornerRadius"] = s.cornerRadius;
	return true;
}

bool deserializeRectangle(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CRectangle s;
	s.x = j.value("x", s.x);
	s.y = j.value("y", s.y);
	s.width = j.value("width", s.width);
	s.height = j.value("height", s.height);
	s.cornerRadius = j.value("cornerRadius", s.cornerRadius);
	reg.emplace_or_replace<ecs::CRectangle>(e, s);
	return true;
}

bool serializeEllipse(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CEllipse>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CEllipse>(e);
	j["cx"] = s.cx;
	j["cy"] = s.cy;
	j["rx"] = s.rx;
	j["ry"] = s.ry;
	return true;
}

bool deserializeEllipse(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CEllipse s;
	s.cx = j.value("cx", s.cx);
	s.cy = j.value("cy", s.cy);
	s.rx = j.value("rx", s.rx);
	s.ry = j.value("ry", s.ry);
	reg.emplace_or_replace<ecs::CEllipse>(e, s);
	return true;
}

bool serializeLine(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CLine>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CLine>(e);
	j["x1"] = s.x1;
	j["y1"] = s.y1;
	j["x2"] = s.x2;
	j["y2"] = s.y2;
	return true;
}

bool deserializeLine(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CLine s;
	s.x1 = j.value("x1", s.x1);
	s.y1 = j.value("y1", s.y1);
	s.x2 = j.value("x2", s.x2);
	s.y2 = j.value("y2", s.y2);
	reg.emplace_or_replace<ecs::CLine>(e, s);
	return true;
}

bool serializePolygon(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CPolygon>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CPolygon>(e);
	auto points = ordered_json::array();
	for (const auto& p : s.points) {
		points.push_back(vec2ToJson(p));
	}
	j["points"] = std::move(points);
	j["closed"] = s.closed;
	return true;
}

bool deserializePolygon(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CPolygon s;
	if (auto it = j.find("points"); it != j.end() && it->is_array()) {
		s.points.reserve(it->size());
		for (const auto& p : *it) {
			s.points.push_back(vec2FromJson(p));
		}
	}
	s.closed = j.value("closed", s.closed);
	reg.emplace_or_replace<ecs::CPolygon>(e, std::move(s));
	return true;
}

bool serializeRegularPolygon(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CRegularPolygon>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CRegularPolygon>(e);
	j["cx"] = s.cx;
	j["cy"] = s.cy;
	j["radius"] = s.radius;
	j["sides"] = s.sides;
	j["rotationDegrees"] = s.rotationDegrees;
	return true;
}

bool deserializeRegularPolygon(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CRegularPolygon s;
	s.cx = j.value("cx", s.cx);
	s.cy = j.value("cy", s.cy);
	s.radius = j.value("radius", s.radius);
	s.sides = j.value("sides", s.sides);
	s.rotationDegrees = j.value("rotationDegrees", s.rotationDegrees);
	reg.emplace_or_replace<ecs::CRegularPolygon>(e, s);
	return true;
}

bool serializeStar(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CStar>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CStar>(e);
	j["cx"] = s.cx;
	j["cy"] = s.cy;
	j["radius"] = s.radius;
	j["innerRadius"] = s.innerRadius;
	j["points"] = s.points;
	j["rotationDegrees"] = s.rotationDegrees;
	return true;
}

bool deserializeStar(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CStar s;
	s.cx = j.value("cx", s.cx);
	s.cy = j.value("cy", s.cy);
	s.radius = j.value("radius", s.radius);
	s.innerRadius = j.value("innerRadius", s.innerRadius);
	s.points = j.value("points", s.points);
	s.rotationDegrees = j.value("rotationDegrees", s.rotationDegrees);
	reg.emplace_or_replace<ecs::CStar>(e, s);
	return true;
}

bool serializeArc(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CArc>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CArc>(e);
	j["cx"] = s.cx;
	j["cy"] = s.cy;
	j["radius"] = s.radius;
	j["startAngleDegrees"] = s.startAngleDegrees;
	j["endAngleDegrees"] = s.endAngleDegrees;
	j["pie"] = s.pie;
	return true;
}

bool deserializeArc(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CArc s;
	s.cx = j.value("cx", s.cx);
	s.cy = j.value("cy", s.cy);
	s.radius = j.value("radius", s.radius);
	s.startAngleDegrees = j.value("startAngleDegrees", s.startAngleDegrees);
	s.endAngleDegrees = j.value("endAngleDegrees", s.endAngleDegrees);
	s.pie = j.value("pie", s.pie);
	reg.emplace_or_replace<ecs::CArc>(e, s);
	return true;
}

bool serializeRing(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CRing>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CRing>(e);
	j["cx"] = s.cx;
	j["cy"] = s.cy;
	j["outerRadius"] = s.outerRadius;
	j["innerRadius"] = s.innerRadius;
	return true;
}

bool deserializeRing(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CRing s;
	s.cx = j.value("cx", s.cx);
	s.cy = j.value("cy", s.cy);
	s.outerRadius = j.value("outerRadius", s.outerRadius);
	s.innerRadius = j.value("innerRadius", s.innerRadius);
	reg.emplace_or_replace<ecs::CRing>(e, s);
	return true;
}

const char* meshModeName(ecs::CMesh::Mode mode) {
	switch (mode) {
	case ecs::CMesh::Mode::Triangles:
		return "Triangles";
	case ecs::CMesh::Mode::Lines:
		return "Lines";
	case ecs::CMesh::Mode::LineStrip:
		return "LineStrip";
	}
	return "Triangles";
}

ecs::CMesh::Mode meshModeFromName(const std::string& name) {
	if (name == "Lines") {
		return ecs::CMesh::Mode::Lines;
	}
	if (name == "LineStrip") {
		return ecs::CMesh::Mode::LineStrip;
	}
	return ecs::CMesh::Mode::Triangles;
}

bool serializeMesh(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CMesh>(e)) {
		return false;
	}
	const auto& m = reg.get<ecs::CMesh>(e);
	j["mode"] = meshModeName(m.mode);
	ordered_json positions = ordered_json::array();
	for (const auto& p : m.positions) {
		positions.push_back(vec3ToJson(p));
	}
	j["positions"] = std::move(positions);
	j["indices"] = m.indices;
	if (!m.faceColors.empty()) {
		ordered_json colors = ordered_json::array();
		for (const auto& c : m.faceColors) {
			colors.push_back(vec4ToJson(c));
		}
		j["faceColors"] = std::move(colors);
	}
	if (!m.facePalette.empty()) {
		j["facePalette"] = m.facePalette;
	}
	if (!m.texcoords.empty()) {
		ordered_json uvs = ordered_json::array();
		for (const auto& t : m.texcoords) {
			uvs.push_back(ordered_json::array({t.x, t.y}));
		}
		j["texcoords"] = std::move(uvs);
	}
	return true;
}

bool deserializeMesh(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CMesh m;
	m.mode = meshModeFromName(j.value("mode", std::string("Triangles")));
	if (j.contains("positions") && j["positions"].is_array()) {
		for (const auto& p : j["positions"]) {
			m.positions.push_back(vec3FromJson(p));
		}
	}
	if (j.contains("indices") && j["indices"].is_array()) {
		m.indices = j["indices"].get<std::vector<uint32_t>>();
	}
	if (j.contains("faceColors") && j["faceColors"].is_array()) {
		for (const auto& c : j["faceColors"]) {
			m.faceColors.push_back(vec4FromJson(c));
		}
	}
	if (j.contains("facePalette") && j["facePalette"].is_array()) {
		m.facePalette = j["facePalette"].get<std::vector<uint8_t>>();
	}
	if (j.contains("texcoords") && j["texcoords"].is_array()) {
		for (const auto& t : j["texcoords"]) {
			if (t.is_array() && t.size() >= 2) {
				m.texcoords.push_back({t[0].get<float>(), t[1].get<float>()});
			}
		}
	}
	reg.emplace_or_replace<ecs::CMesh>(e, m);
	return true;
}

bool serializeCamera(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CCamera>(e)) {
		return false;
	}
	const auto& c = reg.get<ecs::CCamera>(e);
	j["active"] = c.active;
	j["projection"] =
		(c.projection == ecs::CCamera::Projection::Orthographic) ? "orthographic" : "perspective";
	j["fovYDegrees"] = c.fovYDegrees;
	j["orthoHeight"] = c.orthoHeight;
	j["nearClip"] = c.nearClip;
	j["farClip"] = c.farClip;
	j["aspect"] = c.aspect;
	return true;
}

bool deserializeCamera(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CCamera c;
	c.active = j.value("active", c.active);
	c.projection = (j.value("projection", std::string("perspective")) == "orthographic")
					   ? ecs::CCamera::Projection::Orthographic
					   : ecs::CCamera::Projection::Perspective;
	c.fovYDegrees = j.value("fovYDegrees", c.fovYDegrees);
	c.orthoHeight = j.value("orthoHeight", c.orthoHeight);
	c.nearClip = j.value("nearClip", c.nearClip);
	c.farClip = j.value("farClip", c.farClip);
	c.aspect = j.value("aspect", c.aspect);
	reg.emplace_or_replace<ecs::CCamera>(e, c);
	return true;
}

bool serializeLight(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CLight>(e)) {
		return false;
	}
	const auto& l = reg.get<ecs::CLight>(e);
	j["enabled"] = l.enabled;
	j["type"] = (l.type == ecs::CLight::Type::Point) ? "point" : "directional";
	j["rgb"] = ordered_json::array({l.colorR, l.colorG, l.colorB});
	j["intensity"] = l.intensity;
	j["ambient"] = l.ambient;
	j["banded"] = l.banded;
	j["bands"] = l.bands;
	return true;
}

bool deserializeLight(entt::registry& reg, entt::entity e, const ordered_json& j) {
	auto& l = reg.get_or_emplace<ecs::CLight>(e);
	l.enabled = j.value("enabled", l.enabled);
	l.type = (j.value("type", std::string("directional")) == "point")
				 ? ecs::CLight::Type::Point
				 : ecs::CLight::Type::Directional;
	if (j.contains("rgb") && j["rgb"].is_array() && j["rgb"].size() >= 3) {
		l.colorR = j["rgb"][0].get<float>();
		l.colorG = j["rgb"][1].get<float>();
		l.colorB = j["rgb"][2].get<float>();
	}
	l.intensity = j.value("intensity", l.intensity);
	l.ambient = j.value("ambient", l.ambient);
	l.banded = j.value("banded", l.banded);
	l.bands = j.value("bands", l.bands);
	return true;
}

/// Dither is a Kit shading choice with no Contract field, and the light schema
/// admits no extras, so it travels beside the light rather than inside it.
bool serializeLightShading(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CLight>(e)) {
		return false;
	}
	const auto& l = reg.get<ecs::CLight>(e);
	if (!l.dither) {
		return false;
	}
	j["dither"] = l.dither;
	return true;
}

bool deserializeLightShading(entt::registry& reg, entt::entity e, const ordered_json& j) {
	auto& l = reg.get_or_emplace<ecs::CLight>(e);
	l.dither = j.value("dither", l.dither);
	return true;
}

bool serializePalette(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CPalette>(e)) {
		return false;
	}
	const auto& p = reg.get<ecs::CPalette>(e);
	ordered_json colors = ordered_json::array();
	ordered_json next = ordered_json::array();
	for (int i = 0; i < ecs::CPalette::kCount; ++i) {
		colors.push_back(vec4ToJson(p.colors[static_cast<size_t>(i)]));
		next.push_back(p.shadeNext[static_cast<size_t>(i)]);
	}
	j["colors"] = std::move(colors);
	j["shadeNext"] = std::move(next);
	return true;
}

bool deserializePalette(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CPalette p = ecs::CPalette::default16();
	if (j.contains("colors") && j["colors"].is_array()) {
		const size_t n = std::min(j["colors"].size(), static_cast<size_t>(ecs::CPalette::kCount));
		for (size_t i = 0; i < n; ++i) {
			p.colors[i] = vec4FromJson(j["colors"][i], p.colors[i]);
		}
	}
	if (j.contains("shadeNext") && j["shadeNext"].is_array()) {
		const size_t n =
			std::min(j["shadeNext"].size(), static_cast<size_t>(ecs::CPalette::kCount));
		for (size_t i = 0; i < n; ++i) {
			p.shadeNext[i] = j["shadeNext"][i].get<int>();
		}
	}
	reg.emplace_or_replace<ecs::CPalette>(e, p);
	return true;
}

bool serializeIndexedAtlas(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CIndexedAtlas>(e)) {
		return false;
	}
	const auto& a = reg.get<ecs::CIndexedAtlas>(e);
	j["width"] = a.width;
	j["height"] = a.height;
	j["indices"] = a.indices;
	return true;
}

bool deserializeIndexedAtlas(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CIndexedAtlas a;
	a.width = j.value("width", a.width);
	a.height = j.value("height", a.height);
	if (j.contains("indices") && j["indices"].is_array()) {
		a.indices = j["indices"].get<std::vector<uint8_t>>();
	} else {
		a.resize(a.width, a.height, 0);
	}
	reg.emplace_or_replace<ecs::CIndexedAtlas>(e, a);
	return true;
}

bool serializeFaceSelection(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CFaceSelection>(e)) {
		return false;
	}
	j["faces"] = reg.get<ecs::CFaceSelection>(e).faces;
	return true;
}

bool deserializeFaceSelection(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CFaceSelection s;
	if (j.contains("faces") && j["faces"].is_array()) {
		s.faces = j["faces"].get<std::vector<uint32_t>>();
	}
	reg.emplace_or_replace<ecs::CFaceSelection>(e, s);
	return true;
}

const char* strokeCapName(ecs::CDrawStyle::StrokeCap cap) {
	switch (cap) {
	case ecs::CDrawStyle::StrokeCap::Butt:
		return "Butt";
	case ecs::CDrawStyle::StrokeCap::Square:
		return "Square";
	case ecs::CDrawStyle::StrokeCap::Round:
		return "Round";
	}
	return "Butt";
}

ecs::CDrawStyle::StrokeCap strokeCapFromName(const std::string& name) {
	if (name == "Square") {
		return ecs::CDrawStyle::StrokeCap::Square;
	}
	if (name == "Round") {
		return ecs::CDrawStyle::StrokeCap::Round;
	}
	return ecs::CDrawStyle::StrokeCap::Butt;
}

const char* strokeJoinName(ecs::CDrawStyle::StrokeJoin join) {
	switch (join) {
	case ecs::CDrawStyle::StrokeJoin::Miter:
		return "Miter";
	case ecs::CDrawStyle::StrokeJoin::Bevel:
		return "Bevel";
	case ecs::CDrawStyle::StrokeJoin::Round:
		return "Round";
	}
	return "Miter";
}

ecs::CDrawStyle::StrokeJoin strokeJoinFromName(const std::string& name) {
	if (name == "Bevel") {
		return ecs::CDrawStyle::StrokeJoin::Bevel;
	}
	if (name == "Round") {
		return ecs::CDrawStyle::StrokeJoin::Round;
	}
	return ecs::CDrawStyle::StrokeJoin::Miter;
}

bool serializeDrawStyle(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CDrawStyle>(e)) {
		return false;
	}
	const auto& d = reg.get<ecs::CDrawStyle>(e);
	j["fillRgba"] = ordered_json::array({d.fillR, d.fillG, d.fillB, d.fillA});
	j["strokeRgba"] = ordered_json::array({d.strokeR, d.strokeG, d.strokeB, d.strokeA});
	j["strokeWidth"] = d.strokeWidth;
	j["hasFill"] = d.hasFill;
	j["hasStroke"] = d.hasStroke;
	return true;
}

// CDrawStyle spans two wire components, so both codecs patch the same struct
// instead of replacing it — whichever key is read second must not wipe the first.
bool deserializeDrawStyle(entt::registry& reg, entt::entity e, const ordered_json& j) {
	auto& d = reg.get_or_emplace<ecs::CDrawStyle>(e);
	if (j.contains("fillRgba") && j["fillRgba"].is_array() && j["fillRgba"].size() >= 4) {
		d.fillR = j["fillRgba"][0].get<float>();
		d.fillG = j["fillRgba"][1].get<float>();
		d.fillB = j["fillRgba"][2].get<float>();
		d.fillA = j["fillRgba"][3].get<float>();
	}
	if (j.contains("strokeRgba") && j["strokeRgba"].is_array() && j["strokeRgba"].size() >= 4) {
		d.strokeR = j["strokeRgba"][0].get<float>();
		d.strokeG = j["strokeRgba"][1].get<float>();
		d.strokeB = j["strokeRgba"][2].get<float>();
		d.strokeA = j["strokeRgba"][3].get<float>();
	}
	d.strokeWidth = j.value("strokeWidth", d.strokeWidth);
	d.hasFill = j.value("hasFill", d.hasFill);
	d.hasStroke = j.value("hasStroke", d.hasStroke);
	return true;
}

/// Cap, join, and dash have no Contract schema, so they ride a host extension
/// rather than smuggling extra fields into `rig.paint.fill_stroke`.
bool serializeStrokeStyle(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CDrawStyle>(e)) {
		return false;
	}
	const auto& d = reg.get<ecs::CDrawStyle>(e);
	const ecs::CDrawStyle plain;
	// Silence on defaults — most strokes have nothing to add here, and every
	// omitted key is bytes off documents that hold thousands of styled entities.
	if (d.strokeCap == plain.strokeCap && d.strokeJoin == plain.strokeJoin &&
		d.dashPattern.empty() && d.dashOffset == plain.dashOffset) {
		return false;
	}
	j["strokeCap"] = strokeCapName(d.strokeCap);
	j["strokeJoin"] = strokeJoinName(d.strokeJoin);
	if (!d.dashPattern.empty()) {
		j["dashPattern"] = d.dashPattern;
		j["dashOffset"] = d.dashOffset;
	}
	return true;
}

bool deserializeStrokeStyle(entt::registry& reg, entt::entity e, const ordered_json& j) {
	auto& d = reg.get_or_emplace<ecs::CDrawStyle>(e);
	d.strokeCap = strokeCapFromName(j.value("strokeCap", std::string("Butt")));
	d.strokeJoin = strokeJoinFromName(j.value("strokeJoin", std::string("Miter")));
	if (j.contains("dashPattern") && j["dashPattern"].is_array()) {
		d.dashPattern = j["dashPattern"].get<std::vector<float>>();
	}
	d.dashOffset = j.value("dashOffset", d.dashOffset);
	return true;
}

bool serializeRelationship(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CRelationship>(e)) {
		return false;
	}
	const auto& r = reg.get<ecs::CRelationship>(e);
	if (r.parent == entt::null) {
		j["parent"] = nullptr;
	} else {
		j["parent"] = entityIdString(r.parent);
	}
	return true;
}

bool deserializeRelationship(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CRelationship r;
	// Parks the saved handle here; the load pass remaps it to the live entity
	// once every id in the document has been seen.
	if (j.contains("parent") && j["parent"].is_string()) {
		r.parent = entityIdFromString(j["parent"].get<std::string>());
	} else {
		r.parent = entt::null;
	}
	reg.emplace_or_replace<ecs::CRelationship>(e, r);
	return true;
}

bool serializeModLfo(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CModLfo>(e)) {
		return false;
	}
	const auto& m = reg.get<ecs::CModLfo>(e);
	j["waveform"] = m.waveform;
	j["frequency"] = m.frequency;
	j["amplitude"] = m.amplitude;
	j["offset"] = m.offset;
	j["phase"] = m.phase;
	return true;
}

bool deserializeModLfo(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CModLfo m;
	m.waveform = j.value("waveform", m.waveform);
	m.frequency = j.value("frequency", m.frequency);
	m.amplitude = j.value("amplitude", m.amplitude);
	m.offset = j.value("offset", m.offset);
	m.phase = j.value("phase", m.phase);
	reg.emplace_or_replace<ecs::CModLfo>(e, m);
	return true;
}

bool serializeModBinding(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CModBinding>(e)) {
		return false;
	}
	const auto& b = reg.get<ecs::CModBinding>(e);
	j["source"] = b.source;
	j["target"] = b.target;
	j["propertyKey"] = b.propertyKey;
	j["depth"] = b.depth;
	j["additive"] = b.additive;
	if (b.hasMin) {
		j["min"] = b.min;
	}
	if (b.hasMax) {
		j["max"] = b.max;
	}
	return true;
}

bool deserializeModBinding(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CModBinding b;
	b.source = j.value("source", b.source);
	b.target = j.value("target", b.target);
	b.propertyKey = j.value("propertyKey", b.propertyKey);
	b.depth = j.value("depth", b.depth);
	b.additive = j.value("additive", b.additive);
	if (j.contains("min") && j["min"].is_number()) {
		b.min = j["min"].get<float>();
		b.hasMin = true;
	}
	if (j.contains("max") && j["max"].is_number()) {
		b.max = j["max"].get<float>();
		b.hasMax = true;
	}
	reg.emplace_or_replace<ecs::CModBinding>(e, b);
	return true;
}

bool serializeTween(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CTween>(e)) {
		return false;
	}
	const auto& t = reg.get<ecs::CTween>(e);
	j["target"] = t.target;
	j["propertyKey"] = t.propertyKey;
	j["from"] = t.from;
	j["to"] = t.to;
	j["duration"] = t.duration;
	j["elapsed"] = t.elapsed;
	j["easing"] = t.easing;
	j["loop"] = t.loop;
	j["playing"] = t.playing;
	return true;
}

bool deserializeTween(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CTween t;
	t.target = j.value("target", t.target);
	t.propertyKey = j.value("propertyKey", t.propertyKey);
	t.from = j.value("from", t.from);
	t.to = j.value("to", t.to);
	t.duration = j.value("duration", t.duration);
	t.elapsed = j.value("elapsed", t.elapsed);
	t.easing = j.value("easing", t.easing);
	t.loop = j.value("loop", t.loop);
	t.playing = j.value("playing", t.playing);
	reg.emplace_or_replace<ecs::CTween>(e, t);
	return true;
}

bool serializeMusicClock(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CMusicClock>(e)) {
		return false;
	}
	const auto& c = reg.get<ecs::CMusicClock>(e);
	j["ticksPerQuarter"] = c.ticksPerQuarter;
	j["phaseTicks"] = c.phaseTicks;
	j["swingAmount"] = c.swingAmount;
	j["swingSubdiv"] = c.swingSubdiv;
	j["externalSync"] = c.externalSync;
	if (c.externalSync) {
		j["syncBeat"] = c.syncBeat;
		j["syncPhase"] = c.syncPhase;
	}
	if (c.syncPeriodBars > 0.f) {
		j["syncPeriodBars"] = c.syncPeriodBars;
	}
	return true;
}

bool deserializeMusicClock(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CMusicClock c;
	c.ticksPerQuarter = j.value("ticksPerQuarter", c.ticksPerQuarter);
	c.phaseTicks = j.value("phaseTicks", c.phaseTicks);
	c.swingAmount = j.value("swingAmount", c.swingAmount);
	c.swingSubdiv = j.value("swingSubdiv", c.swingSubdiv);
	c.externalSync = j.value("externalSync", c.externalSync);
	c.syncBeat = j.value("syncBeat", c.syncBeat);
	c.syncPhase = j.value("syncPhase", c.syncPhase);
	c.syncPeriodBars = j.value("syncPeriodBars", c.syncPeriodBars);
	reg.emplace_or_replace<ecs::CMusicClock>(e, c);
	return true;
}

bool serializeMusicTransport(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CMusicTransport>(e)) {
		return false;
	}
	const auto& t = reg.get<ecs::CMusicTransport>(e);
	j["playing"] = t.playing;
	j["bpm"] = t.bpm;
	j["timeSigNum"] = t.timeSigNum;
	j["timeSigDen"] = t.timeSigDen;
	j["positionBeats"] = t.positionBeats;
	j["loop"] = t.loop;
	if (t.loop) {
		j["loopStartBeats"] = t.loopStartBeats;
		j["loopEndBeats"] = t.loopEndBeats;
	}
	return true;
}

bool deserializeMusicTransport(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CMusicTransport t;
	t.playing = j.value("playing", t.playing);
	t.bpm = j.value("bpm", t.bpm);
	t.timeSigNum = j.value("timeSigNum", t.timeSigNum);
	t.timeSigDen = j.value("timeSigDen", t.timeSigDen);
	t.positionBeats = j.value("positionBeats", t.positionBeats);
	t.loop = j.value("loop", t.loop);
	t.loopStartBeats = j.value("loopStartBeats", t.loopStartBeats);
	t.loopEndBeats = j.value("loopEndBeats", t.loopEndBeats);
	reg.emplace_or_replace<ecs::CMusicTransport>(e, t);
	return true;
}

bool serializeGuide(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CGuide>(e)) {
		return false;
	}
	const auto& g = reg.get<ecs::CGuide>(e);
	j["position"] = g.position;
	j["vertical"] = g.vertical;
	j["color"] = vec4ToJson(g.color);
	return true;
}

bool deserializeGuide(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CGuide g{};
	g.position = j.value("position", 0.0f);
	g.vertical = j.value("vertical", false);
	if (j.contains("color")) {
		g.color = vec4FromJson(j["color"], g.color);
	}
	reg.emplace_or_replace<ecs::CGuide>(e, g);
	return true;
}

template <typename T>
void addSerializer(ComponentSerializerRegistry& registry, const char* key, const char* schemaId,
				   bool (*serializeFn)(entt::registry&, entt::entity, ordered_json&),
				   bool (*deserializeFn)(entt::registry&, entt::entity, const ordered_json&)) {
	ComponentSerializer ser;
	ser.name = key;
	ser.schemaId = schemaId;
	ser.hasComponent = [](entt::registry& reg, entt::entity e) { return reg.all_of<T>(e); };
	ser.forEachEntity = [](entt::registry& reg, std::function<void(entt::entity)> fn) {
		for (auto entity : reg.view<T>()) {
			fn(entity);
		}
	};
	ser.serialize = serializeFn;
	ser.deserialize = deserializeFn;
	registry.registerSerializer(std::move(ser));
}

} // namespace

void registerCoreSerializers(ComponentSerializerRegistry& registry) {
	// Contract schemas where one exists; x.rigkit.* where the concept is ours.
	addSerializer<ecs::CTransform>(registry, "Transform", "rig.spatial.transform",
								   serializeTransform, deserializeTransform);
	addSerializer<ecs::CRelationship>(registry, "Relationship", "rig.spatial.relationship",
									  serializeRelationship, deserializeRelationship);
	addSerializer<ecs::CGroup>(registry, "Group", "rig.spatial.group", serializeGroup,
							   deserializeGroup);
	addSerializer<ecs::CCamera>(registry, "Camera", "rig.spatial.camera", serializeCamera,
								deserializeCamera);
	addSerializer<ecs::CSelectable>(registry, "Selectable", "rig.interact.selectable",
									serializeSelectable, deserializeSelectable);
	addSerializer<ecs::CDrawStyle>(registry, "DrawStyle", "rig.paint.fill_stroke",
								   serializeDrawStyle, deserializeDrawStyle);
	addSerializer<ecs::CLight>(registry, "Light", "rig.render.light", serializeLight,
							   deserializeLight);
	addSerializer<ecs::CModLfo>(registry, "ModLfo", "rig.mod.lfo", serializeModLfo,
								deserializeModLfo);
	addSerializer<ecs::CModBinding>(registry, "ModBinding", "rig.mod.binding", serializeModBinding,
									deserializeModBinding);
	addSerializer<ecs::CTween>(registry, "Tween", "rig.anim.tween", serializeTween, deserializeTween);
	addSerializer<ecs::CMusicClock>(registry, "MusicClock", "rig.music.clock", serializeMusicClock,
									deserializeMusicClock);
	addSerializer<ecs::CMusicTransport>(registry, "MusicTransport", "rig.music.transport",
										serializeMusicTransport, deserializeMusicTransport);
	addSerializer<ecs::CDrawStyle>(registry, "StrokeStyle", "x.rigkit.stroke_style",
								   serializeStrokeStyle, deserializeStrokeStyle);
	addSerializer<ecs::CLight>(registry, "LightShading", "x.rigkit.light_shading",
							   serializeLightShading, deserializeLightShading);

	addSerializer<ecs::CRectangle>(registry, "Rectangle", "rig.geometry.rectangle",
								   serializeRectangle, deserializeRectangle);
	addSerializer<ecs::CEllipse>(registry, "Ellipse", "rig.geometry.ellipse", serializeEllipse,
								 deserializeEllipse);
	addSerializer<ecs::CLine>(registry, "Line", "rig.geometry.line", serializeLine,
							  deserializeLine);
	addSerializer<ecs::CPolygon>(registry, "Polygon", "rig.geometry.polygon", serializePolygon,
								 deserializePolygon);
	addSerializer<ecs::CRegularPolygon>(registry, "RegularPolygon", "rig.geometry.regular_polygon",
										serializeRegularPolygon, deserializeRegularPolygon);
	addSerializer<ecs::CStar>(registry, "Star", "rig.geometry.star", serializeStar,
							  deserializeStar);
	addSerializer<ecs::CArc>(registry, "Arc", "rig.geometry.arc", serializeArc, deserializeArc);
	addSerializer<ecs::CRing>(registry, "Ring", "rig.geometry.ring", serializeRing,
							  deserializeRing);
	addSerializer<ecs::CMesh>(registry, "Mesh", "rig.geometry.mesh", serializeMesh,
							  deserializeMesh);

	// No Contract schema covers these yet, so they travel as host extensions.
	addSerializer<ecs::CPage>(registry, "Page", "x.rigkit.page", serializePage, deserializePage);
	addSerializer<ecs::CGuide>(registry, "Guide", "x.rigkit.guide", serializeGuide,
							   deserializeGuide);
	addSerializer<ecs::CPalette>(registry, "Palette", "x.rigkit.palette", serializePalette,
								 deserializePalette);
	addSerializer<ecs::CIndexedAtlas>(registry, "IndexedAtlas", "x.rigkit.indexed_atlas",
									  serializeIndexedAtlas, deserializeIndexedAtlas);
	addSerializer<ecs::CFaceSelection>(registry, "FaceSelection", "x.rigkit.face_selection",
									   serializeFaceSelection, deserializeFaceSelection);
}

} // namespace project
} // namespace rigkit
