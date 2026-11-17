#pragma once

#include "resiris/module.hpp"

namespace resiris {

// Text and numeric conversion module. The first version is ASCII-only: every
// text input and every decoded text result must stay in 0..127.
class RsConvertModule : public Module {
public:
    std::string module_name() const override { return "RSConvert"; }

    std::vector<std::string> function_names() const override {
        return {
            // Text encodings.
            "to_base64", "from_base64",
            "text_to_hex", "hex_to_text",
            "integer_to_hex", "hex_to_integer",
            "url_encode", "url_decode",
            "integer_to_binary", "binary_to_integer",
            "integer_to_octal", "octal_to_integer",
            "integer_to_base", "base_to_integer",
            "text_to_binary", "binary_to_text",
            // Escaping.
            "json_escape", "json_unescape",
            "html_escape", "html_unescape",
            "csv_escape", "csv_unescape",
            "url_query_encode", "url_query_decode",
            "xml_escape", "xml_unescape",
            // Characters and numbers.
            "ascii_to_integer", "integer_to_ascii",
            "integer_to_roman", "roman_to_integer",
            // Colors and time.
            "rgb_to_hex", "hex_to_red", "hex_to_green", "hex_to_blue",
            "seconds_to_duration", "duration_to_seconds",
            // Sizes.
            "bytes_to_decimal_size", "bytes_to_binary_size",
            "storage_size_to_bytes", "bytes_to_bits", "bits_to_bytes",
            // Binary helpers.
            "integer_to_bytes", "bytes_to_integer",
            "signed_to_unsigned", "unsigned_to_signed",
            "float_to_bits", "bits_to_float",
            "integer_to_gray", "gray_to_integer",
            "integer_to_bcd", "bcd_to_integer",
            "integer_to_leb128", "leb128_to_integer",
            "signed_to_zigzag", "zigzag_to_signed",
            "decimal_to_fixed", "fixed_to_decimal",
            "rgb888_to_rgb565", "rgb565_to_red", "rgb565_to_green",
            "rgb565_to_blue",
            "pcm_sample_to_bytes", "bytes_to_pcm_sample",
            "signed_to_offset_binary", "offset_binary_to_signed",
            "extract_bit_field", "replace_bit_field",
            // Newlines.
            "newline_normalize",
        };
    }

    std::map<std::string, std::string> variables() const override { return {}; }

    Value call_function(const std::string& fn, const std::vector<Value>& args) override;
    Value call_object_method(const std::string&, const std::shared_ptr<FrameAwareState>&,
                             const std::vector<Value>&) override {
        throw ModuleError("RSConvert has no object methods");
    }
    Value get_constant(const std::string& name) const override;
};

} // namespace resiris
