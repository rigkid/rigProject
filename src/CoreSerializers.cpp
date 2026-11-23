#include "core/TypeJson.h"
#include "AddSerializer.h"
#include "CBookBinding.h"
#include "CBookPublication.h"
#include "CPage.h"
#include "FaceInsets.h"
#include "ProjectSerializer.h"

namespace rigkit {
namespace project {
namespace {

void writePageInsets(ordered_json& j, const char* key, float top, float right, float bottom,
					 float left, float zFloor, float zCeiling) {
	FaceInsets in;
	in.top = top;
	in.right = right;
	in.bottom = bottom;
	in.left = left;
	in.floor = zFloor;
	in.ceiling = zCeiling;
	writeFaceInsets(j, key, in);
}

void readPageInsets(const ordered_json& j, const char* key, float& top, float& right, float& bottom,
					float& left, float& zFloor, float& zCeiling) {
	if (!j.contains(key)) {
		return;
	}
	FaceInsets in;
	if (!expandFaceInsets(j[key], in)) {
		return;
	}
	top = in.top;
	right = in.right;
	bottom = in.bottom;
	left = in.left;
	zFloor = in.floor;
	zCeiling = in.ceiling;
}

bool serializePage(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CPage>(e)) {
		return false;
	}
	const auto& p = reg.get<ecs::CPage>(e);
	j["width"] = p.width;
	j["height"] = p.height;
	if (p.index != 0) {
		j["index"] = p.index;
	}
	if (!p.unit.empty()) {
		j["unit"] = p.unit;
	}
	writePageInsets(j, "margins", p.marginTop, p.marginRight, p.marginBottom, p.marginLeft,
					p.marginFloor, p.marginCeiling);
	writePageInsets(j, "bleed", p.bleedTop, p.bleedRight, p.bleedBottom, p.bleedLeft, p.bleedFloor,
					p.bleedCeiling);
	writePageInsets(j, "slug", p.slugTop, p.slugRight, p.slugBottom, p.slugLeft, p.slugFloor,
					p.slugCeiling);
	if (p.keepWithNext) {
		j["keepWithNext"] = true;
	}
	return true;
}

bool deserializePage(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CPage p;
	p.index = j.value("index", p.index);
	p.unit = j.value("unit", p.unit);
	p.width = j.value("width", p.width);
	p.height = j.value("height", p.height);
	readPageInsets(j, "margins", p.marginTop, p.marginRight, p.marginBottom, p.marginLeft,
				   p.marginFloor, p.marginCeiling);
	readPageInsets(j, "bleed", p.bleedTop, p.bleedRight, p.bleedBottom, p.bleedLeft, p.bleedFloor,
				   p.bleedCeiling);
	readPageInsets(j, "slug", p.slugTop, p.slugRight, p.slugBottom, p.slugLeft, p.slugFloor,
				   p.slugCeiling);
	p.keepWithNext = j.value("keepWithNext", false);
	reg.emplace_or_replace<ecs::CPage>(e, p);
	return true;
}

bool serializeBookBinding(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CBookBinding>(e)) {
		return false;
	}
	const auto& b = reg.get<ecs::CBookBinding>(e);
	static const char* const kForm[] = {"paperback", "case", "jacket", "saddle"};
	const int fi = static_cast<int>(b.form);
	if (fi >= 0 && fi < 4) {
		j["form"] = kForm[fi];
	}
	j["paperCaliper"] = b.paperCaliper;
	j["caliperUnit"] = (b.caliperUnit == ecs::BookCaliperUnit::In) ? "in" : "mm";
	if (b.boardThickness > 0.f) {
		j["boardThickness"] = b.boardThickness;
	}
	if (b.hinge > 0.f) {
		j["hinge"] = b.hinge;
	}
	if (b.wrap > 0.f) {
		j["wrap"] = b.wrap;
	}
	if (b.flap > 0.f) {
		j["flap"] = b.flap;
	}
	if (b.fold > 0.f) {
		j["fold"] = b.fold;
	}
	if (b.hasSpineOverride) {
		j["spineOverride"] = b.spineOverride;
	}
	return true;
}

bool deserializeBookBinding(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CBookBinding b;
	const std::string form = j.value("form", std::string("paperback"));
	if (form == "case") {
		b.form = ecs::BookBindingForm::Case;
	} else if (form == "jacket") {
		b.form = ecs::BookBindingForm::Jacket;
	} else if (form == "saddle") {
		b.form = ecs::BookBindingForm::Saddle;
	} else {
		b.form = ecs::BookBindingForm::Paperback;
	}
	b.paperCaliper = j.value("paperCaliper", b.paperCaliper);
	const std::string unit = j.value("caliperUnit", std::string("mm"));
	b.caliperUnit = (unit == "in") ? ecs::BookCaliperUnit::In : ecs::BookCaliperUnit::Mm;
	b.boardThickness = j.value("boardThickness", 0.f);
	b.hinge = j.value("hinge", 0.f);
	b.wrap = j.value("wrap", 0.f);
	b.flap = j.value("flap", 0.f);
	b.fold = j.value("fold", 0.f);
	if (j.contains("spineOverride")) {
		b.hasSpineOverride = true;
		b.spineOverride = j.value("spineOverride", 0.f);
	}
	reg.emplace_or_replace<ecs::CBookBinding>(e, std::move(b));
	return true;
}

bool serializeBookPublication(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CBookPublication>(e)) {
		return false;
	}
	const auto& p = reg.get<ecs::CBookPublication>(e);
	if (p.pageCount > 0) {
		j["pageCount"] = p.pageCount;
	}
	static const char* const kForm[] = {"hardcover", "paperback", "ebook", "audiobook", "other"};
	if (p.productForm >= 0 && p.productForm < 5) {
		j["productForm"] = kForm[p.productForm];
	}
	return !j.empty();
}

bool deserializeBookPublication(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CBookPublication p;
	p.pageCount = j.value("pageCount", 0u);
	const std::string form = j.value("productForm", std::string("paperback"));
	if (form == "hardcover") {
		p.productForm = 0;
	} else if (form == "ebook") {
		p.productForm = 2;
	} else if (form == "audiobook") {
		p.productForm = 3;
	} else if (form == "other") {
		p.productForm = 4;
	} else {
		p.productForm = 1;
	}
	reg.emplace_or_replace<ecs::CBookPublication>(e, std::move(p));
	return true;
}

void registerInto(ComponentSerializerRegistry& registry) {
	addSerializer<ecs::CPage>(registry, "Page", "rig.layout.page", serializePage, deserializePage);
	addSerializer<ecs::CBookBinding>(registry, "BookBinding", "rig.book.binding",
									 serializeBookBinding, deserializeBookBinding);
	addSerializer<ecs::CBookPublication>(registry, "BookPublication", "rig.book.publication",
										 serializeBookPublication, deserializeBookPublication);
}

} // namespace

void registerPageSerializers(ComponentSerializerRegistry& registry) {
	registerInto(registry);
}

} // namespace project
} // namespace rigkit
