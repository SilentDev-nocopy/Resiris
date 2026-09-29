#pragma once

#include <string>
#include <vector>

#include "resiris/token.hpp"

namespace resiris {

class Tokenizer {
public:
    std::vector<Token> tokenize(const std::string& source) const;
};

} // namespace resiris