#include "helpers.hpp"

using namespace resiris;

namespace {

Value call(const std::string& fn, const std::vector<Value>& args) {
    RsConvertModule module;
    return module.call_function(fn, args);
}

std::string text(const Value& value) {
    REQUIRE(value.type == Value::Type::String);
    return value.string_value;
}

Int integer(const Value& value) {
    REQUIRE(value.type == Value::Type::Int);
    return value.int_value;
}

double decimal(const Value& value) {
    REQUIRE(value.type == Value::Type::Float);
    return value.float_value;
}

} // namespace

TEST_CASE("rsconvert: module metadata", "[rsconvert]") {
    RsConvertModule module;
    auto functions = module.function_names();
    REQUIRE(functions.front() == "to_base64");
    REQUIRE(functions.back() == "newline_normalize");
    REQUIRE(functions.size() == 68);
    REQUIRE(module.variables().empty());
    REQUIRE(module.module_name() == "RSConvert");
}

TEST_CASE("rsconvert: base64", "[rsconvert]") {
    REQUIRE(text(call("to_base64", {Value::make_string("Resiris")})) == "UmVzaXJpcw==");
    REQUIRE(text(call("from_base64", {Value::make_string("UmVzaXJpcw==")})) == "Resiris");
    REQUIRE(text(call("to_base64", {Value::make_string("")})) == "");
    REQUIRE(text(call("from_base64", {Value::make_string("")})) == "");
    REQUIRE_THROWS_AS(call("from_base64", {Value::make_string("@@@@")}), ResirisError);
}

TEST_CASE("rsconvert: hex text", "[rsconvert]") {
    REQUIRE(text(call("text_to_hex", {Value::make_string("Hi")})) == "4869");
    REQUIRE(text(call("hex_to_text", {Value::make_string("4869")})) == "Hi");
    REQUIRE(text(call("hex_to_text", {Value::make_string("4869")})) == "Hi");
    REQUIRE_THROWS_AS(call("hex_to_text", {Value::make_string("123")}), ResirisError);
    REQUIRE_THROWS_AS(call("text_to_hex", {Value::make_string("\x80")}), ResirisError);
}

TEST_CASE("rsconvert: numeric bases", "[rsconvert]") {
    REQUIRE(text(call("integer_to_hex", {Value::make_int(255)})) == "ff");
    REQUIRE(integer(call("hex_to_integer", {Value::make_string("0xFF")})) == 255);
    REQUIRE(text(call("integer_to_binary", {Value::make_int(10)})) == "1010");
    REQUIRE(integer(call("binary_to_integer", {Value::make_string("1010")})) == 10);
    REQUIRE(text(call("integer_to_octal", {Value::make_int(64)})) == "100");
    REQUIRE(integer(call("octal_to_integer", {Value::make_string("100")})) == 64);
    REQUIRE(text(call("integer_to_base", {Value::make_int(255), Value::make_int(16)})) == "ff");
    REQUIRE(integer(call("base_to_integer", {Value::make_string("ff"), Value::make_int(16)})) == 255);
    REQUIRE_THROWS_AS(call("integer_to_hex", {Value::make_int(-1)}), ResirisError);
    REQUIRE_THROWS_AS(call("binary_to_integer", {Value::make_string("102")}), ResirisError);
    REQUIRE_THROWS_AS(call("integer_to_base", {Value::make_int(1), Value::make_int(37)}),
                      ResirisError);
}

TEST_CASE("rsconvert: url encoding", "[rsconvert]") {
    REQUIRE(text(call("url_encode", {Value::make_string("hello world")})) == "hello%20world");
    REQUIRE(text(call("url_decode", {Value::make_string("hello%20world")})) == "hello world");
    REQUIRE(text(call("url_encode", {Value::make_string("a-b_c.d~e")})) == "a-b_c.d~e");
    REQUIRE(text(call("url_query_encode", {Value::make_string("hello world")})) == "hello+world");
    REQUIRE(text(call("url_query_decode", {Value::make_string("hello+world")})) == "hello world");
    REQUIRE_THROWS_AS(call("url_decode", {Value::make_string("%zz")}), ResirisError);
}

TEST_CASE("rsconvert: text and binary", "[rsconvert]") {
    REQUIRE(text(call("text_to_binary", {Value::make_string("A")})) == "01000001");
    REQUIRE(text(call("binary_to_text", {Value::make_string("01000001")})) == "A");
    REQUIRE_THROWS_AS(call("binary_to_text", {Value::make_string("0100000")}), ResirisError);
    REQUIRE_THROWS_AS(call("binary_to_text", {Value::make_string("10000000")}), ResirisError);
}

TEST_CASE("rsconvert: json escaping", "[rsconvert]") {
    Value escaped = call("json_escape", {Value::make_string("say \"hello\"\n")});
    REQUIRE(text(escaped) == "say \\\"hello\\\"\\n");
    REQUIRE(text(call("json_unescape", {escaped})) == "say \"hello\"\n");
    REQUIRE(text(call("json_unescape", {Value::make_string("\\u0041")})) == "A");
    REQUIRE_THROWS_AS(call("json_unescape", {Value::make_string("\\u0080")}), ResirisError);
    REQUIRE_THROWS_AS(call("json_unescape", {Value::make_string("\\x")}), ResirisError);
}

TEST_CASE("rsconvert: html and xml escaping", "[rsconvert]") {
    REQUIRE(text(call("html_escape", {Value::make_string("<a & \"b\">")})) ==
            "&lt;a &amp; &quot;b&quot;&gt;");
    REQUIRE(text(call("html_unescape", {Value::make_string("&lt;a &amp; &quot;b&quot;&gt;")})) ==
            "<a & \"b\">");
    REQUIRE(text(call("xml_escape", {Value::make_string("'x'")})) == "&apos;x&apos;");
    REQUIRE(text(call("xml_unescape", {Value::make_string("&apos;x&apos;")})) == "'x'");
    REQUIRE_THROWS_AS(call("html_unescape", {Value::make_string("&bogus;")}), ResirisError);
}

TEST_CASE("rsconvert: csv escaping", "[rsconvert]") {
    REQUIRE(text(call("csv_escape", {Value::make_string("hello, world")})) ==
            "\"hello, world\"");
    REQUIRE(text(call("csv_unescape", {Value::make_string("\"hello, world\"")})) ==
            "hello, world");
    REQUIRE(text(call("csv_escape", {Value::make_string("a\"b")})) == "\"a\"\"b\"");
    REQUIRE(text(call("csv_unescape", {Value::make_string("\"a\"\"b\"")})) == "a\"b");
    REQUIRE_THROWS_AS(call("csv_unescape", {Value::make_string("\"a\"x")}), ResirisError);
}

TEST_CASE("rsconvert: ascii and roman", "[rsconvert]") {
    REQUIRE(integer(call("ascii_to_integer", {Value::make_string("A")})) == 65);
    REQUIRE(text(call("integer_to_ascii", {Value::make_int(65)})) == "A");
    REQUIRE_THROWS_AS(call("ascii_to_integer", {Value::make_string("AB")}), ResirisError);
    REQUIRE(text(call("integer_to_roman", {Value::make_int(2024)})) == "MMXXIV");
    REQUIRE(integer(call("roman_to_integer", {Value::make_string("MMXXIV")})) == 2024);
    REQUIRE_THROWS_AS(call("roman_to_integer", {Value::make_string("IIII")}), ResirisError);
}

TEST_CASE("rsconvert: colors and durations", "[rsconvert]") {
    REQUIRE(text(call("rgb_to_hex", {Value::make_int(255), Value::make_int(128),
                                     Value::make_int(0)})) == "#ff8000");
    REQUIRE(integer(call("hex_to_red", {Value::make_string("#ff8000")})) == 255);
    REQUIRE(integer(call("hex_to_green", {Value::make_string("#ff8000")})) == 128);
    REQUIRE(integer(call("hex_to_blue", {Value::make_string("#ff8000")})) == 0);
    REQUIRE(text(call("seconds_to_duration", {Value::make_int(3661)})) == "01:01:01");
    REQUIRE(integer(call("duration_to_seconds", {Value::make_string("01:01:01")})) == 3661);
    REQUIRE(integer(call("duration_to_seconds", {Value::make_string("100:00:00")})) == 360000);
    REQUIRE_THROWS_AS(call("duration_to_seconds", {Value::make_string("1:00:00")}), ResirisError);
}

TEST_CASE("rsconvert: storage sizes", "[rsconvert]") {
    REQUIRE(text(call("bytes_to_decimal_size", {Value::make_int(1500000)})) == "1.5 MB");
    REQUIRE(text(call("bytes_to_binary_size", {Value::make_int(1572864)})) == "1.5 MiB");
    REQUIRE(integer(call("storage_size_to_bytes", {Value::make_string("1.5 MiB")})) == 1572864);
    REQUIRE(integer(call("storage_size_to_bytes", {Value::make_string("8 bit")})) == 1);
    REQUIRE(integer(call("bytes_to_bits", {Value::make_int(4)})) == 32);
    REQUIRE(integer(call("bits_to_bytes", {Value::make_int(32)})) == 4);
    REQUIRE_THROWS_AS(call("storage_size_to_bytes", {Value::make_string("1.5 MiB x")}),
                      ResirisError);
    REQUIRE_THROWS_AS(call("bits_to_bytes", {Value::make_int(7)}), ResirisError);
}

TEST_CASE("rsconvert: byte order", "[rsconvert]") {
    REQUIRE(text(call("integer_to_bytes", {Value::make_int(4660), Value::make_int(2),
                                           Value::make_string("big")})) == "1234");
    REQUIRE(text(call("integer_to_bytes", {Value::make_int(4660), Value::make_int(2),
                                           Value::make_string("little")})) == "3412");
    REQUIRE(integer(call("bytes_to_integer", {Value::make_string("1234"),
                                              Value::make_string("big")})) == 4660);
    REQUIRE(integer(call("bytes_to_integer", {Value::make_string("3412"),
                                              Value::make_string("little")})) == 4660);
    REQUIRE_THROWS_AS(call("integer_to_bytes", {Value::make_int(256), Value::make_int(1),
                                                Value::make_string("big")}), ResirisError);
}

TEST_CASE("rsconvert: signed and unsigned", "[rsconvert]") {
    REQUIRE(integer(call("signed_to_unsigned", {Value::make_int(-1), Value::make_int(8)})) == 255);
    REQUIRE(integer(call("unsigned_to_signed", {Value::make_int(255), Value::make_int(8)})) == -1);
    REQUIRE(integer(call("signed_to_unsigned", {Value::make_int(-128), Value::make_int(8)})) == 128);
    REQUIRE(integer(call("unsigned_to_signed", {Value::make_int(128), Value::make_int(8)})) == -128);
    REQUIRE(integer(call("unsigned_to_signed",
                         {Value::make_int(std::numeric_limits<Int>::max()),
                          Value::make_int(63)})) == -1);
    REQUIRE_THROWS_AS(call("signed_to_unsigned", {Value::make_int(128), Value::make_int(8)}),
                      ResirisError);
}

TEST_CASE("rsconvert: float bits", "[rsconvert]") {
    REQUIRE(text(call("float_to_bits", {Value::make_float(1.5), Value::make_int(32)})) ==
            "00111111110000000000000000000000");
    REQUIRE(decimal(call("bits_to_float",
                         {Value::make_string("00111111110000000000000000000000")})) == 1.5);
    REQUIRE(text(call("float_to_bits", {Value::make_float(1.5), Value::make_int(16)})) ==
            "0011111000000000");
    REQUIRE(decimal(call("bits_to_float", {Value::make_string("0011111000000000")})) == 1.5);
    REQUIRE(text(call("float_to_bits", {Value::make_float(1.5), Value::make_int(64)})) ==
            "0011111111111000000000000000000000000000000000000000000000000000");
    REQUIRE(decimal(call("bits_to_float",
                         {Value::make_string(
                             "0011111111111000000000000000000000000000000000000000000000000000")})) ==
            1.5);
    REQUIRE_THROWS_AS(call("float_to_bits", {Value::make_float(1.0), Value::make_int(8)}),
                      ResirisError);
}

TEST_CASE("rsconvert: gray, bcd, leb128, zigzag", "[rsconvert]") {
    REQUIRE(text(call("integer_to_gray", {Value::make_int(10), Value::make_int(4)})) == "1111");
    REQUIRE(integer(call("gray_to_integer", {Value::make_string("1111")})) == 10);
    REQUIRE(text(call("integer_to_bcd", {Value::make_int(42)})) == "01000010");
    REQUIRE(integer(call("bcd_to_integer", {Value::make_string("01000010")})) == 42);
    REQUIRE(text(call("integer_to_leb128", {Value::make_int(300)})) == "ac02");
    REQUIRE(integer(call("leb128_to_integer", {Value::make_string("ac02")})) == 300);
    REQUIRE(integer(call("signed_to_zigzag", {Value::make_int(-10)})) == 19);
    REQUIRE(integer(call("zigzag_to_signed", {Value::make_int(19)})) == -10);
    REQUIRE_THROWS_AS(call("leb128_to_integer", {Value::make_string("8000")}), ResirisError);
}

TEST_CASE("rsconvert: fixed point and rgb565", "[rsconvert]") {
    REQUIRE(integer(call("decimal_to_fixed", {Value::make_float(1.5), Value::make_int(8)})) == 384);
    REQUIRE(decimal(call("fixed_to_decimal", {Value::make_int(384), Value::make_int(8)})) == 1.5);
    REQUIRE(integer(call("rgb888_to_rgb565", {Value::make_int(255), Value::make_int(128),
                                              Value::make_int(0)})) == 64512);
    REQUIRE(integer(call("rgb565_to_red", {Value::make_int(64512)})) == 255);
    REQUIRE(integer(call("rgb565_to_green", {Value::make_int(64512)})) == 130);
    REQUIRE(integer(call("rgb565_to_blue", {Value::make_int(64512)})) == 0);
}

TEST_CASE("rsconvert: pcm and offset binary", "[rsconvert]") {
    REQUIRE(text(call("pcm_sample_to_bytes", {Value::make_int(-1000), Value::make_int(16),
                                              Value::make_string("little")})) == "18fc");
    REQUIRE(text(call("pcm_sample_to_bytes", {Value::make_int(-1000), Value::make_int(16),
                                              Value::make_string("big")})) == "fc18");
    REQUIRE(integer(call("bytes_to_pcm_sample", {Value::make_string("18fc"), Value::make_int(16),
                                                 Value::make_string("little")})) == -1000);
    REQUIRE(integer(call("signed_to_offset_binary", {Value::make_int(-1000),
                                                     Value::make_int(16)})) == 31768);
    REQUIRE(integer(call("offset_binary_to_signed", {Value::make_int(31768),
                                                     Value::make_int(16)})) == -1000);
}

TEST_CASE("rsconvert: bit fields and newlines", "[rsconvert]") {
    REQUIRE(integer(call("extract_bit_field", {Value::make_int(42), Value::make_int(1),
                                               Value::make_int(3)})) == 5);
    REQUIRE(integer(call("replace_bit_field", {Value::make_int(42), Value::make_int(1),
                                               Value::make_int(3), Value::make_int(2)})) == 36);
    REQUIRE(text(call("newline_normalize", {Value::make_string("a\r\nb\rc")})) == "a\nb\nc");
}

TEST_CASE("rsconvert: interpreter integration", "[rsconvert]") {
    testutil::Interp app;
    app.exec("<include> RSConvert\n"
             "v x string = RSConvert.to_base64(\"Resiris\")\n");
    REQUIRE(testutil::variable(*app.interp, "x").string_value == "UmVzaXJpcw==");
}
