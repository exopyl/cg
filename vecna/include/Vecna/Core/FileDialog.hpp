#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Vecna::Core {

/// Filter for file dialogs specifying description and allowed extensions.
struct FileFilter {
    std::string description;  ///< Human-readable description (e.g., "Fichiers 3D")
    std::string extensions;   ///< Space-separated extensions without dots (e.g., "obj stl")
};

/// Cross-platform file dialog utility class.
/// Uses native OS dialogs via portable-file-dialogs library.
class FileDialog {
public:
    /// Opens a file selection dialog.
    /// @param title Dialog window title.
    /// @param filters List of file filters to display.
    /// @return Path to selected file, or std::nullopt if cancelled.
    [[nodiscard]] static std::optional<std::filesystem::path> openFile(
        const std::string& title,
        const std::vector<FileFilter>& filters
    );

    /// Opens a file selection dialog with default 3D model filters.
    /// Filters: "Fichiers 3D" (obj, stl), "Wavefront OBJ" (obj), "STL" (stl)
    /// @return Path to selected file, or std::nullopt if cancelled.
    [[nodiscard]] static std::optional<std::filesystem::path> openModel();
};

} // namespace Vecna::Core
