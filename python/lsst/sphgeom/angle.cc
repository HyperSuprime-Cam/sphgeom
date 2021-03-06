/*
 * LSST Data Management System
 * See COPYRIGHT file at the top of the source tree.
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
#include "pybind11/pybind11.h"

#include "lsst/sphgeom/Angle.h"
#include "lsst/sphgeom/NormalizedAngle.h"

namespace py = pybind11;
using namespace pybind11::literals;

namespace lsst {
namespace sphgeom {
namespace {

PYBIND11_MODULE(angle, mod) {
    py::class_<Angle> cls(mod, "Angle");

    cls.def_static("nan", &Angle::nan);
    cls.def_static("fromDegrees", &Angle::fromDegrees);
    cls.def_static("fromRadians", &Angle::fromRadians);

    cls.def(py::init<>());
    cls.def(py::init<double>(), "radians"_a);
    cls.def(py::init<Angle>(), "angle"_a);
    // Construct an Angle from a NormalizedAngle, enabling implicit
    // conversion from NormalizedAngle to Angle in python via
    // py::implicitly_convertible
    cls.def(py::init(
            [](NormalizedAngle &a) {
                return new Angle(a.asRadians());
            }),
            "normalizedAngle"_a);

    cls.def("__eq__", &Angle::operator==, py::is_operator());
    cls.def("__ne__", &Angle::operator!=, py::is_operator());
    cls.def("__lt__", &Angle::operator<, py::is_operator());
    cls.def("__gt__", &Angle::operator>, py::is_operator());
    cls.def("__le__", &Angle::operator<=, py::is_operator());
    cls.def("__ge__", &Angle::operator>=, py::is_operator());

    cls.def("__neg__", (Angle(Angle::*)() const) & Angle::operator-);
    cls.def("__add__", &Angle::operator+, py::is_operator());
    cls.def("__sub__",
            (Angle(Angle::*)(Angle const &) const) & Angle::operator-,
            py::is_operator());
    cls.def("__mul__", &Angle::operator*, py::is_operator());
    cls.def("__rmul__", &Angle::operator*, py::is_operator());
    cls.def("__truediv__", (Angle(Angle::*)(double) const) & Angle::operator/,
            py::is_operator());
    cls.def("__truediv__",
            (double (Angle::*)(Angle const &) const) & Angle::operator/,
            py::is_operator());

    cls.def("__iadd__", &Angle::operator+=);
    cls.def("__isub__", &Angle::operator-=);
    cls.def("__imul__", &Angle::operator*=);
    cls.def("__itruediv__", &Angle::operator/=);

    cls.def("asDegrees", &Angle::asDegrees);
    cls.def("asRadians", &Angle::asRadians);
    cls.def("isNormalized", &Angle::isNormalized);
    cls.def("isNan", &Angle::isNan);

    cls.def("__str__", [](Angle const &self) {
        return py::str("{!s}").format(self.asRadians());
    });
    cls.def("__repr__", [](Angle const &self) {
        return py::str("Angle({!r})").format(self.asRadians());
    });

    cls.def("__reduce__", [cls](Angle const &self) {
        return py::make_tuple(cls, py::make_tuple(self.asRadians()));
    });
}

}  // <anonymous>
}  // sphgeom
}  // lsst
