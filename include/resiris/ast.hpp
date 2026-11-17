#pragma once

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "resiris/value.hpp"

namespace resiris {

class Expression;
class Statement;

using ExprPtr = std::shared_ptr<Expression>;
using StmtPtr = std::shared_ptr<Statement>;
using StmtList = std::vector<StmtPtr>;

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------

enum class ExprKind {
    Literal,
    Name,
    Unary,
    Binary,
    Call,
    ObjectAccess,
    ModuleAccess,
    ModuleConstantAccess,
    TypeConversion,
    FunctionalObjectDef,
};

class Expression : public std::enable_shared_from_this<Expression> {
public:
    explicit Expression(ExprKind kind) : kind(kind) {}
    virtual ~Expression() = default;

    ExprKind kind;
    int source_line = -1;
    int source_column = -1;
};

using ExprPtr = std::shared_ptr<Expression>;

class Literal : public Expression {
public:
    explicit Literal(Value value) : Expression(ExprKind::Literal), value(std::move(value)) {}
    Value value;
};

class Name : public Expression {
public:
    explicit Name(std::string name) : Expression(ExprKind::Name), name(std::move(name)) {}
    std::string name;
};

class UnaryExpr : public Expression {
public:
    UnaryExpr(std::string op, ExprPtr operand)
        : Expression(ExprKind::Unary),
          op(std::move(op)),
          operand(std::move(operand)) {}
    std::string op;
    ExprPtr operand;
};

class BinaryExpr : public Expression {
public:
    BinaryExpr(ExprPtr left, std::string op, ExprPtr right)
        : Expression(ExprKind::Binary),
          left(std::move(left)),
          op(std::move(op)),
          right(std::move(right)) {}
    ExprPtr left;
    std::string op;
    ExprPtr right;
};

class CallExpr : public Expression {
public:
    CallExpr(ExprPtr function, std::vector<ExprPtr> arguments)
        : Expression(ExprKind::Call),
          function(std::move(function)),
          arguments(std::move(arguments)) {}
    ExprPtr function;
    std::vector<ExprPtr> arguments;
};

class ObjectAccessExpr : public Expression {
public:
    ObjectAccessExpr(ExprPtr target, std::string member_name)
        : Expression(ExprKind::ObjectAccess),
          target(std::move(target)),
          member_name(std::move(member_name)) {}
    ExprPtr target;
    std::string member_name;
};

class ModuleAccessExpr : public Expression {
public:
    ModuleAccessExpr(std::string module_name, std::string member_name)
        : Expression(ExprKind::ModuleAccess),
          module_name(std::move(module_name)),
          member_name(std::move(member_name)) {}
    std::string module_name;
    std::string member_name;
};

class ModuleConstantAccessExpr : public Expression {
public:
    ModuleConstantAccessExpr(std::string module_name, std::string constant_name)
        : Expression(ExprKind::ModuleConstantAccess),
          module_name(std::move(module_name)),
          constant_name(std::move(constant_name)) {}
    std::string module_name;
    std::string constant_name;
};

class TypeConversionExpr : public Expression {
public:
    // target_type == nullopt means `type()` with no argument: query current type.
    TypeConversionExpr(ExprPtr value, std::optional<std::string> target_type)
        : Expression(ExprKind::TypeConversion),
          value(std::move(value)),
          target_type(std::move(target_type)) {}
    ExprPtr value;
    std::optional<std::string> target_type;
};

class FunctionalObjectDef : public Expression {
public:
    FunctionalObjectDef(std::vector<std::string> parameters, StmtList body)
        : Expression(ExprKind::FunctionalObjectDef),
          parameters(std::move(parameters)),
          body(std::move(body)) {}
    std::vector<std::string> parameters;
    StmtList body;
};

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------

enum class StmtKind {
    Include,
    Declaration,
    FunctionDef,
    LifecycleDef,
    IfStmt,
    ReturnStmt,
    PassStmt,
    PrintCmdStmt,
    Assignment,
    ExpressionStmt,
    MatStmt,
};

class Statement : public std::enable_shared_from_this<Statement> {
public:
    explicit Statement(StmtKind kind) : kind(kind) {}
    virtual ~Statement() = default;

    StmtKind kind;
    int source_line = -1;
    int source_column = -1;
};

class Program {
public:
    StmtList statements;
};

class Include : public Statement {
public:
    Include(std::vector<std::string> modules)
        : Statement(StmtKind::Include), modules(std::move(modules)) {}
    std::vector<std::string> modules;
};

class Declaration : public Statement {
public:
    Declaration(std::string kind, std::string name, std::string type_name, ExprPtr value)
        : Statement(StmtKind::Declaration),
          kind(std::move(kind)),
          name(std::move(name)),
          type_name(std::move(type_name)),
          value(std::move(value)) {}
    std::string kind;  // "v" or "c"
    std::string name;
    std::string type_name;
    ExprPtr value;  // may be null
};

class FunctionDef : public Statement {
public:
    FunctionDef(std::string name, std::vector<std::string> parameters, StmtList body)
        : Statement(StmtKind::FunctionDef),
          name(std::move(name)),
          parameters(std::move(parameters)),
          body(std::move(body)) {}
    std::string name;
    std::vector<std::string> parameters;
    StmtList body;
};

class LifecycleDef : public Statement {
public:
    LifecycleDef(std::string name, std::optional<std::string> parameter_name, StmtList body)
        : Statement(StmtKind::LifecycleDef),
          name(std::move(name)),
          parameter_name(std::move(parameter_name)),
          body(std::move(body)) {}
    std::string name;
    std::optional<std::string> parameter_name;
    StmtList body;
};

class IfStmt : public Statement {
public:
    IfStmt(ExprPtr condition, StmtList body, std::vector<std::pair<ExprPtr, StmtList>> elif_blocks, StmtList else_body)
        : Statement(StmtKind::IfStmt),
          condition(std::move(condition)),
          body(std::move(body)),
          elif_blocks(std::move(elif_blocks)),
          else_body(std::move(else_body)) {}
    ExprPtr condition;
    StmtList body;
    std::vector<std::pair<ExprPtr, StmtList>> elif_blocks;
    StmtList else_body;
};

class ReturnStmt : public Statement {
public:
    explicit ReturnStmt(ExprPtr value) : Statement(StmtKind::ReturnStmt), value(std::move(value)) {}
    ExprPtr value;  // may be null
};

class PassStmt : public Statement {
public:
    PassStmt() : Statement(StmtKind::PassStmt) {}
};

class PrintCmdStmt : public Statement {
public:
    explicit PrintCmdStmt(ExprPtr expression) : Statement(StmtKind::PrintCmdStmt), expression(std::move(expression)) {}
    ExprPtr expression;
};

class Assignment : public Statement {
public:
    Assignment(std::string target, std::string op, ExprPtr value)
        : Statement(StmtKind::Assignment),
          target(std::move(target)),
          op(std::move(op)),
          value(std::move(value)) {}
    std::string target;
    std::string op;
    ExprPtr value;
};

class ExpressionStmt : public Statement {
public:
    explicit ExpressionStmt(ExprPtr expression)
        : Statement(StmtKind::ExpressionStmt), expression(std::move(expression)) {}
    ExprPtr expression;
};

class MatCase {
public:
    MatCase(Value value, StmtList body, bool type_case)
        : value(std::move(value)), body(std::move(body)), type_case(type_case) {}
    Value value;
    StmtList body;
    bool type_case;
};

class MatStmt : public Statement {
public:
    MatStmt(ExprPtr value, std::vector<MatCase> cases, StmtList else_body)
        : Statement(StmtKind::MatStmt),
          value(std::move(value)),
          cases(std::move(cases)),
          else_body(std::move(else_body)) {}
    ExprPtr value;
    std::vector<MatCase> cases;
    StmtList else_body;
};

} // namespace resiris