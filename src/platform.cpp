#include "resiris/platform.hpp"

#include <stdexcept>
#include <utility>

namespace resiris {

namespace {

ProgramClock program_clock;

} // namespace

void set_program_clock(ProgramClock clock) {
    program_clock = std::move(clock);
}

bool has_program_clock() {
    return static_cast<bool>(program_clock.seconds) &&
           static_cast<bool>(program_clock.cpu_load);
}

double program_seconds() {
    if (!program_clock.seconds) {
        throw std::runtime_error("resiris: no host installed a program clock");
    }
    return program_clock.seconds();
}

float program_cpu_load() {
    if (!program_clock.cpu_load) {
        throw std::runtime_error("resiris: no host installed a load measurement");
    }
    return program_clock.cpu_load();
}

} // namespace resiris
