#include "helpers.hpp"

#include <string>

using namespace resiris;

namespace {

struct RestoreClock {
    ~RestoreClock() {
        resiris::set_program_clock({
            [] { return 1.25; },
            [] { return 12.5f; },
        });
    }
};

} // namespace

TEST_CASE("rssystem: portable facts", "[rssystem]") {
    testutil::Interp app;
    app.exec("<include> RSSystem\n"
             "v cores int = RSSystem.cpu_cores()\n"
             "v load float = RSSystem.cpu_load()\n"
             "v uptime float = RSSystem.uptime()\n"
             "v names string = RSSystem.modules()\n");

    REQUIRE(testutil::variable(*app.interp, "cores").int_value > 0);
    REQUIRE(testutil::variable(*app.interp, "load").float_value == 12.5f);
    REQUIRE(testutil::variable(*app.interp, "uptime").float_value == 1.25);

    const std::string names = testutil::variable(*app.interp, "names").string_value;
    REQUIRE(names.find("RSSystem") != std::string::npos);
    REQUIRE(names.find("RSMath") != std::string::npos);
    REQUIRE(names.find("RSBase") != std::string::npos);
    REQUIRE(names.find("RSConvert") != std::string::npos);
    REQUIRE(names.find("RSSignal") != std::string::npos);
    REQUIRE(names.find("RSNetwork") != std::string::npos);
}

TEST_CASE("rssystem: report omits hardware rows on PC", "[rssystem]") {
    testutil::Interp app;
    app.exec("<include> RSSystem\nv text string = RSSystem.report()\n");
    const std::string text = testutil::variable(*app.interp, "text").string_value;

    REQUIRE(text.find("core") != std::string::npos);
    REQUIRE(text.find("12.5% load") != std::string::npos);
    REQUIRE(text.find("1.25 s") != std::string::npos);
    REQUIRE(text.find("modules") != std::string::npos);
    REQUIRE(text.find("chip") == std::string::npos);
    REQUIRE(text.find("psram") == std::string::npos);
    REQUIRE(text.find("flash") == std::string::npos);
    REQUIRE(text.find("memory") == std::string::npos);
}

TEST_CASE("rssystem: hardware functions refuse on PC", "[rssystem]") {
    const char* functions[] = {"chip_model", "chip_revision", "cpu_speed",
                               "memory_total", "memory_free", "memory_used",
                               "memory_min_free", "psram_size",
                               "flash_total", "flash_used"};
    for (const char* fn : functions) {
        testutil::Interp app;
        std::string source = std::string("<include> RSSystem\nRSSystem.") + fn + "()\n";
        REQUIRE_THROWS_WITH(app.exec(source),
                            Catch::Contains("reads the ESP32's own hardware"));
    }
}

TEST_CASE("rssystem: signatures, constants and metadata", "[rssystem]") {
    testutil::Interp arguments;
    REQUIRE_THROWS_WITH(arguments.exec("<include> RSSystem\nRSSystem.cpu_cores(1)\n"),
                        Catch::Contains("invalid argument count"));

    testutil::Interp unknown_constant;
    REQUIRE_THROWS_WITH(
        unknown_constant.exec("<include> RSSystem\nv x string = RSSystem[NOPE]\n"),
        Catch::Contains("unknown module constant"));

    testutil::Interp metadata;
    metadata.exec("<include> RSSystem\nv name string = RSSystem[NAME]\n");
    REQUIRE(testutil::variable(*metadata.interp, "name").string_value == "RSSystem");
}

TEST_CASE("rssystem: missing program clock is an error", "[rssystem]") {
    RestoreClock restore;
    testutil::Interp app;
    testutil::Interp load_app;
    testutil::Interp report_app;
    resiris::set_program_clock({});

    REQUIRE_THROWS_WITH(
        app.exec("<include> RSSystem\nv x float = RSSystem.uptime()\n"),
        Catch::Contains("RSSystem.uptime: no host installed a program clock"));

    REQUIRE_THROWS_WITH(
        load_app.exec("<include> RSSystem\nv x float = RSSystem.cpu_load()\n"),
        Catch::Contains("RSSystem.cpu_load: no host installed a program clock"));

    REQUIRE_THROWS_WITH(
        report_app.exec("<include> RSSystem\nv x string = RSSystem.report()\n"),
        Catch::Contains("RSSystem.report: no host installed a program clock"));
}
