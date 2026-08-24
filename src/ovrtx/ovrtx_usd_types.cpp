/****************************************************************************
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "ovrtx_usd_types.h"

#include <cctype>
#include <string>
#include <string_view>

namespace Mayo {
namespace Ovrtx {

Vec3f zUpToYUp(Vec3f p)
{
    // OCCT Z-up (X right, Y forward, Z up) -> USD Y-up (X right, Y up, Z back).
    return { p.x, p.z, -p.y };
}

std::string sanitizePrimName(std::string_view raw, int fallbackIndex)
{
    std::string out;
    out.reserve(raw.size() + 8);
    for (char c : raw) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || c == '_')
            out.push_back(c);
        else
            out.push_back('_');
    }
    while (!out.empty() && out.front() == '_')
        out.erase(out.begin());
    if (out.empty() || std::isdigit(static_cast<unsigned char>(out.front())))
        out = "Mesh_" + std::to_string(fallbackIndex) + (out.empty() ? std::string() : ("_" + out));
    return out;
}

} // namespace Ovrtx
} // namespace Mayo
