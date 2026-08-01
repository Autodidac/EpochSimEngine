#include "sandhybrid/app.hpp"

#include <cstdio>
#include <exception>
#include <iostream>

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    std::fprintf(stderr, "[SandHybrid] Entered main.\n");

    try {
        return sandhybrid::run_application();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "[SandHybrid] Fatal error: %s\n", error.what());
        return 1;
    } catch (...) {
        std::fprintf(stderr, "[SandHybrid] Fatal error: unknown exception.\n");
        return 1;
    }
}
