#include "Vecna/Core/FileDialog.hpp"
#include "Vecna/Core/Logger.hpp"

#include <portable-file-dialogs.h>
#include <sstream>

namespace Vecna::Core {

std::optional<std::filesystem::path> FileDialog::openFile(
    const std::string& title,
    const std::vector<FileFilter>& filters
) {
    // Convert filters to pfd format: pairs of (description, pattern)
    // pfd expects patterns like "*.obj *.stl" for multiple extensions
    std::vector<std::string> pfdFilters;
    for (const auto& filter : filters) {
        pfdFilters.push_back(filter.description);

        // Convert "obj stl" to "*.obj *.stl"
        std::string pattern;
        std::istringstream iss(filter.extensions);
        std::string ext;
        while (iss >> ext) {
            if (!pattern.empty()) {
                pattern += " ";
            }
            pattern += "*." + ext;
        }
        pfdFilters.push_back(pattern);
    }

    Logger::debug("Core", "Opening file dialog: " + title);

    auto selection = pfd::open_file(title, ".", pfdFilters).result();

    if (selection.empty()) {
        Logger::debug("Core", "File dialog cancelled");
        return std::nullopt;
    }

    std::filesystem::path path(selection[0]);
    Logger::info("Core", "File selected: " + path.filename().string());

    return path;
}

std::optional<std::filesystem::path> FileDialog::openModel() {
    return openFile(
        "Ouvrir un modele 3D",
        {
            {"Fichiers 3D", "obj stl"},
            {"Wavefront OBJ", "obj"},
            {"STL", "stl"}
        }
    );
}

} // namespace Vecna::Core
