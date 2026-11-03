#include "resiris/modules/rsconvert.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace resiris {

namespace {

constexpr Int kMaxInt = std::numeric_limits<Int>::max();
constexpr std::uint64_t kMaxUInt = std::numeric_limits<std::uint64_t>::max();

[[noreturn]] void fail(const std::string& fn, const std::string& message) {
    throw ResirisError("RSConvert." + fn + ": " + message);
}

void require_count(const std::vector<Value>& args, std::size_t count) {
    if (args.size() != count) {
        throw ModuleSignatureError();
    }
}

const Value& require_string(const std::vector<Value>& args, std::size_t index) {
    if (index >= args.size() || args[index].type != Value::Type::String) {
        throw ModuleSignatureError();
    }
    return args[index];
}

const Value& require_integer(const std::vector<Value>& args, std::size_t index) {
    if (index >= args.size()) {
        throw ModuleSignatureError();
    }
    const Value& v = args[index];
    if (v.type != Value::Type::Int && v.type != Value::Type::Bool) {
        throw ModuleSignatureError();
    }
    return v;
}

const Value& require_number(const std::vector<Value>& args, std::size_t index) {
    if (index >= args.size()) {
        throw ModuleSignatureError();
    }
    const Value& v = args[index];
    if (v.type != Value::Type::Int && v.type != Value::Type::Float &&
        v.type != Value::Type::Bool) {
        throw ModuleSignatureError();
    }
    return v;
}

Int as_int(const Value& v) {
    if (v.type == Value::Type::Int) {
        return v.int_value;
    }
    if (v.type == Value::Type::Bool) {
        return v.bool_value ? 1 : 0;
    }
    return static_cast<Int>(v.float_value);
}

double as_double(const Value& v) {
    switch (v.type) {
        case Value::Type::Int: return static_cast<double>(v.int_value);
        case Value::Type::Bool: return v.bool_value ? 1.0 : 0.0;
        default: return v.float_value;
    }
}

void require_ascii(const std::string& fn, const std::string& text) {
    for (unsigned char c : text) {
        if (c > 0x7F) {
            fail(fn, "only ASCII text is supported");
        }
    }
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

char hex_digit(int value) {
    return static_cast<char>(value < 10 ? '0' + value : 'a' + (value - 10));
}

std::string hex_byte(unsigned value) {
    std::string out;
    out.push_back(hex_digit((value >> 4) & 0xF));
    out.push_back(hex_digit(value & 0xF));
    return out;
}

std::string format_bits(std::uint64_t value, int width) {
    std::string out;
    out.reserve(static_cast<std::size_t>(width));
    for (int bit = width - 1; bit >= 0; --bit) {
        out.push_back(((value >> bit) & 1U) ? '1' : '0');
    }
    return out;
}

std::string base64_encode(const std::string& text) {
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    std::size_t i = 0;
    while (i + 3 <= text.size()) {
        unsigned n = (static_cast<unsigned char>(text[i]) << 16) |
                     (static_cast<unsigned char>(text[i + 1]) << 8) |
                     static_cast<unsigned char>(text[i + 2]);
        out.push_back(alphabet[(n >> 18) & 63]);
        out.push_back(alphabet[(n >> 12) & 63]);
        out.push_back(alphabet[(n >> 6) & 63]);
        out.push_back(alphabet[n & 63]);
        i += 3;
    }
    const std::size_t left = text.size() - i;
    if (left == 1) {
        unsigned n = static_cast<unsigned char>(text[i]) << 16;
        out.push_back(alphabet[(n >> 18) & 63]);
        out.push_back(alphabet[(n >> 12) & 63]);
        out += "==";
    } else if (left == 2) {
        unsigned n = (static_cast<unsigned char>(text[i]) << 16) |
                     (static_cast<unsigned char>(text[i + 1]) << 8);
        out.push_back(alphabet[(n >> 18) & 63]);
        out.push_back(alphabet[(n >> 12) & 63]);
        out.push_back(alphabet[(n >> 6) & 63]);
        out.push_back('=');
    }
    return out;
}

std::string base64_decode(const std::string& fn, const std::string& text) {
    std::size_t end = text.size();
    int padding = 0;
    while (end > 0 && text[end - 1] == '=') {
        --end;
        ++padding;
    }
    if (padding > 2) {
        fail(fn, "invalid Base64 padding");
    }
    for (std::size_t i = 0; i < end; ++i) {
        if (text[i] == '=') {
            fail(fn, "invalid Base64 padding");
        }
    }
    if (end % 4 == 1) {
        fail(fn, "invalid Base64 length");
    }
    if ((padding == 1 && end % 4 != 3) || (padding == 2 && end % 4 != 2)) {
        fail(fn, "invalid Base64 padding");
    }

    std::string out;
    unsigned buffer = 0;
    int bits = 0;
    for (std::size_t i = 0; i < end; ++i) {
        char c = text[i];
        int value = -1;
        if (c >= 'A' && c <= 'Z') value = c - 'A';
        else if (c >= 'a' && c <= 'z') value = c - 'a' + 26;
        else if (c >= '0' && c <= '9') value = c - '0' + 52;
        else if (c == '+') value = 62;
        else if (c == '/') value = 63;
        if (value < 0) {
            fail(fn, "invalid Base64 character");
        }
        buffer = (buffer << 6) | static_cast<unsigned>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            unsigned char byte = static_cast<unsigned char>((buffer >> bits) & 0xFF);
            if (byte > 0x7F) {
                fail(fn, "decoded text is not ASCII");
            }
            out.push_back(static_cast<char>(byte));
        }
    }
    // Unused low bits must be zero in canonical Base64.
    if (bits > 0 && (buffer & ((1U << bits) - 1U)) != 0) {
        fail(fn, "invalid Base64 padding bits");
    }
    return out;
}

bool url_unreserved(unsigned char c) {
    return std::isalnum(c) != 0 || c == '-' || c == '.' || c == '_' || c == '~';
}

std::string percent_encode(const std::string& fn, const std::string& text, bool plus_space) {
    require_ascii(fn, text);
    std::string out;
    for (unsigned char c : text) {
        if (url_unreserved(c)) {
            out.push_back(static_cast<char>(c));
        } else if (plus_space && c == ' ') {
            out.push_back('+');
        } else {
            out.push_back('%');
            out.push_back(hex_digit((c >> 4) & 0xF));
            out.push_back(hex_digit(c & 0xF));
            // Uppercase percent escapes are the conventional URL form.
            out[out.size() - 2] = static_cast<char>(std::toupper(out[out.size() - 2]));
            out[out.size() - 1] = static_cast<char>(std::toupper(out[out.size() - 1]));
        }
    }
    return out;
}

std::string percent_decode(const std::string& fn, const std::string& text, bool plus_space) {
    require_ascii(fn, text);
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (plus_space && c == '+') {
            out.push_back(' ');
            continue;
        }
        if (c != '%') {
            out.push_back(c);
            continue;
        }
        if (i + 2 >= text.size()) {
            fail(fn, "malformed percent escape");
        }
        int high = hex_value(text[i + 1]);
        int low = hex_value(text[i + 2]);
        if (high < 0 || low < 0) {
            fail(fn, "malformed percent escape");
        }
        unsigned char byte = static_cast<unsigned char>((high << 4) | low);
        if (byte > 0x7F) {
            fail(fn, "decoded text is not ASCII");
        }
        out.push_back(static_cast<char>(byte));
        i += 2;
    }
    return out;
}

std::string json_escape_text(const std::string& fn, const std::string& text) {
    require_ascii(fn, text);
    std::string out;
    for (unsigned char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out.push_back(hex_digit((c >> 4) & 0xF));
                    out.push_back(hex_digit(c & 0xF));
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    return out;
}

std::string json_unescape_text(const std::string& fn, const std::string& text) {
    require_ascii(fn, text);
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x20) {
            fail(fn, "unescaped control character in JSON text");
        }
        if (c != '\\') {
            out.push_back(static_cast<char>(c));
            continue;
        }
        if (++i >= text.size()) {
            fail(fn, "trailing backslash in JSON text");
        }
        char escape = text[i];
        switch (escape) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                if (i + 4 >= text.size()) {
                    fail(fn, "malformed \\u escape");
                }
                int value = 0;
                for (int digit = 0; digit < 4; ++digit) {
                    int h = hex_value(text[i + 1 + static_cast<std::size_t>(digit)]);
                    if (h < 0) {
                        fail(fn, "malformed \\u escape");
                    }
                    value = value * 16 + h;
                }
                if (value > 0x7F) {
                    fail(fn, "decoded text is not ASCII");
                }
                out.push_back(static_cast<char>(value));
                i += 4;
                break;
            }
            default:
                fail(fn, "unknown JSON escape");
        }
    }
    return out;
}

std::string escape_entities(const std::string& fn, const std::string& text) {
    require_ascii(fn, text);
    std::string out;
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

std::string unescape_entities(const std::string& fn, const std::string& text) {
    require_ascii(fn, text);
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '&') {
            out.push_back(text[i]);
            continue;
        }
        std::size_t semicolon = text.find(';', i + 1);
        if (semicolon == std::string::npos) {
            fail(fn, "malformed entity");
        }
        std::string name = text.substr(i + 1, semicolon - i - 1);
        if (name == "amp") out.push_back('&');
        else if (name == "lt") out.push_back('<');
        else if (name == "gt") out.push_back('>');
        else if (name == "quot") out.push_back('"');
        else if (name == "apos") out.push_back('\'');
        else fail(fn, "unsupported entity: &" + name + ";");
        i = semicolon;
    }
    return out;
}

std::string csv_escape_text(const std::string& fn, const std::string& text) {
    require_ascii(fn, text);
    bool quote = false;
    for (char c : text) {
        if (c == ',' || c == '"' || c == '\r' || c == '\n') {
            quote = true;
            break;
        }
    }
    if (!quote) {
        return text;
    }
    std::string out = "\"";
    for (char c : text) {
        if (c == '"') {
            out += "\"\"";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('"');
    return out;
}

std::string csv_unescape_text(const std::string& fn, const std::string& text) {
    require_ascii(fn, text);
    if (text.empty()) {
        return text;
    }
    if (text[0] != '"') {
        for (char c : text) {
            if (c == ',' || c == '"' || c == '\r' || c == '\n') {
                fail(fn, "invalid unquoted CSV field");
            }
        }
        return text;
    }
    std::string out;
    std::size_t i = 1;
    bool closed = false;
    while (i < text.size()) {
        char c = text[i];
        if (c == '"') {
            if (i + 1 < text.size() && text[i + 1] == '"') {
                out.push_back('"');
                i += 2;
            } else {
                ++i;
                closed = true;
                break;
            }
        } else {
            out.push_back(c);
            ++i;
        }
    }
    if (!closed || i != text.size()) {
        fail(fn, "malformed quoted CSV field");
    }
    return out;
}

std::string roman_encode(const std::string& fn, Int number) {
    if (number < 1 || number > 3999) {
        fail(fn, "Roman numerals support values from 1 through 3999");
    }
    static const std::array<std::pair<int, const char*>, 13> table = {{
        {1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"}, {100, "C"},
        {90, "XC"}, {50, "L"}, {40, "XL"}, {10, "X"}, {9, "IX"},
        {5, "V"}, {4, "IV"}, {1, "I"},
    }};
    std::string out;
    for (const auto& [value, text] : table) {
        while (number >= value) {
            out += text;
            number -= value;
        }
    }
    return out;
}

int roman_digit(char c) {
    switch (c) {
        case 'I': return 1;
        case 'V': return 5;
        case 'X': return 10;
        case 'L': return 50;
        case 'C': return 100;
        case 'D': return 500;
        case 'M': return 1000;
        default: return 0;
    }
}

Int roman_decode(const std::string& fn, const std::string& text) {
    if (text.empty()) {
        fail(fn, "empty Roman numeral");
    }
    Int total = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        int value = roman_digit(text[i]);
        if (value == 0) {
            fail(fn, "invalid Roman numeral");
        }
        int next = (i + 1 < text.size()) ? roman_digit(text[i + 1]) : 0;
        if (next > value) {
            total -= value;
        } else {
            total += value;
        }
    }
    if (total < 1 || total > 3999 || roman_encode(fn, total) != text) {
        fail(fn, "invalid or non-canonical Roman numeral");
    }
    return total;
}

std::string int_to_base(const std::string& fn, Int value, int base) {
    if (value < 0) {
        fail(fn, "a non-negative integer is required");
    }
    if (base < 2 || base > 36) {
        fail(fn, "base must be from 2 through 36");
    }
    if (value == 0) {
        return "0";
    }
    std::string out;
    while (value > 0) {
        out.push_back(hex_digit(static_cast<int>(value % base)));
        value /= base;
    }
    std::reverse(out.begin(), out.end());
    return out;
}

Int base_to_int(const std::string& fn, const std::string& text, int base, bool allow_hex_prefix) {
    if (base < 2 || base > 36) {
        fail(fn, "base must be from 2 through 36");
    }
    std::size_t start = 0;
    if (allow_hex_prefix && text.size() >= 2 && text[0] == '0' &&
        (text[1] == 'x' || text[1] == 'X')) {
        start = 2;
    }
    if (start >= text.size()) {
        fail(fn, "empty number");
    }
    Int value = 0;
    for (std::size_t i = start; i < text.size(); ++i) {
        int digit = hex_value(text[i]);
        if (digit < 0 || digit >= base) {
            fail(fn, "invalid digit for base");
        }
        if (value > (kMaxInt - digit) / base) {
            fail(fn, "number is too large");
        }
        value = value * base + digit;
    }
    return value;
}

void require_byte_order(const std::string& fn, const std::string& order) {
    if (order != "big" && order != "little") {
        fail(fn, "byte order must be \"big\" or \"little\"");
    }
}

std::string bytes_to_hex_text(const std::vector<unsigned char>& bytes) {
    std::string out;
    out.reserve(bytes.size() * 2);
    for (unsigned char byte : bytes) {
        out += hex_byte(byte);
    }
    return out;
}

std::vector<unsigned char> parse_hex_bytes(const std::string& fn, const std::string& text,
                                           std::size_t min_bytes, std::size_t max_bytes) {
    if (text.empty()) {
        fail(fn, "empty hex byte string");
    }
    if (text.size() % 2 != 0) {
        fail(fn, "hex byte string must have an even number of digits");
    }
    std::size_t count = text.size() / 2;
    if (count < min_bytes || count > max_bytes) {
        fail(fn, "wrong number of hex bytes");
    }
    std::vector<unsigned char> bytes;
    bytes.reserve(count);
    for (std::size_t i = 0; i < text.size(); i += 2) {
        int high = hex_value(text[i]);
        int low = hex_value(text[i + 1]);
        if (high < 0 || low < 0) {
            fail(fn, "invalid hex digit");
        }
        bytes.push_back(static_cast<unsigned char>((high << 4) | low));
    }
    return bytes;
}

Int size_multiplier(const std::string& fn, const std::string& unit) {
    if (unit == "B") return 1;
    if (unit == "kB") return 1000LL;
    if (unit == "MB") return 1000LL * 1000LL;
    if (unit == "GB") return 1000LL * 1000LL * 1000LL;
    if (unit == "TB") return 1000LL * 1000LL * 1000LL * 1000LL;
    if (unit == "PB") return 1000LL * 1000LL * 1000LL * 1000LL * 1000LL;
    if (unit == "EB") return 1000LL * 1000LL * 1000LL * 1000LL * 1000LL * 1000LL;
    if (unit == "KiB") return 1024LL;
    if (unit == "MiB") return 1024LL * 1024LL;
    if (unit == "GiB") return 1024LL * 1024LL * 1024LL;
    if (unit == "TiB") return 1024LL * 1024LL * 1024LL * 1024LL;
    if (unit == "PiB") return 1024LL * 1024LL * 1024LL * 1024LL * 1024LL;
    if (unit == "EiB") return 1024LL * 1024LL * 1024LL * 1024LL * 1024LL * 1024LL;
    (void)fn;
    return 0;
}

std::string format_size(Int bytes, bool binary) {
    static const char* decimal_units[] = {"B", "kB", "MB", "GB", "TB", "PB", "EB"};
    static const char* binary_units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
    const Int base = binary ? 1024 : 1000;
    const char* const* units = binary ? binary_units : decimal_units;
    int index = 0;
    long double divisor = 1.0L;
    while (index < 6 && static_cast<long double>(bytes) >= divisor * base) {
        divisor *= base;
        ++index;
    }
    long double value = static_cast<long double>(bytes) / divisor;
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << static_cast<double>(value);
    std::string number = out.str();
    while (!number.empty() && number.back() == '0') {
        number.pop_back();
    }
    if (!number.empty() && number.back() == '.') {
        number.pop_back();
    }
    return number + " " + units[index];
}

std::string trim(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(begin, end - begin);
}

Int parse_storage_size(const std::string& fn, const std::string& input) {
    std::string text = trim(input);
    if (text.empty()) {
        fail(fn, "empty size string");
    }
    std::size_t split = 0;
    while (split < text.size() &&
           (std::isdigit(static_cast<unsigned char>(text[split])) != 0 ||
            text[split] == '.')) {
        ++split;
    }
    std::string number_text = text.substr(0, split);
    std::string unit = trim(text.substr(split));
    if (number_text.empty() || unit.empty()) {
        fail(fn, "invalid size string");
    }
    bool digits = false;
    for (char c : number_text) {
        if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
            digits = true;
        }
    }
    if (!digits) {
        fail(fn, "invalid size number");
    }
    char* end = nullptr;
    long double number = std::strtold(number_text.c_str(), &end);
    if (end == nullptr || *end != '\0' || !std::isfinite(number) || number < 0.0L) {
        fail(fn, "invalid size number");
    }
    if (unit == "bit") {
        long double bits = number;
        long double bytes = bits / 8.0L;
        long double rounded = std::round(bytes);
        if (std::fabs(bytes - rounded) > 1e-9L) {
            fail(fn, "bit count does not represent whole bytes");
        }
        if (rounded > static_cast<long double>(kMaxInt)) {
            fail(fn, "size is too large");
        }
        return static_cast<Int>(rounded);
    }
    Int multiplier = size_multiplier(fn, unit);
    if (multiplier == 0) {
        fail(fn, "unsupported size unit: " + unit);
    }
    long double bytes = number * static_cast<long double>(multiplier);
    long double rounded = std::round(bytes);
    if (std::fabs(bytes - rounded) > 1e-9L) {
        fail(fn, "size does not represent a whole number of bytes");
    }
    if (rounded > static_cast<long double>(kMaxInt)) {
        fail(fn, "size is too large");
    }
    return static_cast<Int>(rounded);
}

std::uint32_t float_to_half_bits(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    std::uint32_t sign = (bits >> 16) & 0x8000U;
    std::uint32_t exponent = (bits >> 23) & 0xFFU;
    std::uint32_t mantissa = bits & 0x7FFFFFU;

    if (exponent == 0xFFU) {
        if (mantissa == 0) {
            return sign | 0x7C00U;
        }
        return sign | 0x7E00U;
    }

    int half_exponent = static_cast<int>(exponent) - 127 + 15;
    if (half_exponent >= 31) {
        return sign | 0x7C00U;
    }
    if (half_exponent <= 0) {
        if (half_exponent < -10) {
            return sign;
        }
        mantissa |= 0x800000U;
        int shift = 14 - half_exponent;
        std::uint32_t half = mantissa >> shift;
        std::uint32_t remainder = mantissa & ((1U << shift) - 1U);
        std::uint32_t halfway = 1U << (shift - 1);
        if (remainder > halfway || (remainder == halfway && (half & 1U) != 0)) {
            ++half;
        }
        return sign | half;
    }

    std::uint32_t half = (static_cast<std::uint32_t>(half_exponent) << 10) |
                         (mantissa >> 13);
    std::uint32_t remainder = mantissa & 0x1FFFU;
    if (remainder > 0x1000U || (remainder == 0x1000U && (half & 1U) != 0)) {
        ++half;
    }
    return sign | half;
}

double half_bits_to_float(std::uint16_t half) {
    std::uint32_t sign = (static_cast<std::uint32_t>(half) & 0x8000U) << 16;
    std::uint32_t exponent = (half >> 10) & 0x1FU;
    std::uint32_t mantissa = half & 0x3FFU;
    std::uint32_t bits = 0;
    if (exponent == 0) {
        if (mantissa == 0) {
            bits = sign;
        } else {
            int shift = 0;
            while ((mantissa & 0x400U) == 0) {
                mantissa <<= 1;
                ++shift;
            }
            mantissa &= 0x3FFU;
            std::uint32_t final_exponent = static_cast<std::uint32_t>(127 - 14 - shift);
            bits = sign | (final_exponent << 23) | (mantissa << 13);
        }
    } else if (exponent == 0x1FU) {
        bits = sign | 0x7F800000U | (mantissa != 0 ? 0x400000U : 0U);
    } else {
        std::uint32_t final_exponent = exponent - 15 + 127;
        bits = sign | (final_exponent << 23) | (mantissa << 13);
    }
    float result = 0.0F;
    std::memcpy(&result, &bits, sizeof(result));
    return static_cast<double>(result);
}

void require_bit_width(const std::string& fn, Int width, int minimum, int maximum) {
    if (width < minimum || width > maximum) {
        fail(fn, "bit width must be from " + std::to_string(minimum) + " through " +
                      std::to_string(maximum));
    }
}

std::uint64_t bit_mask(int width) {
    if (width >= 64) {
        return kMaxUInt;
    }
    return (1ULL << static_cast<unsigned>(width)) - 1ULL;
}

std::uint64_t checked_unsigned(const std::string& fn, Int value, int width) {
    if (value < 0) {
        fail(fn, "a non-negative integer is required");
    }
    if (static_cast<std::uint64_t>(value) > bit_mask(width)) {
        fail(fn, "value does not fit the requested bit width");
    }
    return static_cast<std::uint64_t>(value);
}

std::string encode_hex_bytes_ordered(std::uint64_t value, std::size_t count,
                                     const std::string& order) {
    std::vector<unsigned char> bytes(count, 0);
    for (std::size_t i = 0; i < count; ++i) {
        unsigned shift = static_cast<unsigned>((count - 1 - i) * 8);
        bytes[i] = static_cast<unsigned char>((value >> shift) & 0xFFU);
    }
    if (order == "little") {
        std::reverse(bytes.begin(), bytes.end());
    }
    return bytes_to_hex_text(bytes);
}

std::uint64_t decode_hex_bytes_ordered(const std::string& fn, const std::string& text,
                                       const std::string& order) {
    auto bytes = parse_hex_bytes(fn, text, 1, 8);
    if (order == "little") {
        std::reverse(bytes.begin(), bytes.end());
    }
    std::uint64_t value = 0;
    for (unsigned char byte : bytes) {
        value = (value << 8) | byte;
    }
    return value;
}

} // namespace

Value RsConvertModule::call_function(const std::string& fn, const std::vector<Value>& args) {
    if (fn == "to_base64") {
        require_count(args, 1);
        const std::string& text = require_string(args, 0).string_value;
        require_ascii(fn, text);
        return Value::make_string(base64_encode(text));
    }
    if (fn == "from_base64") {
        require_count(args, 1);
        return Value::make_string(
            base64_decode(fn, require_string(args, 0).string_value));
    }
    if (fn == "text_to_hex") {
        require_count(args, 1);
        const std::string& text = require_string(args, 0).string_value;
        require_ascii(fn, text);
        std::string out;
        for (unsigned char c : text) {
            out += hex_byte(c);
        }
        return Value::make_string(out);
    }
    if (fn == "hex_to_text") {
        require_count(args, 1);
        const std::string& text = require_string(args, 0).string_value;
        if (text.size() % 2 != 0) {
            fail(fn, "hex text must have an even number of digits");
        }
        std::string out;
        for (std::size_t i = 0; i < text.size(); i += 2) {
            int high = hex_value(text[i]);
            int low = hex_value(text[i + 1]);
            if (high < 0 || low < 0) {
                fail(fn, "invalid hex digit");
            }
            unsigned char byte = static_cast<unsigned char>((high << 4) | low);
            if (byte > 0x7F) {
                fail(fn, "decoded text is not ASCII");
            }
            out.push_back(static_cast<char>(byte));
        }
        return Value::make_string(out);
    }
    if (fn == "integer_to_hex") {
        require_count(args, 1);
        return Value::make_string(
            int_to_base(fn, as_int(require_integer(args, 0)), 16));
    }
    if (fn == "hex_to_integer") {
        require_count(args, 1);
        return Value::make_int(
            base_to_int(fn, require_string(args, 0).string_value, 16, true));
    }
    if (fn == "url_encode") {
        require_count(args, 1);
        return Value::make_string(
            percent_encode(fn, require_string(args, 0).string_value, false));
    }
    if (fn == "url_decode") {
        require_count(args, 1);
        return Value::make_string(
            percent_decode(fn, require_string(args, 0).string_value, false));
    }
    if (fn == "integer_to_binary") {
        require_count(args, 1);
        return Value::make_string(
            int_to_base(fn, as_int(require_integer(args, 0)), 2));
    }
    if (fn == "binary_to_integer") {
        require_count(args, 1);
        return Value::make_int(
            base_to_int(fn, require_string(args, 0).string_value, 2, false));
    }
    if (fn == "integer_to_octal") {
        require_count(args, 1);
        return Value::make_string(
            int_to_base(fn, as_int(require_integer(args, 0)), 8));
    }
    if (fn == "octal_to_integer") {
        require_count(args, 1);
        return Value::make_int(
            base_to_int(fn, require_string(args, 0).string_value, 8, false));
    }
    if (fn == "integer_to_base") {
        require_count(args, 2);
        Int value = as_int(require_integer(args, 0));
        int base = static_cast<int>(as_int(require_integer(args, 1)));
        return Value::make_string(int_to_base(fn, value, base));
    }
    if (fn == "base_to_integer") {
        require_count(args, 2);
        const std::string& text = require_string(args, 0).string_value;
        int base = static_cast<int>(as_int(require_integer(args, 1)));
        return Value::make_int(base_to_int(fn, text, base, false));
    }
    if (fn == "text_to_binary") {
        require_count(args, 1);
        const std::string& text = require_string(args, 0).string_value;
        require_ascii(fn, text);
        std::string out;
        for (unsigned char c : text) {
            out += format_bits(c, 8);
        }
        return Value::make_string(out);
    }
    if (fn == "binary_to_text") {
        require_count(args, 1);
        const std::string& text = require_string(args, 0).string_value;
        if (text.size() % 8 != 0) {
            fail(fn, "binary text must contain whole eight-bit groups");
        }
        std::string out;
        for (std::size_t i = 0; i < text.size(); i += 8) {
            unsigned value = 0;
            for (std::size_t bit = 0; bit < 8; ++bit) {
                char c = text[i + bit];
                if (c != '0' && c != '1') {
                    fail(fn, "binary text may contain only 0 and 1");
                }
                value = (value << 1) | static_cast<unsigned>(c - '0');
            }
            if (value > 0x7F) {
                fail(fn, "decoded text is not ASCII");
            }
            out.push_back(static_cast<char>(value));
        }
        return Value::make_string(out);
    }
    if (fn == "json_escape") {
        require_count(args, 1);
        return Value::make_string(
            json_escape_text(fn, require_string(args, 0).string_value));
    }
    if (fn == "json_unescape") {
        require_count(args, 1);
        return Value::make_string(
            json_unescape_text(fn, require_string(args, 0).string_value));
    }
    if (fn == "html_escape") {
        require_count(args, 1);
        return Value::make_string(
            escape_entities(fn, require_string(args, 0).string_value));
    }
    if (fn == "html_unescape") {
        require_count(args, 1);
        return Value::make_string(
            unescape_entities(fn, require_string(args, 0).string_value));
    }
    if (fn == "xml_escape") {
        require_count(args, 1);
        return Value::make_string(
            escape_entities(fn, require_string(args, 0).string_value));
    }
    if (fn == "xml_unescape") {
        require_count(args, 1);
        return Value::make_string(
            unescape_entities(fn, require_string(args, 0).string_value));
    }
    if (fn == "csv_escape") {
        require_count(args, 1);
        return Value::make_string(
            csv_escape_text(fn, require_string(args, 0).string_value));
    }
    if (fn == "csv_unescape") {
        require_count(args, 1);
        return Value::make_string(
            csv_unescape_text(fn, require_string(args, 0).string_value));
    }
    if (fn == "url_query_encode") {
        require_count(args, 1);
        return Value::make_string(
            percent_encode(fn, require_string(args, 0).string_value, true));
    }
    if (fn == "url_query_decode") {
        require_count(args, 1);
        return Value::make_string(
            percent_decode(fn, require_string(args, 0).string_value, true));
    }
    if (fn == "ascii_to_integer") {
        require_count(args, 1);
        const std::string& text = require_string(args, 0).string_value;
        if (text.size() != 1 || static_cast<unsigned char>(text[0]) > 0x7F) {
            fail(fn, "exactly one ASCII character is required");
        }
        return Value::make_int(static_cast<unsigned char>(text[0]));
    }
    if (fn == "integer_to_ascii") {
        require_count(args, 1);
        Int value = as_int(require_integer(args, 0));
        if (value < 0 || value > 0x7F) {
            fail(fn, "ASCII code must be from 0 through 127");
        }
        return Value::make_string(std::string(1, static_cast<char>(value)));
    }
    if (fn == "integer_to_roman") {
        require_count(args, 1);
        return Value::make_string(
            roman_encode(fn, as_int(require_integer(args, 0))));
    }
    if (fn == "roman_to_integer") {
        require_count(args, 1);
        return Value::make_int(
            roman_decode(fn, require_string(args, 0).string_value));
    }
    if (fn == "rgb_to_hex") {
        require_count(args, 3);
        int red = static_cast<int>(as_int(require_integer(args, 0)));
        int green = static_cast<int>(as_int(require_integer(args, 1)));
        int blue = static_cast<int>(as_int(require_integer(args, 2)));
        if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255) {
            fail(fn, "RGB components must be from 0 through 255");
        }
        std::string out = "#";
        out += hex_byte(static_cast<unsigned>(red));
        out += hex_byte(static_cast<unsigned>(green));
        out += hex_byte(static_cast<unsigned>(blue));
        return Value::make_string(out);
    }
    if (fn == "hex_to_red" || fn == "hex_to_green" || fn == "hex_to_blue") {
        require_count(args, 1);
        const std::string& text = require_string(args, 0).string_value;
        std::size_t start = (!text.empty() && text[0] == '#') ? 1 : 0;
        if (text.size() - start != 6) {
            fail(fn, "a color needs exactly six hex digits");
        }
        int values[3] = {0, 0, 0};
        for (int component = 0; component < 3; ++component) {
            int high = hex_value(text[start + static_cast<std::size_t>(component) * 2]);
            int low = hex_value(text[start + static_cast<std::size_t>(component) * 2 + 1]);
            if (high < 0 || low < 0) {
                fail(fn, "invalid hex digit in color");
            }
            values[component] = (high << 4) | low;
        }
        if (fn == "hex_to_red") return Value::make_int(values[0]);
        if (fn == "hex_to_green") return Value::make_int(values[1]);
        return Value::make_int(values[2]);
    }
    if (fn == "seconds_to_duration") {
        require_count(args, 1);
        Int seconds = as_int(require_integer(args, 0));
        if (seconds < 0) {
            fail(fn, "duration must be non-negative");
        }
        Int hours = seconds / 3600;
        Int minutes = (seconds % 3600) / 60;
        Int secs = seconds % 60;
        std::ostringstream out;
        out << std::setw(2) << std::setfill('0') << hours << ':'
            << std::setw(2) << std::setfill('0') << minutes << ':'
            << std::setw(2) << std::setfill('0') << secs;
        return Value::make_string(out.str());
    }
    if (fn == "duration_to_seconds") {
        require_count(args, 1);
        const std::string& text = require_string(args, 0).string_value;
        std::size_t first = text.find(':');
        std::size_t second = first == std::string::npos ? std::string::npos
                                                       : text.find(':', first + 1);
        if (first == std::string::npos || second == std::string::npos ||
            text.find(':', second + 1) != std::string::npos) {
            fail(fn, "duration must use HH:MM:SS");
        }
        std::string hours_text = text.substr(0, first);
        std::string minutes_text = text.substr(first + 1, second - first - 1);
        std::string seconds_text = text.substr(second + 1);
        if (hours_text.size() < 2 || minutes_text.size() != 2 || seconds_text.size() != 2) {
            fail(fn, "duration must use HH:MM:SS with two-digit minutes and seconds");
        }
        auto all_digits = [](const std::string& value) {
            return !value.empty() &&
                   std::all_of(value.begin(), value.end(), [](char c) {
                       return std::isdigit(static_cast<unsigned char>(c)) != 0;
                   });
        };
        if (!all_digits(hours_text) || !all_digits(minutes_text) || !all_digits(seconds_text)) {
            fail(fn, "duration must contain decimal digits");
        }
        Int hours = 0;
        for (char c : hours_text) {
            int digit = c - '0';
            if (hours > (kMaxInt - digit) / 10) {
                fail(fn, "duration is too large");
            }
            hours = hours * 10 + digit;
        }
        int minutes = (minutes_text[0] - '0') * 10 + (minutes_text[1] - '0');
        int seconds = (seconds_text[0] - '0') * 10 + (seconds_text[1] - '0');
        if (minutes > 59 || seconds > 59) {
            fail(fn, "minutes and seconds must be from 00 through 59");
        }
        if (hours > (kMaxInt - minutes * 60 - seconds) / 3600) {
            fail(fn, "duration is too large");
        }
        return Value::make_int(hours * 3600 + minutes * 60 + seconds);
    }
    if (fn == "bytes_to_decimal_size" || fn == "bytes_to_binary_size") {
        require_count(args, 1);
        Int bytes = as_int(require_integer(args, 0));
        if (bytes < 0) {
            fail(fn, "byte count must be non-negative");
        }
        return Value::make_string(format_size(bytes, fn == "bytes_to_binary_size"));
    }
    if (fn == "storage_size_to_bytes") {
        require_count(args, 1);
        return Value::make_int(
            parse_storage_size(fn, require_string(args, 0).string_value));
    }
    if (fn == "bytes_to_bits") {
        require_count(args, 1);
        Int bytes = as_int(require_integer(args, 0));
        if (bytes < 0) {
            fail(fn, "byte count must be non-negative");
        }
        if (bytes > kMaxInt / 8) {
            fail(fn, "bit count is too large");
        }
        return Value::make_int(bytes * 8);
    }
    if (fn == "bits_to_bytes") {
        require_count(args, 1);
        Int bits = as_int(require_integer(args, 0));
        if (bits < 0) {
            fail(fn, "bit count must be non-negative");
        }
        if (bits % 8 != 0) {
            fail(fn, "bit count must be divisible by 8");
        }
        return Value::make_int(bits / 8);
    }
    if (fn == "integer_to_bytes") {
        require_count(args, 3);
        Int number = as_int(require_integer(args, 0));
        Int count_value = as_int(require_integer(args, 1));
        const std::string& order = require_string(args, 2).string_value;
        if (number < 0) {
            fail(fn, "a non-negative integer is required");
        }
        if (count_value < 1 || count_value > 8) {
            fail(fn, "byte count must be from 1 through 8");
        }
        require_byte_order(fn, order);
        std::size_t count = static_cast<std::size_t>(count_value);
        if (static_cast<std::uint64_t>(number) > bit_mask(static_cast<int>(count * 8))) {
            fail(fn, "integer does not fit in the requested number of bytes");
        }
        return Value::make_string(
            encode_hex_bytes_ordered(static_cast<std::uint64_t>(number), count, order));
    }
    if (fn == "bytes_to_integer") {
        require_count(args, 2);
        const std::string& text = require_string(args, 0).string_value;
        const std::string& order = require_string(args, 1).string_value;
        require_byte_order(fn, order);
        std::uint64_t value = decode_hex_bytes_ordered(fn, text, order);
        if (value > static_cast<std::uint64_t>(kMaxInt)) {
            fail(fn, "integer is too large");
        }
        return Value::make_int(static_cast<Int>(value));
    }
    if (fn == "signed_to_unsigned" || fn == "unsigned_to_signed") {
        require_count(args, 2);
        Int number = as_int(require_integer(args, 0));
        Int width_value = as_int(require_integer(args, 1));
        require_bit_width(fn, width_value, 1, 63);
        int width = static_cast<int>(width_value);
        std::uint64_t mask = bit_mask(width);
        if (fn == "signed_to_unsigned") {
            Int minimum = -(1LL << (width - 1));
            Int maximum = (1LL << (width - 1)) - 1;
            if (number < minimum || number > maximum) {
                fail(fn, "signed value does not fit the requested bit width");
            }
            return Value::make_int(
                static_cast<Int>(static_cast<std::uint64_t>(number) & mask));
        }
        if (number < 0 || static_cast<std::uint64_t>(number) > mask) {
            fail(fn, "unsigned value does not fit the requested bit width");
        }
        std::uint64_t value = static_cast<std::uint64_t>(number);
        if ((value & (1ULL << (width - 1))) != 0) {
            std::uint64_t magnitude = (1ULL << width) - value;
            return Value::make_int(-static_cast<Int>(magnitude));
        }
        return Value::make_int(static_cast<Int>(value));
    }
    if (fn == "float_to_bits") {
        require_count(args, 2);
        double value = as_double(require_number(args, 0));
        Int width_value = as_int(require_integer(args, 1));
        if (width_value != 16 && width_value != 32 && width_value != 64) {
            fail(fn, "float bit width must be 16, 32, or 64");
        }
        int width = static_cast<int>(width_value);
        std::uint64_t bits = 0;
        if (width == 16) {
            bits = float_to_half_bits(static_cast<float>(value));
        } else if (width == 32) {
            float single = static_cast<float>(value);
            std::uint32_t raw = 0;
            std::memcpy(&raw, &single, sizeof(raw));
            bits = raw;
        } else {
            std::uint64_t raw = 0;
            std::memcpy(&raw, &value, sizeof(raw));
            bits = raw;
        }
        return Value::make_string(format_bits(bits, width));
    }
    if (fn == "bits_to_float") {
        require_count(args, 1);
        const std::string& text = require_string(args, 0).string_value;
        if (text.size() != 16 && text.size() != 32 && text.size() != 64) {
            fail(fn, "bit string must be 16, 32, or 64 characters long");
        }
        std::uint64_t bits = 0;
        for (char c : text) {
            if (c != '0' && c != '1') {
                fail(fn, "bit string may contain only 0 and 1");
            }
            bits = (bits << 1) | static_cast<std::uint64_t>(c - '0');
        }
        if (text.size() == 16) {
            return Value::make_float(
                half_bits_to_float(static_cast<std::uint16_t>(bits)));
        }
        if (text.size() == 32) {
            std::uint32_t raw = static_cast<std::uint32_t>(bits);
            float value = 0.0F;
            std::memcpy(&value, &raw, sizeof(value));
            return Value::make_float(static_cast<double>(value));
        }
        double value = 0.0;
        std::memcpy(&value, &bits, sizeof(value));
        return Value::make_float(value);
    }
    if (fn == "integer_to_gray") {
        require_count(args, 2);
        Int number = as_int(require_integer(args, 0));
        Int width_value = as_int(require_integer(args, 1));
        require_bit_width(fn, width_value, 1, 63);
        int width = static_cast<int>(width_value);
        std::uint64_t value = checked_unsigned(fn, number, width);
        return Value::make_string(format_bits(value ^ (value >> 1), width));
    }
    if (fn == "gray_to_integer") {
        require_count(args, 1);
        const std::string& text = require_string(args, 0).string_value;
        if (text.empty() || text.size() > 63) {
            fail(fn, "Gray code must contain 1 through 63 bits");
        }
        std::uint64_t value = 0;
        for (char c : text) {
            if (c != '0' && c != '1') {
                fail(fn, "Gray code may contain only 0 and 1");
            }
            std::uint64_t bit = static_cast<std::uint64_t>(c - '0');
            value = (value << 1) | ((value & 1ULL) ^ bit);
        }
        return Value::make_int(static_cast<Int>(value));
    }
    if (fn == "integer_to_bcd") {
        require_count(args, 1);
        Int number = as_int(require_integer(args, 0));
        if (number < 0) {
            fail(fn, "a non-negative integer is required");
        }
        std::string digits = std::to_string(number);
        std::string out;
        for (char digit : digits) {
            out += format_bits(static_cast<std::uint64_t>(digit - '0'), 4);
        }
        return Value::make_string(out);
    }
    if (fn == "bcd_to_integer") {
        require_count(args, 1);
        const std::string& text = require_string(args, 0).string_value;
        if (text.empty() || text.size() % 4 != 0) {
            fail(fn, "BCD must contain a non-empty multiple of four bits");
        }
        Int value = 0;
        for (std::size_t i = 0; i < text.size(); i += 4) {
            int digit = 0;
            for (std::size_t bit = 0; bit < 4; ++bit) {
                char c = text[i + bit];
                if (c != '0' && c != '1') {
                    fail(fn, "BCD may contain only 0 and 1");
                }
                digit = (digit << 1) | (c - '0');
            }
            if (digit > 9) {
                fail(fn, "BCD nibble is not a decimal digit");
            }
            if (value > (kMaxInt - digit) / 10) {
                fail(fn, "BCD value is too large");
            }
            value = value * 10 + digit;
        }
        return Value::make_int(value);
    }
    if (fn == "integer_to_leb128") {
        require_count(args, 1);
        Int number = as_int(require_integer(args, 0));
        if (number < 0) {
            fail(fn, "a non-negative integer is required");
        }
        std::uint64_t value = static_cast<std::uint64_t>(number);
        std::vector<unsigned char> bytes;
        do {
            unsigned char byte = static_cast<unsigned char>(value & 0x7FU);
            value >>= 7;
            if (value != 0) {
                byte |= 0x80U;
            }
            bytes.push_back(byte);
        } while (value != 0);
        return Value::make_string(bytes_to_hex_text(bytes));
    }
    if (fn == "leb128_to_integer") {
        require_count(args, 1);
        const std::string& text = require_string(args, 0).string_value;
        auto bytes = parse_hex_bytes(fn, text, 1, 10);
        unsigned __int128 value = 0;
        int shift = 0;
        bool done = false;
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            unsigned char byte = bytes[i];
            if (shift >= 128) {
                fail(fn, "LEB128 value is too large");
            }
            value |= static_cast<unsigned __int128>(byte & 0x7FU) << shift;
            shift += 7;
            if ((byte & 0x80U) == 0) {
                if (i + 1 != bytes.size()) {
                    fail(fn, "trailing bytes after LEB128 value");
                }
                done = true;
                break;
            }
        }
        if (!done) {
            fail(fn, "truncated LEB128 value");
        }
        if (value > static_cast<unsigned __int128>(kMaxInt)) {
            fail(fn, "LEB128 value is too large");
        }
        // Reject non-minimal encodings.
        std::uint64_t encoded = static_cast<std::uint64_t>(value);
        std::vector<unsigned char> canonical;
        do {
            unsigned char byte = static_cast<unsigned char>(encoded & 0x7FU);
            encoded >>= 7;
            if (encoded != 0) {
                byte |= 0x80U;
            }
            canonical.push_back(byte);
        } while (encoded != 0);
        if (canonical != bytes) {
            fail(fn, "non-minimal LEB128 encoding");
        }
        return Value::make_int(static_cast<Int>(value));
    }
    if (fn == "signed_to_zigzag") {
        require_count(args, 1);
        Int number = as_int(require_integer(args, 0));
        constexpr Int limit = 1LL << 62;
        if (number < -limit || number > limit - 1) {
            fail(fn, "ZigZag input is out of range");
        }
        if (number >= 0) {
            return Value::make_int(number * 2);
        }
        std::uint64_t magnitude = static_cast<std::uint64_t>(-number);
        return Value::make_int(static_cast<Int>(magnitude * 2ULL - 1ULL));
    }
    if (fn == "zigzag_to_signed") {
        require_count(args, 1);
        Int number = as_int(require_integer(args, 0));
        if (number < 0) {
            fail(fn, "ZigZag input must be non-negative");
        }
        if ((number & 1) == 0) {
            return Value::make_int(number / 2);
        }
        return Value::make_int(-(number / 2) - 1);
    }
    if (fn == "decimal_to_fixed") {
        require_count(args, 2);
        double value = as_double(require_number(args, 0));
        Int bits = as_int(require_integer(args, 1));
        if (bits < 0 || bits > 62) {
            fail(fn, "fractional_bits must be from 0 through 62");
        }
        if (!std::isfinite(value)) {
            fail(fn, "fixed-point value must be finite");
        }
        long double scaled = static_cast<long double>(value) * std::ldexp(1.0L, static_cast<int>(bits));
        long double rounded = std::round(scaled);
        if (rounded < static_cast<long double>(std::numeric_limits<Int>::min()) ||
            rounded > static_cast<long double>(std::numeric_limits<Int>::max())) {
            fail(fn, "fixed-point value is out of range");
        }
        return Value::make_int(static_cast<Int>(rounded));
    }
    if (fn == "fixed_to_decimal") {
        require_count(args, 2);
        Int number = as_int(require_integer(args, 0));
        Int bits = as_int(require_integer(args, 1));
        if (bits < 0 || bits > 62) {
            fail(fn, "fractional_bits must be from 0 through 62");
        }
        return Value::make_float(
            static_cast<double>(std::ldexp(static_cast<long double>(number),
                                           -static_cast<int>(bits))));
    }
    if (fn == "rgb888_to_rgb565") {
        require_count(args, 3);
        int red = static_cast<int>(as_int(require_integer(args, 0)));
        int green = static_cast<int>(as_int(require_integer(args, 1)));
        int blue = static_cast<int>(as_int(require_integer(args, 2)));
        if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255) {
            fail(fn, "RGB components must be from 0 through 255");
        }
        Int packed = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
        return Value::make_int(packed);
    }
    if (fn == "rgb565_to_red" || fn == "rgb565_to_green" || fn == "rgb565_to_blue") {
        require_count(args, 1);
        Int color = as_int(require_integer(args, 0));
        if (color < 0 || color > 65535) {
            fail(fn, "RGB565 color must be from 0 through 65535");
        }
        int red5 = static_cast<int>((color >> 11) & 0x1F);
        int green6 = static_cast<int>((color >> 5) & 0x3F);
        int blue5 = static_cast<int>(color & 0x1F);
        if (fn == "rgb565_to_red") return Value::make_int((red5 << 3) | (red5 >> 2));
        if (fn == "rgb565_to_green") return Value::make_int((green6 << 2) | (green6 >> 4));
        return Value::make_int((blue5 << 3) | (blue5 >> 2));
    }
    if (fn == "pcm_sample_to_bytes") {
        require_count(args, 3);
        Int sample = as_int(require_integer(args, 0));
        Int width_value = as_int(require_integer(args, 1));
        const std::string& order = require_string(args, 2).string_value;
        if (width_value != 8 && width_value != 16 && width_value != 24 && width_value != 32) {
            fail(fn, "PCM bit width must be 8, 16, 24, or 32");
        }
        require_byte_order(fn, order);
        int width = static_cast<int>(width_value);
        Int minimum = -(1LL << (width - 1));
        Int maximum = (1LL << (width - 1)) - 1;
        if (sample < minimum || sample > maximum) {
            fail(fn, "PCM sample does not fit the selected bit width");
        }
        std::uint64_t raw = static_cast<std::uint64_t>(sample) & bit_mask(width);
        return Value::make_string(
            encode_hex_bytes_ordered(raw, static_cast<std::size_t>(width / 8), order));
    }
    if (fn == "bytes_to_pcm_sample") {
        require_count(args, 3);
        const std::string& text = require_string(args, 0).string_value;
        Int width_value = as_int(require_integer(args, 1));
        const std::string& order = require_string(args, 2).string_value;
        if (width_value != 8 && width_value != 16 && width_value != 24 && width_value != 32) {
            fail(fn, "PCM bit width must be 8, 16, 24, or 32");
        }
        require_byte_order(fn, order);
        int width = static_cast<int>(width_value);
        std::size_t count = static_cast<std::size_t>(width / 8);
        if (text.size() != count * 2) {
            fail(fn, "PCM byte string has the wrong length for the selected bit width");
        }
        std::uint64_t raw = decode_hex_bytes_ordered(fn, text, order);
        if ((raw & (1ULL << (width - 1))) != 0) {
            std::uint64_t magnitude = (1ULL << width) - raw;
            return Value::make_int(-static_cast<Int>(magnitude));
        }
        return Value::make_int(static_cast<Int>(raw));
    }
    if (fn == "signed_to_offset_binary") {
        require_count(args, 2);
        Int sample = as_int(require_integer(args, 0));
        Int width_value = as_int(require_integer(args, 1));
        if (width_value != 8 && width_value != 16 && width_value != 24 && width_value != 32) {
            fail(fn, "offset-binary bit width must be 8, 16, 24, or 32");
        }
        int width = static_cast<int>(width_value);
        Int minimum = -(1LL << (width - 1));
        Int maximum = (1LL << (width - 1)) - 1;
        if (sample < minimum || sample > maximum) {
            fail(fn, "sample does not fit the selected bit width");
        }
        return Value::make_int(sample + (1LL << (width - 1)));
    }
    if (fn == "offset_binary_to_signed") {
        require_count(args, 2);
        Int code = as_int(require_integer(args, 0));
        Int width_value = as_int(require_integer(args, 1));
        if (width_value != 8 && width_value != 16 && width_value != 24 && width_value != 32) {
            fail(fn, "offset-binary bit width must be 8, 16, 24, or 32");
        }
        int width = static_cast<int>(width_value);
        Int maximum = (1LL << width) - 1;
        if (code < 0 || code > maximum) {
            fail(fn, "offset-binary code does not fit the selected bit width");
        }
        return Value::make_int(code - (1LL << (width - 1)));
    }
    if (fn == "extract_bit_field" || fn == "replace_bit_field") {
        const bool replace = fn == "replace_bit_field";
        require_count(args, replace ? 4 : 3);
        Int number = as_int(require_integer(args, 0));
        Int start_value = as_int(require_integer(args, 1));
        Int width_value = as_int(require_integer(args, 2));
        if (number < 0) {
            fail(fn, "bit-field input must be a non-negative integer");
        }
        if (start_value < 0 || start_value > 62) {
            fail(fn, "start_bit must be from 0 through 62");
        }
        if (width_value < 1 || width_value > 63) {
            fail(fn, "width must be from 1 through 63");
        }
        int start = static_cast<int>(start_value);
        int width = static_cast<int>(width_value);
        if (start + width > 63) {
            fail(fn, "bit field must fit within 63 bits");
        }
        std::uint64_t mask = bit_mask(width);
        std::uint64_t input = static_cast<std::uint64_t>(number);
        if (!replace) {
            return Value::make_int(static_cast<Int>((input >> start) & mask));
        }
        Int field_value = as_int(require_integer(args, 3));
        if (field_value < 0 || static_cast<std::uint64_t>(field_value) > mask) {
            fail(fn, "replacement value does not fit the field width");
        }
        std::uint64_t cleared = input & ~(mask << start);
        return Value::make_int(
            static_cast<Int>(cleared | ((static_cast<std::uint64_t>(field_value) & mask) << start)));
    }
    if (fn == "newline_normalize") {
        require_count(args, 1);
        const std::string& text = require_string(args, 0).string_value;
        require_ascii(fn, text);
        std::string out;
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '\r') {
                out.push_back('\n');
                if (i + 1 < text.size() && text[i + 1] == '\n') {
                    ++i;
                }
            } else {
                out.push_back(text[i]);
            }
        }
        return Value::make_string(out);
    }

    throw ModuleError("RSConvert." + fn + ": unknown module function");
}

Value RsConvertModule::get_constant(const std::string& name) const {
    throw ModuleError("RSConvert." + name + ": unknown module constant");
}

} // namespace resiris
