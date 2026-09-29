#include <cctype>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

#include <sys/stat.h>

#include "resiris/interpreter.hpp"
#include "resiris/parser.hpp"
#include "resiris/rsbase.hpp"
#include "resiris/rsmath.hpp"
#include "resiris/syntax_error.hpp"
#include "resiris/tokenizer.hpp"
#include "resiris/version.hpp"

namespace resiris {

namespace {

constexpr const char* kRed = "\033[31m";
constexpr const char* kGreen = "\033[32m";
constexpr const char* kBlue = "\033[34m";
constexpr const char* kReset = "\033[0m";

// Set from the SIGINT handler, polled by the PROCESS frame loop.
volatile std::sig_atomic_t g_interrupted = 0;

extern "C" void handle_sigint(int signal_number) {
    (void)signal_number;
    g_interrupted = 1;
}

bool file_exists(const std::string& path) {
    struct stat info;
    return ::stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

std::string frontend_config_path() {
    const char* home = std::getenv("HOME");
    if (home == nullptr) {
        return "";
    }
    return std::string(home) + "/.config/resiris/frontend_visibility";
}

bool frontend_visibility_enabled() {
    return file_exists(frontend_config_path());
}

void set_frontend_visibility(bool enabled) {
    const std::string path = frontend_config_path();
    if (path.empty()) {
        return;
    }
    if (!enabled) {
        std::remove(path.c_str());
        return;
    }
    std::size_t slash = path.find_last_of('/');
    if (slash != std::string::npos) {
        std::string dir = path.substr(0, slash);
        std::string mkdir_cmd = "mkdir -p '" + dir + "'";
        std::system(mkdir_cmd.c_str());
    }
    std::ofstream out(path);
    out << "enabled\n";
}

const char* exception_name(const std::exception& error) {
    if (dynamic_cast<const ResirisSyntaxError*>(&error)) {
        return "ResirisSyntaxError";
    }
    if (dynamic_cast<const IntDivisionError*>(&error)) {
        return "IntDivisionError";
    }
    if (dynamic_cast<const UnknownVariableError*>(&error)) {
        return "UnknownVariableError";
    }
    if (dynamic_cast<const ResirisTypeError*>(&error)) {
        return "ResirisTypeError";
    }
    if (dynamic_cast<const ConstantAssignmentError*>(&error)) {
        return "ConstantAssignmentError";
    }
    if (dynamic_cast<const MissingValueError*>(&error)) {
        return "MissingValueError";
    }
    if (dynamic_cast<const FunctionError*>(&error)) {
        return "FunctionError";
    }
    return "RuntimeErrorResiris";
}

bool has_suffix(const std::string& value, const std::string& suffix) {
    if (suffix.size() > value.size()) {
        return false;
    }
    for (std::size_t i = 0; i < suffix.size(); ++i) {
        char a = static_cast<char>(std::tolower(static_cast<unsigned char>(
            value[value.size() - suffix.size() + i])));
        char b = static_cast<char>(std::tolower(static_cast<unsigned char>(suffix[i])));
        if (a != b) {
            return false;
        }
    }
    return true;
}

void print_banner() {
    std::cout << kRed
              << "██████╗ ███████╗███████╗██╗██████╗ ██╗███████╗\n"
                 "██╔══██╗██╔════╝██╔════╝██║██╔══██╗██║██╔════╝\n"
                 "██████╔╝█████╗  ███████╗██║██████╔╝██║███████╗\n"
                 "██╔══██╗██╔══╝  ╚════██║██║██╔══██╗██║╚════██║\n"
                 "██║  ██║███████╗███████║██║██║  ██║██║███████║\n"
                 "╚═╝  ╚═╝╚══════╝╚══════╝╚═╝╚═╝  ╚═╝╚══════╝\n"
              << kReset << "\n";
    std::cout << "Resiris CLI\n";
    std::cout << "Version:" << kVersion << "\n\n";
}

std::string read_all(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// Python repr() of a string, used by the frontend AST dump.
std::string json_escape(const std::string& value) {
    std::string out = "'";
    for (char ch : value) {
        switch (ch) {
            case '\'': out += "\\'"; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            default: out.push_back(ch); break;
        }
    }
    out += "'";
    return out;
}

class JsonDumper {
public:
    std::string dump(const Program& program) {
        std::string out = "{";
        out += json_escape("statements") + ": [";
        bool first = true;
        for (const auto& statement : program.statements) {
            if (!first) out += ", ";
            first = false;
            out += dump_statement(statement);
        }
        out += "]";
        out += "}";
        return out;
    }

private:
    std::string dump_statement(const std::shared_ptr<Statement>& statement) {
        switch (statement->kind) {
            case StmtKind::Include:
                return object("Include", {{"modules", array(std::static_pointer_cast<Include>(statement)->modules)}});
            case StmtKind::Declaration: {
                auto decl = std::static_pointer_cast<Declaration>(statement);
                return object("Declaration", {
                    {"kind", jstr(decl->kind)},
                    {"name", jstr(decl->name)},
                    {"type_name", jstr(decl->type_name)},
                    {"value", decl->value ? dump_expr(decl->value) : "None"},
                });
            }
            case StmtKind::FunctionDef: {
                auto fn = std::static_pointer_cast<FunctionDef>(statement);
                return object("FunctionDef", {
                    {"name", jstr(fn->name)},
                    {"parameters", array(fn->parameters)},
                    {"body", block(fn->body)},
                });
            }
            case StmtKind::LifecycleDef: {
                auto lc = std::static_pointer_cast<LifecycleDef>(statement);
                return object("LifecycleDef", {
                    {"name", jstr(lc->name)},
                    {"parameter_name", lc->parameter_name ? jstr(*lc->parameter_name) : "None"},
                    {"body", block(lc->body)},
                });
            }
            case StmtKind::IfStmt: {
                auto stmt = std::static_pointer_cast<IfStmt>(statement);
                std::string elifs = "[";
                for (std::size_t i = 0; i < stmt->elif_blocks.size(); ++i) {
                    if (i) elifs += ", ";
                    elifs += "(" + dump_expr(stmt->elif_blocks[i].first) + ", " +
                             block(stmt->elif_blocks[i].second) + ")";
                }
                elifs += "]";
                return object("IfStmt", {
                    {"condition", dump_expr(stmt->condition)},
                    {"body", block(stmt->body)},
                    {"elif_blocks", elifs},
                    {"else_body", stmt->else_body.empty() ? "None" : block(stmt->else_body)},
                });
            }
            case StmtKind::ReturnStmt: {
                auto stmt = std::static_pointer_cast<ReturnStmt>(statement);
                return object("ReturnStmt", {{"value", stmt->value ? dump_expr(stmt->value) : "None"}});
            }
            case StmtKind::PassStmt:
                return object("PassStmt", {});
            case StmtKind::PrintCmdStmt: {
                auto stmt = std::static_pointer_cast<PrintCmdStmt>(statement);
                return object("PrintCmdStmt", {{"expression", dump_expr(stmt->expression)}});
            }
            case StmtKind::Assignment: {
                auto stmt = std::static_pointer_cast<Assignment>(statement);
                return object("Assignment", {
                    {"target", jstr(stmt->target)},
                    {"operator", jstr(stmt->op)},
                    {"value", dump_expr(stmt->value)},
                });
            }
            case StmtKind::ExpressionStmt: {
                auto stmt = std::static_pointer_cast<ExpressionStmt>(statement);
                return object("ExpressionStmt", {{"expression", dump_expr(stmt->expression)}});
            }
            case StmtKind::MatStmt: {
                auto stmt = std::static_pointer_cast<MatStmt>(statement);
                std::string cases = "[";
                for (std::size_t i = 0; i < stmt->cases.size(); ++i) {
                    if (i) cases += ", ";
                    const auto& mat_case = stmt->cases[i];
                    cases += "{";
                    cases += jstr("value") + ": " + dump_value(mat_case.value);
                    cases += ", " + jstr("body") + ": " + block(mat_case.body);
                    cases += ", " + jstr("type_case") + ": " +
                             (mat_case.type_case ? "True" : "False");
                    cases += "}";
                }
                cases += "]";
                return object("MatStmt", {
                    {"value", dump_expr(stmt->value)},
                    {"cases", cases},
                    {"else_body", stmt->else_body.empty() ? "None" : block(stmt->else_body)},
                });
            }
        }
        return "None";
    }

    std::string dump_expr(const std::shared_ptr<Expression>& expression) {
        switch (expression->kind) {
            case ExprKind::Literal:
                return object("Literal", {{"value", dump_value(std::static_pointer_cast<Literal>(expression)->value)}});
            case ExprKind::Name: {
                auto node = std::static_pointer_cast<Name>(expression);
                return object("Name", {{"name", jstr(node->name)}});
            }
            case ExprKind::Unary: {
                auto node = std::static_pointer_cast<UnaryExpr>(expression);
                return object("UnaryExpr", {
                    {"operator", jstr(node->op)},
                    {"operand", dump_expr(node->operand)},
                });
            }
            case ExprKind::Binary: {
                auto node = std::static_pointer_cast<BinaryExpr>(expression);
                return object("BinaryExpr", {
                    {"left", dump_expr(node->left)},
                    {"operator", jstr(node->op)},
                    {"right", dump_expr(node->right)},
                });
            }
            case ExprKind::Call: {
                auto node = std::static_pointer_cast<CallExpr>(expression);
                std::string args = "[";
                for (std::size_t i = 0; i < node->arguments.size(); ++i) {
                    if (i) args += ", ";
                    args += dump_expr(node->arguments[i]);
                }
                args += "]";
                return object("CallExpr", {{"function", dump_expr(node->function)}, {"arguments", args}});
            }
            case ExprKind::ObjectAccess: {
                auto node = std::static_pointer_cast<ObjectAccessExpr>(expression);
                return object("ObjectAccessExpr", {
                    {"target", dump_expr(node->target)},
                    {"member_name", jstr(node->member_name)},
                });
            }
            case ExprKind::ModuleAccess: {
                auto node = std::static_pointer_cast<ModuleAccessExpr>(expression);
                return object("ModuleAccessExpr", {
                    {"module_name", jstr(node->module_name)},
                    {"member_name", jstr(node->member_name)},
                });
            }
            case ExprKind::ModuleConstantAccess: {
                auto node = std::static_pointer_cast<ModuleConstantAccessExpr>(expression);
                return object("ModuleConstantAccessExpr", {
                    {"module_name", jstr(node->module_name)},
                    {"constant_name", jstr(node->constant_name)},
                });
            }
            case ExprKind::TypeConversion: {
                auto node = std::static_pointer_cast<TypeConversionExpr>(expression);
                return object("TypeConversionExpr", {
                    {"value", dump_expr(node->value)},
                    {"target_type", node->target_type ? jstr(*node->target_type) : "None"},
                });
            }
            case ExprKind::FunctionalObjectDef: {
                auto node = std::static_pointer_cast<FunctionalObjectDef>(expression);
                return object("FunctionalObjectDef", {
                    {"parameters", array(node->parameters)},
                    {"body", block(node->body)},
                });
            }
        }
        return "None";
    }

    std::string block(const StmtList& statements) {
        std::string out = "[";
        for (std::size_t i = 0; i < statements.size(); ++i) {
            if (i) out += ", ";
            out += dump_statement(statements[i]);
        }
        out += "]";
        return out;
    }

    std::string array(const std::vector<std::string>& values) {
        std::string out = "[";
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i) out += ", ";
            out += jstr(values[i]);
        }
        out += "]";
        return out;
    }

    std::string dump_value(const Value& value) {
        switch (value.type) {
            case Value::Type::Bool: return value.bool_value ? "True" : "False";
            case Value::Type::Int: return std::to_string(value.int_value);
            case Value::Type::Float: return float_to_string(value.float_value);
            case Value::Type::String: return jstr(value.string_value);
            default: return "None";
        }
    }

    // The prototype dumps `asdict(node)`, so no node class name is included.
    std::string object(const std::string&, const std::vector<std::pair<std::string, std::string>>& fields) {
        std::string out = "{";
        bool first = true;
        for (const auto& [key, value] : fields) {
            if (!first) out += ", ";
            first = false;
            out += jstr(key) + ": " + value;
        }
        out += "}";
        return out;
    }

    std::string jstr(const std::string& value) { return json_escape(value); }
};

int run_file(const std::string& source_path, bool show_frontend) {
    if (!has_suffix(source_path, ".resy")) {
        std::cout << kRed << "ResirisError:" << kReset
                  << " Resiris files must have the .resy extension.\n";
        return 1;
    }

    if (!file_exists(source_path)) {
        std::cout << kRed << "ResirisError:" << kReset
                  << " file not found: " << source_path << "\n";
        return 1;
    }

    try {
        std::string source = read_all(source_path);

        Tokenizer tokenizer;
        std::vector<Token> tokens = tokenizer.tokenize(source);

        Parser parser(tokens);
        Program program = parser.parse();

        auto registry = std::make_shared<ModuleRegistry>(std::vector<std::shared_ptr<Module>>{
            std::make_shared<RsMathModule>(),
            std::make_shared<RsBaseModule>(),
        });

        Interpreter interpreter(registry);
        interpreter.set_stop_flag(&g_interrupted);
        std::signal(SIGINT, handle_sigint);
        interpreter.run(program);
        interpreter.run_process_forever();
        std::signal(SIGINT, SIG_DFL);

        if (g_interrupted) {
            std::cout << "\n" << kGreen << "Resiris program stopped." << kReset << "\n";
            return 0;
        }

        if (show_frontend) {
            std::cout << "\n<TOKENS>\n";
            for (const auto& token : tokens) {
                std::cout << "Token(" << token_type_name(token.type) << ", "
                          << python_repr(token.value) << ", " << token.line << ":"
                          << token.column << ")\n";
            }

            std::cout << "\n<AST>\n";
            JsonDumper dumper;
            std::cout << dumper.dump(program) << "\n";
        }
    } catch (const ResirisSyntaxError& error) {
        std::cout << kRed << "ResirisSyntaxError:" << kReset << " " << error.what() << "\n";
        return 1;
    } catch (const ResirisError& error) {
        std::cout << kRed << exception_name(error) << ":" << kReset << " "
                  << error.what() << "\n";
        return 1;
    } catch (const std::exception& error) {
        std::cout << kRed << "ResirisError:" << kReset << " " << error.what() << "\n";
        return 1;
    } catch (...) {
        std::cout << kRed << "ResirisError:" << kReset << " unknown failure\n";
        return 1;
    }

    return 0;
}

} // namespace

int main_impl(int argc, char** argv) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    bool enable_frontend = false;
    bool disable_frontend = false;
    bool show_version = false;
    std::optional<std::string> file;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--enable-frontend-visibility") {
            enable_frontend = true;
        } else if (arg == "--disable-frontend-visibility") {
            disable_frontend = true;
        } else if (arg == "--version") {
            show_version = true;
        } else if (arg == "--help" || arg == "-h") {
            print_banner();
            std::cout << "Usage:\n";
            std::cout << "  resiris <file.resy>\n\n";
            std::cout << "Options:\n";
            std::cout << "  --enable-frontend-visibility\n";
            std::cout << "  --disable-frontend-visibility\n";
            std::cout << "  --help\n";
            std::cout << "  --version\n";
            return 0;
        } else {
            if (file) {
                std::cout << kRed << "Resiris CLI error:" << kReset
                          << " a single .resy file is accepted\n";
                return 2;
            }
            file = arg;
        }
    }

    if (show_version) {
        std::cout << "Version:" << kVersion << "\n";
        return 0;
    }

    if (enable_frontend) {
        set_frontend_visibility(true);
        std::cout << kGreen << "Frontend visibility enabled." << kReset << "\n";
        if (!file) {
            return 0;
        }
    }

    if (disable_frontend) {
        set_frontend_visibility(false);
        std::cout << kGreen << "Frontend visibility disabled." << kReset << "\n";
        if (!file) {
            return 0;
        }
    }

    if (!file) {
        print_banner();
        std::cout << "Usage:\n";
        std::cout << "  resiris <file.resy>\n\n";
        std::cout << "Example:\n";
        std::cout << "  resiris main.resy\n\n";
        std::cout << kBlue << "Options:" << kReset << "\n";
        std::cout << "  --enable-frontend-visibility\n";
        std::cout << "               Enable frontend output for .resy files\n";
        std::cout << "  --disable-frontend-visibility\n";
        std::cout << "               Disable frontend output for .resy files\n";
        std::cout << "  --help       Show this help message\n";
        std::cout << "  --version    Show the Resiris version\n";
        return 0;
    }

    return run_file(*file, frontend_visibility_enabled());
}

} // namespace resiris

int main(int argc, char** argv) {
    return resiris::main_impl(argc, argv);
}