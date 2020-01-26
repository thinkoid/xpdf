// -*- mode: c++ -*-
// Copyright 2019-2020 Thinkoid, LLC.

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE array

#include <defs.hh>

#include <iostream>
#include <exception>

#include <boost/type_index.hpp>
#include <boost/test/unit_test.hpp>
namespace utf = boost::unit_test;

#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>
namespace data = boost::unit_test::data;

BOOST_AUTO_TEST_SUITE(array_iterator)

BOOST_AUTO_TEST_CASE(dummy) {
    BOOST_TEST (true);
}

BOOST_AUTO_TEST_SUITE_END()
