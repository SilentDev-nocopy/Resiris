#pragma once

#include "resiris/errors.hpp"

namespace resiris {

class ResirisSyntaxError : public ResirisError {
public:
    using ResirisError::ResirisError;
};

} // namespace resiris