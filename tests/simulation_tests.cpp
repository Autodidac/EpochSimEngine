#include "epochwater/simulation.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{
    using fastfreddy::testbed::Cell;
    using fastfreddy::testbed::Material;
    using fastfreddy::testbed::Simulation;

    void require(const bool condition, const std::string_view message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    void test_half_and_full_mass()
    {
        Simulation simulation{8, 8};
        simulation.clear();
        simulation.add_water(1, 1, Simulation::half_water);
        simulation.add_water(1, 1, Simulation::half_water);
        require(simulation.cell(1, 1).water_units == Simulation::full_water,
                "two half-water units must produce one full cell");
        require(simulation.total_water_units() == 2U,
                "water mass must be represented in half-units");
    }

    void test_half_moves_twice_as_fast()
    {
        Simulation simulation{8, 8};
        simulation.clear();
        simulation.set_cell(2, 0, Cell{Material::water, Simulation::half_water, 0, true});
        simulation.set_cell(5, 0, Cell{Material::water, Simulation::full_water, 0, true});
        simulation.step();
        require(simulation.cell(2, 2).is_water() &&
                    simulation.cell(2, 2).water_units == Simulation::half_water,
                "half water must receive two falling movement substeps");
        require(simulation.cell(5, 1).is_water() &&
                    simulation.cell(5, 1).water_units == Simulation::full_water,
                "full water must receive one falling movement substep");
    }

    void test_ledge_threshold()
    {
        Simulation simulation{10, 8};
        simulation.clear();
        simulation.set_cell(3, 3, Cell{Material::stone, 0, 0, false});
        simulation.set_cell(4, 2, Cell{Material::water, Simulation::full_water, 1, false});

        simulation.step();
        require(simulation.cell(4, 2).is_water(), "a lone full edge cell must hang");
        require(simulation.cell(4, 3).is_empty(),
                "a lone full edge cell must not begin a waterfall");

        simulation.set_cell(3, 2, Cell{Material::water, Simulation::half_water, 1, false});
        simulation.step();
        require(simulation.cell(4, 3).is_water() &&
                    simulation.cell(4, 3).water_units == Simulation::full_water,
                "a full edge cell followed by a half cell must release");
        require(simulation.total_water_units() == 3U,
                "ledge release must conserve all three half-units");
    }

    void test_vertical_merge()
    {
        Simulation simulation{6, 6};
        simulation.clear();
        simulation.set_cell(2, 1, Cell{Material::water, Simulation::half_water, 0, true});
        simulation.set_cell(2, 2, Cell{Material::water, Simulation::half_water, 0, false});
        simulation.set_cell(2, 3, Cell{Material::stone, 0, 0, false});
        simulation.step();
        require(simulation.cell(2, 2).is_water() &&
                    simulation.cell(2, 2).water_units == Simulation::full_water,
                "one half falling onto another half must merge into full water");
    }

    void test_stone_alignment()
    {
        Simulation simulation{20, 20};
        simulation.clear();
        simulation.paint_stone_block(11, 14);
        for (std::uint32_t y = 8; y < 16; ++y)
        {
            for (std::uint32_t x = 8; x < 16; ++x)
                require(simulation.cell(x, y).is_stone(),
                        "stone brush must fill the aligned 8x8 block");
        }
        require(simulation.cell(7, 8).is_empty(),
                "stone painting must not cross its aligned block boundary");
    }

    void test_long_run_conservation()
    {
        Simulation simulation{64, 48};
        const std::uint64_t initial = simulation.total_water_units();
        for (int i = 0; i < 240; ++i)
            simulation.step();
        require(simulation.total_water_units() == initial,
                "simulation must conserve water over long runs");
    }
}

int main()
{
    test_half_and_full_mass();
    test_half_moves_twice_as_fast();
    test_ledge_threshold();
    test_vertical_merge();
    test_stone_alignment();
    test_long_run_conservation();
    std::cout << "All simulation contracts passed.\n";
    return EXIT_SUCCESS;
}
