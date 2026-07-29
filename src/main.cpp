#include "epoch/sand/app.hpp"

#include <cstdio>
#include <exception>
#include <iostream>

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    std::fprintf(stderr, "[EpochSand] Entered main.\n");

    try {
        return epoch::sand::run_application();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "[EpochSand] Fatal error: %s\n", error.what());
        return 1;
    } catch (...) {
        std::fprintf(stderr, "[EpochSand] Fatal error: unknown exception.\n");
        return 1;
    }
}
