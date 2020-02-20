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
    std::tuple<
        xpdf::bboxi_t, xpdf::bboxi_t,
        xpdf::bboxi_t, xpdf::bboxi_t, xpdf::bboxi_t > >
rotate_dataset{
    { {  20, 400,  80, 600 }, {   0,   0, 200, 800 },
      { 400, 120, 600, 180 }, { 120, 200, 180, 400 }, { 200,  20, 400,  80 } }
};

BOOST_DATA_TEST_CASE(
    rotate_, data::make (rotate_dataset),
    box, superbox, result90, result180, result270) {

    using namespace xpdf;

    {
        auto value = rotate< rotation_t::none > (box, superbox);
        BOOST_TEST (value == box);
    }

    {
        auto value = rotate< rotation_t::quarter_turn > (box, superbox);
        BOOST_TEST (value == result90);
    }

    {
        auto value = rotate< rotation_t::half_turn > (box, superbox);
        BOOST_TEST (value == result180);
    }

    {
        auto value = rotate< rotation_t::three_quarters_turn > (box, superbox);
        BOOST_TEST (value == result270);
    }
}

BOOST_AUTO_TEST_SUITE_END()
