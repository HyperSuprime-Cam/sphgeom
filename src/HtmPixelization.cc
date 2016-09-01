/*
 * LSST Data Management System
 * Copyright 2016 AURA/LSST.
 *
 * This product includes software developed by the
 * LSST Project (http://www.lsst.org/).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the LSST License Statement and
 * the GNU General Public License along with this program.  If not,
 * see <https://www.lsstcorp.org/LegalNotices/>.
 */

/// \file
/// \brief This file contains the HtmPixelization class implementation.

#include "lsst/sphgeom/HtmPixelization.h"

#include "lsst/sphgeom/curve.h"
#include "lsst/sphgeom/orientation.h"

#include "PixelFinder.h"


namespace lsst {
namespace sphgeom {

namespace {

// Raw HTM root vertex coordinates. UnitVector3d objects are avoided so that
// library clients cannot run into the static initialization order fiasco.
alignas(64) double const HTM_ROOT_VERTEX[8][3][3] = {
    {{ 1.0,  0.0, 0.0}, {0.0, 0.0, -1.0}, { 0.0,  1.0, 0.0}},
    {{ 0.0,  1.0, 0.0}, {0.0, 0.0, -1.0}, {-1.0,  0.0, 0.0}},
    {{-1.0,  0.0, 0.0}, {0.0, 0.0, -1.0}, { 0.0, -1.0, 0.0}},
    {{ 0.0, -1.0, 0.0}, {0.0, 0.0, -1.0}, { 1.0,  0.0, 0.0}},
    {{ 1.0,  0.0, 0.0}, {0.0, 0.0,  1.0}, { 0.0, -1.0, 0.0}},
    {{ 0.0, -1.0, 0.0}, {0.0, 0.0,  1.0}, {-1.0,  0.0, 0.0}},
    {{-1.0,  0.0, 0.0}, {0.0, 0.0,  1.0}, { 0.0,  1.0, 0.0}},
    {{ 0.0,  1.0, 0.0}, {0.0, 0.0,  1.0}, { 1.0,  0.0, 0.0}}
};

// `HtmPixelFinder` locates trixels that intersect a region.
template <typename RegionType, bool InteriorOnly>
class HtmPixelFinder: public detail::PixelFinder<
    HtmPixelFinder<RegionType, InteriorOnly>, RegionType, InteriorOnly, 3>
{
    using Base = detail::PixelFinder<
        HtmPixelFinder<RegionType, InteriorOnly>, RegionType, InteriorOnly, 3>;
    using Base::visit;

public:
    HtmPixelFinder(RangeSet & ranges,
                   RegionType const & region,
                   int level,
                   size_t maxRanges):
        Base(ranges, region, level, maxRanges)
    {}

    void operator()() {
        UnitVector3d trixel[3];
        // Loop over HTM root triangles.
        for (uint64_t r = 0; r < 8; ++r) {
            for (int v = 0; v < 3; ++v) {
                trixel[v] = UnitVector3d::fromNormalized(
                    HTM_ROOT_VERTEX[r][v][0],
                    HTM_ROOT_VERTEX[r][v][1],
                    HTM_ROOT_VERTEX[r][v][2]
                );
            }
            visit(trixel, r + 8, 0);
        }
    }

    void subdivide(UnitVector3d const * trixel, uint64_t index, int level) {
        UnitVector3d mid[3] = {
            UnitVector3d(trixel[1] + trixel[2]),
            UnitVector3d(trixel[2] + trixel[0]),
            UnitVector3d(trixel[0] + trixel[1])
        };
        UnitVector3d child[3] = {trixel[0], mid[2], mid[1]};
        index *= 4;
        ++level;
        visit(child, index, level);
        child[0] = trixel[1];
        child[1] = mid[0];
        child[2] = mid[2];
        ++index;
        visit(child, index, level);
        child[0] = trixel[2];
        child[1] = mid[1];
        child[2] = mid[0];
        ++index;
        visit(child, index, level);
        ++index;
        visit(mid, index, level);
    }
};

} // unnamed namespace


int HtmPixelization::level(uint64_t i) {
    // An HTM index consists of 4 bits encoding the root triangle
    // number (8 - 15), followed by 2l bits, where each of the l bit pairs
    // encodes a child triangle number (0-3), and l is the desired level.
    int j = log2(i);
    // The level l is derivable from the index j of the MSB of i.
    // For i to be valid, j must be an odd integer > 1.
    if ((j & 1) == 0 || (j == 1)) {
        return -1;
    }
    return (j - 3) >> 1;
}

ConvexPolygon HtmPixelization::triangle(uint64_t i) {
    int l = level(i);
    if (l < 0 || l > MAX_LEVEL) {
        throw std::invalid_argument("Invalid HTM index");
    }
    l *= 2;
    uint64_t r = (i >> l) & 7;
    UnitVector3d v0 = UnitVector3d::fromNormalized(
        HTM_ROOT_VERTEX[r][0][0],
        HTM_ROOT_VERTEX[r][0][1],
        HTM_ROOT_VERTEX[r][0][2]
    );
    UnitVector3d v1 = UnitVector3d::fromNormalized(
        HTM_ROOT_VERTEX[r][1][0],
        HTM_ROOT_VERTEX[r][1][1],
        HTM_ROOT_VERTEX[r][1][2]
    );
    UnitVector3d v2 = UnitVector3d::fromNormalized(
        HTM_ROOT_VERTEX[r][2][0],
        HTM_ROOT_VERTEX[r][2][1],
        HTM_ROOT_VERTEX[r][2][2]
    );
    for (l -= 2; l >= 0; l -= 2) {
        int child = (i >> l) & 3;
        UnitVector3d m12 = UnitVector3d(v1 + v2);
        UnitVector3d m20 = UnitVector3d(v2 + v0);
        UnitVector3d m01 = UnitVector3d(v0 + v1);
        switch (child) {
            case 0: v1 = m01; v2 = m20; break;
            case 1: v0 = v1; v1 = m12; v2 = m01; break;
            case 2: v0 = v2; v1 = m20; v2 = m12; break;
            case 3: v0 = m12; v1 = m20; v2 = m01; break;
        }
    }
    return ConvexPolygon(v0, v1, v2);
}

std::string HtmPixelization::toString(uint64_t i) {
    char s[MAX_LEVEL + 2];
    int l = level(i);
    if (l < 0 || l > MAX_LEVEL) {
        throw std::invalid_argument("Invalid HTM index");
    }
    // Print in base-4, from least to most significant digit.
    char * p = s + (sizeof(s) - 1);
    for (; l >= 0; --l, --p, i >>= 2) {
        *p = '0' + (i & 3);
    }
    // The remaining bit corresponds to the hemisphere.
    *p = (i & 1) == 0 ? 'S' : 'N';
    return std::string(p, sizeof(s) - static_cast<size_t>(p - s));
}

HtmPixelization::HtmPixelization(int level) : _level(level) {
    if (level < 0 || level > MAX_LEVEL) {
        throw std::invalid_argument("Invalid HTM subdivision level");
    }
}

uint64_t HtmPixelization::index(UnitVector3d const & v) const {
    // Find the root triangle containing v.
    uint64_t r;
    if (v.z() < 0.0) {
        // v is in the southern hemisphere (root triangle 0, 1, 2, or 3).
        if (v.y() > 0.0) {
            r = (v.x() > 0.0) ? 0 : 1;
        } else if (v.y() == 0.0) {
            r = (v.x() >= 0.0) ? 0 : 2;
        } else {
            r = (v.x() < 0.0) ? 2 : 3;
        }
    } else {
        // v is in the northern hemisphere (root triangle 4, 5, 6, or 7).
        if (v.y() > 0.0) {
            r = (v.x() > 0.0) ? 7 : 6;
        } else if (v.y() == 0.0) {
            r = (v.x() >= 0.0) ? 7 : 5;
        } else {
            r = (v.x() < 0.0) ? 5 : 4;
        }
    }
    UnitVector3d v0 = UnitVector3d::fromNormalized(
        HTM_ROOT_VERTEX[r][0][0],
        HTM_ROOT_VERTEX[r][0][1],
        HTM_ROOT_VERTEX[r][0][2]
    );
    UnitVector3d v1 = UnitVector3d::fromNormalized(
        HTM_ROOT_VERTEX[r][1][0],
        HTM_ROOT_VERTEX[r][1][1],
        HTM_ROOT_VERTEX[r][1][2]
    );
    UnitVector3d v2 = UnitVector3d::fromNormalized(
        HTM_ROOT_VERTEX[r][2][0],
        HTM_ROOT_VERTEX[r][2][1],
        HTM_ROOT_VERTEX[r][2][2]
    );
    uint64_t i = r + 8;
    for (int l = 0; l < _level; ++l) {
        UnitVector3d m01 = UnitVector3d(v0 + v1);
        UnitVector3d m20 = UnitVector3d(v2 + v0);
        i <<= 2;
        if (orientation(v, m01, m20) >= 0) {
            v1 = m01; v2 = m20;
            continue;
        }
        UnitVector3d m12 = UnitVector3d(v1 + v2);
        if (orientation(v, m12, m01) >= 0) {
            v0 = v1; v1 = m12; v2 = m01;
            i += 1;
        } else if (orientation(v, m20, m12) >= 0) {
            v0 = v2; v1 = m20; v2 = m12;
            i += 2;
        } else {
            v0 = m12; v1 = m20; v2 = m01;
            i += 3;
        }
    }
    return i;
}

RangeSet HtmPixelization::_envelope(Region const & r, size_t maxRanges) const {
    return detail::findPixels<HtmPixelFinder, false>(r, maxRanges, _level);
}

RangeSet HtmPixelization::_interior(Region const & r, size_t maxRanges) const {
    return detail::findPixels<HtmPixelFinder, true>(r, maxRanges, _level);
}

}} // namespace lsst::sphgeom
