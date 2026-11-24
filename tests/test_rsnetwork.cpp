#include "helpers.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <string>

using namespace resiris;

namespace {

int free_port() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(fd >= 0);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = 0;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    REQUIRE(::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);

    socklen_t length = sizeof(address);
    REQUIRE(::getsockname(fd, reinterpret_cast<sockaddr*>(&address), &length) == 0);
    int port = ntohs(address.sin_port);
    ::close(fd);
    return port;
}

struct Client {
    int fd = -1;

    explicit Client(int port) {
        fd = ::socket(AF_INET, SOCK_STREAM, 0);
        REQUIRE(fd >= 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(static_cast<uint16_t>(port));
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        REQUIRE(::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    }

    ~Client() {
        if (fd >= 0) {
            ::close(fd);
        }
    }

    void request(const std::string& target) {
        std::string message = "GET " + target + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
        REQUIRE(::send(fd, message.data(), message.size(), MSG_NOSIGNAL) ==
                static_cast<ssize_t>(message.size()));
    }

    std::string response() {
        char buffer[4096];
        ssize_t received = ::recv(fd, buffer, sizeof(buffer) - 1, 0);
        REQUIRE(received > 0);
        buffer[received] = '\0';
        return std::string(buffer);
    }
};

std::string server_program(int port, const std::string& globals,
                           const std::string& process_body) {
    return "<include> RSNetwork\n"
           "c FPS float = 100.0\n"
           "v page ModuleObject = RSNetwork.html_server_handler()\n"
           "v data string = \"\"\n" + globals +
           "START():\n"
           "\tpage.create_server(" + std::to_string(port) + ")\n"
           "\tpage.set_page_content(\"<h1>hello</h1>\")\n"
           "\tpage.set_client_script(\"//js\")\n"
           "\tpage.start_server()\n"
           "PROCESS(FPS):\n" + process_body;
}

} // namespace

TEST_CASE("rsnetwork: route mode constants are strings", "[rsnetwork]") {
    testutil::Interp app;
    app.exec("<include> RSNetwork\n"
             "v cre string = RSNetwork[ROUTEMODE_CRE]\n"
             "v sr string = RSNetwork[ROUTEMODE_SR]\n"
             "v mb string = RSNetwork[ROUTEMODE_MB]\n");
    REQUIRE(testutil::variable(*app.interp, "cre").string_value == "CRE");
    REQUIRE(testutil::variable(*app.interp, "sr").string_value == "SR");
    REQUIRE(testutil::variable(*app.interp, "mb").string_value == "MB");
}

TEST_CASE("rsnetwork: Wi-Fi is unavailable in the CLI", "[rsnetwork]") {
    testutil::Interp app;
    REQUIRE_THROWS_WITH(app.exec("<include> RSNetwork\nv wifi ModuleObject = RSNetwork.wifi()\n"),
                        Catch::Contains("RSNetwork.wifi: Wi-Fi is ESP32 only"));

    testutil::Interp data_app;
    REQUIRE_THROWS_WITH(
        data_app.exec("<include> RSNetwork\n"
                      "v data ModuleObject = RSNetwork.create_wifi_data_process_handler()\n"),
        Catch::Contains("RSNetwork.create_wifi_data_process_handler: Wi-Fi is ESP32 only"));
}

TEST_CASE("rsnetwork: html_server_handler returns a module object", "[rsnetwork]") {
    testutil::Interp app;
    app.exec("<include> RSNetwork\nv page ModuleObject = RSNetwork.html_server_handler()\n");
    const auto& page = testutil::variable(*app.interp, "page");
    REQUIRE(page.type == Value::Type::ModuleObject);
    REQUIRE(page.module_value->module_name == "RSNetwork");
}

TEST_CASE("rsnetwork: start_server requires create_server", "[rsnetwork]") {
    testutil::Interp app;
    REQUIRE_THROWS_WITH(
        app.exec("<include> RSNetwork\n"
                 "v page ModuleObject = RSNetwork.html_server_handler()\n"
                 "page.start_server()\n"),
        Catch::Contains("RSNetwork.start_server: call create_server(port_number) first"));
}

TEST_CASE("rsnetwork: create_server validates the port and duplicate calls", "[rsnetwork]") {
    int port = free_port();
    testutil::Interp invalid;
    REQUIRE_THROWS_WITH(
        invalid.exec("<include> RSNetwork\n"
                     "v page ModuleObject = RSNetwork.html_server_handler()\n"
                     "page.create_server(0)\n"),
        Catch::Contains("port must be between 1 and 65535"));

    testutil::Interp duplicate;
    std::string source = "<include> RSNetwork\n"
                         "v page ModuleObject = RSNetwork.html_server_handler()\n"
                         "page.create_server(" + std::to_string(port) + ")\n"
                         "page.create_server(" + std::to_string(port) + ")\n";
    REQUIRE_THROWS_WITH(duplicate.exec(source), Catch::Contains("server already created"));
}

TEST_CASE("rsnetwork: static HTML and client script are served", "[rsnetwork]") {
    int port = free_port();
    testutil::Interp app;
    app.exec(server_program(port, "",
                            "\tpage.process()\n"));

    Client page_client(port);
    page_client.request("/");
    app.interp->run_process_frames(1);
    std::string page = page_client.response();
    REQUIRE(page.rfind("HTTP/1.1 200 OK", 0) == 0);
    REQUIRE(page.find("<h1>hello</h1>") != std::string::npos);

    Client script_client(port);
    script_client.request("/rsclient.js");
    app.interp->run_process_frames(1);
    std::string script = script_client.response();
    REQUIRE(script.find("//js") != std::string::npos);

    Client missing_client(port);
    missing_client.request("/missing");
    app.interp->run_process_frames(1);
    REQUIRE(missing_client.response().rfind("HTTP/1.1 404 Not Found", 0) == 0);
}

TEST_CASE("rsnetwork: request state is true until data is read", "[rsnetwork]") {
    int port = free_port();
    testutil::Interp app;
    app.exec(server_program(port, "v state bool = false\n",
                            "\tpage.process()\n"
                            "\tstate = page.get_request_state()\n"));

    Client client(port);
    client.request("/call/add?value=42");
    app.interp->run_process_frames(1);
    REQUIRE(testutil::variable(*app.interp, "state").bool_value);

    // No new request: the first call sent the response and cleared the state.
    app.interp->run_process_frames(1);
    REQUIRE_FALSE(testutil::variable(*app.interp, "state").bool_value);
}

TEST_CASE("rsnetwork: string request and buffered response", "[rsnetwork]") {
    int port = free_port();
    testutil::Interp app;
    app.exec(server_program(port, "",
                            "\tpage.process()\n"
                            "\tif page.get_request_state():\n"
                            "\t\tdata = page.request_string_data()\n"
                            "\t\tpage.respond_string_data(\"answer:\" + data)\n"));

    Client client(port);
    client.request("/call/add?value=hello%20world");
    app.interp->run_process_frames(2);

    REQUIRE(testutil::variable(*app.interp, "data").string_value == "hello world");
    std::string response = client.response();
    REQUIRE(response.find("answer:hello world") != std::string::npos);
}

TEST_CASE("rsnetwork: typed request getters", "[rsnetwork]") {
    auto run_typed = [](const std::string& type, const std::string& getter,
                        const std::string& value) {
        int port = free_port();
        auto app = std::make_unique<testutil::Interp>();
        app->exec(server_program(port, "v result " + type + "\n",
                                 "\tpage.process()\n"
                                 "\tif page.get_request_state():\n"
                                 "\t\tresult = " + getter + "\n"));

        Client client(port);
        client.request("/call/add?value=" + value);
        app->interp->run_process_frames(1);
        return testutil::variable(*app->interp, "result");
    };

    REQUIRE(run_typed("int = 0", "page.request_int_data()", "7").int_value == 7);
    REQUIRE(run_typed("float = 0.0", "page.request_float_data()", "1.5").float_value == 1.5);
    REQUIRE(run_typed("bool = false", "page.request_boolean_data()", "true").bool_value);
}

TEST_CASE("rsnetwork: getters and responders require a current request", "[rsnetwork]") {
    testutil::Interp app;
    REQUIRE_THROWS_WITH(
        app.exec("<include> RSNetwork\n"
                 "v page ModuleObject = RSNetwork.html_server_handler()\n"
                 "v text string = page.request_string_data()\n"),
        Catch::Contains("RSNetwork.request_string_data: no most recent string data"));

    testutil::Interp responder;
    REQUIRE_THROWS_WITH(
        responder.exec("<include> RSNetwork\n"
                       "v page ModuleObject = RSNetwork.html_server_handler()\n"
                       "page.respond_int_data(5)\n"),
        Catch::Contains("RSNetwork.respond_int_data: no request awaiting a response"));
}

TEST_CASE("rsnetwork: respond values are formatted for the browser", "[rsnetwork]") {
    int port = free_port();
    testutil::Interp app;
    app.exec(server_program(port, "",
                            "\tpage.process()\n"
                            "\tif page.get_request_state():\n"
                            "\t\tpage.respond_boolean_data(true)\n"));

    Client client(port);
    client.request("/call/add?value=1");
    app.interp->run_process_frames(2);
    REQUIRE(client.response().find("\r\n\r\ntrue") != std::string::npos);
}
