// -*- mode: c++; -*-
// Copyright 2020- Thinkoid, LLC

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE iostreams

#include <cassert>
#include <iostream>

#include <boost/type_index.hpp>
#include <boost/test/unit_test.hpp>
namespace utf = boost::unit_test;

#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>
namespace data = boost::unit_test::data;

#include <boost/iostreams/filtering_stream.hpp>
namespace io = boost::iostreams;

#include <iostreams/array_source.hh>
#include <iostreams/asciihex_input_filter.hh>

#define UNUSED(x) ((void)x)

BOOST_AUTO_TEST_SUITE(asciihex_filter)

static const std::vector< std::tuple< std::string, std::string, bool, bool, bool > >
input_dataset = {
    {      ">",                         "",   true,  false,   true },
    {     " >",                         "",   true,  false,   true },
    {    "  >",                         "",   true,  false,   true },
    {   "   >",                         "",   true,  false,   true },
    {     "0>",                         "",  false,   true,   true }, // divergent behavior
    {    "00>",     std::string(  "\0", 1),   true,  false,   true },
    {   "000>",                         "",  false,   true,   true }, // divergent behavior
    {  "0000>",     std::string("\0\0", 2),   true,  false,   true },
    {     "A>",                         "",  false,   true,   true }, // divergent behavior
    {    "AA>", std::string(    "\xAA", 1),   true,  false,   true },
    {   "AAA>",                         "",  false,   true,   true }, // divergent behavior
    {  "AAAA>", std::string("\xAA\xAA", 2),   true,  false,   true },
};

BOOST_DATA_TEST_CASE(
    input, data::make(input_dataset), in, result, eof, bad, fail)
{
    using namespace xpdf;

    io::filtering_istream str;

    str.push(xpdf::iostreams::asciihex_input_filter_t());
    str.push(xpdf::iostreams::array_source_t< char >(in.begin(), in.end()));

    std::string out;

    for (int c; str && EOF != (c = str.get());)
        out.append(1, c);

    BOOST_TEST(result.size() == out.size());
    BOOST_TEST(result == out);

    BOOST_TEST(str.eof()  == eof);
    BOOST_TEST(str.bad()  == bad);
    BOOST_TEST(str.fail() == fail);
}

BOOST_AUTO_TEST_SUITE_END()
