#include "sandhybrid/app.hpp"
#include "sandhybrid/world_save.hpp"

#include <cstdio>
#include <exception>
#include <string_view>

namespace {

void print_usage() {
    std::fprintf(stderr,
        "Usage: sandhybrid [--world-size SIZE] [--save-slot NAME] [--runtime-acceptance-report FILE]\n"
        "World sizes: compact, standard, large\n"
        "Aliases: small=compact, medium=standard\n"
        "Save slots are portable named folders; the default is quick.\n");
}

[[nodiscard]] bool apply_world_size(const std::string_view value,
                                    sandhybrid::ApplicationOptions& options) {
    const auto parsed = sandhybrid::parse_world_size(value);
    if (!parsed.has_value()) {
        std::fprintf(stderr, "[SandHybrid] Invalid world size: %.*s\n",
                     static_cast<int>(value.size()), value.data());
        return false;
    }
    options.world_size = *parsed;
    return true;
}

} // namespace

int main(const int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    std::fprintf(stderr, "[SandHybrid] Entered main.\n");

    sandhybrid::ApplicationOptions options{};
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--help" || argument == "-h") {
            print_usage();
            return 0;
        }
        if (argument == "--world-size") {
            if (index + 1 >= argc ||
                !apply_world_size(std::string_view{argv[++index]}, options)) {
                print_usage();
                return 2;
            }
            continue;
        }
        constexpr std::string_view world_size_prefix{"--world-size="};
        if (argument.starts_with(world_size_prefix)) {
            if (!apply_world_size(argument.substr(world_size_prefix.size()), options)) {
                print_usage();
                return 2;
            }
            continue;
        }
        if (argument == "--save-slot") {
            if (index + 1 >= argc) {
                std::fprintf(stderr, "[SandHybrid] --save-slot requires a name.\n");
                print_usage();
                return 2;
            }
            options.save_slot = sandhybrid::normalize_world_slot(argv[++index]);
            continue;
        }
        constexpr std::string_view save_slot_prefix{"--save-slot="};
        if (argument.starts_with(save_slot_prefix)) {
            options.save_slot = sandhybrid::normalize_world_slot(
                argument.substr(save_slot_prefix.size()));
            continue;
        }
        if (argument == "--runtime-acceptance-report") {
            if (index + 1 >= argc) {
                std::fprintf(stderr, "[SandHybrid] --runtime-acceptance-report requires a path.\n");
                print_usage();
                return 2;
            }
            options.runtime_acceptance_report = argv[++index];
            continue;
        }
        constexpr std::string_view acceptance_prefix{"--runtime-acceptance-report="};
        if (argument.starts_with(acceptance_prefix)) {
            options.runtime_acceptance_report = argument.substr(acceptance_prefix.size());
            if (options.runtime_acceptance_report.empty()) {
                std::fprintf(stderr, "[SandHybrid] --runtime-acceptance-report requires a path.\n");
                return 2;
            }
            continue;
        }
        std::fprintf(stderr, "[SandHybrid] Unknown option: %.*s\n",
                     static_cast<int>(argument.size()), argument.data());
        print_usage();
        return 2;
    }

    try {
        return sandhybrid::run_application(options);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "[SandHybrid] Fatal error: %s\n", error.what());
        return 1;
    } catch (...) {
        std::fprintf(stderr, "[SandHybrid] Fatal error: unknown exception.\n");
        return 1;
    }
}
