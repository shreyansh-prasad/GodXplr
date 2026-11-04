#include "godxplr/power/batch_renamer.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <unordered_set>
#include <cwctype>

namespace gx::power {

static std::wstring applyCase(const std::wstring& str, CaseTransform mode) {
    if (mode == CaseTransform::None || str.empty()) {
        return str;
    }

    std::wstring result = str;
    if (mode == CaseTransform::Uppercase) {
        for (auto& c : result) c = static_cast<wchar_t>(std::towupper(c));
    } else if (mode == CaseTransform::Lowercase) {
        for (auto& c : result) c = static_cast<wchar_t>(std::towlower(c));
    } else if (mode == CaseTransform::TitleCase) {
        bool newWord = true;
        for (auto& c : result) {
            if (std::iswspace(c) || c == L'_' || c == L'-' || c == L'.') {
                newWord = true;
            } else if (newWord) {
                c = static_cast<wchar_t>(std::towupper(c));
                newWord = false;
            } else {
                c = static_cast<wchar_t>(std::towlower(c));
            }
        }
    }
    return result;
}

std::wstring BatchRenamer::transformName(
    const std::wstring& filename,
    const RenameRule& rule,
    int itemIndex)
{
    std::filesystem::path p(filename);
    std::wstring stem = p.stem().wstring();
    std::wstring ext = p.extension().wstring();

    std::wstring working = stem;

    // Plain text find-and-replace
    if (!rule.findPattern.empty()) {
        size_t pos = 0;
        while (pos < working.size()) {
            size_t matchPos = std::wstring::npos;
            if (rule.matchCase) {
                matchPos = working.find(rule.findPattern, pos);
            } else {
                // Case-insensitive scan
                for (size_t i = pos; i + rule.findPattern.size() <= working.size(); ++i) {
                    bool matches = true;
                    for (size_t j = 0; j < rule.findPattern.size(); ++j) {
                        if (std::towlower(working[i + j]) != std::towlower(rule.findPattern[j])) {
                            matches = false;
                            break;
                        }
                    }
                    if (matches) {
                        matchPos = i;
                        break;
                    }
                }
            }

            if (matchPos == std::wstring::npos) {
                break;
            }

            working.replace(matchPos, rule.findPattern.size(), rule.replacePattern);
            pos = matchPos + rule.replacePattern.size();
        }
    }

    // Replace <counter> token
    size_t counterToken = working.find(L"<counter>");
    if (counterToken != std::wstring::npos) {
        int val = rule.counterStart + itemIndex * rule.counterStep;
        std::wostringstream wss;
        wss << std::setfill(L'0') << std::setw(rule.counterPadding) << val;
        working.replace(counterToken, 9, wss.str());
    }

    // Apply case transformation
    working = applyCase(working, rule.caseMode);

    return working + ext;
}

BatchRenamePlan BatchRenamer::generatePlan(
    const std::vector<std::wstring>& inputPaths,
    const RenameRule& rule)
{
    BatchRenamePlan plan;
    plan.items.reserve(inputPaths.size());

    std::unordered_set<std::wstring> proposedSet;

    for (size_t i = 0; i < inputPaths.size(); ++i) {
        const auto& orig = inputPaths[i];
        std::filesystem::path p(orig);
        auto parent = p.parent_path();
        std::wstring newFilename = transformName(p.filename().wstring(), rule, static_cast<int>(i));
        std::wstring proposed = (parent / newFilename).wstring();

        RenameItemPreview item;
        item.originalPath = orig;
        item.proposedPath = proposed;

        // Collision Check 1: Duplicate proposed name within the batch
        if (proposedSet.find(proposed) != proposedSet.end()) {
            item.hasConflict = true;
            item.conflictReason = L"Name collision within batch";
            plan.totalConflicts++;
        } else {
            proposedSet.insert(proposed);
        }

        // Collision Check 2: Clashes with existing file on disk (not part of original file)
        if (!item.hasConflict && proposed != orig) {
            DWORD attrs = GetFileAttributesW(proposed.c_str());
            if (attrs != INVALID_FILE_ATTRIBUTES) {
                item.hasConflict = true;
                item.conflictReason = L"Destination file already exists on disk";
                plan.totalConflicts++;
            }
        }

        plan.items.push_back(std::move(item));
    }

    plan.canCommit = (plan.totalConflicts == 0);
    return plan;
}

} // namespace gx::power

