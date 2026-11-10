#include "shunya/matdb/elements_118_catalog.hpp"

#include <sstream>
#include <unordered_set>

namespace shunya::matdb {

namespace {

std::vector<ExtendedElementRecord> build_118_table() {
    using P = StandardPhase;
    using F = ElementFamily;
    return {
        {  1, "H",  "Hydrogen",      1,  1, 's',   1.00800, P::Gas,    F::ReactiveNonmetal,   "1s1",                     {-1, 1}},
        {  2, "He", "Helium",        1, 18, 's',   4.00260, P::Gas,    F::NobleGas,           "1s2",                     {0}},
        {  3, "Li", "Lithium",       2,  1, 's',   6.94000, P::Solid,  F::AlkaliMetal,        "[He] 2s1",                {1}},
        {  4, "Be", "Beryllium",     2,  2, 's',   9.01218, P::Solid,  F::AlkalineEarth,      "[He] 2s2",                {2}},
        {  5, "B",  "Boron",         2, 13, 'p',  10.81000, P::Solid,  F::Metalloid,          "[He] 2s2 2p1",            {3}},
        {  6, "C",  "Carbon",        2, 14, 'p',  12.01100, P::Solid,  F::ReactiveNonmetal,   "[He] 2s2 2p2",            {-4, 2, 4}},
        {  7, "N",  "Nitrogen",      2, 15, 'p',  14.00700, P::Gas,    F::ReactiveNonmetal,   "[He] 2s2 2p3",            {-3, 3, 5}},
        {  8, "O",  "Oxygen",        2, 16, 'p',  15.99900, P::Gas,    F::ReactiveNonmetal,   "[He] 2s2 2p4",            {-2}},
        {  9, "F",  "Fluorine",      2, 17, 'p',  18.99840, P::Gas,    F::ReactiveNonmetal,   "[He] 2s2 2p5",            {-1}},
        { 10, "Ne", "Neon",          2, 18, 'p',  20.17970, P::Gas,    F::NobleGas,           "[He] 2s2 2p6",            {0}},
        { 11, "Na", "Sodium",        3,  1, 's',  22.98977, P::Solid,  F::AlkaliMetal,        "[Ne] 3s1",                {1}},
        { 12, "Mg", "Magnesium",     3,  2, 's',  24.30500, P::Solid,  F::AlkalineEarth,      "[Ne] 3s2",                {2}},
        { 13, "Al", "Aluminum",      3, 13, 'p',  26.98154, P::Solid,  F::PostTransitionMetal,"[Ne] 3s2 3p1",            {3}},
        { 14, "Si", "Silicon",       3, 14, 'p',  28.08500, P::Solid,  F::Metalloid,          "[Ne] 3s2 3p2",            {-4, 4}},
        { 15, "P",  "Phosphorus",    3, 15, 'p',  30.97376, P::Solid,  F::ReactiveNonmetal,   "[Ne] 3s2 3p3",            {-3, 3, 5}},
        { 16, "S",  "Sulfur",        3, 16, 'p',  32.06000, P::Solid,  F::ReactiveNonmetal,   "[Ne] 3s2 3p4",            {-2, 4, 6}},
        { 17, "Cl", "Chlorine",      3, 17, 'p',  35.45000, P::Gas,    F::ReactiveNonmetal,   "[Ne] 3s2 3p5",            {-1, 1, 5, 7}},
        { 18, "Ar", "Argon",         3, 18, 'p',  39.95000, P::Gas,    F::NobleGas,           "[Ne] 3s2 3p6",            {0}},
        { 19, "K",  "Potassium",     4,  1, 's',  39.09830, P::Solid,  F::AlkaliMetal,        "[Ar] 4s1",                {1}},
        { 20, "Ca", "Calcium",       4,  2, 's',  40.07800, P::Solid,  F::AlkalineEarth,      "[Ar] 4s2",                {2}},
        { 21, "Sc", "Scandium",      4,  3, 'd',  44.95591, P::Solid,  F::TransitionMetal,    "[Ar] 3d1 4s2",            {3}},
        { 22, "Ti", "Titanium",      4,  4, 'd',  47.86700, P::Solid,  F::TransitionMetal,    "[Ar] 3d2 4s2",            {2, 3, 4}},
        { 23, "V",  "Vanadium",      4,  5, 'd',  50.94150, P::Solid,  F::TransitionMetal,    "[Ar] 3d3 4s2",            {2, 3, 4, 5}},
        { 24, "Cr", "Chromium",      4,  6, 'd',  51.99610, P::Solid,  F::TransitionMetal,    "[Ar] 3d5 4s1",            {2, 3, 6}},
        { 25, "Mn", "Manganese",     4,  7, 'd',  54.93804, P::Solid,  F::TransitionMetal,    "[Ar] 3d5 4s2",            {2, 4, 7}},
        { 26, "Fe", "Iron",          4,  8, 'd',  55.84500, P::Solid,  F::TransitionMetal,    "[Ar] 3d6 4s2",            {2, 3}},
        { 27, "Co", "Cobalt",        4,  9, 'd',  58.93319, P::Solid,  F::TransitionMetal,    "[Ar] 3d7 4s2",            {2, 3}},
        { 28, "Ni", "Nickel",        4, 10, 'd',  58.69340, P::Solid,  F::TransitionMetal,    "[Ar] 3d8 4s2",            {2}},
        { 29, "Cu", "Copper",        4, 11, 'd',  63.54600, P::Solid,  F::TransitionMetal,    "[Ar] 3d10 4s1",           {1, 2}},
        { 30, "Zn", "Zinc",          4, 12, 'd',  65.38000, P::Solid,  F::TransitionMetal,    "[Ar] 3d10 4s2",           {2}},
        { 31, "Ga", "Gallium",       4, 13, 'p',  69.72300, P::Solid,  F::PostTransitionMetal,"[Ar] 3d10 4s2 4p1",       {3}},
        { 32, "Ge", "Germanium",     4, 14, 'p',  72.63000, P::Solid,  F::Metalloid,          "[Ar] 3d10 4s2 4p2",       {2, 4}},
        { 33, "As", "Arsenic",       4, 15, 'p',  74.92159, P::Solid,  F::Metalloid,          "[Ar] 3d10 4s2 4p3",       {-3, 3, 5}},
        { 34, "Se", "Selenium",      4, 16, 'p',  78.97100, P::Solid,  F::ReactiveNonmetal,   "[Ar] 3d10 4s2 4p4",       {-2, 4, 6}},
        { 35, "Br", "Bromine",       4, 17, 'p',  79.90400, P::Liquid, F::ReactiveNonmetal,   "[Ar] 3d10 4s2 4p5",       {-1, 1, 5}},
        { 36, "Kr", "Krypton",       4, 18, 'p',  83.79800, P::Gas,    F::NobleGas,           "[Ar] 3d10 4s2 4p6",       {0, 2}},
        { 37, "Rb", "Rubidium",      5,  1, 's',  85.46780, P::Solid,  F::AlkaliMetal,        "[Kr] 5s1",                {1}},
        { 38, "Sr", "Strontium",     5,  2, 's',  87.62000, P::Solid,  F::AlkalineEarth,      "[Kr] 5s2",                {2}},
        { 39, "Y",  "Yttrium",       5,  3, 'd',  88.90584, P::Solid,  F::TransitionMetal,    "[Kr] 4d1 5s2",            {3}},
        { 40, "Zr", "Zirconium",     5,  4, 'd',  91.22400, P::Solid,  F::TransitionMetal,    "[Kr] 4d2 5s2",            {4}},
        { 41, "Nb", "Niobium",       5,  5, 'd',  92.90637, P::Solid,  F::TransitionMetal,    "[Kr] 4d4 5s1",            {5}},
        { 42, "Mo", "Molybdenum",    5,  6, 'd',  95.95000, P::Solid,  F::TransitionMetal,    "[Kr] 4d5 5s1",            {4, 6}},
        { 43, "Tc", "Technetium",    5,  7, 'd',  98.00000, P::Solid,  F::TransitionMetal,    "[Kr] 4d5 5s2",            {4, 7}},
        { 44, "Ru", "Ruthenium",     5,  8, 'd', 101.07000, P::Solid,  F::TransitionMetal,    "[Kr] 4d7 5s1",            {3, 4}},
        { 45, "Rh", "Rhodium",       5,  9, 'd', 102.90550, P::Solid,  F::TransitionMetal,    "[Kr] 4d8 5s1",            {3}},
        { 46, "Pd", "Palladium",     5, 10, 'd', 106.42000, P::Solid,  F::TransitionMetal,    "[Kr] 4d10",               {2, 4}},
        { 47, "Ag", "Silver",        5, 11, 'd', 107.86820, P::Solid,  F::TransitionMetal,    "[Kr] 4d10 5s1",           {1}},
        { 48, "Cd", "Cadmium",       5, 12, 'd', 112.41400, P::Solid,  F::TransitionMetal,    "[Kr] 4d10 5s2",           {2}},
        { 49, "In", "Indium",        5, 13, 'p', 114.81800, P::Solid,  F::PostTransitionMetal,"[Kr] 4d10 5s2 5p1",       {3}},
        { 50, "Sn", "Tin",           5, 14, 'p', 118.71000, P::Solid,  F::PostTransitionMetal,"[Kr] 4d10 5s2 5p2",       {2, 4}},
        { 51, "Sb", "Antimony",      5, 15, 'p', 121.76000, P::Solid,  F::Metalloid,          "[Kr] 4d10 5s2 5p3",       {-3, 3, 5}},
        { 52, "Te", "Tellurium",     5, 16, 'p', 127.60000, P::Solid,  F::Metalloid,          "[Kr] 4d10 5s2 5p4",       {-2, 4, 6}},
        { 53, "I",  "Iodine",        5, 17, 'p', 126.90447, P::Solid,  F::ReactiveNonmetal,   "[Kr] 4d10 5s2 5p5",       {-1, 1, 5, 7}},
        { 54, "Xe", "Xenon",         5, 18, 'p', 131.29300, P::Gas,    F::NobleGas,           "[Kr] 4d10 5s2 5p6",       {0, 2, 4, 6}},
        { 55, "Cs", "Cesium",        6,  1, 's', 132.90545, P::Solid,  F::AlkaliMetal,        "[Xe] 6s1",                {1}},
        { 56, "Ba", "Barium",        6,  2, 's', 137.32700, P::Solid,  F::AlkalineEarth,      "[Xe] 6s2",                {2}},
        { 57, "La", "Lanthanum",     6,  3, 'f', 138.90547, P::Solid,  F::Lanthanide,         "[Xe] 5d1 6s2",            {3}},
        { 58, "Ce", "Cerium",        6,  3, 'f', 140.11600, P::Solid,  F::Lanthanide,         "[Xe] 4f1 5d1 6s2",        {3, 4}},
        { 59, "Pr", "Praseodymium",  6,  3, 'f', 140.90766, P::Solid,  F::Lanthanide,         "[Xe] 4f3 6s2",            {3}},
        { 60, "Nd", "Neodymium",     6,  3, 'f', 144.24200, P::Solid,  F::Lanthanide,         "[Xe] 4f4 6s2",            {3}},
        { 61, "Pm", "Promethium",    6,  3, 'f', 145.00000, P::Solid,  F::Lanthanide,         "[Xe] 4f5 6s2",            {3}},
        { 62, "Sm", "Samarium",      6,  3, 'f', 150.36000, P::Solid,  F::Lanthanide,         "[Xe] 4f6 6s2",            {2, 3}},
        { 63, "Eu", "Europium",      6,  3, 'f', 151.96400, P::Solid,  F::Lanthanide,         "[Xe] 4f7 6s2",            {2, 3}},
        { 64, "Gd", "Gadolinium",    6,  3, 'f', 157.25000, P::Solid,  F::Lanthanide,         "[Xe] 4f7 5d1 6s2",        {3}},
        { 65, "Tb", "Terbium",       6,  3, 'f', 158.92535, P::Solid,  F::Lanthanide,         "[Xe] 4f9 6s2",            {3}},
        { 66, "Dy", "Dysprosium",    6,  3, 'f', 162.50000, P::Solid,  F::Lanthanide,         "[Xe] 4f10 6s2",           {3}},
        { 67, "Ho", "Holmium",       6,  3, 'f', 164.93033, P::Solid,  F::Lanthanide,         "[Xe] 4f11 6s2",           {3}},
        { 68, "Er", "Erbium",        6,  3, 'f', 167.25900, P::Solid,  F::Lanthanide,         "[Xe] 4f12 6s2",           {3}},
        { 69, "Tm", "Thulium",       6,  3, 'f', 168.93422, P::Solid,  F::Lanthanide,         "[Xe] 4f13 6s2",           {3}},
        { 70, "Yb", "Ytterbium",     6,  3, 'f', 173.04500, P::Solid,  F::Lanthanide,         "[Xe] 4f14 6s2",           {2, 3}},
        { 71, "Lu", "Lutetium",      6,  3, 'd', 174.96680, P::Solid,  F::Lanthanide,         "[Xe] 4f14 5d1 6s2",       {3}},
        { 72, "Hf", "Hafnium",       6,  4, 'd', 178.48600, P::Solid,  F::TransitionMetal,    "[Xe] 4f14 5d2 6s2",       {4}},
        { 73, "Ta", "Tantalum",      6,  5, 'd', 180.94788, P::Solid,  F::TransitionMetal,    "[Xe] 4f14 5d3 6s2",       {5}},
        { 74, "W",  "Tungsten",      6,  6, 'd', 183.84000, P::Solid,  F::TransitionMetal,    "[Xe] 4f14 5d4 6s2",       {4, 6}},
        { 75, "Re", "Rhenium",       6,  7, 'd', 186.20700, P::Solid,  F::TransitionMetal,    "[Xe] 4f14 5d5 6s2",       {4, 7}},
        { 76, "Os", "Osmium",        6,  8, 'd', 190.23000, P::Solid,  F::TransitionMetal,    "[Xe] 4f14 5d6 6s2",       {4, 8}},
        { 77, "Ir", "Iridium",       6,  9, 'd', 192.21700, P::Solid,  F::TransitionMetal,    "[Xe] 4f14 5d7 6s2",       {3, 4}},
        { 78, "Pt", "Platinum",      6, 10, 'd', 195.08400, P::Solid,  F::TransitionMetal,    "[Xe] 4f14 5d9 6s1",       {2, 4}},
        { 79, "Au", "Gold",          6, 11, 'd', 196.96657, P::Solid,  F::TransitionMetal,    "[Xe] 4f14 5d10 6s1",      {1, 3}},
        { 80, "Hg", "Mercury",       6, 12, 'd', 200.59200, P::Liquid, F::TransitionMetal,    "[Xe] 4f14 5d10 6s2",      {1, 2}},
        { 81, "Tl", "Thallium",      6, 13, 'p', 204.38000, P::Solid,  F::PostTransitionMetal,"[Xe] 4f14 5d10 6s2 6p1",  {1, 3}},
        { 82, "Pb", "Lead",          6, 14, 'p', 207.20000, P::Solid,  F::PostTransitionMetal,"[Xe] 4f14 5d10 6s2 6p2",  {2, 4}},
        { 83, "Bi", "Bismuth",       6, 15, 'p', 208.98040, P::Solid,  F::PostTransitionMetal,"[Xe] 4f14 5d10 6s2 6p3",  {3, 5}},
        { 84, "Po", "Polonium",      6, 16, 'p', 209.00000, P::Solid,  F::PostTransitionMetal,"[Xe] 4f14 5d10 6s2 6p4",  {2, 4}},
        { 85, "At", "Astatine",      6, 17, 'p', 210.00000, P::Solid,  F::Metalloid,          "[Xe] 4f14 5d10 6s2 6p5",  {-1, 1}},
        { 86, "Rn", "Radon",         6, 18, 'p', 222.00000, P::Gas,    F::NobleGas,           "[Xe] 4f14 5d10 6s2 6p6",  {0, 2}},
        { 87, "Fr", "Francium",      7,  1, 's', 223.00000, P::Solid,  F::AlkaliMetal,        "[Rn] 7s1",                {1}},
        { 88, "Ra", "Radium",        7,  2, 's', 226.00000, P::Solid,  F::AlkalineEarth,      "[Rn] 7s2",                {2}},
        { 89, "Ac", "Actinium",      7,  3, 'f', 227.00000, P::Solid,  F::Actinide,           "[Rn] 6d1 7s2",            {3}},
        { 90, "Th", "Thorium",       7,  3, 'f', 232.03770, P::Solid,  F::Actinide,           "[Rn] 6d2 7s2",            {4}},
        { 91, "Pa", "Protactinium",  7,  3, 'f', 231.03588, P::Solid,  F::Actinide,           "[Rn] 5f2 6d1 7s2",        {5}},
        { 92, "U",  "Uranium",       7,  3, 'f', 238.02891, P::Solid,  F::Actinide,           "[Rn] 5f3 6d1 7s2",        {4, 6}},
        { 93, "Np", "Neptunium",     7,  3, 'f', 237.00000, P::Solid,  F::Actinide,           "[Rn] 5f4 6d1 7s2",        {5}},
        { 94, "Pu", "Plutonium",     7,  3, 'f', 244.00000, P::Solid,  F::Actinide,           "[Rn] 5f6 7s2",            {4}},
        { 95, "Am", "Americium",     7,  3, 'f', 243.00000, P::Solid,  F::Actinide,           "[Rn] 5f7 7s2",            {3}},
        { 96, "Cm", "Curium",        7,  3, 'f', 247.00000, P::Solid,  F::Actinide,           "[Rn] 5f7 6d1 7s2",        {3}},
        { 97, "Bk", "Berkelium",     7,  3, 'f', 247.00000, P::Solid,  F::Actinide,           "[Rn] 5f9 7s2",            {3, 4}},
        { 98, "Cf", "Californium",   7,  3, 'f', 251.00000, P::Solid,  F::Actinide,           "[Rn] 5f10 7s2",           {3}},
        { 99, "Es", "Einsteinium",   7,  3, 'f', 252.00000, P::Solid,  F::Actinide,           "[Rn] 5f11 7s2",           {3}},
        {100, "Fm", "Fermium",       7,  3, 'f', 257.00000, P::Solid,  F::Actinide,           "[Rn] 5f12 7s2",           {3}},
        {101, "Md", "Mendelevium",   7,  3, 'f', 258.00000, P::Solid,  F::Actinide,           "[Rn] 5f13 7s2",           {2, 3}},
        {102, "No", "Nobelium",      7,  3, 'f', 259.00000, P::Solid,  F::Actinide,           "[Rn] 5f14 7s2",           {2}},
        {103, "Lr", "Lawrencium",    7,  3, 'd', 266.00000, P::Solid,  F::Actinide,           "[Rn] 5f14 7s2 7p1",       {3}},
        {104, "Rf", "Rutherfordium", 7,  4, 'd', 267.00000, P::Unknown,F::TransitionMetal,    "[Rn] 5f14 6d2 7s2",       {4}},
        {105, "Db", "Dubnium",       7,  5, 'd', 268.00000, P::Unknown,F::TransitionMetal,    "[Rn] 5f14 6d3 7s2",       {5}},
        {106, "Sg", "Seaborgium",    7,  6, 'd', 269.00000, P::Unknown,F::TransitionMetal,    "[Rn] 5f14 6d4 7s2",       {6}},
        {107, "Bh", "Bohrium",       7,  7, 'd', 270.00000, P::Unknown,F::TransitionMetal,    "[Rn] 5f14 6d5 7s2",       {7}},
        {108, "Hs", "Hassium",       7,  8, 'd', 277.00000, P::Unknown,F::TransitionMetal,    "[Rn] 5f14 6d6 7s2",       {8}},
        {109, "Mt", "Meitnerium",    7,  9, 'd', 278.00000, P::Unknown,F::TransitionMetal,    "[Rn] 5f14 6d7 7s2",       {3}},
        {110, "Ds", "Darmstadtium",  7, 10, 'd', 281.00000, P::Unknown,F::TransitionMetal,    "[Rn] 5f14 6d8 7s2",       {2}},
        {111, "Rg", "Roentgenium",   7, 11, 'd', 282.00000, P::Unknown,F::TransitionMetal,    "[Rn] 5f14 6d9 7s2",       {3}},
        {112, "Cn", "Copernicium",   7, 12, 'd', 285.00000, P::Unknown,F::TransitionMetal,    "[Rn] 5f14 6d10 7s2",      {2}},
        {113, "Nh", "Nihonium",      7, 13, 'p', 286.00000, P::Unknown,F::PostTransitionMetal,"[Rn] 5f14 6d10 7s2 7p1",  {1}},
        {114, "Fl", "Flerovium",     7, 14, 'p', 289.00000, P::Unknown,F::PostTransitionMetal,"[Rn] 5f14 6d10 7s2 7p2",  {2}},
        {115, "Mc", "Moscovium",     7, 15, 'p', 290.00000, P::Unknown,F::PostTransitionMetal,"[Rn] 5f14 6d10 7s2 7p3",  {1, 3}},
        {116, "Lv", "Livermorium",   7, 16, 'p', 293.00000, P::Unknown,F::PostTransitionMetal,"[Rn] 5f14 6d10 7s2 7p4",  {2}},
        {117, "Ts", "Tennessine",    7, 17, 'p', 294.00000, P::Unknown,F::Metalloid,          "[Rn] 5f14 6d10 7s2 7p5",  {-1, 1}},
        {118, "Og", "Oganesson",     7, 18, 'p', 294.00000, P::Unknown,F::NobleGas,           "[Rn] 5f14 6d10 7s2 7p6",  {0, 2}}
    };
}

} // namespace

const std::vector<ExtendedElementRecord>& Elements118Catalog::all_elements() {
    static const std::vector<ExtendedElementRecord> table = build_118_table();
    return table;
}

const ExtendedElementRecord* Elements118Catalog::find_by_z(int atomic_number) noexcept {
    if (atomic_number < 1 || atomic_number > 118) return nullptr;
    const auto& tbl = all_elements();
    const auto& item = tbl[static_cast<size_t>(atomic_number - 1)];
    return (item.atomic_number == atomic_number) ? &item : nullptr;
}

const ExtendedElementRecord* Elements118Catalog::find_by_symbol(std::string_view symbol) noexcept {
    for (const auto& item : all_elements()) {
        if (item.symbol == symbol) return &item;
    }
    return nullptr;
}

void Elements118Catalog::populate_database(core::domain::PeriodicTableDatabase& db) {
    for (const auto& rec : all_elements()) {
        core::domain::PeriodicElement elem;
        elem.atomic_number = rec.atomic_number;
        elem.symbol = rec.symbol;
        elem.name = rec.name;
        elem.period = rec.period;
        elem.group = rec.group;
        elem.block = rec.block;
        elem.standard_atomic_weight = rec.atomic_weight_amu;
        elem.electron_configuration = rec.electron_configuration;
        (void)db.add_element(std::move(elem));
    }
}

std::string Elements118Catalog::to_json() {
    std::ostringstream out;
    out << "[\n";
    const auto& tbl = all_elements();
    for (size_t i = 0; i < tbl.size(); ++i) {
        const auto& e = tbl[i];
        out << "  {\"atomic_number\":" << e.atomic_number
            << ",\"symbol\":\"" << e.symbol << "\""
            << ",\"name\":\"" << e.name << "\""
            << ",\"period\":" << e.period
            << ",\"group\":" << e.group
            << ",\"block\":\"" << e.block << "\""
            << ",\"atomic_weight_amu\":" << e.atomic_weight_amu
            << ",\"electron_configuration\":\"" << e.electron_configuration << "\"}";
        if (i + 1 < tbl.size()) out << ",";
        out << "\n";
    }
    out << "]\n";
    return out.str();
}

std::string Elements118Catalog::generate_sqlite_elements_v2_ddl() {
    return "CREATE TABLE IF NOT EXISTS elements_v2 (\n"
           "  atomic_number INTEGER PRIMARY KEY CHECK (atomic_number BETWEEN 1 AND 118),\n"
           "  symbol TEXT NOT NULL UNIQUE,\n"
           "  name TEXT NOT NULL,\n"
           "  period INTEGER NOT NULL,\n"
           "  group_num INTEGER NOT NULL,\n"
           "  block CHAR(1) NOT NULL,\n"
           "  atomic_weight_amu REAL NOT NULL,\n"
           "  standard_state TEXT NOT NULL,\n"
           "  electron_configuration TEXT NOT NULL\n"
           ");\n";
}

core::base::ValidationReport Elements118Catalog::audit_completeness() {
    core::base::ValidationReport report;
    const auto& tbl = all_elements();
    if (tbl.size() != 118) {
        report.add_error("elements_v2", "Expected 118 IUPAC elements, found " + std::to_string(tbl.size()));
        return report;
    }
    std::unordered_set<std::string> seen_syms;
    for (int z = 1; z <= 118; ++z) {
        const auto* rec = find_by_z(z);
        if (!rec) {
            report.add_error("Z=" + std::to_string(z), "Missing atomic number Z=" + std::to_string(z));
            continue;
        }
        if (rec->symbol.empty() || rec->name.empty() || rec->atomic_weight_amu <= 0.0) {
            report.add_error("Z=" + std::to_string(z), "Incomplete data at Z=" + std::to_string(z));
        }
        if (!seen_syms.insert(rec->symbol).second) {
            report.add_error(rec->symbol, "Duplicate symbol: " + rec->symbol);
        }
        if (rec->electron_configuration.empty()) {
            report.add_error("Z=" + std::to_string(z), "Missing electron configuration at Z=" + std::to_string(z));
        }
    }
    return report;
}

} // namespace shunya::matdb
