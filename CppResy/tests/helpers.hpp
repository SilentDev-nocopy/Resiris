#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "catch.hpp"
#include "resiris/errors.hpp"
#include "resiris/interpreter.hpp"
#include "resiris/module.hpp"
#include "resiris/parser.hpp"
#include "resiris/rsbase.hpp"
#include "resiris/rsmath.hpp"
#include "resiris/syntax_error.hpp"
#include "resiris/tokenizer.hpp"
#include "resiris/value.hpp"

namespace testutil {

inline std::vector<resiris::Token> tokenize(const std::string& source) {
    resiris::Tokenizer tokenizer;
    return tokenizer.tokenize(source);
}

inline resiris::Program parse(const std::string& source) {
    auto tokens = tokenize(source);
    resiris::Parser parser(tokens);
    return parser.parse();
}

inline std::shared_ptr<resiris::ModuleRegistry> make_registry() {
    std::vector<std::shared_ptr<resiris::Module>> modules;
    modules.push_back(std::make_shared<resiris::RsMathModule>());
    modules.push_back(std::make_shared<resiris::RsBaseModule>());
    return std::make_shared<resiris::ModuleRegistry>(std::move(modules));
}

// Keeps the interpreter alive until the caller inspects `variables`.
struct Interp {
    std::shared_ptr<resiris::ModuleRegistry> registry = make_registry();
    std::unique_ptr<resiris::Interpreter> interp =
        std::make_unique<resiris::Interpreter>(registry);
    resiris::Program program;

    // Parse + execute the source fully (lifecycles, start, top-level).
    std::map<std::string, resiris::Variable>& exec(const std::string& source) {
        program = parse(source);
        return interp->run(program);
    }
};

inline const resiris::Value& variable(resiris::Interpreter& interp,
                                      const std::string& name) {
    auto it = interp.variables.find(name);
    REQUIRE(it != interp.variables.end());
    return it->second.value;
}

// Capture everything written to std::cout during its lifetime.
struct CaptureStdout {
    std::ostringstream buffer;
    std::streambuf* old_rdbuf;

    CaptureStdout() : old_rdbuf(std::cout.rdbuf()) {
        std::cout.rdbuf(buffer.rdbuf());
    }

    ~CaptureStdout() { std::cout.rdbuf(old_rdbuf); }

    std::string str() const { return buffer.str(); }
};

} // namespace testutil