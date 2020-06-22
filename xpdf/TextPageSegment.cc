// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#include <defs.hh>

#include <xpdf/xpdf.hh>
#include <xpdf/TextChar.hh>
#include <xpdf/TextPage.hh>

#include <range/v3/iterator.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/transform.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/reverse.hpp>
using namespace ranges;

template< xpdf::rotation_t R, typename T >
inline bool reading_order(const T &lhs, const T &rhs)
{
    const auto &a = bbox_from(lhs).arr;
    const auto &b = bbox_from(rhs).arr;
    return do_reading_order< R >(a[0], a[1], b[0], b[1]);
}

inline xpdf::bbox_t dilate_horizontally(xpdf::bbox_t x, double factor = .10)
{
    auto delta = factor * height_of(x);
    x.arr[0] -= delta;
    x.arr[2] += delta;
    return x;
}

inline xpdf::bbox_t dilate_vertically(xpdf::bbox_t x, double factor = .15)
{
    auto delta = factor * height_of(x);
    x.arr[1] -= delta;
    x.arr[3] += delta;
    return x;
}

template< typename T >
std::vector< xpdf::bbox_t >
simple_aggregate(const std::vector< xpdf::bbox_t > &xs, T test)
{
    if (xs.size() < 2) {
        return xs;
    }

    std::vector< xpdf::bbox_t > Xs;

    for (auto &x : xs) {
        bool coalesced = false;

        for (auto &X : Xs | views::reverse) {
            if (test(X, x)) {
                X = coalesce(X, x);
                coalesced = true;
                break;
            }
        }

        if (!coalesced) {
            Xs.push_back(x);
        }
    }

    return Xs;
}

template< typename T >
std::vector< xpdf::bbox_t > aggregate(std::vector< xpdf::bbox_t > xs, T test)
{
    if (xs.size() < 2) {
        return xs;
    }

    std::vector< xpdf::bbox_t > Xs;

    for (;; xs = std::move(Xs)) {
        Xs = simple_aggregate(xs, test);

        if (xs.size() == Xs.size()) {
            break;
        }
    }

    return Xs;
}

std::vector< xpdf::bbox_t > aggregate(std::vector< xpdf::bbox_t > letters)
{
    if (letters.size() < 2) {
        return letters;
    }

    auto test = [](double factor = .10) {
        return [=](auto &lhs, auto &rhs) {
            return overlapping(lhs, dilate_horizontally(rhs, factor));
        };
    };

    auto words = aggregate(letters, test());
    auto lines = aggregate(words, test(.50));

    auto test2 = [](double factor = .15) {
        return [=](auto &lhs, auto &rhs) {
            return overlapping(lhs, dilate_vertically(rhs, factor));
        };
    };

    auto paras = simple_aggregate(lines, test2());

    return paras;
}

inline auto rotate_by(int rotation)
{
    using namespace xpdf;
    switch (rotation) {
    default:
    case 0:
        return xpdf::rotate< rotation_t::none, double >;
    case 3:
        return xpdf::rotate< rotation_t::quarter_turn, double >;
    case 2:
        return xpdf::rotate< rotation_t::half_turn, double >;
    case 1:
        return xpdf::rotate< rotation_t::three_quarters_turn, double >;
    }
}

inline auto undo_rotate_by(int rotation)
{
    using namespace xpdf;
    switch (rotation) {
    default:
    case 0:
        return unrotate< rotation_t::none, double >;
    case 3:
        return unrotate< rotation_t::quarter_turn, double >;
    case 2:
        return unrotate< rotation_t::half_turn, double >;
    case 1:
        return unrotate< rotation_t::three_quarters_turn, double >;
    }
}

template< typename Fn >
inline void do_upright(std::vector< xpdf::bbox_t > &xs, const xpdf::bbox_t &X,
                       Fn fn)
{
    for_each(xs, [&](auto &x) { x = fn(x, X); });
}

inline void upright(std::vector< xpdf::bbox_t > &xs, int rot,
                    const xpdf::bbox_t &X)
{
    do_upright(xs, X, rotate_by(rot));
};

inline void undo_upright(std::vector< xpdf::bbox_t > &xs, int rot,
                         const xpdf::bbox_t &X)
{
    do_upright(xs, X, undo_rotate_by(rot));
};

std::vector< xpdf::bbox_t > TextPage::segment() const
{
    using namespace xpdf;

    const xpdf::bbox_t          superbox{ 0, 0, pageWidth, pageHeight };
    std::vector< xpdf::bbox_t > boxes;

    for (int rotation : { 0, 1, 2, 3 }) {
        std::vector< xpdf::bbox_t > xs;

        auto rng = chars |
                   views::filter([=](auto &ch) { return rotation == ch->rot; });

        transform(rng, back_inserter(xs), [](auto &ch) { return bbox_from(ch); });

        upright(xs, rotation, superbox);

        xs = aggregate(xs);
        undo_upright(xs, rotation, superbox);

        boxes.insert(boxes.end(), xs.begin(), xs.end());
    }

    return boxes;
}
