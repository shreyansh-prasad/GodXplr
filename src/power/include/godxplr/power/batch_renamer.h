#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace gx::power {

enum class CaseTransform : uint8_t {
    None,
    Uppercase,
    Lowercase,
    TitleCase
};

struct RenameRule {
    std::wstring findPattern;
    std::wstring replacePattern;
    bool useRegex{false};
    bool matchCase{false};
    CaseTransform caseMode{CaseTransform::None};
    int counterStart{1};
    int counterStep{1};
    int counterPadding{3}; // e.g. 001
};

struct RenameItemPreview {
    std::wstring originalPath;
    std::wstring proposedPath;
    bool hasConflict{false};
    std::wstring conflictReason;
};

struct BatchRenamePlan {
    std::vector<RenameItemPreview> items;
    size_t totalConflicts{0};
    bool canCommit{false};
};

class BatchRenamer {
public:
    BatchRenamer() = default;
    ~BatchRenamer() = default;

    // Evaluates rules across a list of file paths and generates a preview with collision detection
    static BatchRenamePlan generatePlan(
        const std::vector<std::wstring>& inputPaths,
        const RenameRule& rule);

    // Transforms a single filename string according to the rule and index
    static std::wstring transformName(
        const std::wstring& filename,
        const RenameRule& rule,
        int itemIndex);
};

} // namespace gx::power
