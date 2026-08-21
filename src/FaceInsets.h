#pragma once

/**
 * @file FaceInsets.h
 * @brief Six face insets on a local AABB (same cuboid as rig.spatial.anchor).
 * @details Channel order matches rig.layout.page margins/bleed/slug:
 * top, right, bottom, left, then optional floor/ceiling (Z faces).
 * Host Space: +X right, +Y up, +Z up. Margins inset inward; bleed/slug outward.
 */

#include "ProjectJson.h"

namespace rigkit {
namespace project {

struct FaceInsets {
	float top = 0.f;
	float right = 0.f;
	float bottom = 0.f;
	float left = 0.f;
	float floor = 0.f;	  ///< inset/extent from min-Z face
	float ceiling = 0.f; ///< inset/extent from max-Z face

	bool xyAllZero() const {
		return top == 0.f && right == 0.f && bottom == 0.f && left == 0.f;
	}
	bool allZero() const { return xyAllZero() && floor == 0.f && ceiling == 0.f; }

	void setUniformXy(float v) { top = right = bottom = left = v; }
	void setUniformZ(float v) { floor = ceiling = v; }
};

/**
 * @brief Expand Contract margin/bleed/slug wire into six channels.
 * @details Accepts a number or array length 1–6 (CSS + optional Z pair).
 * Length 5 → both Z faces get the fifth value. Lengths 1–4 leave Z at 0.
 */
template <typename Json>
bool expandFaceInsets(const Json& src, FaceInsets& out) {
	FaceInsets v;
	if (src.is_number()) {
		v.setUniformXy(src.template get<float>());
		out = v;
		return true;
	}
	if (!src.is_array() || src.empty() || src.size() > 6) {
		return false;
	}
	const auto n = static_cast<int>(src.size());
	auto at = [&](int i) { return src[static_cast<size_t>(i)].template get<float>(); };
	switch (n) {
	case 1:
		v.setUniformXy(at(0));
		break;
	case 2:
		v.top = v.bottom = at(0);
		v.left = v.right = at(1);
		break;
	case 3:
		v.top = at(0);
		v.left = v.right = at(1);
		v.bottom = at(2);
		break;
	case 4:
		v.top = at(0);
		v.right = at(1);
		v.bottom = at(2);
		v.left = at(3);
		break;
	case 5:
		v.top = at(0);
		v.right = at(1);
		v.bottom = at(2);
		v.left = at(3);
		v.setUniformZ(at(4));
		break;
	case 6:
		v.top = at(0);
		v.right = at(1);
		v.bottom = at(2);
		v.left = at(3);
		v.floor = at(4);
		v.ceiling = at(5);
		break;
	default:
		return false;
	}
	out = v;
	return true;
}

/** @brief Write shortest equivalent array (or omit when all zero). */
template <typename Json>
void writeFaceInsets(Json& j, const char* key, const FaceInsets& in) {
	if (in.allZero()) {
		return;
	}
	const bool zZero = in.floor == 0.f && in.ceiling == 0.f;
	if (zZero && in.top == in.right && in.right == in.bottom && in.bottom == in.left) {
		j[key] = in.top;
		return;
	}
	if (zZero && in.top == in.bottom && in.left == in.right) {
		j[key] = Json::array({in.top, in.left});
		return;
	}
	if (zZero && in.left == in.right) {
		j[key] = Json::array({in.top, in.left, in.bottom});
		return;
	}
	if (zZero) {
		j[key] = Json::array({in.top, in.right, in.bottom, in.left});
		return;
	}
	if (in.floor == in.ceiling) {
		j[key] = Json::array({in.top, in.right, in.bottom, in.left, in.floor});
		return;
	}
	j[key] = Json::array({in.top, in.right, in.bottom, in.left, in.floor, in.ceiling});
}

} // namespace project
} // namespace rigkit
