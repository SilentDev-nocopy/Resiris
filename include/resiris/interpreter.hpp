#pragma once

#include <csignal>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "resiris/ast.hpp"
#include "resiris/module.hpp"
#include "resiris/value.hpp"

namespace resiris {

struct Variable {
    Value value;
    std::string type_name;
    bool is_constant = false;
};

using Scope = std::map<std::string, Variable>;

class Interpreter {
public:
    explicit Interpreter(std::shared_ptr<ModuleRegistry> registry);

    // Execute a full program. Lifecycle/function registration happens before
    // ordinary top-level statements, matching the Python prototype.
    std::map<std::string, Variable>& run(Program& program);

    void register_lifecycle(const std::shared_ptr<LifecycleDef>& statement);
    void execute_start();
    void run_process_frames(int frame_count);

    // Runs the PROCESS lifecycle until the optional stop flag is set. The flag
    // is written by a signal handler, so only a sig_atomic_t is accepted.
    void run_process_forever();
    void set_stop_flag(const volatile std::sig_atomic_t* flag) { stop_flag_ = flag; }

    double frame_time() const { return frame_time_; }

    // Public state (mirrors the Python prototype attributes).
    std::map<std::string, Variable> variables;
    std::map<std::string, std::shared_ptr<FunctionDef>> functions;

private:
    std::shared_ptr<LifecycleDef> start_lifecycle;
    std::shared_ptr<LifecycleDef> process_lifecycle;
    std::vector<Scope> scope_stack;
    std::shared_ptr<ModuleRegistry> registry_;
    double frame_time_ = 0.0;
    const volatile std::sig_atomic_t* stop_flag_ = nullptr;

    Scope& current_scope();
    Variable* find_variable(const std::string& name);

    void execute(const std::shared_ptr<Statement>& statement);
    void execute_declaration(const std::shared_ptr<Declaration>& statement);
    void execute_assignment(const std::shared_ptr<Assignment>& statement);
    void execute_if(const std::shared_ptr<IfStmt>& statement);
    void execute_mat(const std::shared_ptr<MatStmt>& statement);
    void execute_block(const StmtList& statements);
    void execute_return(const std::shared_ptr<ReturnStmt>& statement);
    void execute_print_cmd(const std::shared_ptr<PrintCmdStmt>& statement);

    Value call_function(const std::string& function_name, const std::vector<Value>& arguments);
    Value call_function_object(const std::shared_ptr<FunctionalObject>& function,
                               const std::vector<Value>& arguments);
    Value call_module_object_method(const std::shared_ptr<ModuleObject>& object,
                                    const std::string& method_name,
                                    const std::vector<Value>& arguments);

    Value evaluate(const std::shared_ptr<Expression>& expression);
    Value apply_binary(Value left, const std::string& op, Value right,
                       const std::string& target_name);
    Value convert_type(Value value, const std::optional<std::string>& target_type);
    std::string infer_type_name(const Value& value);
    Value validate_and_coerce(const std::string& type_name, Value value,
                              const std::string& name);

    float get_process_fps();
    void execute_lifecycle(const std::shared_ptr<LifecycleDef>& lifecycle,
                           std::optional<float> fps);

    void register_function(const std::shared_ptr<FunctionDef>& statement);
};

} // namespace resiris