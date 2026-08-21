#include "Rigz.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <system_error>
#include <vector>

#include <miniz.h>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace rigkit {
namespace project {
namespace {

std::string lowerExt(std::string s) {
	for (char& c : s) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return s;
}

bool endsWithCi(const std::string& s, const char* suffix) {
	const std::string needle = suffix;
	if (s.size() < needle.size()) {
		return false;
	}
	return lowerExt(s.substr(s.size() - needle.size())) == lowerExt(needle);
}

bool isIgnoredZipName(const std::string& name) {
	if (name.rfind("__MACOSX/", 0) == 0) {
		return true;
	}
	return name.find("/.DS_Store") != std::string::npos || name == ".DS_Store";
}

bool isRootRigName(const std::string& name) {
	if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) {
		return false;
	}
	return endsWithCi(name, ".rig") && name.size() > 4;
}

bool safeZipPath(const std::string& name) {
	if (name.empty() || name.front() == '/' || name.find("..") != std::string::npos) {
		return false;
	}
	return true;
}

fs::path makeTempPackageDir() {
	const auto base = fs::temp_directory_path() / "rigkit-rigz";
	std::error_code ec;
	fs::create_directories(base, ec);
	std::mt19937_64 rng{static_cast<std::uint64_t>(
		std::chrono::steady_clock::now().time_since_epoch().count())};
	for (int i = 0; i < 64; ++i) {
		const auto dir = base / ("pkg-" + std::to_string(rng()));
		if (fs::create_directory(dir, ec)) {
			return dir;
		}
	}
	const auto fallback = base / ("pkg-" + std::to_string(rng()));
	fs::create_directories(fallback, ec);
	return fallback;
}

PackageOpen openRigzArchive(const std::string& path) {
	PackageOpen out;
	out.displayPath = path;

	std::ifstream in(path, std::ios::binary);
	if (!in) {
		out.error = "cannot open .rigz";
		return out;
	}
	std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (bytes.empty()) {
		out.error = "empty .rigz";
		return out;
	}

	mz_zip_archive zip{};
	if (!mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0)) {
		out.error = "not a ZIP archive (.rigz)";
		return out;
	}

	const fs::path root = makeTempPackageDir();
	out.packetRoot = root.string();
	out.temporary = true;

	std::string rootRigRel;
	const mz_uint n = mz_zip_reader_get_num_files(&zip);
	for (mz_uint i = 0; i < n; ++i) {
		mz_zip_archive_file_stat st{};
		if (!mz_zip_reader_file_stat(&zip, i, &st)) {
			continue;
		}
		std::string name = st.m_filename;
		std::replace(name.begin(), name.end(), '\\', '/');
		if (isIgnoredZipName(name) || !safeZipPath(name)) {
			continue;
		}
		if (st.m_is_directory) {
			fs::create_directories(root / name);
			continue;
		}
		if (isRootRigName(name)) {
			if (!rootRigRel.empty()) {
				mz_zip_reader_end(&zip);
				out.ok = false;
				out.error = "ZIP has multiple root .rig files; there must be exactly one";
				releasePackage(out);
				return out;
			}
			rootRigRel = name;
		}
		const fs::path dest = root / name;
		fs::create_directories(dest.parent_path());
		size_t size = 0;
		void* data = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
		if (!data) {
			mz_zip_reader_end(&zip);
			out.error = "failed to extract " + name;
			releasePackage(out);
			return out;
		}
		{
			std::ofstream file(dest, std::ios::binary);
			file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
		}
		mz_free(data);
	}
	mz_zip_reader_end(&zip);

	if (rootRigRel.empty()) {
		out.error = "ZIP has no .rig at the archive root";
		releasePackage(out);
		return out;
	}

	out.rigPath = (root / rootRigRel).string();
	out.ok = true;
	spdlog::info("[rigz] extracted {} → {}", path, out.packetRoot);
	return out;
}

} // namespace

bool isRigzPath(const std::string& path) {
	return endsWithCi(path, ".rigz");
}

PackageOpen openPackage(const std::string& path) {
	PackageOpen out;
	out.displayPath = path;
	if (path.empty()) {
		out.error = "empty path";
		return out;
	}
	std::error_code ec;
	if (!fs::exists(path, ec)) {
		out.error = "file not found";
		return out;
	}
	if (isRigzPath(path)) {
		return openRigzArchive(path);
	}
	if (!endsWithCi(path, ".rig")) {
		out.error = "expected .rig or .rigz";
		return out;
	}
	out.rigPath = path;
	out.packetRoot = fs::path(path).parent_path().string();
	out.ok = true;
	return out;
}

void releasePackage(PackageOpen& opened) {
	if (!opened.temporary || opened.packetRoot.empty()) {
		opened.temporary = false;
		return;
	}
	std::error_code ec;
	fs::remove_all(opened.packetRoot, ec);
	if (ec) {
		spdlog::warn("[rigz] could not remove temp package {}: {}", opened.packetRoot, ec.message());
	}
	opened.temporary = false;
	opened.packetRoot.clear();
	opened.rigPath.clear();
}

std::string resolvePacketAssetPath(const std::string& packetRoot, const std::string& assetPath) {
	if (assetPath.empty()) {
		return assetPath;
	}
	fs::path p(assetPath);
	if (p.is_absolute()) {
		return assetPath;
	}
	std::error_code ec;
	if (!packetRoot.empty()) {
		const fs::path underData = fs::path(packetRoot) / "data" / p;
		if (fs::exists(underData, ec)) {
			return underData.string();
		}
		const fs::path underRoot = fs::path(packetRoot) / p;
		if (fs::exists(underRoot, ec)) {
			return underRoot.string();
		}
	}
	return assetPath;
}

} // namespace project
} // namespace rigkit
