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

#include <iostreams/container_source.hh>
#include <iostreams/container_sink.hh>
#include <iostreams/asciihex_input_filter.hh>
#include <iostreams/asciihex_output_filter.hh>

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
    input, data::make(input_dataset), input, result, eof, bad, fail)
{
    using namespace xpdf;

    io::filtering_istream str;

    str.push(xpdf::iostreams::asciihex_input_filter_t());
    str.push(xpdf::iostreams::container_source_t< std::string >(input));

    std::string buf;

    for (int c; str && EOF != (c = str.get());)
        buf.append(1, c);

    BOOST_TEST(result == buf);

    BOOST_TEST(str.eof()  == eof);
    BOOST_TEST(str.bad()  == bad);
    BOOST_TEST(str.fail() == fail);
}

static const std::vector< std::tuple< std::string, std::string, bool, bool > >
output_dataset = {
    {          "",     ">",  false, false },
    {      "\xA0",   "A0>",  false, false },
    {  "\xA0\x72",  "A072>", false, false },
};

BOOST_DATA_TEST_CASE(
    output, data::make(output_dataset), output, result, bad, fail)
{
    using namespace xpdf;

    std::string buf;
    io::filtering_ostream str;

    str.push(xpdf::iostreams::asciihex_output_filter_t());
    str.push(xpdf::iostreams::container_sink_t< std::string >(buf));

    for (auto c : output)
        str.put(c);

    boost::iostreams::close(str);

    BOOST_TEST(result == buf);

    BOOST_TEST(str.bad()  == bad);
    BOOST_TEST(str.fail() == fail);
}

BOOST_AUTO_TEST_SUITE_END()
