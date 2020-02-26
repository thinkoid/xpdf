// -*- mode: c++ -*-
// Copyright 2019-2020 Thinkoid, LLC.

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE xpdf

#include <defs.hh>

#include <iostream>
#include <exception>

#include <boost/type_index.hpp>
#include <boost/test/unit_test.hpp>
namespace utf = boost::unit_test;

#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>
namespace data = boost::unit_test::data;

#include <xpdf/bbox.hh>
#include <xpdf/xpdf.hh>

BOOST_AUTO_TEST_SUITE(box)

static const std::vector<
    std::tuple< xpdf::bbox_t, xpdf::bbox_t >
    >
normalize_dataset = {
    { {   1, 2, 0, 4 }, { 0, 2, 1, 4 } },
    { {   1, 4, 0, 2 }, { 0, 2, 1, 4 } },
    { {   0, 4, 1, 2 }, { 0, 2, 1, 4 } }
};

BOOST_DATA_TEST_CASE(
    normalize_, data::make (normalize_dataset), box, result) {

    using namespace xpdf;

    {
        auto value = normalize (box);
        BOOST_TEST (value == result);
    }
}

BOOST_AUTO_TEST_CASE(width_of_) {
    using namespace xpdf;

    {
        auto box = bbox_t{ 2, 10, 14, 12 };
        BOOST_TEST (12 == width_of (box));
    }
}

BOOST_AUTO_TEST_CASE(height_of_) {
    using namespace xpdf;

    {
        auto box = bbox_t{ 2, 10, 14, 12 };
        BOOST_TEST (2 == height_of (box));
    }
}

static const std::vector<
    std::tuple< xpdf::bbox_t, xpdf::bbox_t, double >
    >
horizontal_overlap_dataset = {
    { { 10,  5, 20, 10 }, { 30, 15, 40, 20 },  0 },
    { { 10,  5, 29, 10 }, { 30, 15, 40, 20 },  0 },
    { { 10,  5, 30, 10 }, { 30, 15, 40, 20 },  0 },
    { { 10,  5, 31, 10 }, { 30, 15, 40, 20 },  1 },
    { { 10,  5, 35, 10 }, { 30, 15, 40, 20 },  5 },
    { { 10,  5, 42, 10 }, { 30, 15, 40, 20 }, 10 },
    { { 10,  5, 52, 10 }, { 30, 15, 40, 20 }, 10 },
    { { 29,  5, 52, 10 }, { 30, 15, 40, 20 }, 10 },
    { { 32,  5, 52, 10 }, { 30, 15, 40, 20 },  8 },
    { { 39,  5, 52, 10 }, { 30, 15, 40, 20 },  1 },
    { { 40,  5, 52, 10 }, { 30, 15, 40, 20 },  0 },
    { { 41,  5, 52, 10 }, { 30, 15, 40, 20 },  0 }
};

BOOST_DATA_TEST_CASE(
    horizontal_overlap_, data::make (horizontal_overlap_dataset),
    lhs, rhs, result) {

    using namespace xpdf;

    {
        auto value = horizontal_overlap (lhs, rhs);
        BOOST_TEST (value == result);
    }
}

static const std::vector<
    std::tuple< xpdf::bbox_t, xpdf::bbox_t, double >
    >
vertical_overlap_dataset = {
    { { 10,  5, 20, 10 }, { 30, 15, 40, 20 },  0 },
    { { 10,  5, 20, 14 }, { 30, 15, 40, 20 },  0 },
    { { 10,  5, 20, 15 }, { 30, 15, 40, 20 },  0 },
    { { 10,  5, 20, 16 }, { 30, 15, 40, 20 },  1 },
    { { 10,  5, 20, 19 }, { 30, 15, 40, 20 },  4 },
    { { 10,  5, 20, 20 }, { 30, 15, 40, 20 },  5 },
    { { 10,  5, 20, 21 }, { 30, 15, 40, 20 },  5 },
    { { 10,  5, 20, 30 }, { 30, 15, 40, 20 },  5 },
    { { 10, 14, 20, 30 }, { 30, 15, 40, 20 },  5 },
    { { 10, 15, 20, 30 }, { 30, 15, 40, 20 },  5 },
    { { 10, 16, 20, 30 }, { 30, 15, 40, 20 },  4 },
    { { 10, 19, 20, 30 }, { 30, 15, 40, 20 },  1 },
    { { 10, 20, 20, 30 }, { 30, 15, 40, 20 },  0 },
    { { 10, 21, 20, 30 }, { 30, 15, 40, 20 },  0 },
};

BOOST_DATA_TEST_CASE(
    vertical_overlap_, data::make (vertical_overlap_dataset),
    lhs, rhs, result) {

    using namespace xpdf;

    {
        auto value = vertical_overlap (lhs, rhs);
        BOOST_TEST (value == result);
    }
}

static const std::vector<
    std::tuple< xpdf::bbox_t, xpdf::bbox_t, double >
    >
horizontal_distance_dataset = {
    { {  2, 10, 14, 18 }, { 16, 11, 24, 15 },  2 },
    { {  2, 10, 15, 18 }, { 16, 11, 24, 15 },  1 },
    { {  2, 10, 16, 18 }, { 16, 11, 24, 15 },  0 },
    { {  2, 10, 17, 18 }, { 16, 11, 24, 15 },  0 },
    { {  2, 10, 30, 18 }, { 16, 11, 24, 15 },  0 },
    { { 23, 10, 30, 18 }, { 16, 11, 24, 15 },  0 },
    { { 24, 10, 30, 18 }, { 16, 11, 24, 15 },  0 },
    { { 25, 10, 30, 18 }, { 16, 11, 24, 15 },  1 },
    { { 26, 10, 30, 18 }, { 16, 11, 24, 15 },  2 },
};

BOOST_DATA_TEST_CASE(
    horizontal_distance_, data::make (horizontal_distance_dataset),
    lhs, rhs, result) {

    using namespace xpdf;

    {
        auto value = horizontal_distance (lhs, rhs);
        BOOST_TEST (value == result);
    }
}

static const std::vector<
    std::tuple< xpdf::bbox_t, xpdf::bbox_t, double >
    >
vertical_distance_dataset = {
    { { 16,  2, 24,  9 }, {  2, 10, 14, 18 }, 1 },
    { { 16,  2, 24, 10 }, {  2, 10, 14, 18 }, 0 },
    { { 16,  2, 24, 11 }, {  2, 10, 14, 18 }, 0 },
    { { 16,  2, 24, 17 }, {  2, 10, 14, 18 }, 0 },
    { { 16,  2, 24, 18 }, {  2, 10, 14, 18 }, 0 },
    { { 16,  2, 24, 19 }, {  2, 10, 14, 18 }, 0 },
    { { 16, 17, 24, 45 }, {  2, 10, 14, 18 }, 0 },
    { { 16, 18, 24, 45 }, {  2, 10, 14, 18 }, 0 },
    { { 16, 19, 24, 45 }, {  2, 10, 14, 18 }, 1 },
    { { 16, 20, 24, 45 }, {  2, 10, 14, 18 }, 2 },
    { { 16, 21, 24, 45 }, {  2, 10, 14, 18 }, 3 },
};

BOOST_DATA_TEST_CASE(
    vertical_distance_, data::make (vertical_distance_dataset),
    lhs, rhs, result) {

    using namespace xpdf;

    {
        auto value = vertical_distance (lhs, rhs);
        BOOST_TEST (value == result);
    }
}

BOOST_AUTO_TEST_CASE(rotate_) {
    using namespace xpdf;

    const auto superbox = bbox_t{ 0, 0, 100, 200 };

    {
        auto box = bbox_t{ 2, 10, 14, 16 };

        {
            auto a =   rotate< rotation_t::none > (box, superbox);
            auto b = unrotate< rotation_t::none > (box, superbox);
            BOOST_TEST (box == a);
            BOOST_TEST (box == b);
        }

        {
            auto a =   rotate< rotation_t::quarter_turn > (box, superbox);
            auto b = unrotate< rotation_t::quarter_turn > (a, superbox);
            BOOST_TEST (b == box);
        }

        {
            auto a =   rotate< rotation_t::half_turn > (box, superbox);
            auto b = unrotate< rotation_t::half_turn > (a, superbox);
            BOOST_TEST (b == box);
        }

        {
            auto a =   rotate< rotation_t::three_quarters_turn > (box, superbox);
            auto b = unrotate< rotation_t::three_quarters_turn > (a, superbox);
            BOOST_TEST (b == box);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
