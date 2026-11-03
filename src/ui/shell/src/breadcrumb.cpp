#include "godxplr/ui/shell/breadcrumb.h"
#include <sstream>

namespace gx::ui {

std::vector<BreadcrumbSegment> BreadcrumbParser::parse(std::string_view uri) {
    std::vector<BreadcrumbSegment> segments;
    if (uri.empty() || uri == "godxplr://this-pc") {
        segments.push_back(BreadcrumbSegment{"This PC", "godxplr://this-pc", true});
        return segments;
    }
    if (uri == "godxplr://trash") {
        segments.push_back(BreadcrumbSegment{"Recycle Bin", "godxplr://trash", true});
        return segments;
    }

    std::string path(uri);
    for (char& c : path) {
        if (c == '/') c = '\\';
    }

    // Windows drive path: e.g. "C:\Users\Lenovo"
    if (path.size() >= 2 && path[1] == ':') {
        segments.push_back(BreadcrumbSegment{"This PC", "godxplr://this-pc", true});

        std::string driveLetter(1, path[0]);
        std::string driveRoot = driveLetter + ":\\";
        std::string driveLabel = "Local Disk (" + driveLetter + ":)";

        if (path.size() <= 3) {
            segments.push_back(BreadcrumbSegment{driveLabel, driveRoot, false});
            return segments;
        }

        segments.push_back(BreadcrumbSegment{driveLetter + ":", driveRoot, false});

        // Tokenize remaining segments after "C:\"
        size_t start = 3;
        std::string currentAccum = driveLetter + ":";

        while (start < path.size()) {
            size_t nextSep = path.find('\\', start);
            std::string part;
            if (nextSep == std::string::npos) {
                part = path.substr(start);
                start = path.size();
            } else {
                part = path.substr(start, nextSep - start);
                start = nextSep + 1;
            }

            if (!part.empty()) {
                currentAccum += "\\" + part;
                segments.push_back(BreadcrumbSegment{part, currentAccum, false});
            }
        }
        return segments;
    }

    // UNC network path: e.g. "\\server\share\folder"
    if (path.rfind("\\\\", 0) == 0) {
        segments.push_back(BreadcrumbSegment{"Network", "godxplr://network", true});
        size_t secondSep = path.find('\\', 2);
        if (secondSep != std::string::npos) {
            size_t thirdSep = path.find('\\', secondSep + 1);
            std::string serverShare = (thirdSep == std::string::npos) ? path : path.substr(0, thirdSep);
            segments.push_back(BreadcrumbSegment{serverShare, serverShare, false});

            if (thirdSep != std::string::npos) {
                size_t start = thirdSep + 1;
                std::string currentAccum = serverShare;
                while (start < path.size()) {
                    size_t nextSep = path.find('\\', start);
                    std::string part = (nextSep == std::string::npos) ? path.substr(start) : path.substr(start, nextSep - start);
                    start = (nextSep == std::string::npos) ? path.size() : nextSep + 1;
                    if (!part.empty()) {
                        currentAccum += "\\" + part;
                        segments.push_back(BreadcrumbSegment{part, currentAccum, false});
                    }
                }
            }
            return segments;
        }
    }

    // Fallback: single generic segment
    segments.push_back(BreadcrumbSegment{std::string(uri), std::string(uri), false});
    return segments;
}

} // namespace gx::ui
