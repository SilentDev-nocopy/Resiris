#pragma once

#include <stdexcept>
#include <string>

namespace resiris {

class ResirisError : public std::runtime_error {
public:
    explicit ResirisError(const std::string& message) : std::runtime_error(message) {}
};

class IntDivisionError : public ResirisError {
public:
    using ResirisError::ResirisError;
};

class UnknownVariableError : public ResirisError {
public:
    using ResirisError::ResirisError;
};

class ResirisTypeError : public ResirisError {
public:
    using ResirisError::ResirisError;
};

class ConstantAssignmentError : public ResirisError {
public:
    using ResirisError::ResirisError;
};

class MissingValueError : public ResirisError {
public:
    using ResirisError::ResirisError;
};

class FunctionError : public ResirisError {
public:
    using ResirisError::ResirisError;
};

class ModuleError : public ResirisError {
public:
    using ResirisError::ResirisError;
};

} // namespace resiris