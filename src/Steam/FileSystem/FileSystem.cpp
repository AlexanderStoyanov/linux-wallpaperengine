#include "FileSystem.h"
#include "WallpaperEngine/Logging/Log.h"
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <sys/stat.h>

static const std::vector<std::string> defaultSteamRoots = {
    ".local/share/Steam",
    ".steam/steam",
    ".var/app/com.valvesoftware.Steam/.local/share/Steam",
    "snap/steam/common/.local/share/Steam",
};

static std::filesystem::path detectHomepath () {
    char* home = getenv ("HOME");

    if (home == nullptr) {
	sLog.exception ("Cannot find home directory for the current user");
    }

    const std::filesystem::path path = home;

    if (!std::filesystem::is_directory (path)) {
	sLog.exception ("Cannot find home directory for current user, ", home, " is not a directory");
    }

    return path;
}

// Parse libraryfolders.vdf to discover all Steam library root paths.
// The VDF format has entries like:  "path"    "/some/path"
static std::vector<std::filesystem::path> discoverLibraryRoots () {
    auto homepath = detectHomepath ();
    std::set<std::string> seen;
    std::vector<std::filesystem::path> roots;

    auto tryAdd = [&] (const std::filesystem::path& p) {
	if (!std::filesystem::is_directory (p / "steamapps"))
	    return;
	auto canonical = std::filesystem::canonical (p).string ();
	if (seen.count (canonical))
	    return;
	seen.insert (canonical);
	roots.push_back (p);
    };

    for (const auto& rel : defaultSteamRoots)
	tryAdd (homepath / rel);

    // Parse libraryfolders.vdf from each known root
    static const std::regex pathRe (R"re("path"\s+"([^"]+)")re");
    for (size_t i = 0; i < roots.size (); i++) {
	auto vdf = roots [i] / "steamapps" / "libraryfolders.vdf";
	if (!std::filesystem::is_regular_file (vdf))
	    continue;
	std::ifstream ifs (vdf);
	std::string line;
	while (std::getline (ifs, line)) {
	    std::smatch m;
	    if (std::regex_search (line, m, pathRe))
		tryAdd (m [1].str ());
	}
    }

    return roots;
}

std::filesystem::path Steam::FileSystem::workshopDirectory (int appID, const std::string& contentID) {
    auto roots = discoverLibraryRoots ();

    for (const auto& root : roots) {
	auto currentpath = root / "steamapps" / "workshop" / "content" / std::to_string (appID) / contentID;

	if (std::filesystem::is_directory (currentpath))
	    return currentpath;
    }

    sLog.exception ("Cannot find workshop directory for steam app ", appID, " and content ", contentID);
}

std::filesystem::path Steam::FileSystem::appDirectory (const std::string& appDirectory, const std::string& path) {
    auto roots = discoverLibraryRoots ();

    for (const auto& root : roots) {
	auto currentpath = root / "steamapps" / "common" / appDirectory / path;

	if (std::filesystem::is_directory (currentpath))
	    return currentpath;
    }

    sLog.exception ("Cannot find directory for steam app ", appDirectory, ": ", path);
}