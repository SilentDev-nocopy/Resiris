#include "resiris/modules/rsnetwork.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "resiris/value.hpp"

namespace resiris {

namespace {

void require_count(const std::vector<Value>& args, std::size_t count) {
    if (args.size() != count) {
        throw ModuleSignatureError();
    }
}

[[noreturn]] void wifi_unavailable(const std::string& fn) {
    throw ModuleError("RSNetwork." + fn + ": Wi-Fi is ESP32 only and unavailable in the CLI");
}

std::shared_ptr<HtmlServerState> require_server(const std::shared_ptr<FrameAwareState>& handle) {
    if (!handle) {
        throw ModuleError("module object handle is missing");
    }
    auto state = std::dynamic_pointer_cast<HtmlServerState>(handle);
    if (!state) {
        throw ModuleError("RSNetwork method requires an HTML server handle");
    }
    return state;
}

bool is_wifi_method(const std::string& fn) {
    return fn == "get_all_networks" || fn == "connect" || fn == "set_route_mode" ||
           fn == "get_ip" || fn == "get_ssid" || fn == "disconnect" ||
           fn == "get_route_mode" || fn == "set_route_info" ||
           fn == "get_own_route_info" || fn == "get_global_speed";
}

std::string url_decode(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '+') {
            out += ' ';
        } else if (c == '%' && i + 2 < text.size() &&
                   std::isxdigit(static_cast<unsigned char>(text[i + 1])) &&
                   std::isxdigit(static_cast<unsigned char>(text[i + 2]))) {
            const char digits[] = {text[i + 1], text[i + 2], '\0'};
            out += static_cast<char>(std::strtol(digits, nullptr, 16));
            i += 2;
        } else {
            out += c;
        }
    }
    return out;
}

// The client script sends /call/<name>?value=<text>. Only `value` carries data.
std::string query_value(const std::string& target) {
    auto question = target.find('?');
    if (question == std::string::npos) {
        return "";
    }
    std::string query = target.substr(question + 1);
    std::size_t start = 0;
    while (start <= query.size()) {
        std::size_t end = query.find('&', start);
        if (end == std::string::npos) {
            end = query.size();
        }
        std::string pair = query.substr(start, end - start);
        auto equals = pair.find('=');
        if (equals != std::string::npos && pair.substr(0, equals) == "value") {
            return url_decode(pair.substr(equals + 1));
        }
        if (end == query.size()) {
            break;
        }
        start = end + 1;
    }
    return "";
}

void send_all(int fd, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        ssize_t result = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (result <= 0) {
            return;
        }
        sent += static_cast<std::size_t>(result);
    }
}

void send_http(int fd, const std::string& status, const std::string& content_type,
               const std::string& body) {
    std::string response = "HTTP/1.1 " + status + "\r\n" +
                           "Content-Type: " + content_type + "\r\n" +
                           "Content-Length: " + std::to_string(body.size()) + "\r\n" +
                           "Connection: close\r\n\r\n" + body;
    send_all(fd, response);
}

void set_nonblocking(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

bool parse_int(const std::string& text, long long& out) {
    char* end = nullptr;
    long long value = std::strtoll(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') {
        return false;
    }
    out = value;
    return true;
}

bool parse_float(const std::string& text, double& out) {
    char* end = nullptr;
    double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') {
        return false;
    }
    out = value;
    return true;
}

std::string lower(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

std::string read_request(HtmlServerState& state, const std::string& fn,
                         const std::string& type) {
    if (!state.has_request) {
        throw ModuleError("RSNetwork." + fn + ": no most recent " + type + " data");
    }
    state.request_read = true;
    return state.request_value;
}

void require_awaiting_response(const HtmlServerState& state,
                                            const std::string& fn) {
    if (!state.has_request) {
        throw ModuleError("RSNetwork." + fn + ": no request awaiting a response");
    }
}

} // namespace

HtmlServerState::~HtmlServerState() {
    if (client_fd >= 0) {
        ::close(client_fd);
    }
    if (listen_fd >= 0) {
        ::close(listen_fd);
    }
}

std::vector<std::string> RsNetworkModule::function_names() const {
    return {
        "wifi", "create_wifi_data_process_handler", "html_server_handler",
        "get_all_networks", "connect", "set_route_mode", "get_ip", "get_ssid",
        "disconnect", "get_route_mode", "set_route_info", "get_own_route_info",
        "get_global_speed",
        "create_server", "set_page_content", "set_client_script", "start_server",
        "process", "request_string_data", "request_int_data", "request_float_data",
        "request_boolean_data", "get_request_state", "respond_string_data",
        "respond_int_data", "respond_float_data", "respond_boolean_data",
    };
}

Value RsNetworkModule::call_function(const std::string& fn, const std::vector<Value>& args) {
    require_count(args, 0);

    if (fn == "wifi" || fn == "create_wifi_data_process_handler") {
        wifi_unavailable(fn);
    }
    if (fn == "html_server_handler") {
        return Value::make_module(
            std::make_shared<ModuleObject>("RSNetwork", std::make_shared<HtmlServerState>()));
    }
    throw ModuleError("RSNetwork." + fn + ": unknown module function");
}

Value RsNetworkModule::call_object_method(const std::string& fn,
                                          const std::shared_ptr<FrameAwareState>& handle,
                                          const std::vector<Value>& args) {
    if (is_wifi_method(fn)) {
        wifi_unavailable(fn);
    }

    auto server = require_server(handle);

    if (fn == "create_server") {
        require_count(args, 1);
        if (args[0].type != Value::Type::Int) {
            throw ModuleSignatureError();
        }
        if (server->created) {
            throw ModuleError("RSNetwork.create_server: server already created");
        }
        long long port = args[0].int_value;
        if (port < 1 || port > 65535) {
            throw ModuleError("RSNetwork.create_server: port must be between 1 and 65535");
        }
        server->port = static_cast<int>(port);
        server->created = true;
        return Value::make_bool(true);
    }

    if (fn == "set_page_content") {
        require_count(args, 1);
        if (args[0].type != Value::Type::String) {
            throw ModuleSignatureError();
        }
        server->page_content = args[0].string_value;
        return Value::make_bool(true);
    }

    if (fn == "set_client_script") {
        require_count(args, 1);
        if (args[0].type != Value::Type::String) {
            throw ModuleSignatureError();
        }
        server->client_script = args[0].string_value;
        return Value::make_bool(true);
    }

    if (fn == "start_server") {
        require_count(args, 0);
        if (!server->created) {
            throw ModuleError("RSNetwork.start_server: call create_server(port_number) first");
        }
        if (server->started) {
            throw ModuleError("RSNetwork.start_server: server already started");
        }

        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            throw ModuleError("RSNetwork.start_server: could not create socket: " +
                              std::string(std::strerror(errno)));
        }
        int reuse = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(static_cast<uint16_t>(server->port));
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
            std::string message = std::strerror(errno);
            ::close(fd);
            throw ModuleError("RSNetwork.start_server: could not bind 127.0.0.1:" +
                              std::to_string(server->port) + ": " + message);
        }
        if (::listen(fd, 8) != 0) {
            std::string message = std::strerror(errno);
            ::close(fd);
            throw ModuleError("RSNetwork.start_server: could not listen: " + message);
        }
        set_nonblocking(fd);
        server->listen_fd = fd;
        server->started = true;
        return Value::make_bool(true);
    }

    if (fn == "process") {
        require_count(args, 0);
        if (!server->started) {
            throw ModuleError("RSNetwork.process: call start_server() first");
        }

        // First send the response buffered since the previous call.
        if (server->client_fd >= 0) {
            send_http(server->client_fd, "200 OK", "text/plain; charset=utf-8",
                      server->pending_response);
            ::close(server->client_fd);
            server->client_fd = -1;
        }
        server->has_request = false;
        server->request_read = false;
        server->request_value.clear();
        server->pending_response.clear();

        // Then accept at most one new request.
        sockaddr_in peer{};
        socklen_t peer_length = sizeof(peer);
        int fd = ::accept(server->listen_fd, reinterpret_cast<sockaddr*>(&peer), &peer_length);
        if (fd < 0) {
            return Value::make_bool(false);
        }
        set_nonblocking(fd);

        pollfd descriptor{};
        descriptor.fd = fd;
        descriptor.events = POLLIN;
        if (::poll(&descriptor, 1, 20) <= 0 || (descriptor.revents & POLLIN) == 0) {
            ::close(fd);
            return Value::make_bool(false);
        }

        char buffer[8192];
        ssize_t received = ::recv(fd, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
        if (received <= 0) {
            ::close(fd);
            return Value::make_bool(false);
        }
        buffer[received] = '\0';
        std::string request(buffer);
        auto first_space = request.find(' ');
        auto second_space = first_space == std::string::npos
                                ? std::string::npos
                                : request.find(' ', first_space + 1);
        std::string target = second_space == std::string::npos
                                 ? std::string()
                                 : request.substr(first_space + 1, second_space - first_space - 1);

        if (target.rfind("/call/", 0) == 0) {
            server->has_request = true;
            server->request_read = false;
            server->request_value = query_value(target);
            server->client_fd = fd;
            return Value::make_bool(true);
        }
        if (target == "/" || target == "/index.html") {
            send_http(fd, "200 OK", "text/html; charset=utf-8", server->page_content);
        } else if (target == "/rsclient.js") {
            send_http(fd, "200 OK", "application/javascript; charset=utf-8",
                      server->client_script);
        } else {
            send_http(fd, "404 Not Found", "text/plain; charset=utf-8", "Not Found");
        }
        ::close(fd);
        return Value::make_bool(true);
    }

    if (fn == "request_string_data") {
        require_count(args, 0);
        return Value::make_string(read_request(*server, fn, "string"));
    }

    if (fn == "request_int_data") {
        require_count(args, 0);
        std::string text = read_request(*server, fn, "int");
        long long number = 0;
        if (!parse_int(text, number)) {
            throw ModuleError("RSNetwork.request_int_data: no most recent int data");
        }
        return Value::make_int(number);
    }

    if (fn == "request_float_data") {
        require_count(args, 0);
        std::string text = read_request(*server, fn, "float");
        double number = 0.0;
        if (!parse_float(text, number)) {
            throw ModuleError("RSNetwork.request_float_data: no most recent float data");
        }
        return Value::make_float(number);
    }

    if (fn == "request_boolean_data") {
        require_count(args, 0);
        std::string text = lower(read_request(*server, fn, "boolean"));
        if (text == "true" || text == "1") {
            return Value::make_bool(true);
        }
        if (text == "false" || text == "0") {
            return Value::make_bool(false);
        }
        throw ModuleError("RSNetwork.request_boolean_data: no most recent boolean data");
    }

    if (fn == "get_request_state") {
        require_count(args, 0);
        return Value::make_bool(server->has_request && !server->request_read);
    }

    if (fn == "respond_string_data" || fn == "respond_int_data" ||
        fn == "respond_float_data" || fn == "respond_boolean_data") {
        require_count(args, 1);
        require_awaiting_response(*server, fn);

        Value::Type expected = Value::Type::String;
        if (fn == "respond_int_data") {
            expected = Value::Type::Int;
        } else if (fn == "respond_float_data") {
            expected = Value::Type::Float;
        } else if (fn == "respond_boolean_data") {
            expected = Value::Type::Bool;
        }
        if (args[0].type != expected) {
            throw ModuleSignatureError();
        }

        switch (args[0].type) {
            case Value::Type::String:
                server->pending_response = args[0].string_value;
                break;
            case Value::Type::Int:
                server->pending_response = std::to_string(args[0].int_value);
                break;
            case Value::Type::Float:
                server->pending_response = float_to_string(args[0].float_value);
                break;
            case Value::Type::Bool:
                server->pending_response = args[0].bool_value ? "true" : "false";
                break;
            default:
                throw ModuleSignatureError();
        }
        return Value::make_bool(true);
    }

    throw ModuleError("RSNetwork." + fn + ": unknown module function");
}

Value RsNetworkModule::get_constant(const std::string& name) const {
    if (name == "ROUTEMODE_CRE") {
        return Value::make_string("CRE");
    }
    if (name == "ROUTEMODE_SR") {
        return Value::make_string("SR");
    }
    if (name == "ROUTEMODE_MB") {
        return Value::make_string("MB");
    }
    throw ModuleError("RSNetwork." + name + ": unknown module constant");
}

} // namespace resiris
