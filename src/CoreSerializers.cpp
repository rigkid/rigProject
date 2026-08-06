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
#include "CPage.h"
#include "CPalette.h"
#include "CRelationship.h"
#include "CShape.h"
#include "CTransform.h"
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

const char* shapeTypeName(ecs::CShape::Type type) {
	switch (type) {
	case ecs::CShape::Type::Rectangle:
		return "Rectangle";
	case ecs::CShape::Type::Ellipse:
		return "Ellipse";
	case ecs::CShape::Type::Line:
		return "Line";
	case ecs::CShape::Type::Polygon:
		return "Polygon";
	case ecs::CShape::Type::Star:
		return "Star";
	}
	return "Rectangle";
}

ecs::CShape::Type shapeTypeFromName(const std::string& name) {
	if (name == "Ellipse") {
		return ecs::CShape::Type::Ellipse;
	}
	if (name == "Line") {
		return ecs::CShape::Type::Line;
	}
	if (name == "Polygon") {
		return ecs::CShape::Type::Polygon;
	}
	if (name == "Star") {
		return ecs::CShape::Type::Star;
	}
	return ecs::CShape::Type::Rectangle;
}

bool serializeShape(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CShape>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CShape>(e);
	j["type"] = shapeTypeName(s.type);
	j["x1"] = s.x1;
	j["y1"] = s.y1;
	j["x2"] = s.x2;
	j["y2"] = s.y2;
	j["sides"] = s.sides;
	j["innerRadius"] = s.innerRadius;
	return true;
}

bool deserializeShape(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CShape s;
	s.type = shapeTypeFromName(j.value("type", std::string("Rectangle")));
	s.x1 = j.value("x1", s.x1);
	s.y1 = j.value("y1", s.y1);
	s.x2 = j.value("x2", s.x2);
	s.y2 = j.value("y2", s.y2);
	s.sides = j.value("sides", s.sides);
	s.innerRadius = j.value("innerRadius", s.innerRadius);
	reg.emplace_or_replace<ecs::CShape>(e, s);
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
	j["projection"] = static_cast<int>(c.projection);
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
	c.projection = static_cast<ecs::CCamera::Projection>(j.value("projection", 0));
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
	j["type"] = static_cast<int>(l.type);
	j["color"] = ordered_json::array({l.colorR, l.colorG, l.colorB});
	j["intensity"] = l.intensity;
	j["ambient"] = l.ambient;
	j["banded"] = l.banded;
	j["bands"] = l.bands;
	return true;
}

bool deserializeLight(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CLight l;
	l.enabled = j.value("enabled", l.enabled);
	l.type = static_cast<ecs::CLight::Type>(j.value("type", 0));
	if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 3) {
		l.colorR = j["color"][0].get<float>();
		l.colorG = j["color"][1].get<float>();
		l.colorB = j["color"][2].get<float>();
	}
	l.intensity = j.value("intensity", l.intensity);
	l.ambient = j.value("ambient", l.ambient);
	l.banded = j.value("banded", l.banded);
	l.bands = j.value("bands", l.bands);
	reg.emplace_or_replace<ecs::CLight>(e, l);
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
	j["fill"] = ordered_json::array({d.fillR, d.fillG, d.fillB, d.fillA});
	j["stroke"] = ordered_json::array({d.strokeR, d.strokeG, d.strokeB, d.strokeA});
	j["strokeWidth"] = d.strokeWidth;
	j["hasFill"] = d.hasFill;
	j["hasStroke"] = d.hasStroke;
	j["strokeCap"] = strokeCapName(d.strokeCap);
	j["strokeJoin"] = strokeJoinName(d.strokeJoin);
	j["dashPattern"] = d.dashPattern;
	j["dashOffset"] = d.dashOffset;
	return true;
}

bool deserializeDrawStyle(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CDrawStyle d;
	if (j.contains("fill") && j["fill"].is_array() && j["fill"].size() >= 4) {
		d.fillR = j["fill"][0].get<float>();
		d.fillG = j["fill"][1].get<float>();
		d.fillB = j["fill"][2].get<float>();
		d.fillA = j["fill"][3].get<float>();
	}
	if (j.contains("stroke") && j["stroke"].is_array() && j["stroke"].size() >= 4) {
		d.strokeR = j["stroke"][0].get<float>();
		d.strokeG = j["stroke"][1].get<float>();
		d.strokeB = j["stroke"][2].get<float>();
		d.strokeA = j["stroke"][3].get<float>();
	}
	d.strokeWidth = j.value("strokeWidth", d.strokeWidth);
	d.hasFill = j.value("hasFill", d.hasFill);
	d.hasStroke = j.value("hasStroke", d.hasStroke);
	d.strokeCap = strokeCapFromName(j.value("strokeCap", std::string("Butt")));
	d.strokeJoin = strokeJoinFromName(j.value("strokeJoin", std::string("Miter")));
	if (j.contains("dashPattern") && j["dashPattern"].is_array()) {
		d.dashPattern = j["dashPattern"].get<std::vector<float>>();
	}
	d.dashOffset = j.value("dashOffset", d.dashOffset);
	reg.emplace_or_replace<ecs::CDrawStyle>(e, d);
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
		j["parent"] = static_cast<std::uint32_t>(r.parent);
	}
	return true;
}

bool deserializeRelationship(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CRelationship r;
	if (j.contains("parent") && !j["parent"].is_null()) {
		r.parent = static_cast<entt::entity>(j["parent"].get<std::uint32_t>());
	} else {
		r.parent = entt::null;
	}
	reg.emplace_or_replace<ecs::CRelationship>(e, r);
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
void addSerializer(ComponentSerializerRegistry& registry, const char* key,
				   bool (*serializeFn)(entt::registry&, entt::entity, ordered_json&),
				   bool (*deserializeFn)(entt::registry&, entt::entity, const ordered_json&)) {
	ComponentSerializer ser;
	ser.name = key;
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
	addSerializer<ecs::CPage>(registry, "Page", serializePage, deserializePage);
	addSerializer<ecs::CTransform>(registry, "Transform", serializeTransform, deserializeTransform);
	addSerializer<ecs::CSelectable>(registry, "Selectable", serializeSelectable, deserializeSelectable);
	addSerializer<ecs::CGroup>(registry, "Group", serializeGroup, deserializeGroup);
	addSerializer<ecs::CShape>(registry, "Shape", serializeShape, deserializeShape);
	addSerializer<ecs::CMesh>(registry, "Mesh", serializeMesh, deserializeMesh);
	addSerializer<ecs::CDrawStyle>(registry, "DrawStyle", serializeDrawStyle, deserializeDrawStyle);
	addSerializer<ecs::CRelationship>(registry, "Relationship", serializeRelationship,
									  deserializeRelationship);
	addSerializer<ecs::CGuide>(registry, "Guide", serializeGuide, deserializeGuide);
	addSerializer<ecs::CCamera>(registry, "Camera", serializeCamera, deserializeCamera);
	addSerializer<ecs::CLight>(registry, "Light", serializeLight, deserializeLight);
	addSerializer<ecs::CPalette>(registry, "Palette", serializePalette, deserializePalette);
	addSerializer<ecs::CIndexedAtlas>(registry, "IndexedAtlas", serializeIndexedAtlas,
									  deserializeIndexedAtlas);
	addSerializer<ecs::CFaceSelection>(registry, "FaceSelection", serializeFaceSelection,
									   deserializeFaceSelection);
}

} // namespace project
} // namespace rigkit
