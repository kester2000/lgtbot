// Generated from FactoryFunner/src/data.js; see PORTING.md.
#pragma once
#include <vector>
#include <string>
namespace factory_funner {
struct MachinePort { int edge, colors, amount; bool input; };
struct Machine { int id; std::string name; int revenue; std::vector<MachinePort> ports; };
inline const std::vector<Machine> kMachines {
    {1, "Accelerator", 6, {{0, 4, 1, true}, {1, 1, 2, true}, {4, 1, 3, false}}},
    {2, "Bigtimizer", 11, {{4, 2, 1, true}, {3, 1, 3, true}, {1, 16, 1, false}}},
    {3, "Boilnado", 4, {{1, 4, 1, true}, {4, 8, 3, false}}},
    {4, "Boom Tube", 7, {{0, 8, 3, true}, {1, 2, 2, true}, {3, 2, 1, false}}},
    {5, "Boxer X", 7, {{0, 4, 3, true}, {1, 8, 3, true}, {4, 1, 1, false}}},
    {6, "Compressor", 3, {{3, 1, 3, true}, {0, 2, 2, false}}},
    {7, "Cwalichecker", 14, {{5, 8, 1, true}, {0, 4, 3, true}, {1, 2, 1, true}, {3, 16, 1, false}}},
    {8, "Cannervator", 12, {{5, 1, 2, true}, {4, 2, 2, true}, {3, 4, 1, true}, {1, 1, 2, false}}},
    {9, "Crystallizer", 8, {{5, 2, 3, true}, {0, 4, 3, true}, {3, 16, 1, false}}},
    {10, "D-6 Quantizer", 6, {{3, 1, 3, true}, {1, 2, 3, true}, {5, 4, 2, false}}},
    {11, "Das Hulk", 6, {{5, 1, 3, true}, {4, 2, 1, true}, {1, 2, 3, false}}},
    {12, "Dinkip-82", 8, {{4, 4, 1, true}, {3, 2, 2, true}, {0, 8, 2, false}}},
    {13, "Ectotrapper", 7, {{4, 1, 1, true}, {3, 2, 1, true}, {1, 4, 3, false}}},
    {14, "Exploser", 8, {{3, 1, 1, true}, {1, 8, 1, true}, {5, 8, 3, false}}},
    {15, "Focuser", 2, {{3, 2, 2, true}, {5, 4, 3, false}}},
    {16, "Fredomizer", 11, {{5, 4, 2, true}, {4, 8, 1, true}, {3, 1, 1, true}, {1, 2, 3, false}}},
    {17, "Hexopacker", 13, {{5, 1, 1, true}, {1, 8, 1, true}, {3, 16, 1, false}}},
    {18, "Lavafuzzer", 9, {{5, 8, 2, true}, {1, 4, 3, true}, {3, 2, 1, false}}},
    {19, "Magnetizer", 9, {{4, 4, 1, true}, {3, 8, 1, true}, {0, 1, 2, false}}},
    {20, "Maxi Mixer", 5, {{5, 4, 2, true}, {0, 1, 2, true}, {3, 8, 3, false}}},
    {21, "Max-X-Boil", 9, {{0, 2, 3, true}, {1, 1, 1, true}, {3, 8, 1, false}}},
    {22, "Meeplor", 8, {{4, 1, 3, true}, {3, 4, 1, true}, {0, 2, 2, false}}},
    {23, "Meltdown", 7, {{5, 4, 2, true}, {0, 8, 1, true}, {3, 2, 2, false}}},
    {24, "Metamorphizor", 10, {{5, 2, 1, true}, {0, 1, 1, true}, {3, 2, 1, false}}},
    {25, "Moisture-stir", 6, {{5, 8, 3, true}, {0, 4, 2, true}, {3, 4, 2, false}}},
    {26, "Mondrianator", 10, {{4, 4, 2, true}, {3, 8, 2, true}, {1, 16, 1, false}}},
    {27, "Multiplier", 5, {{1, 1, 1, true}, {5, 1, 2, false}}},
    {28, "Nautilus", 6, {{1, 8, 1, true}, {3, 1, 1, false}}},
    {29, "Packmaster", 9, {{0, 2, 3, true}, {1, 8, 2, true}, {3, 16, 1, false}}},
    {30, "Piper", 4, {{0, 1, 3, true}, {1, 8, 2, true}, {3, 2, 3, false}}},
    {31, "Pixellator", 7, {{0, 8, 2, true}, {1, 1, 2, true}, {3, 4, 2, false}}},
    {32, "Rainbowrisor", 6, {{5, 1, 3, true}, {4, 2, 1, true}, {1, 15, 1, false}}},
    {33, "Rebuilder", 8, {{5, 8, 2, true}, {0, 3, 2, true}, {3, 16, 1, false}}},
    {34, "Reduc-Tank", 5, {{3, 2, 3, true}, {1, 8, 1, false}}},
    {35, "Retrofactor", 9, {{5, 8, 2, true}, {0, 2, 2, true}, {3, 4, 1, false}}},
    {36, "Ringmaster", 7, {{0, 2, 2, true}, {1, 8, 1, true}, {4, 4, 3, false}}},
    {37, "Shredder", 13, {{5, 2, 1, true}, {0, 1, 3, true}, {1, 8, 2, true}, {3, 4, 1, false}}},
    {38, "Solidificator", 7, {{3, 8, 3, true}, {1, 2, 1, true}, {5, 1, 3, false}}},
    {39, "Spectrumizer", 9, {{4, 2, 2, true}, {3, 4, 2, true}, {0, 1, 1, false}}},
    {40, "The Judger", 7, {{5, 8, 1, true}, {3, 2, 1, false}}},
    {41, "Time Machine", 7, {{4, 4, 1, true}, {1, 16, 1, false}}},
    {42, "Thinktank", 9, {{5, 8, 3, true}, {1, 4, 2, true}, {3, 8, 1, false}}},
    {43, "Trifurnace", 7, {{0, 8, 1, true}, {1, 2, 3, true}, {3, 8, 2, false}}},
    {44, "Uberlight", 8, {{4, 2, 1, true}, {3, 4, 3, true}, {1, 1, 2, false}}},
    {45, "Umeaker", 7, {{0, 8, 3, true}, {1, 4, 1, true}, {4, 1, 1, false}, {3, 2, 2, false}}},
    {46, "Vinylnator", 8, {{5, 1, 2, true}, {1, 2, 2, true}, {3, 8, 2, false}}},
    {47, "Zapp 3000", 10, {{5, 1, 3, true}, {1, 4, 1, true}, {3, 4, 1, false}}},
    {48, "Supersealer", 12, {{0, 4, 2, true}, {1, 1, 1, true}, {3, 16, 1, false}}}
};
}
