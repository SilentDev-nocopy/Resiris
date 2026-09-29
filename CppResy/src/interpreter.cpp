#include "resiris/interpreter.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <set>
#include <thread>

#include "resiris/parser.hpp"

namespace resiris {

namespace {

// Python's type(value).__name__ semantics used in error messages.
const char* py_typename(const Value& value) {
    switch (value.type) {
        case Value::Type::Bool: return "bool";
        case Value::Type::Int: return "int";
        case Value::Type::Float: return "float";
        case Value::Type::String: return "str";
        case Value::Type::ModuleObject: return "ModuleObject";
        case Value::Type::FunctionalObject: return "FunctionalObject";
        default: return "NoneType";
    }
}

std::string location_prefix(const std::shared_ptr<Statement>& statement) {
    std::string location;
    if (statement->source_line >= 0) {
        location = "line " + std::to_string(statement->source_line);
        if (statement->source_column >= 0) {
            location += ", column " + std::to_string(statement->source_column);
        }
    }
    return location;
}

[[noreturn]] void rethrow_with_prefix(const ResirisError& error, const std::string& message) {
    if (dynamic_cast<const IntDivisionError*>(&error)) {
        throw IntDivisionError(message);
    }
    if (dynamic_cast<const UnknownVariableError*>(&error)) {
        throw UnknownVariableError(message);
    }
    if (dynamic_cast<const ResirisTypeError*>(&error)) {
        throw ResirisTypeError(message);
    }
    if (dynamic_cast<const ConstantAssignmentError*>(&error)) {
        throw ConstantAssignmentError(message);
    }
    if (dynamic_cast<const MissingValueError*>(&error)) {
        throw MissingValueError(message);
    }
    if (dynamic_cast<const FunctionError*>(&error)) {
        throw FunctionError(message);
    }
    if (dynamic_cast<const ModuleError*>(&error)) {
        throw ModuleError(message);
    }
    throw ResirisError(message);
}

const char* statement_kind_name(StmtKind kind) {
    switch (kind) {
        case StmtKind::Include: return "Include";
        case StmtKind::Declaration: return "Declaration";
        case StmtKind::FunctionDef: return "FunctionDef";
        case StmtKind::LifecycleDef: return "LifecycleDef";
        case StmtKind::IfStmt: return "IfStmt";
        case StmtKind::ReturnStmt: return "ReturnStmt";
        case StmtKind::PassStmt: return "PassStmt";
        case StmtKind::PrintCmdStmt: return "PrintCmdStmt";
        case StmtKind::Assignment: return "Assignment";
        case StmtKind::ExpressionStmt: return "ExpressionStmt";
        case StmtKind::MatStmt: return "MatStmt";
    }
    return "Unknown";
}

const char* expression_kind_name(ExprKind kind) {
    switch (kind) {
        case ExprKind::Literal: return "Literal";
        case ExprKind::Name: return "Name";
        case ExprKind::Unary: return "UnaryExpr";
        case ExprKind::Binary: return "BinaryExpr";
        case ExprKind::Call: return "CallExpr";
        case ExprKind::ObjectAccess: return "ObjectAccessExpr";
        case ExprKind::ModuleAccess: return "ModuleAccessExpr";
        case ExprKind::ModuleConstantAccess: return "ModuleConstantAccessExpr";
        case ExprKind::TypeConversion: return "TypeConversionExpr";
        case ExprKind::FunctionalObjectDef: return "FunctionalObjectDef";
    }
    return "Unknown";
}

Value negate_value(Value value) {
    if (value.type == Value::Type::Int) {
        value.int_value = -value.int_value;
    } else if (value.type == Value::Type::Float) {
        value.float_value = -value.float_value;
    }
    return value;
}

} // namespace

Interpreter::Interpreter(std::shared_ptr<ModuleRegistry> registry)
    : registry_(std::move(registry)) {}

std::map<std::string, Variable>& Interpreter::run(Program& program) {
    std::set<std::string> top_level_declaration_names;
    for (const auto& statement : program.statements) {
        if (statement->kind == StmtKind::Declaration) {
            top_level_declaration_names.insert(
                std::static_pointer_cast<Declaration>(statement)->name);
        }
    }

    for (const auto& statement : program.statements) {
        if (statement->kind == StmtKind::FunctionDef) {
            auto function = std::static_pointer_cast<FunctionDef>(statement);
            if (top_level_declaration_names.count(function->name) != 0) {
                throw FunctionError(function->name + ": the name is already used as a variable");
            }
            register_function(function);
        } else if (statement->kind == StmtKind::LifecycleDef) {
            register_lifecycle(std::static_pointer_cast<LifecycleDef>(statement));
        }
    }

    for (const auto& statement : program.statements) {
        if (statement->kind == StmtKind::FunctionDef ||
            statement->kind == StmtKind::LifecycleDef) {
            continue;
        }
        execute(statement);
    }

    if (start_lifecycle) {
        execute_start();
    }

    return variables;
}

void Interpreter::register_function(const std::shared_ptr<FunctionDef>& statement) {
    if (functions.count(statement->name) != 0) {
        throw FunctionError(statement->name + ": the function already exists");
    }
    if (variables.count(statement->name) != 0) {
        throw FunctionError(statement->name + ": the name is already used as a variable");
    }
    functions[statement->name] = statement;
}

void Interpreter::register_lifecycle(const std::shared_ptr<LifecycleDef>& statement) {
    if (statement->name == "START") {
        if (start_lifecycle) {
            throw FunctionError("START: the lifecycle already exists");
        }
        start_lifecycle = statement;
        return;
    }
    if (statement->name == "PROCESS") {
        if (process_lifecycle) {
            throw FunctionError("PROCESS: the lifecycle already exists");
        }
        process_lifecycle = statement;
        return;
    }
    throw FunctionError(statement->name + ": unknown lifecycle");
}

void Interpreter::execute_start() {
    if (!start_lifecycle) {
        return;
    }
    execute_lifecycle(start_lifecycle, std::nullopt);
}

float Interpreter::get_process_fps() {
    auto variable = variables.find("FPS");
    if (variable == variables.end()) {
        throw ResirisError("PROCESS(FPS) requires a global constant `c FPS float`");
    }
    if (!variable->second.is_constant || variable->second.type_name != "float") {
        throw ResirisTypeError("PROCESS(FPS) requires `FPS` to be a global float constant");
    }
    return static_cast<float>(variable->second.value.float_value);
}

void Interpreter::execute_lifecycle(const std::shared_ptr<LifecycleDef>& lifecycle,
                                    std::optional<float> fps) {
    Scope local_scope;
    if (lifecycle->name == "PROCESS") {
        if (!fps) {
            throw ResirisError("PROCESS(FPS) requires an FPS value");
        }
        local_scope["FPS"] = Variable{
            Value::make_float(*fps), "float", false,
        };
    }

    scope_stack.push_back(std::move(local_scope));
    try {
        try {
            execute_block(lifecycle->body);
        } catch (const ReturnSignal&) {
        }
    } catch (...) {
        scope_stack.pop_back();
        throw;
    }
    scope_stack.pop_back();
}

void Interpreter::run_process_frames(int frame_count) {
    if (frame_count < 0) {
        throw std::invalid_argument("frame_count must not be negative");
    }
    if (!process_lifecycle) {
        return;
    }

    double fps = get_process_fps();
    if (fps <= 0.0) {
        throw ResirisTypeError("FPS must be greater than 0.0");
    }

    for (int i = 0; i < frame_count; ++i) {
        frame_time_ += 1.0 / fps;
        execute_lifecycle(process_lifecycle, static_cast<float>(fps));
    }
}

void Interpreter::run_process_forever() {
    if (!process_lifecycle) {
        return;
    }

    double fps = get_process_fps();
    if (fps <= 0.0) {
        throw ResirisTypeError("FPS must be greater than 0.0");
    }

    double frame_period = 1.0 / fps;
    auto frame_period_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double>(frame_period));
    auto next_frame = std::chrono::steady_clock::now();

    while (stop_flag_ == nullptr || *stop_flag_ == 0) {
        frame_time_ += frame_period;
        execute_lifecycle(process_lifecycle, static_cast<float>(fps));
        next_frame += frame_period_ns;
        auto now = std::chrono::steady_clock::now();
        auto sleep_time = next_frame - now;
        if (sleep_time > std::chrono::nanoseconds(0)) {
            std::this_thread::sleep_for(sleep_time);
        } else {
            next_frame = std::chrono::steady_clock::now();
        }
    }
}

Scope& Interpreter::current_scope() {
    if (!scope_stack.empty()) {
        return scope_stack.back();
    }
    return variables;
}

Variable* Interpreter::find_variable(const std::string& name) {
    if (!scope_stack.empty()) {
        auto& local_scope = scope_stack.back();
        auto found = local_scope.find(name);
        if (found != local_scope.end()) {
            return &found->second;
        }
    }
    auto found = variables.find(name);
    if (found != variables.end()) {
        return &found->second;
    }
    return nullptr;
}

void Interpreter::execute(const std::shared_ptr<Statement>& statement) {
    try {
        switch (statement->kind) {
            case StmtKind::Include: {
                for (const auto& module_name : std::static_pointer_cast<Include>(statement)->modules) {
                    registry_->load(module_name);
                }
                return;
            }
            case StmtKind::Declaration:
                execute_declaration(std::static_pointer_cast<Declaration>(statement));
                return;
            case StmtKind::Assignment:
                execute_assignment(std::static_pointer_cast<Assignment>(statement));
                return;
            case StmtKind::IfStmt:
                execute_if(std::static_pointer_cast<IfStmt>(statement));
                return;
            case StmtKind::MatStmt:
                execute_mat(std::static_pointer_cast<MatStmt>(statement));
                return;
            case StmtKind::FunctionDef:
            case StmtKind::LifecycleDef:
                return;
            case StmtKind::ReturnStmt:
                execute_return(std::static_pointer_cast<ReturnStmt>(statement));
                return;
            case StmtKind::PassStmt:
                return;
            case StmtKind::PrintCmdStmt:
                execute_print_cmd(std::static_pointer_cast<PrintCmdStmt>(statement));
                return;
            case StmtKind::ExpressionStmt:
                evaluate(std::static_pointer_cast<ExpressionStmt>(statement)->expression);
                return;
        }
        throw ResirisError(
            "The current interpreter version does not support: " +
            std::string(statement_kind_name(statement->kind)));
    } catch (const ModuleError& error) {
        std::string location = location_prefix(statement);
        std::string message;
        if (!location.empty()) {
            message = location + ": ";
        }
        message += error.what();
        throw ResirisError(message);
    } catch (const ResirisError& error) {
        std::string location = location_prefix(statement);
        if (!location.empty()) {
            std::string message = location + ": " + error.what();
            rethrow_with_prefix(error, message);
        }
        throw;
    }
}

void Interpreter::execute_declaration(const std::shared_ptr<Declaration>& statement) {
    if (current_scope().count(statement->name) != 0) {
        throw ResirisError(statement->name + ": the name is already in use");
    }

    if (!statement->value) {
        if (statement->type_name == "UnknownObject") {
            throw MissingValueError(
                statement->name + ": the first assignment of UnknownObject is required");
        }
        throw MissingValueError(
            statement->name + ": a value is currently required for the declaration");
    }

    Value value;
    std::string actual_type = statement->type_name;

    if (statement->type_name == "FunctionalObject") {
        if (statement->value->kind != ExprKind::FunctionalObjectDef) {
            throw ResirisTypeError(
                statement->name + ": FunctionalObject.new(...) is required");
        }
        auto def = std::static_pointer_cast<FunctionalObjectDef>(statement->value);
        auto function = std::make_shared<FunctionalObject>();
        function->parameters = def->parameters;
        function->body = def->body;
        value = Value::make_function(function);
    } else {
        // Evaluating the value can call a function, which pushes and pops
        // scopes. The scope must be looked up again afterwards: a reference
        // taken before the call would dangle once scope_stack reallocates.
        value = evaluate(statement->value);
    }

    if (statement->type_name == "UnknownObject") {
        actual_type = infer_type_name(value);
    }

    value = validate_and_coerce(actual_type, std::move(value), statement->name);

    current_scope()[statement->name] = Variable{
        std::move(value),
        actual_type,
        statement->kind == "c",
    };
}

void Interpreter::execute_assignment(const std::shared_ptr<Assignment>& statement) {
    Variable* variable = find_variable(statement->target);
    if (variable == nullptr) {
        throw UnknownVariableError(statement->target + ": unknown name");
    }

    if (variable->is_constant) {
        throw ConstantAssignmentError(statement->target + ": a constant cannot be modified");
    }

    // The right hand side can call a function, which may reallocate
    // scope_stack, so the variable is looked up again afterwards.
    Value right = evaluate(statement->value);
    variable = find_variable(statement->target);
    if (variable == nullptr) {
        throw UnknownVariableError(statement->target + ": unknown name");
    }

    Value new_value;
    if (statement->op == "=") {
        new_value = std::move(right);
    } else if (statement->op == "+=") {
        new_value = apply_binary(variable->value, "+", right, statement->target);
    } else if (statement->op == "-=") {
        new_value = apply_binary(variable->value, "-", right, statement->target);
    } else if (statement->op == "*=") {
        new_value = apply_binary(variable->value, "*", right, statement->target);
    } else if (statement->op == "/=") {
        new_value = apply_binary(variable->value, "/", right, statement->target);
    } else {
        throw ResirisError("Unknown assignment operator: " + statement->op);
    }

    variable->value = validate_and_coerce(
        variable->type_name, std::move(new_value), statement->target);
}

void Interpreter::execute_if(const std::shared_ptr<IfStmt>& statement) {
    Value condition = evaluate(statement->condition);
    if (condition.type != Value::Type::Bool) {
        throw ResirisTypeError("the if condition must produce a bool value");
    }

    if (condition.bool_value) {
        execute_block(statement->body);
        return;
    }

    for (const auto& [elif_condition_expr, elif_body] : statement->elif_blocks) {
        Value elif_condition = evaluate(elif_condition_expr);
        if (elif_condition.type != Value::Type::Bool) {
            throw ResirisTypeError("the elif condition must produce a bool value");
        }
        if (elif_condition.bool_value) {
            execute_block(elif_body);
            return;
        }
    }

    if (!statement->else_body.empty()) {
        execute_block(statement->else_body);
    }
}

void Interpreter::execute_mat(const std::shared_ptr<MatStmt>& statement) {
    Value value = evaluate(statement->value);
    std::string value_type = infer_type_name(value);

    for (const auto& case_node : statement->cases) {
        if (case_node.type_case) {
            if (value != case_node.value) {
                continue;
            }
            execute_block(case_node.body);
            return;
        }

        const Value& case_value = case_node.value;
        if (infer_type_name(case_value) != value_type) {
            continue;
        }

        if (value == case_value) {
            try {
                execute_block(case_node.body);
            } catch (const ResirisError& error) {
                throw ResirisError(
                    std::string(error.what()) + " Error code:\"MatchCaseExecutionError\"");
            }
            return;
        }
    }

    if (!statement->else_body.empty()) {
        execute_block(statement->else_body);
    }
}

void Interpreter::execute_block(const StmtList& statements) {
    for (const auto& statement : statements) {
        execute(statement);
    }
}

void Interpreter::execute_return(const std::shared_ptr<ReturnStmt>& statement) {
    if (scope_stack.empty()) {
        throw FunctionError("`return` can only be used inside a function");
    }

    std::optional<Value> value;
    if (statement->value) {
        value = evaluate(statement->value);
    }

    throw ReturnSignal(std::move(value));
}

void Interpreter::execute_print_cmd(const std::shared_ptr<PrintCmdStmt>& statement) {
    Value value = evaluate(statement->expression);
    std::cout << value_to_string(value) << "\n";
}

Value Interpreter::call_function(const std::string& function_name,
                                 const std::vector<Value>& arguments) {
    auto found = functions.find(function_name);
    if (found == functions.end()) {
        throw FunctionError(function_name + ": unknown function");
    }
    const auto& function = found->second;

    if (arguments.size() != function->parameters.size()) {
        throw FunctionError(
            function_name + ": " + std::to_string(function->parameters.size()) +
            " parameters required, but " + std::to_string(arguments.size()) +
            " arguments received");
    }

    Scope local_scope;
    for (std::size_t i = 0; i < function->parameters.size(); ++i) {
        const std::string& parameter_name = function->parameters[i];
        if (local_scope.count(parameter_name) != 0) {
            throw FunctionError(
                function_name + ": duplicate parameter name: " + parameter_name);
        }
        const Value& argument_value = arguments[i];
        local_scope[parameter_name] = Variable{
            argument_value,
            infer_type_name(argument_value),
            false,
        };
    }

    scope_stack.push_back(std::move(local_scope));

    Value result;
    try {
        try {
            execute_block(function->body);
        } catch (const ReturnSignal& signal) {
            if (signal.value) {
                result = std::move(*signal.value);
            }
        }
    } catch (...) {
        scope_stack.pop_back();
        throw;
    }
    scope_stack.pop_back();
    return result;
}

Value Interpreter::call_function_object(const std::shared_ptr<FunctionalObject>& function,
                                        const std::vector<Value>& arguments) {
    if (arguments.size() != function->parameters.size()) {
        throw FunctionError(
            "FunctionalObject: " + std::to_string(function->parameters.size()) +
            " parameters required, but " + std::to_string(arguments.size()) +
            " arguments received");
    }

    Scope local_scope;
    for (std::size_t i = 0; i < function->parameters.size(); ++i) {
        const std::string& parameter_name = function->parameters[i];
        if (local_scope.count(parameter_name) != 0) {
            throw FunctionError(
                "FunctionalObject: duplicate parameter name: " + parameter_name);
        }
        const Value& argument_value = arguments[i];
        local_scope[parameter_name] = Variable{
            argument_value,
            infer_type_name(argument_value),
            false,
        };
    }

    scope_stack.push_back(std::move(local_scope));

    Value result;
    try {
        try {
            execute_block(function->body);
        } catch (const ReturnSignal& signal) {
            if (signal.value) {
                result = std::move(*signal.value);
            }
        }
    } catch (...) {
        scope_stack.pop_back();
        throw;
    }
    scope_stack.pop_back();
    return result;
}

Value Interpreter::call_module_object_method(const std::shared_ptr<ModuleObject>& object,
                                             const std::string& method_name,
                                             const std::vector<Value>& arguments) {
    std::shared_ptr<FrameAwareState> handle = object->handle;
    if (handle) {
        handle->now = frame_time_;
    }

    Value result = registry_->call_object_method(
        object->module_name, handle, method_name, arguments);

    if (handle && handle->fire_callback) {
        std::string pending_callback = *handle->fire_callback;
        handle->fire_callback.reset();
        call_function(pending_callback, {});
    }

    return result;
}

Value Interpreter::evaluate(const std::shared_ptr<Expression>& expression) {
    switch (expression->kind) {
        case ExprKind::Literal: {
            auto literal = std::static_pointer_cast<Literal>(expression);
            return literal->value;
        }

        case ExprKind::Name: {
            auto name = std::static_pointer_cast<Name>(expression);
            Variable* variable = find_variable(name->name);
            if (variable != nullptr) {
                return variable->value;
            }
            if (functions.count(name->name) != 0) {
                return Value::make_string(name->name);
            }
            if (registry_->is_loaded(name->name)) {
                return Value::make_string(name->name);
            }
            throw UnknownVariableError(name->name + ": unknown name");
        }

        case ExprKind::Unary: {
            auto unary = std::static_pointer_cast<UnaryExpr>(expression);
            Value value = evaluate(unary->operand);
            bool is_number = value.is_number() && value.type != Value::Type::Bool;
            if (unary->op == "+") {
                if (!is_number) {
                    throw ResirisTypeError(
                        "unary + can only be used with numbers: " + value_repr(value));
                }
                return value;
            }
            if (unary->op == "-") {
                if (!is_number) {
                    throw ResirisTypeError(
                        "unary - can only be used with numbers: " + value_repr(value));
                }
                return negate_value(value);
            }
            throw ResirisError("Unknown unary operator: " + unary->op);
        }

        case ExprKind::Binary: {
            auto binary = std::static_pointer_cast<BinaryExpr>(expression);
            Value left = evaluate(binary->left);
            Value right = evaluate(binary->right);
            return apply_binary(std::move(left), binary->op, std::move(right), "");
        }

        case ExprKind::TypeConversion: {
            auto conversion = std::static_pointer_cast<TypeConversionExpr>(expression);
            Value value = evaluate(conversion->value);
            return convert_type(std::move(value), conversion->target_type);
        }

        case ExprKind::Call: {
            auto call = std::static_pointer_cast<CallExpr>(expression);
            std::vector<Value> arguments;
            arguments.reserve(call->arguments.size());
            for (const auto& argument : call->arguments) {
                arguments.push_back(evaluate(argument));
            }

            if (call->function->kind == ExprKind::ModuleAccess) {
                auto access = std::static_pointer_cast<ModuleAccessExpr>(call->function);
                Variable* variable = find_variable(access->module_name);
                if (variable != nullptr &&
                    variable->value.type == Value::Type::ModuleObject) {
                    return call_module_object_method(
                        variable->value.module_value, access->member_name, arguments);
                }
                return registry_->call_function(
                    access->module_name, access->member_name, arguments);
            }

            if (call->function->kind == ExprKind::ObjectAccess) {
                auto access = std::static_pointer_cast<ObjectAccessExpr>(call->function);
                Value target = evaluate(access->target);
                if (target.type != Value::Type::ModuleObject) {
                    throw ResirisTypeError(
                        "object method calls require a ModuleObject value");
                }
                return call_module_object_method(
                    target.module_value, access->member_name, arguments);
            }

            if (call->function->kind == ExprKind::Name) {
                auto name = std::static_pointer_cast<Name>(call->function);
                const std::string& function_name = name->name;

                if (function_name == "str") {
                    if (arguments.size() != 1) {
                        throw FunctionError(
                            "str: 1 argument required, but " +
                            std::to_string(arguments.size()) + " arguments received");
                    }
                    return Value::make_string(value_to_string(arguments[0]));
                }

                Variable* variable = find_variable(function_name);
                if (variable != nullptr &&
                    variable->value.type == Value::Type::FunctionalObject) {
                    return call_function_object(
                        variable->value.function_value, arguments);
                }

                return call_function(function_name, arguments);
            }

            throw FunctionError(
                "the function call target must currently be a name or FunctionalObject");
        }

        case ExprKind::ModuleAccess: {
            auto access = std::static_pointer_cast<ModuleAccessExpr>(expression);
            Variable* variable = find_variable(access->module_name);
            if (variable != nullptr &&
                variable->value.type == Value::Type::ModuleObject) {
                return call_module_object_method(
                    variable->value.module_value, access->member_name, {});
            }

            if (registry_->is_loaded(access->module_name)) {
                try {
                    if (registry_->has_function(access->module_name, access->member_name)) {
                        return registry_->call_function(
                            access->module_name, access->member_name, {});
                    }
                } catch (const ModuleError&) {
                    // fall through to the "only valid for function calls" error
                }
            }

            throw ResirisError("module `.` access is only valid for function calls");
        }

        case ExprKind::ObjectAccess: {
            auto access = std::static_pointer_cast<ObjectAccessExpr>(expression);
            Value target = evaluate(access->target);
            if (target.type != Value::Type::ModuleObject) {
                throw ResirisTypeError("object member access requires a ModuleObject value");
            }
            return call_module_object_method(
                target.module_value, access->member_name, {});
        }

        case ExprKind::ModuleConstantAccess: {
            auto access = std::static_pointer_cast<ModuleConstantAccessExpr>(expression);
            try {
                return registry_->get_constant(access->module_name, access->constant_name);
            } catch (const ModuleError& error) {
                throw ResirisError(error.what());
            }
        }

        case ExprKind::FunctionalObjectDef:
            break;
    }

    throw ResirisError(
        "The current interpreter version does not recognize this expression: " +
        std::string(expression_kind_name(expression->kind)));
}

Value Interpreter::apply_binary(Value left, const std::string& op, Value right,
                                const std::string& target_name) {
    (void)target_name;
    bool left_is_number =
        (left.type == Value::Type::Int || left.type == Value::Type::Float) &&
        left.type != Value::Type::Bool;
    bool right_is_number =
        (right.type == Value::Type::Int || right.type == Value::Type::Float) &&
        right.type != Value::Type::Bool;

    if (op == "+") {
        if (left.type == Value::Type::String && right.type == Value::Type::String) {
            left.string_value += right.string_value;
            return left;
        }
        if (!(left_is_number && right_is_number)) {
            throw ResirisTypeError(
                "+: strings of the same type or numeric operands are required; "
                "received: " + std::string(py_typename(left)) + ", " + py_typename(right));
        }
        if (left.type == Value::Type::Int && right.type == Value::Type::Int) {
            left.int_value += right.int_value;
            return left;
        }
        double result = (left.type == Value::Type::Float ? left.float_value
                                                         : static_cast<double>(left.int_value)) +
                        (right.type == Value::Type::Float ? right.float_value
                                                          : static_cast<double>(right.int_value));
        return Value::make_float(result);
    }

    if (op == "-" || op == "*" || op == "/" || op == "%") {
        if (!(left_is_number && right_is_number)) {
            throw ResirisTypeError(
                op + ": numeric operands are required; received: " +
                std::string(py_typename(left)) + ", " + py_typename(right));
        }
        if (op == "-") {
            if (left.type == Value::Type::Int && right.type == Value::Type::Int) {
                left.int_value -= right.int_value;
                return left;
            }
            double result =
                (left.type == Value::Type::Float ? left.float_value
                                                 : static_cast<double>(left.int_value)) -
                (right.type == Value::Type::Float ? right.float_value
                                                  : static_cast<double>(right.int_value));
            return Value::make_float(result);
        }
        if (op == "*") {
            if (left.type == Value::Type::Int && right.type == Value::Type::Int) {
                left.int_value *= right.int_value;
                return left;
            }
            double result =
                (left.type == Value::Type::Float ? left.float_value
                                                 : static_cast<double>(left.int_value)) *
                (right.type == Value::Type::Float ? right.float_value
                                                  : static_cast<double>(right.int_value));
            return Value::make_float(result);
        }
        if (op == "/") {
            double right_num = right.type == Value::Type::Float
                                   ? right.float_value
                                   : static_cast<double>(right.int_value);
            if (right_num == 0.0) {
                throw ResirisError("division by zero");
            }
            if (left.type == Value::Type::Int) {  // bool is excluded by is_number
                throw IntDivisionError(
                    "Cant division with int type! Must use float! Error code:\"IntDivisionError\"");
            }
            return Value::make_float(left.float_value / right_num);
        }
        if (op == "%") {
            double right_num = right.type == Value::Type::Float
                                   ? right.float_value
                                   : static_cast<double>(right.int_value);
            if (right_num == 0.0) {
                throw ResirisError("modulo by zero");
            }
            if (left.type == Value::Type::Int && right.type == Value::Type::Int) {
                // Python floor modulo: the result takes the divisor's sign.
                Int remainder = left.int_value % right.int_value;
                if (remainder != 0 && ((remainder < 0) != (right.int_value < 0))) {
                    remainder += right.int_value;
                }
                left.int_value = remainder;
                return left;
            }
            double result =
                (left.type == Value::Type::Float ? left.float_value
                                                 : static_cast<double>(left.int_value)) -
                std::floor((left.type == Value::Type::Float ? left.float_value
                                                            : static_cast<double>(left.int_value)) /
                           right_num) *
                    right_num;
            return Value::make_float(result);
        }
    }

    if (op == "==" || op == "!=" || op == ">" || op == "<" || op == ">=" || op == "<=") {
        if (op == "==") {
            return Value::make_bool(left == right);
        }
        if (op == "!=") {
            return Value::make_bool(!(left == right));
        }

        // Ordering comparisons follow Python semantics: booleans/int/float
        // compare numerically; strings compare lexically.
        bool comparable = false;
        if (left.type == Value::Type::Bool || left.type == Value::Type::Int ||
            left.type == Value::Type::Float) {
            comparable = right.type == Value::Type::Bool || right.type == Value::Type::Int ||
                         right.type == Value::Type::Float;
        } else if (left.type == Value::Type::String && right.type == Value::Type::String) {
            comparable = true;
        }
        if (!comparable) {
            throw ResirisTypeError(
                "'" + op + "' not supported between instances of '" +
                std::string(py_typename(left)) + "' and '" + py_typename(right) + "'");
        }

        auto as_double = [](const Value& v) {
            switch (v.type) {
                case Value::Type::Bool: return v.bool_value ? 1.0 : 0.0;
                case Value::Type::Int: return static_cast<double>(v.int_value);
                default: return v.float_value;
            }
        };

        if (left.type == Value::Type::String && right.type == Value::Type::String) {
            bool result;
            if (op == ">") result = left.string_value > right.string_value;
            else if (op == "<") result = left.string_value < right.string_value;
            else if (op == ">=") result = left.string_value >= right.string_value;
            else result = left.string_value <= right.string_value;
            return Value::make_bool(result);
        }

        double l = as_double(left);
        double r = as_double(right);
        bool result;
        if (op == ">") result = l > r;
        else if (op == "<") result = l < r;
        else if (op == ">=") result = l >= r;
        else result = l <= r;
        return Value::make_bool(result);
    }

    throw ResirisError("Unknown binary operator: " + op);
}

Value Interpreter::convert_type(Value value, const std::optional<std::string>& target_type) {
    if (!target_type) {
        return Value::make_string(infer_type_name(value));
    }

    const std::string& target = *target_type;
    std::string source_type = infer_type_name(value);

    if (target == source_type) {
        return value;
    }

    if (target == "int") {
        if (source_type == "bool") {
            return Value::make_int(value.bool_value ? 1 : 0);
        }
        if (source_type == "float") {
            return Value::make_int(static_cast<Int>(value.float_value + 0.5));
        }
        if (source_type == "int") {
            return value;
        }
        throw ResirisError(source_type + " cannot convert to int. Error code:\"ConversionFail\"");
    }

    if (target == "float") {
        if (source_type == "bool") {
            return Value::make_float(value.bool_value ? 1.0 : 0.0);
        }
        if (source_type == "int") {
            return Value::make_float(static_cast<double>(value.int_value));
        }
        if (source_type == "float") {
            return value;
        }
        throw ResirisError(source_type + " cannot convert to float. Error code:\"ConversionFail\"");
    }

    if (target == "string") {
        if (source_type == "bool") {
            return Value::make_string(value.bool_value ? "true" : "false");
        }
        if (source_type == "int" || source_type == "float") {
            return Value::make_string(value_to_string(value));
        }
        if (source_type == "string") {
            return value;
        }
        throw ResirisError(source_type + " cannot convert to string. Error code:\"ConversionFail\"");
    }

    if (target == "bool") {
        if (source_type == "bool") {
            return value;
        }
        if (source_type == "int" || source_type == "float") {
            double number = source_type == "int" ? static_cast<double>(value.int_value)
                                                 : value.float_value;
            return Value::make_bool(number > 0);
        }
        throw ResirisError(source_type + " cannot convert to bool. Error code:\"ConversionFail\"");
    }

    throw ResirisTypeError(target + " is an invalid type. Error code:\"TypeError\"");
}

std::string Interpreter::infer_type_name(const Value& value) {
    switch (value.type) {
        case Value::Type::Bool: return "bool";
        case Value::Type::Int: return "int";
        case Value::Type::Float: return "float";
        case Value::Type::String: return "string";
        case Value::Type::ModuleObject: return "ModuleObject";
        case Value::Type::FunctionalObject:
        default:
            throw ResirisTypeError(
                "UnknownObject: type cannot be determined: " +
                std::string(py_typename(value)));
    }
}

Value Interpreter::validate_and_coerce(const std::string& type_name, Value value,
                                       const std::string& name) {
    if (type_name == "UnknownObject") {
        return value;
    }

    if (type_name == "int") {
        if (value.type == Value::Type::Bool || value.type != Value::Type::Int) {
            throw ResirisTypeError(
                name + ": an int value is required, received: " + py_typename(value));
        }
        return value;
    }

    if (type_name == "float") {
        if (value.type == Value::Type::Bool || (value.type != Value::Type::Int &&
                                                value.type != Value::Type::Float)) {
            throw ResirisTypeError(
                name + ": a float value is required, received: " + py_typename(value));
        }
        if (value.type == Value::Type::Int) {
            return Value::make_float(static_cast<double>(value.int_value));
        }
        return value;
    }

    if (type_name == "string") {
        if (value.type != Value::Type::String) {
            throw ResirisTypeError(
                name + ": a string value is required, received: " + py_typename(value));
        }
        return value;
    }

    if (type_name == "bool") {
        if (value.type != Value::Type::Bool) {
            throw ResirisTypeError(
                name + ": a bool value is required, received: " + py_typename(value));
        }
        return value;
    }

    if (type_name == "FunctionalObject") {
        if (value.type != Value::Type::FunctionalObject) {
            throw ResirisTypeError(
                name + ": a FunctionalObject value is required, received: " + py_typename(value));
        }
        return value;
    }

    if (type_name == "ModuleObject") {
        if (value.type != Value::Type::ModuleObject) {
            throw ResirisTypeError(
                name + ": a ModuleObject value is required, received: " + py_typename(value));
        }
        return value;
    }

    throw ResirisError(name + ": unknown type: " + type_name);
}

} // namespace resiris