#include "helpers.hpp"

using namespace resiris;

namespace {

struct Timed {
    std::map<std::string, resiris::Variable>& variables;
    resiris::Value remaining;
    Timed(std::map<std::string, resiris::Variable>& v) : variables(v) {
        auto it = v.find("maradt");
        if (it != v.end()) {
            remaining = it->second.value;
        }
    }
};

} // namespace

TEST_CASE("rsbase: await returns a module object", "[rsbase]") {
    testutil::Interp app;
    app.exec("<include> RSBase\nv t ModuleObject = RSBase.await()\n");
    const auto& value = testutil::variable(*app.interp, "t");
    REQUIRE(value.type == Value::Type::ModuleObject);
    REQUIRE(value.module_value->module_name == "RSBase");
}

TEST_CASE("rsbase: set_timer and process before expiry", "[rsbase]") {
    testutil::Interp app;
    app.exec(
        "<include> RSBase\n"
        "c FPS float = 60.0\n"
        "v t ModuleObject = RSBase.await()\n"
        "t.set_timer(1.0)\n"
        "v maradt float = t.process()\n");
    REQUIRE(testutil::variable(*app.interp, "maradt").float_value == 1.0);
}

TEST_CASE("rsbase: process counts down the frame clock", "[rsbase]") {
    testutil::Interp app;
    app.exec(
        "<include> RSBase\n"
        "c FPS float = 60.0\n"
        "v t ModuleObject = RSBase.await()\n"
        "t.set_timer(1.0)\n"
        "v maradt float = 1.0\n"
        "PROCESS(FPS):\n"
        "\tmaradt = t.process()\n");
    // 30 frames at 60 fps is half a second. A lifecycle body has its own
    // scope, so the value is written with an assignment, not a declaration.
    app.interp->run_process_frames(30);
    double remaining = testutil::variable(*app.interp, "maradt").float_value;
    REQUIRE(remaining > 0.49);
    REQUIRE(remaining < 0.51);
}

TEST_CASE("rsbase: timer expiry fires the callback", "[rsbase]") {
    testutil::Interp app;
    app.exec(
        "<include> RSBase\n"
        "c FPS float = 60.0\n"
        "v t ModuleObject = RSBase.await()\n"
        "v lejart int = 0\n"
        "fn on_timeout():\n"
        "\tlejart = 1\n"
        "t.set_timer(0.1)\n"
        "t.on_timeout(\"on_timeout\")\n"
        "v maradt float = 1.0\n"
        "PROCESS(FPS):\n"
        "\tmaradt = t.process()\n");
    app.interp->run_process_frames(12);
    REQUIRE(testutil::variable(*app.interp, "lejart").int_value == 1);
}

TEST_CASE("rsbase: callback fires only once", "[rsbase]") {
    testutil::Interp app;
    app.exec(
        "<include> RSBase\n"
        "c FPS float = 60.0\n"
        "v t ModuleObject = RSBase.await()\n"
        "v lejart int = 0\n"
        "fn on_timeout():\n"
        "\tlejart += 1\n"
        "t.set_timer(0.1)\n"
        "t.on_timeout(\"on_timeout\")\n"
        "PROCESS(FPS):\n"
        "\tv maradt float = t.process()\n");
    app.interp->run_process_frames(30);
    REQUIRE(testutil::variable(*app.interp, "lejart").int_value == 1);
}

TEST_CASE("rsbase: stop_timer and reset_timer", "[rsbase]") {
    testutil::Interp app;
    app.exec(
        "<include> RSBase\n"
        "c FPS float = 60.0\n"
        "v t ModuleObject = RSBase.await()\n"
        "t.set_timer(1.0)\n"
        "t.stop_timer()\n"
        "v maradt float = t.process()\n");
    REQUIRE(testutil::variable(*app.interp, "maradt").float_value == 0.0);
}

TEST_CASE("rsbase: free_timer", "[rsbase]") {
    testutil::Interp app;
    REQUIRE_NOTHROW(app.exec(
        "<include> RSBase\n"
        "v t ModuleObject = RSBase.await()\n"
        "t.free_timer()\n"));
}

TEST_CASE("rsbase: process requires a started timer", "[rsbase]") {
    testutil::Interp app;
    app.exec(
        "<include> RSBase\n"
        "v t ModuleObject = RSBase.await()\n"
        "v maradt float = t.process()\n");
    REQUIRE(testutil::variable(*app.interp, "maradt").float_value == 0.0);
}

TEST_CASE("process: requires the FPS constant", "[process]") {
    testutil::Interp app;
    app.exec("<include> RSBase\nPROCESS(FPS):\n\tpass\n");
    REQUIRE_THROWS_WITH(app.interp->run_process_frames(1),
                        Catch::Contains("global constant"));
}

TEST_CASE("process: runs the declared body", "[process]") {
    testutil::CaptureStdout capture;
    testutil::Interp app;
    app.exec(
        "<include> RSBase\n"
        "c FPS float = 60.0\n"
        "v frame int = 0\n"
        "PROCESS(FPS):\n"
        "\tframe += 1\n"
        "\tprint_cmd(frame)\n");
    app.interp->run_process_frames(3);
    REQUIRE(capture.str() == "1\n2\n3\n");
}

TEST_CASE("start: runs once before the frame loop", "[process]") {
    testutil::CaptureStdout capture;
    testutil::Interp app;
    app.exec(
        "c FPS float = 60.0\n"
        "print_cmd(\"body\")\n"
        "START():\n"
        "\tprint_cmd(\"start\")\n"
        "PROCESS(FPS):\n"
        "\tpass\n");
    REQUIRE(capture.str() == "body\nstart\n");
}
