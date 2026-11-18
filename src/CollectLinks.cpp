#include "Rigz.h"

#include "ecs/MEcs.h"
#include "CAssetRef.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <spdlog/spdlog.h>
#include <system_error>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace rigkit {
namespace project {
namespace {

using ecs::CAssetRef;

bool isCollectableKind(CAssetRef::Kind kind) {
	switch (kind) {
	case CAssetRef::Kind::Image:
	case CAssetRef::Kind::Audio:
	case CAssetRef::Kind::Video:
	case CAssetRef::Kind::Model:
	case CAssetRef::Kind::Font:
		return true;
	default:
		return false;
	}
}

/// Package subfolder under data/ for this kind (links/ for media, fonts/ for faces).
const char* collectSubdir(CAssetRef::Kind kind) {
	return kind == CAssetRef::Kind::Font ? "fonts" : "links";
}

fs::path canonicalOrNormal(const fs::path& p) {
	std::error_code ec;
	fs::path out = fs::weakly_canonical(p, ec);
	if (ec) {
		return p.lexically_normal();
	}
	return out;
}

/// @return Path of @p p relative to @p base when @p p is inside @p base, else empty.
fs::path relativeInside(const fs::path& base, const fs::path& p) {
	const fs::path rel = canonicalOrNormal(p).lexically_relative(canonicalOrNormal(base));
	if (rel.empty() || *rel.begin() == "..") {
		return {};
	}
	return rel;
}

bool sameBytes(const fs::path& a, const fs::path& b) {
	std::error_code ec;
	if (fs::file_size(a, ec) != fs::file_size(b, ec) || ec) {
		return false;
	}
	std::ifstream fa(a, std::ios::binary);
	std::ifstream fb(b, std::ios::binary);
	if (!fa || !fb) {
		return false;
	}
	return std::equal(std::istreambuf_iterator<char>(fa), std::istreambuf_iterator<char>(),
					  std::istreambuf_iterator<char>(fb));
}

/// Pick a name in @p linksDir for @p src: reuse an identical file, else add `-N`.
fs::path destinationFor(const fs::path& linksDir, const fs::path& src,
						const std::map<fs::path, fs::path>& claimed) {
	std::error_code ec;
	for (int n = 0; n < 10000; ++n) {
		fs::path name = src.stem();
		if (n > 0) {
			name += "-" + std::to_string(n);
		}
		name += src.extension();
		const fs::path dest = linksDir / name;
		const auto taken = claimed.find(dest);
		if (taken != claimed.end()) {
			if (taken->second == src) {
				return dest;
			}
			continue;
		}
		if (!fs::exists(dest, ec) || sameBytes(src, dest)) {
			return dest;
		}
	}
	return linksDir / src.filename();
}

} // namespace

bool collectLinkedAssets(MEcs& ecs, const std::string& packetRoot, std::string* error) {
	if (packetRoot.empty()) {
		if (error) {
			*error = "no package root to collect links into";
		}
		return false;
	}
	const fs::path root(packetRoot);
	const fs::path dataDir = root / "data";

	struct Rewrite {
		entt::entity entity;
		std::string path;
	};
	std::vector<Rewrite> rewrites;
	std::map<fs::path, fs::path> claimed;  // destination file to source it came from
	std::map<fs::path, fs::path> bySource; // canonical source to destination file
	std::error_code ec;

	for (auto e : ecs.view<CAssetRef>()) {
		const auto& ref = ecs.getComponent<CAssetRef>(e);
		if (!isCollectableKind(ref.kind) || ref.path.empty()) {
			continue;
		}
		fs::path src(ref.path);
		if (!src.is_absolute()) {
			const fs::path resolved(resolvePacketAssetPath(packetRoot, ref.path));
			if (resolved.is_relative() && !fs::exists(resolved, ec)) {
				spdlog::warn("[rigz] link not found, left as is: {}", ref.path);
				continue;
			}
			src = resolved;
		}
		if (!fs::is_regular_file(src, ec)) {
			spdlog::warn("[rigz] link not found, left as is: {}", ref.path);
			continue;
		}
		src = canonicalOrNormal(src);

		if (const fs::path underData = relativeInside(dataDir, src); !underData.empty()) {
			// Already packaged: store the path relative to data/ (no copy).
			const std::string rel = underData.generic_string();
			if (rel != ref.path) {
				rewrites.push_back({e, rel});
			}
			continue;
		}
		if (const fs::path underAssets = relativeInside(root / "assets", src);
			!underAssets.empty()) {
			// assets/ also travels in the .rigz; resolves against the package root.
			const std::string rel = "assets/" + underAssets.generic_string();
			if (rel != ref.path) {
				rewrites.push_back({e, rel});
			}
			continue;
		}

		const char* sub = collectSubdir(ref.kind);
		const fs::path outDir = dataDir / sub;
		fs::path dest;
		if (const auto known = bySource.find(src); known != bySource.end()) {
			dest = known->second;
		} else {
			fs::create_directories(outDir, ec);
			if (ec) {
				if (error) {
					*error = "cannot create " + outDir.string();
				}
				return false;
			}
			dest = destinationFor(outDir, src, claimed);
			if (!fs::exists(dest, ec)) {
				fs::copy_file(src, dest, fs::copy_options::none, ec);
				if (ec) {
					if (error) {
						*error = "cannot copy " + src.filename().string() + " into package";
					}
					return false;
				}
			}
			claimed[dest] = src;
			bySource[src] = dest;
		}
		rewrites.push_back({e, std::string(sub) + "/" + dest.filename().generic_string()});
	}

	// Rewrite only after every copy succeeded.
	for (const auto& r : rewrites) {
		ecs.getComponent<CAssetRef>(r.entity).path = r.path;
	}
	if (!rewrites.empty()) {
		spdlog::info("[rigz] collected {} link(s) under {}", rewrites.size(), dataDir.string());
	}
	return true;
}

} // namespace project
} // namespace rigkit
