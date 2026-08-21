#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "AddSerializer.h"
#include "CArc.h"
#include "CAssetRef.h"
#include "CCadBoolean.h"
#include "CCadBox.h"
#include "CCadChamfer.h"
#include "CCadCylinder.h"
#include "CCadDimension.h"
#include "CCadExtrude.h"
#include "CCadFillet.h"
#include "CCadRevolve.h"
#include "CCadSphere.h"
#include "CCamera.h"
#include "CCurve.h"
#include "CDrawStyle.h"
#include "CEdgeSelection.h"
#include "CEllipse.h"
#include "CFaceSelection.h"
#include "CGroup.h"
#include "CIndexedAtlas.h"
#include "CLayer.h"
#include "CLight.h"
#include "CLine.h"
#include "CMesh.h"
#include "CModBinding.h"
#include "CModLfo.h"
#include "CMusicClock.h"
#include "CMusicTransport.h"
#include "CNurbsSurface.h"
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
#include "CText.h"
#include "CTransform.h"
#include "CTween.h"
#include "ComponentSerializers.h"
#include "EntityIdRemap.h"
#include "MeshEdge.h"
#include "MeshFaces.h"
#include "PathEllipticArc.h"
#include "ProjectJson.h"
#include "core/TypeJson.h"
#include "ecs/components/CGuide.h"

namespace rigkit {
namespace {

using project::entityIdFromString;
using project::EntityIdMap;
using project::entityIdString;
using project::ordered_json;
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
	}
	t.world = glm::mat4(1.0f);
	reg.emplace_or_replace<ecs::CTransform>(e, t);
	return true;
}

// Field names below are the Contract's own; the writer keys each blob by schema id.

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
	j["rx"] = s.rx;
	j["ry"] = s.ry;
	if (s.isCircular()) {
		j["radius"] = s.rx;
	}
	j["startAngleDegrees"] = s.startAngleDegrees;
	j["endAngleDegrees"] = s.endAngleDegrees;
	j["pie"] = s.pie;
	return true;
}

bool deserializeArc(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CArc s;
	s.cx = j.value("cx", s.cx);
	s.cy = j.value("cy", s.cy);
	if (j.contains("rx") || j.contains("ry")) {
		s.rx = j.value("rx", j.value("radius", s.rx));
		s.ry = j.value("ry", j.value("radius", s.ry));
	} else {
		s.setRadius(j.value("radius", s.rx));
	}
	s.startAngleDegrees = j.value("startAngleDegrees", s.startAngleDegrees);
	s.endAngleDegrees = j.value("endAngleDegrees", s.endAngleDegrees);
	s.pie = j.value("pie", s.pie);
	reg.emplace_or_replace<ecs::CArc>(e, s);
	return true;
}

bool serializeSpline(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CSpline>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CSpline>(e);
	j["degree"] = s.degree;
	j["closed"] = s.closed;
	j["controlPoints"] = ordered_json::array();
	for (const auto& p : s.controlPoints) {
		j["controlPoints"].push_back({p.x, p.y});
	}
	j["knots"] = s.knots;
	if (!s.weights.empty()) {
		j["weights"] = s.weights;
	}
	if (!s.fitPoints.empty()) {
		j["fitPoints"] = ordered_json::array();
		for (const auto& p : s.fitPoints) {
			j["fitPoints"].push_back({p.x, p.y});
		}
	}
	return true;
}

bool deserializeSpline(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CSpline s;
	s.degree = j.value("degree", s.degree);
	s.closed = j.value("closed", s.closed);
	if (j.contains("controlPoints") && j["controlPoints"].is_array()) {
		for (const auto& p : j["controlPoints"]) {
			s.controlPoints.push_back(vec2FromJson(p));
		}
	}
	if (j.contains("knots") && j["knots"].is_array()) {
		for (const auto& k : j["knots"]) {
			s.knots.push_back(k.get<float>());
		}
	}
	if (j.contains("weights") && j["weights"].is_array()) {
		for (const auto& w : j["weights"]) {
			s.weights.push_back(w.get<float>());
		}
	}
	if (j.contains("fitPoints") && j["fitPoints"].is_array()) {
		for (const auto& p : j["fitPoints"]) {
			s.fitPoints.push_back(vec2FromJson(p));
		}
	}
	reg.emplace_or_replace<ecs::CSpline>(e, std::move(s));
	return true;
}

bool serializeSpline3d(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CSpline3d>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CSpline3d>(e);
	j["degree"] = s.degree;
	if (s.closed) {
		j["closed"] = true;
	}
	j["controlPoints"] = ordered_json::array();
	for (const auto& p : s.controlPoints) {
		j["controlPoints"].push_back(vec3ToJson(p));
	}
	j["knots"] = s.knots;
	if (!s.weights.empty()) {
		j["weights"] = s.weights;
	}
	if (!s.fitPoints.empty()) {
		j["fitPoints"] = ordered_json::array();
		for (const auto& p : s.fitPoints) {
			j["fitPoints"].push_back(vec3ToJson(p));
		}
	}
	return true;
}

bool deserializeSpline3d(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CSpline3d s;
	s.degree = j.value("degree", s.degree);
	s.closed = j.value("closed", s.closed);
	if (j.contains("controlPoints") && j["controlPoints"].is_array()) {
		for (const auto& p : j["controlPoints"]) {
			s.controlPoints.push_back(vec3FromJson(p));
		}
	}
	if (j.contains("knots") && j["knots"].is_array()) {
		for (const auto& k : j["knots"]) {
			s.knots.push_back(k.get<float>());
		}
	}
	if (j.contains("weights") && j["weights"].is_array()) {
		for (const auto& w : j["weights"]) {
			s.weights.push_back(w.get<float>());
		}
	}
	if (j.contains("fitPoints") && j["fitPoints"].is_array()) {
		for (const auto& p : j["fitPoints"]) {
			s.fitPoints.push_back(vec3FromJson(p));
		}
	}
	reg.emplace_or_replace<ecs::CSpline3d>(e, std::move(s));
	return true;
}

bool serializeNurbsSurface(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CNurbsSurface>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CNurbsSurface>(e);
	j["degreeU"] = s.degreeU;
	j["degreeV"] = s.degreeV;
	j["countU"] = s.countU;
	j["countV"] = s.countV;
	j["controlPoints"] = ordered_json::array();
	for (const auto& p : s.controlPoints) {
		j["controlPoints"].push_back(vec3ToJson(p));
	}
	j["knotsU"] = s.knotsU;
	j["knotsV"] = s.knotsV;
	if (!s.weights.empty()) {
		j["weights"] = s.weights;
	}
	if (s.closedU) {
		j["closedU"] = true;
	}
	if (s.closedV) {
		j["closedV"] = true;
	}
	return true;
}

bool deserializeNurbsSurface(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CNurbsSurface s;
	s.degreeU = j.value("degreeU", s.degreeU);
	s.degreeV = j.value("degreeV", s.degreeV);
	s.countU = j.value("countU", s.countU);
	s.countV = j.value("countV", s.countV);
	s.closedU = j.value("closedU", s.closedU);
	s.closedV = j.value("closedV", s.closedV);
	if (j.contains("controlPoints") && j["controlPoints"].is_array()) {
		for (const auto& p : j["controlPoints"]) {
			s.controlPoints.push_back(vec3FromJson(p));
		}
	}
	if (j.contains("knotsU") && j["knotsU"].is_array()) {
		for (const auto& k : j["knotsU"]) {
			s.knotsU.push_back(k.get<float>());
		}
	}
	if (j.contains("knotsV") && j["knotsV"].is_array()) {
		for (const auto& k : j["knotsV"]) {
			s.knotsV.push_back(k.get<float>());
		}
	}
	if (j.contains("weights") && j["weights"].is_array()) {
		for (const auto& w : j["weights"]) {
			s.weights.push_back(w.get<float>());
		}
	}
	reg.emplace_or_replace<ecs::CNurbsSurface>(e, std::move(s));
	return true;
}

void writeMeshEdges(ordered_json& j, const std::vector<ecs::MeshEdge>& edges) {
	if (edges.empty()) {
		return;
	}
	j["edges"] = ordered_json::array();
	for (const auto& e : edges) {
		const ecs::MeshEdge n = ecs::meshEdge(e.a, e.b);
		j["edges"].push_back(ordered_json{{"a", n.a}, {"b", n.b}});
	}
}

void readMeshEdges(const ordered_json& j, std::vector<ecs::MeshEdge>& edges) {
	edges.clear();
	if (!j.contains("edges") || !j["edges"].is_array()) {
		return;
	}
	for (const auto& item : j["edges"]) {
		if (!item.is_object()) {
			continue;
		}
		edges.push_back(ecs::meshEdge(item.value("a", 0u), item.value("b", 0u)));
	}
}

bool serializeCadBox(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CCadBox>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CCadBox>(e);
	j["sizeX"] = s.sizeX;
	j["sizeY"] = s.sizeY;
	j["sizeZ"] = s.sizeZ;
	if (!s.center) {
		j["center"] = false;
	}
	return true;
}

bool deserializeCadBox(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CCadBox s;
	s.sizeX = j.value("sizeX", s.sizeX);
	s.sizeY = j.value("sizeY", s.sizeY);
	s.sizeZ = j.value("sizeZ", s.sizeZ);
	s.center = j.value("center", s.center);
	reg.emplace_or_replace<ecs::CCadBox>(e, s);
	return true;
}

bool serializeCadCylinder(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CCadCylinder>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CCadCylinder>(e);
	j["radius"] = s.radius;
	j["height"] = s.height;
	if (s.circularSegments >= 3) {
		j["circularSegments"] = s.circularSegments;
	}
	if (!s.center) {
		j["center"] = false;
	}
	return true;
}

bool deserializeCadCylinder(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CCadCylinder s;
	s.radius = j.value("radius", s.radius);
	s.height = j.value("height", s.height);
	s.circularSegments = j.value("circularSegments", s.circularSegments);
	s.center = j.value("center", s.center);
	reg.emplace_or_replace<ecs::CCadCylinder>(e, s);
	return true;
}

bool serializeCadSphere(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CCadSphere>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CCadSphere>(e);
	j["radius"] = s.radius;
	if (s.circularSegments >= 3) {
		j["circularSegments"] = s.circularSegments;
	}
	return true;
}

bool deserializeCadSphere(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CCadSphere s;
	s.radius = j.value("radius", s.radius);
	s.circularSegments = j.value("circularSegments", s.circularSegments);
	reg.emplace_or_replace<ecs::CCadSphere>(e, s);
	return true;
}

bool serializeCadExtrude(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CCadExtrude>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CCadExtrude>(e);
	j["profile"] = s.profile.empty() ? ordered_json(nullptr) : ordered_json(s.profile);
	j["height"] = s.height;
	if (s.nDivisions >= 1) {
		j["nDivisions"] = s.nDivisions;
	}
	if (s.twistDegrees != 0.f) {
		j["twistDegrees"] = s.twistDegrees;
	}
	if (s.scaleTop != 1.f) {
		j["scaleTop"] = s.scaleTop;
	}
	return true;
}

bool deserializeCadExtrude(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CCadExtrude s;
	if (j.contains("profile") && j["profile"].is_string()) {
		s.profile = j["profile"].get<std::string>();
	}
	s.height = j.value("height", s.height);
	s.nDivisions = j.value("nDivisions", s.nDivisions);
	s.twistDegrees = j.value("twistDegrees", s.twistDegrees);
	s.scaleTop = j.value("scaleTop", s.scaleTop);
	reg.emplace_or_replace<ecs::CCadExtrude>(e, s);
	return true;
}

bool serializeCadRevolve(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CCadRevolve>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CCadRevolve>(e);
	j["profile"] = s.profile.empty() ? ordered_json(nullptr) : ordered_json(s.profile);
	if (s.revolveDegrees != 360.f) {
		j["revolveDegrees"] = s.revolveDegrees;
	}
	if (s.circularSegments >= 3) {
		j["circularSegments"] = s.circularSegments;
	}
	return true;
}

bool deserializeCadRevolve(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CCadRevolve s;
	if (j.contains("profile") && j["profile"].is_string()) {
		s.profile = j["profile"].get<std::string>();
	}
	s.revolveDegrees = j.value("revolveDegrees", s.revolveDegrees);
	s.circularSegments = j.value("circularSegments", s.circularSegments);
	reg.emplace_or_replace<ecs::CCadRevolve>(e, s);
	return true;
}

const char* cadBooleanOpName(ecs::CCadBoolean::Op op) {
	switch (op) {
	case ecs::CCadBoolean::Op::Union:
		return "union";
	case ecs::CCadBoolean::Op::Difference:
		return "difference";
	case ecs::CCadBoolean::Op::Intersection:
		return "intersection";
	}
	return "difference";
}

ecs::CCadBoolean::Op cadBooleanOpFromName(const std::string& name) {
	if (name == "union") {
		return ecs::CCadBoolean::Op::Union;
	}
	if (name == "intersection") {
		return ecs::CCadBoolean::Op::Intersection;
	}
	return ecs::CCadBoolean::Op::Difference;
}

bool serializeCadBoolean(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CCadBoolean>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CCadBoolean>(e);
	j["op"] = cadBooleanOpName(s.op);
	j["operands"] = ordered_json::array();
	for (const auto& id : s.operands) {
		j["operands"].push_back(id);
	}
	return true;
}

bool deserializeCadBoolean(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CCadBoolean s;
	s.op = cadBooleanOpFromName(j.value("op", std::string("difference")));
	if (j.contains("operands") && j["operands"].is_array()) {
		for (const auto& id : j["operands"]) {
			if (id.is_string()) {
				s.operands.push_back(id.get<std::string>());
			}
		}
	}
	reg.emplace_or_replace<ecs::CCadBoolean>(e, std::move(s));
	return true;
}

bool serializeCadFillet(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CCadFillet>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CCadFillet>(e);
	j["radius"] = s.radius;
	if (s.allEdges) {
		j["allEdges"] = true;
	} else {
		writeMeshEdges(j, s.edges);
	}
	return true;
}

bool deserializeCadFillet(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CCadFillet s;
	s.radius = j.value("radius", s.radius);
	s.allEdges = j.value("allEdges", s.allEdges);
	if (!s.allEdges) {
		readMeshEdges(j, s.edges);
	}
	reg.emplace_or_replace<ecs::CCadFillet>(e, std::move(s));
	return true;
}

bool serializeCadChamfer(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CCadChamfer>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CCadChamfer>(e);
	j["distance"] = s.distance;
	if (s.allEdges) {
		j["allEdges"] = true;
	} else {
		writeMeshEdges(j, s.edges);
	}
	return true;
}

bool deserializeCadChamfer(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CCadChamfer s;
	s.distance = j.value("distance", s.distance);
	s.allEdges = j.value("allEdges", s.allEdges);
	if (!s.allEdges) {
		readMeshEdges(j, s.edges);
	}
	reg.emplace_or_replace<ecs::CCadChamfer>(e, std::move(s));
	return true;
}

const char* cadDimensionKindName(ecs::CCadDimension::Kind kind) {
	switch (kind) {
	case ecs::CCadDimension::Kind::Linear:
		return "linear";
	case ecs::CCadDimension::Kind::Aligned:
		return "aligned";
	case ecs::CCadDimension::Kind::Horizontal:
		return "horizontal";
	case ecs::CCadDimension::Kind::Vertical:
		return "vertical";
	case ecs::CCadDimension::Kind::Diameter:
		return "diameter";
	case ecs::CCadDimension::Kind::Angle:
		return "angle";
	}
	return "linear";
}

ecs::CCadDimension::Kind cadDimensionKindFromName(const std::string& name) {
	if (name == "aligned") {
		return ecs::CCadDimension::Kind::Aligned;
	}
	if (name == "horizontal") {
		return ecs::CCadDimension::Kind::Horizontal;
	}
	if (name == "vertical") {
		return ecs::CCadDimension::Kind::Vertical;
	}
	if (name == "diameter") {
		return ecs::CCadDimension::Kind::Diameter;
	}
	if (name == "angle") {
		return ecs::CCadDimension::Kind::Angle;
	}
	return ecs::CCadDimension::Kind::Linear;
}

bool serializeCadDimension(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CCadDimension>(e)) {
		return false;
	}
	const auto& s = reg.get<ecs::CCadDimension>(e);
	j["kind"] = cadDimensionKindName(s.kind);
	j["a"] = s.a;
	if (!s.b.empty()) {
		j["b"] = s.b;
	}
	j["value"] = s.value;
	if (s.measurement) {
		j["measurement"] = true;
	}
	if (s.offset != glm::vec3{0.f, 0.f, 0.f}) {
		j["offset"] = vec3ToJson(s.offset);
	}
	return true;
}

bool deserializeCadDimension(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CCadDimension s;
	s.kind = cadDimensionKindFromName(j.value("kind", std::string("linear")));
	s.a = j.value("a", s.a);
	s.b = j.value("b", s.b);
	s.value = j.value("value", s.value);
	s.measurement = j.value("measurement", s.measurement);
	if (j.contains("offset")) {
		s.offset = vec3FromJson(j["offset"], s.offset);
	}
	reg.emplace_or_replace<ecs::CCadDimension>(e, std::move(s));
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
	if (!m.loops.empty()) {
		j["loops"] = m.loops;
		j["loopSizes"] = m.loopSizes;
	}
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
			uvs.push_back(vec2ToJson(t));
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
	if (j.contains("loops") && j["loops"].is_array()) {
		m.loops = j["loops"].get<std::vector<uint32_t>>();
	}
	if (j.contains("loopSizes") && j["loopSizes"].is_array()) {
		m.loopSizes = j["loopSizes"].get<std::vector<uint32_t>>();
	}
	if (j.contains("faceColors") && j["faceColors"].is_array()) {
		for (const auto& c : j["faceColors"]) {
			if (c.is_array() && c.size() >= 3) {
				m.faceColors.push_back(rgbaFromJson(c));
			}
		}
	}
	if (j.contains("facePalette") && j["facePalette"].is_array()) {
		m.facePalette = j["facePalette"].get<std::vector<uint8_t>>();
	}
	if (j.contains("texcoords") && j["texcoords"].is_array()) {
		for (const auto& t : j["texcoords"]) {
			m.texcoords.push_back(vec2FromJson(t));
		}
	}
	if (!m.loopSizes.empty()) {
		ecs::meshTriangulate(m);
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
	j["rgb"] = rgbToJson({l.colorR, l.colorG, l.colorB});
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
	if (j.contains("rgb")) {
		const glm::vec3 rgb = rgbFromJson(j["rgb"], {l.colorR, l.colorG, l.colorB});
		l.colorR = rgb.r;
		l.colorG = rgb.g;
		l.colorB = rgb.b;
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
	for (int i = 0; i < ecs::CPalette::kCount; ++i) {
		colors.push_back(vec4ToJson(p.colors[static_cast<size_t>(i)]));
	}
	j["colors"] = std::move(colors);
	return true;
}

bool deserializePalette(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CPalette p =
		reg.all_of<ecs::CPalette>(e) ? reg.get<ecs::CPalette>(e) : ecs::CPalette::default16();
	if (j.contains("colors") && j["colors"].is_array()) {
		const size_t n = std::min(j["colors"].size(), static_cast<size_t>(ecs::CPalette::kCount));
		for (size_t i = 0; i < n; ++i) {
			p.colors[i] = vec4FromJson(j["colors"][i], p.colors[i]);
		}
	}
	reg.emplace_or_replace<ecs::CPalette>(e, p);
	return true;
}

bool serializePaletteShade(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CPalette>(e)) {
		return false;
	}
	const auto& p = reg.get<ecs::CPalette>(e);
	ordered_json next = ordered_json::array();
	for (int i = 0; i < ecs::CPalette::kCount; ++i) {
		next.push_back(p.shadeNext[static_cast<size_t>(i)]);
	}
	j["shadeNext"] = std::move(next);
	return true;
}

bool deserializePaletteShade(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CPalette p =
		reg.all_of<ecs::CPalette>(e) ? reg.get<ecs::CPalette>(e) : ecs::CPalette::default16();
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

bool serializeEdgeSelection(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CEdgeSelection>(e)) {
		return false;
	}
	writeMeshEdges(j, reg.get<ecs::CEdgeSelection>(e).edges);
	return true;
}

bool deserializeEdgeSelection(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CEdgeSelection s;
	readMeshEdges(j, s.edges);
	reg.emplace_or_replace<ecs::CEdgeSelection>(e, std::move(s));
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
	j["fillRgba"] = colorToJson({d.fillR, d.fillG, d.fillB, d.fillA});
	j["strokeRgba"] = colorToJson({d.strokeR, d.strokeG, d.strokeB, d.strokeA});
	j["strokeWidth"] = d.strokeWidth;
	j["hasFill"] = d.hasFill;
	j["hasStroke"] = d.hasStroke;
	return true;
}

// CDrawStyle spans two wire components, so both codecs patch the same struct
// instead of replacing it â€” whichever key is read second must not wipe the first.
bool deserializeDrawStyle(entt::registry& reg, entt::entity e, const ordered_json& j) {
	auto& d = reg.get_or_emplace<ecs::CDrawStyle>(e);
	if (j.contains("fillRgba")) {
		const glm::vec4 fill = rgbaFromJson(j["fillRgba"], {d.fillR, d.fillG, d.fillB, d.fillA});
		d.fillR = fill.r;
		d.fillG = fill.g;
		d.fillB = fill.b;
		d.fillA = fill.a;
	}
	if (j.contains("strokeRgba")) {
		const glm::vec4 stroke =
			rgbaFromJson(j["strokeRgba"], {d.strokeR, d.strokeG, d.strokeB, d.strokeA});
		d.strokeR = stroke.r;
		d.strokeG = stroke.g;
		d.strokeB = stroke.b;
		d.strokeA = stroke.a;
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
	// Silence on defaults â€” most strokes have nothing to add here, and every
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

glm::vec2 pathPointFrom(const ordered_json& cmd, const char* schemaKey, glm::vec2 fallback = {}) {
	if (cmd.contains(schemaKey)) {
		return vec2FromJson(cmd[schemaKey], fallback);
	}
	return fallback;
}

const char* pathCmdType(ecs::CPath::Cmd t) {
	switch (t) {
	case ecs::CPath::Cmd::MoveTo:
		return "move-to";
	case ecs::CPath::Cmd::LineTo:
		return "line-to";
	case ecs::CPath::Cmd::CubicTo:
		return "cubic-to";
	case ecs::CPath::Cmd::QuadTo:
		return "quad-to";
	case ecs::CPath::Cmd::Close:
		return "close";
	case ecs::CPath::Cmd::ArcTo:
		return "cubic-to";
	}
	return "move-to";
}

bool serializePath(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CPath>(e)) {
		return false;
	}
	const auto& path = reg.get<ecs::CPath>(e);
	auto commands = ordered_json::array();
	glm::vec2 cur{0.f, 0.f};
	auto emit = [&](const ecs::CPath::Command& cmd) {
		ordered_json row;
		row["type"] = pathCmdType(cmd.type);
		switch (cmd.type) {
		case ecs::CPath::Cmd::MoveTo:
		case ecs::CPath::Cmd::LineTo:
			row["point"] = vec2ToJson(cmd.p);
			break;
		case ecs::CPath::Cmd::CubicTo:
			row["point"] = vec2ToJson(cmd.p);
			row["control1"] = vec2ToJson(cmd.c1);
			row["control2"] = vec2ToJson(cmd.c2);
			break;
		case ecs::CPath::Cmd::QuadTo:
			row["point"] = vec2ToJson(cmd.p);
			row["control1"] = vec2ToJson(cmd.c1);
			break;
		case ecs::CPath::Cmd::Close:
		case ecs::CPath::Cmd::ArcTo:
			break;
		}
		commands.push_back(std::move(row));
	};
	for (const auto& cmd : path.commands) {
		if (cmd.type == ecs::CPath::Cmd::ArcTo) {
			ecs::CPath cubics;
			path::appendCubics(path::fromCommand(cur, cmd), cubics);
			for (const auto& cubic : cubics.commands) {
				emit(cubic);
			}
			cur = cmd.p;
			continue;
		}
		emit(cmd);
		if (cmd.type != ecs::CPath::Cmd::Close) {
			cur = cmd.p;
		}
	}
	j["commands"] = std::move(commands);
	return true;
}

bool deserializePath(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CPath path;
	if (j.contains("commands") && j["commands"].is_array()) {
		for (const auto& cmd : j["commands"]) {
			const std::string type = cmd.value("type", std::string("move-to"));
			ecs::CPath::Command row;
			if (type == "line-to") {
				row.type = ecs::CPath::Cmd::LineTo;
				row.p = pathPointFrom(cmd, "point");
			} else if (type == "cubic-to") {
				row.type = ecs::CPath::Cmd::CubicTo;
				row.p = pathPointFrom(cmd, "point");
				row.c1 = pathPointFrom(cmd, "control1");
				row.c2 = pathPointFrom(cmd, "control2");
			} else if (type == "quad-to") {
				row.type = ecs::CPath::Cmd::QuadTo;
				row.p = pathPointFrom(cmd, "point");
				row.c1 = pathPointFrom(cmd, "control1");
			} else if (type == "close") {
				row.type = ecs::CPath::Cmd::Close;
			} else {
				row.type = ecs::CPath::Cmd::MoveTo;
				row.p = pathPointFrom(cmd, "point");
			}
			path.commands.push_back(row);
		}
	}
	reg.emplace_or_replace<ecs::CPath>(e, std::move(path));
	return true;
}

bool serializeLayer(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CLayer>(e)) {
		return false;
	}
	const auto& l = reg.get<ecs::CLayer>(e);
	j["order"] = l.order;
	j["locked"] = l.locked;
	j["rgba"] = colorToJson({l.colorR, l.colorG, l.colorB, l.colorA});
	return true;
}

bool deserializeLayer(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CLayer l;
	if (reg.all_of<ecs::CLayer>(e)) {
		l = reg.get<ecs::CLayer>(e);
	}
	l.order = j.value("order", l.order);
	l.locked = j.value("locked", l.locked);
	if (j.contains("rgba")) {
		const glm::vec4 rgba = rgbaFromJson(j["rgba"], {l.colorR, l.colorG, l.colorB, l.colorA});
		l.colorR = rgba.r;
		l.colorG = rgba.g;
		l.colorB = rgba.b;
		l.colorA = rgba.a;
	}
	reg.emplace_or_replace<ecs::CLayer>(e, l);
	return true;
}

bool serializeLayerVisible(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CLayer>(e)) {
		return false;
	}
	const auto& l = reg.get<ecs::CLayer>(e);
	if (l.visible) {
		return false; // default on the wire
	}
	j["visible"] = false;
	return true;
}

bool deserializeLayerVisible(entt::registry& reg, entt::entity e, const ordered_json& j) {
	auto& l = reg.get_or_emplace<ecs::CLayer>(e);
	l.visible = j.value("visible", l.visible);
	return true;
}

const char* assetKindName(ecs::CAssetRef::Kind k) {
	switch (k) {
	case ecs::CAssetRef::Kind::Image:
		return "image";
	case ecs::CAssetRef::Kind::Audio:
		return "audio";
	case ecs::CAssetRef::Kind::Video:
		return "video";
	case ecs::CAssetRef::Kind::Model:
		return "model";
	case ecs::CAssetRef::Kind::Font:
		return "font";
	case ecs::CAssetRef::Kind::Document:
		return "document";
	case ecs::CAssetRef::Kind::Other:
		return "other";
	}
	return "other";
}

ecs::CAssetRef::Kind assetKindFromName(const std::string& name) {
	if (name == "image") {
		return ecs::CAssetRef::Kind::Image;
	}
	if (name == "audio") {
		return ecs::CAssetRef::Kind::Audio;
	}
	if (name == "video") {
		return ecs::CAssetRef::Kind::Video;
	}
	if (name == "model") {
		return ecs::CAssetRef::Kind::Model;
	}
	if (name == "font") {
		return ecs::CAssetRef::Kind::Font;
	}
	if (name == "document") {
		return ecs::CAssetRef::Kind::Document;
	}
	return ecs::CAssetRef::Kind::Other;
}

bool serializeAssetRef(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CAssetRef>(e)) {
		return false;
	}
	const auto& a = reg.get<ecs::CAssetRef>(e);
	j["kind"] = assetKindName(a.kind);
	j["path"] = a.path;
	if (a.loop) {
		j["loop"] = true;
	}
	return true;
}

bool deserializeAssetRef(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CAssetRef a;
	a.kind = assetKindFromName(j.value("kind", std::string("other")));
	a.path = j.value("path", a.path);
	a.loop = j.value("loop", a.loop);
	reg.emplace_or_replace<ecs::CAssetRef>(e, std::move(a));
	return true;
}

bool serializeText(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CText>(e)) {
		return false;
	}
	const auto& t = reg.get<ecs::CText>(e);
	j["text"] = t.text;
	if (t.font != entt::null) {
		j["font"] = entityIdString(t.font);
	}
	j["fontSize"] = t.fontSize;
	if (!t.axes.empty()) {
		auto axes = ordered_json::array();
		for (const auto& ax : t.axes) {
			axes.push_back(ordered_json{{"tag", ax.tag}, {"value", ax.value}});
		}
		j["axes"] = std::move(axes);
	}
	if (!t.features.empty()) {
		j["features"] = t.features;
	}
	if (!t.useKerning) {
		j["useKerning"] = false;
	}
	return true;
}

bool deserializeText(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CText t;
	t.text = j.value("text", t.text);
	if (j.contains("font") && j["font"].is_string()) {
		t.font = entityIdFromString(j["font"].get<std::string>());
	} else {
		t.font = entt::null;
	}
	t.fontSize = j.value("fontSize", t.fontSize);
	if (j.contains("axes") && j["axes"].is_array()) {
		for (const auto& ax : j["axes"]) {
			ecs::FontAxisValue row;
			row.tag = ax.value("tag", 0u);
			row.value = ax.value("value", 0.f);
			t.axes.push_back(row);
		}
	}
	t.features = j.value("features", t.features);
	t.useKerning = j.value("useKerning", t.useKerning);
	reg.emplace_or_replace<ecs::CText>(e, std::move(t));
	return true;
}

void remapTextFont(entt::registry& reg, const EntityIdMap& idMap) {
	for (auto entity : reg.view<ecs::CText>()) {
		auto& t = reg.get<ecs::CText>(entity);
		if (t.font == entt::null) {
			continue;
		}
		const auto it = idMap.find(static_cast<std::uint32_t>(t.font));
		t.font = (it != idMap.end()) ? it->second : entt::null;
	}
}

bool serializeCurve(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CCurve>(e)) {
		return false;
	}
	const auto& c = reg.get<ecs::CCurve>(e);
	auto points = ordered_json::array();
	for (const auto& p : c.points) {
		points.push_back(vec2ToJson(p));
	}
	j["points"] = std::move(points);
	j["interpolation"] = curve::interpId(c.interp);
	if (c.preset != ecs::CCurve::Preset::Custom || c.points.size() >= 2) {
		j["preset"] = curve::presetId(c.preset);
	}
	return true;
}

bool deserializeCurve(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CCurve c;
	if (j.contains("points") && j["points"].is_array()) {
		c.points.clear();
		for (const auto& p : j["points"]) {
			c.points.push_back(vec2FromJson(p));
		}
	}
	if (j.contains("interpolation") && j["interpolation"].is_string()) {
		c.interp = curve::interpFromId(j["interpolation"].get<std::string>());
	} else if (j.contains("interp") && j["interp"].is_string()) {
		c.interp = curve::interpFromId(j["interp"].get<std::string>());
	}
	if (j.contains("preset") && j["preset"].is_string()) {
		c.preset = curve::presetFromId(j["preset"].get<std::string>());
	}
	curve::ensureValid(c);
	reg.emplace_or_replace<ecs::CCurve>(e, std::move(c));
	return true;
}

void registerInto(project::ComponentSerializerRegistry& registry) {
	using project::addBoolMemberSerializer;
	using project::addMarkerSerializer;
	using project::addSerializer;
	using project::EntityIdMap;

	// Contract schemas where one exists; x.rigkit.* where the concept is ours.
	addSerializer<ecs::CTransform>(registry, "Transform", "rig.spatial.transform",
								   serializeTransform, deserializeTransform);
	addSerializer<ecs::CRelationship>(registry, "Relationship", "rig.spatial.relationship",
									  serializeRelationship, deserializeRelationship);
	addMarkerSerializer<ecs::CGroup>(registry, "Group", "rig.spatial.group");
	addSerializer<ecs::CCamera>(registry, "Camera", "rig.spatial.camera", serializeCamera,
								deserializeCamera);
	addBoolMemberSerializer<ecs::CSelectable, &ecs::CSelectable::enabled>(
		registry, "Selectable", "rig.interact.selectable", "enabled");
	addSerializer<ecs::CDrawStyle>(registry, "DrawStyle", "rig.paint.fill_stroke",
								   serializeDrawStyle, deserializeDrawStyle);
	addSerializer<ecs::CLight>(registry, "Light", "rig.render.light", serializeLight,
							   deserializeLight);
	addSerializer<ecs::CModLfo>(registry, "ModLfo", "rig.mod.lfo", serializeModLfo,
								deserializeModLfo);
	addSerializer<ecs::CModBinding>(registry, "ModBinding", "rig.mod.binding", serializeModBinding,
									deserializeModBinding);
	addSerializer<ecs::CTween>(registry, "Tween", "rig.anim.tween", serializeTween,
							   deserializeTween);
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
	addSerializer<ecs::CSpline>(registry, "Spline", "rig.geometry.spline", serializeSpline,
								deserializeSpline);
	addSerializer<ecs::CSpline3d>(registry, "Spline3d", "rig.geometry.spline3d", serializeSpline3d,
								  deserializeSpline3d);
	addSerializer<ecs::CNurbsSurface>(registry, "NurbsSurface", "rig.geometry.nurbs_surface",
									  serializeNurbsSurface, deserializeNurbsSurface);
	addSerializer<ecs::CCadBox>(registry, "CadBox", "rig.cad.box", serializeCadBox,
								deserializeCadBox);
	addSerializer<ecs::CCadCylinder>(registry, "CadCylinder", "rig.cad.cylinder",
									 serializeCadCylinder, deserializeCadCylinder);
	addSerializer<ecs::CCadSphere>(registry, "CadSphere", "rig.cad.sphere", serializeCadSphere,
								   deserializeCadSphere);
	addSerializer<ecs::CCadExtrude>(registry, "CadExtrude", "rig.cad.extrude", serializeCadExtrude,
									deserializeCadExtrude);
	addSerializer<ecs::CCadRevolve>(registry, "CadRevolve", "rig.cad.revolve", serializeCadRevolve,
									deserializeCadRevolve);
	addSerializer<ecs::CCadBoolean>(registry, "CadBoolean", "rig.cad.boolean", serializeCadBoolean,
									deserializeCadBoolean);
	addSerializer<ecs::CCadFillet>(registry, "CadFillet", "rig.cad.fillet", serializeCadFillet,
								   deserializeCadFillet);
	addSerializer<ecs::CCadChamfer>(registry, "CadChamfer", "rig.cad.chamfer", serializeCadChamfer,
									deserializeCadChamfer);
	addSerializer<ecs::CCadDimension>(registry, "CadDimension", "rig.cad.dimension",
									  serializeCadDimension, deserializeCadDimension);
	addSerializer<ecs::CRing>(registry, "Ring", "rig.geometry.ring", serializeRing,
							  deserializeRing);
	addSerializer<ecs::CMesh>(registry, "Mesh", "rig.geometry.mesh", serializeMesh,
							  deserializeMesh);
	addSerializer<ecs::CPath>(registry, "Path", "rig.geometry.path", serializePath,
							  deserializePath);

	addSerializer<ecs::CLayer>(registry, "Layer", "rig.spatial.layer", serializeLayer,
							   deserializeLayer);
	addSerializer<ecs::CLayer>(registry, "LayerVisible", "x.rigkit.layer_visible",
							   serializeLayerVisible, deserializeLayerVisible);
	addSerializer<ecs::CAssetRef>(registry, "AssetRef", "rig.media.asset_ref", serializeAssetRef,
								  deserializeAssetRef);
	addSerializer<ecs::CText>(
		registry, "Text", "rig.media.text", serializeText, deserializeText,
		[](entt::registry& reg, const EntityIdMap& idMap) { remapTextFont(reg, idMap); });
	addSerializer<ecs::CCurve>(registry, "Curve", "rig.anim.curve", serializeCurve,
							   deserializeCurve);

	addSerializer<ecs::CPalette>(registry, "Palette", "rig.pixel.palette", serializePalette,
								 deserializePalette);
	addSerializer<ecs::CPalette>(registry, "PaletteShade", "x.rigkit.palette_shade",
								 serializePaletteShade, deserializePaletteShade);
	// No Contract schema covers these yet, so they travel as host extensions.
	addSerializer<ecs::CGuide>(registry, "Guide", "x.rigkit.guide", serializeGuide,
							   deserializeGuide);
	addSerializer<ecs::CIndexedAtlas>(registry, "IndexedAtlas", "x.rigkit.indexed_atlas",
									  serializeIndexedAtlas, deserializeIndexedAtlas);
	addSerializer<ecs::CFaceSelection>(registry, "FaceSelection", "x.rigkit.face_selection",
									   serializeFaceSelection, deserializeFaceSelection);
	addSerializer<ecs::CEdgeSelection>(registry, "EdgeSelection", "x.rigkit.edge_selection",
									   serializeEdgeSelection, deserializeEdgeSelection);
}

} // namespace

namespace project {

void registerComponentSerializers(ComponentSerializerRegistry& registry) {
	registerInto(registry);
}

} // namespace project

} // namespace rigkit
