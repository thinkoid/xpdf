// -*- mode: c++; -*-

#include <xpdf/object.hh>

namespace xpdf {
namespace ast {

namespace detail {

std::vector< ast::object_t >
object_array_from (XRef* p) {
    return { };
}

} // namespace detail

/* explicit */ array_t::array_t (XRef* p) { }

}}
