#pragma once

#include <functional>

namespace resiris {

// Measurements about the program running right now. Only the host knows them,
// so it installs them instead of passing them into a module's constructor:
// every module has to stay default-constructible.
struct ProgramClock {
    // Seconds since this program started executing, not since boot.
    std::function<double()> seconds;
    // Percent of wall-clock time spent executing, 0..100.
    std::function<float()> cpu_load;
};

// Installs the program clock. RSSystem asks has_program_clock() first, so a
// missing measurement becomes a Resiris error naming the function.
void set_program_clock(ProgramClock clock);

bool has_program_clock();

// Both throw std::runtime_error when no clock is installed.
double program_seconds();

float program_cpu_load();

} // namespace resiris
