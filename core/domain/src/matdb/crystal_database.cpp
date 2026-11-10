#include "shunya/matdb/crystal_database.hpp"

#include "shunya/matdb/elements_118_catalog.hpp"

#include <cmath>
#include <sstream>

namespace shunya::matdb {

namespace {

inline constexpr double kPi = 3.14159265358979323846;
inline constexpr double kAvogadroMolInv = 6.02214076e23;

std::vector<BasisSite> standard_basis(int z, StructurePrototype proto) {
    switch (proto) {
    case StructurePrototype::SC:
        return {{z, 0.0, 0.0, 0.0}};
    case StructurePrototype::BCC:
        return {{z, 0.0, 0.0, 0.0}, {z, 0.5, 0.5, 0.5}};
    case StructurePrototype::FCC:
        return {{z, 0.0, 0.0, 0.0}, {z, 0.0, 0.5, 0.5}, {z, 0.5, 0.0, 0.5}, {z, 0.5, 0.5, 0.0}};
    case StructurePrototype::HCP:
        return {{z, 0.0, 0.0, 0.0}, {z, 1.0 / 3.0, 2.0 / 3.0, 0.5}};
    case StructurePrototype::DiamondCubic:
        return {
            {z, 0.00, 0.00, 0.00}, {z, 0.00, 0.50, 0.50},
            {z, 0.50, 0.00, 0.50}, {z, 0.50, 0.50, 0.00},
            {z, 0.25, 0.25, 0.25}, {z, 0.25, 0.75, 0.75},
            {z, 0.75, 0.25, 0.75}, {z, 0.75, 0.75, 0.25}
        };
    case StructurePrototype::GraphiteHexagonal:
        return {
            {z, 0.0, 0.0, 0.25}, {z, 0.0, 0.0, 0.75},
            {z, 1.0 / 3.0, 2.0 / 3.0, 0.25}, {z, 2.0 / 3.0, 1.0 / 3.0, 0.75}
        };
    case StructurePrototype::Other:
        return {{z, 0.0, 0.0, 0.0}};
    }
    return {{z, 0.0, 0.0, 0.0}};
}

CrystalStructureRecord make_cubic(std::string id, int z, std::string phase, StructurePrototype proto,
                                  int sg_num, std::string sg_hm, double a, int z_cell,
                                  double exp_rho, double temp_k = 298.0, bool std298 = true) {
    BravaisLattice bravais = BravaisLattice::SimpleCubic;
    if (proto == StructurePrototype::BCC) bravais = BravaisLattice::BodyCenteredCubic;
    if (proto == StructurePrototype::FCC || proto == StructurePrototype::DiamondCubic) {
        bravais = BravaisLattice::FaceCenteredCubic;
    }
    return CrystalStructureRecord{
        std::move(id), z, std::move(phase), CrystalSystem::Cubic, bravais, proto,
        sg_num, std::move(sg_hm),
        LatticeParameters{a, a, a, 90.0, 90.0, 90.0},
        z_cell, exp_rho, temp_k, std298, standard_basis(z, proto)
    };
}

CrystalStructureRecord make_hex(std::string id, int z, std::string phase, StructurePrototype proto,
                                double a, double c, int z_cell, double exp_rho) {
    return CrystalStructureRecord{
        std::move(id), z, std::move(phase), CrystalSystem::Hexagonal,
        BravaisLattice::SimpleHexagonal, proto,
        194, "P6_3/mmc",
        LatticeParameters{a, a, c, 90.0, 90.0, 120.0},
        z_cell, exp_rho, 298.0, true, standard_basis(z, proto)
    };
}

std::vector<CrystalStructureRecord> build_crystal_table() {
    using SP = StructurePrototype;
    std::vector<CrystalStructureRecord> t;
    t.reserve(28);

    // FCC Metals at 298 K
    t.push_back(make_cubic("Cu-FCC", 29, "Copper (FCC)",    SP::FCC, 225, "Fm-3m", 3.6149, 4,  8.960));
    t.push_back(make_cubic("Al-FCC", 13, "Aluminum (FCC)",  SP::FCC, 225, "Fm-3m", 4.0495, 4,  2.700));
    t.push_back(make_cubic("Ni-FCC", 28, "Nickel (FCC)",    SP::FCC, 225, "Fm-3m", 3.5240, 4,  8.908));
    t.push_back(make_cubic("Au-FCC", 79, "Gold (FCC)",      SP::FCC, 225, "Fm-3m", 4.0782, 4, 19.300));
    t.push_back(make_cubic("Ag-FCC", 47, "Silver (FCC)",    SP::FCC, 225, "Fm-3m", 4.0853, 4, 10.490));
    t.push_back(make_cubic("Pt-FCC", 78, "Platinum (FCC)",  SP::FCC, 225, "Fm-3m", 3.9242, 4, 21.450));
    t.push_back(make_cubic("Pb-FCC", 82, "Lead (FCC)",      SP::FCC, 225, "Fm-3m", 4.9508, 4, 11.340));

    // Iron Allotropes (alpha BCC 298K & gamma FCC austenite 1185K)
    t.push_back(make_cubic("Fe-alpha", 26, "Iron-alpha (Ferrite BCC)",   SP::BCC, 229, "Im-3m", 2.8665, 2, 7.874));
    t.push_back(make_cubic("Fe-gamma", 26, "Iron-gamma (Austenite FCC)", SP::FCC, 225, "Fm-3m", 3.6467, 4, 7.649, 1185.0, false));

    // BCC Metals at 298 K
    t.push_back(make_cubic("Cr-BCC", 24, "Chromium (BCC)",   SP::BCC, 229, "Im-3m", 2.8848, 2,  7.190));
    t.push_back(make_cubic("W-BCC",  74, "Tungsten (BCC)",   SP::BCC, 229, "Im-3m", 3.1652, 2, 19.250));
    t.push_back(make_cubic("Mo-BCC", 42, "Molybdenum (BCC)", SP::BCC, 229, "Im-3m", 3.1470, 2, 10.280));
    t.push_back(make_cubic("Li-BCC",  3, "Lithium (BCC)",    SP::BCC, 229, "Im-3m", 3.5100, 2,  0.534));
    t.push_back(make_cubic("Na-BCC", 11, "Sodium (BCC)",     SP::BCC, 229, "Im-3m", 4.2906, 2,  0.968));
    t.push_back(make_cubic("V-BCC",  23, "Vanadium (BCC)",   SP::BCC, 229, "Im-3m", 3.0240, 2,  6.110));
    t.push_back(make_cubic("Ta-BCC", 73, "Tantalum (BCC)",   SP::BCC, 229, "Im-3m", 3.3058, 2, 16.650));

    // Simple Cubic (alpha-Polonium)
    t.push_back(make_cubic("Po-SC",  84, "Polonium-alpha (SC)", SP::SC, 221, "Pm-3m", 3.3450, 1, 9.200));

    // Diamond Cubic & Graphite Allotropes
    t.push_back(make_cubic("C-diamond", 6,  "Carbon (Diamond Cubic)", SP::DiamondCubic, 227, "Fd-3m", 3.5670, 8, 3.515));
    t.push_back(make_hex  ("C-graphite",6,  "Carbon (Graphite Hex)",  SP::GraphiteHexagonal, 2.464, 6.711, 4, 2.260));
    t.push_back(make_cubic("Si-diamond",14, "Silicon (Diamond Cubic)",SP::DiamondCubic, 227, "Fd-3m", 5.4309, 8, 2.329));
    t.push_back(make_cubic("Ge-diamond",32, "Germanium (Diamond Cubic)",SP::DiamondCubic,227,"Fd-3m", 5.6575, 8, 5.323));

    // HCP Metals at 298 K
    t.push_back(make_hex("Ti-alpha-HCP", 22, "Titanium-alpha (HCP)", SP::HCP, 2.9508, 4.6855, 2, 4.506));
    t.push_back(make_hex("Mg-HCP",       12, "Magnesium (HCP)",      SP::HCP, 3.2094, 5.2108, 2, 1.738));
    t.push_back(make_hex("Zn-HCP",       30, "Zinc (HCP)",           SP::HCP, 2.6649, 4.9468, 2, 7.140));
    t.push_back(make_hex("Co-alpha-HCP", 27, "Cobalt-alpha (HCP)",   SP::HCP, 2.5071, 4.0695, 2, 8.900));
    t.push_back(make_hex("Zr-alpha-HCP", 40, "Zirconium-alpha (HCP)",SP::HCP, 3.2320, 5.1470, 2, 6.520));

    return t;
}

} // namespace

double LatticeParameters::volume_angstrom3() const noexcept {
    constexpr double kDegToRad = kPi / 180.0;
    const double ca = std::cos(alpha_deg * kDegToRad);
    const double cb = std::cos(beta_deg * kDegToRad);
    const double cg = std::cos(gamma_deg * kDegToRad);
    const double radicand = 1.0 - ca * ca - cb * cb - cg * cg + 2.0 * ca * cb * cg;
    if (radicand <= 0.0) return 0.0;
    return a_angstrom * b_angstrom * c_angstrom * std::sqrt(radicand);
}

double CrystalStructureRecord::unit_cell_volume_angstrom3() const noexcept {
    return lattice.volume_angstrom3();
}

double CrystalStructureRecord::atomic_packing_factor() const noexcept {
    const double v_cell = unit_cell_volume_angstrom3();
    if (v_cell <= 0.0) return 0.0;

    // Hard-sphere touching atomic radius per prototype
    double r = 0.0;
    switch (prototype) {
    case StructurePrototype::SC:
        r = 0.5 * lattice.a_angstrom;
        break;
    case StructurePrototype::BCC:
        r = std::sqrt(3.0) * 0.25 * lattice.a_angstrom;
        break;
    case StructurePrototype::FCC:
        r = std::sqrt(2.0) * 0.25 * lattice.a_angstrom;
        break;
    case StructurePrototype::HCP:
        r = 0.5 * lattice.a_angstrom;
        break;
    case StructurePrototype::DiamondCubic:
        r = std::sqrt(3.0) * 0.125 * lattice.a_angstrom;
        break;
    case StructurePrototype::GraphiteHexagonal:
        r = 0.5 * 1.422; // C-C sp2 in-plane bond half-length (angstrom)
        break;
    case StructurePrototype::Other:
        return 0.0;
    }
    const double v_atom = (4.0 / 3.0) * kPi * r * r * r;
    return (static_cast<double>(atoms_per_unit_cell) * v_atom) / v_cell;
}

double CrystalStructureRecord::theoretical_density_g_cm3() const noexcept {
    const double v_cell = unit_cell_volume_angstrom3();
    if (v_cell <= 0.0) return 0.0;
    const auto* elem = Elements118Catalog::find_by_z(atomic_number);
    if (!elem) return 0.0;
    // 1 A^3 = 1e-24 cm^3 => rho = (Z * M) / (N_A * V_A3 * 1e-24)
    return (static_cast<double>(atoms_per_unit_cell) * elem->atomic_weight_amu) /
           (kAvogadroMolInv * v_cell * 1e-24);
}

double CrystalStructureRecord::density_relative_error_pct() const noexcept {
    if (experimental_density_g_cm3 <= 0.0) return 0.0;
    const double theo = theoretical_density_g_cm3();
    return 100.0 * std::abs(theo - experimental_density_g_cm3) / experimental_density_g_cm3;
}

const std::vector<CrystalStructureRecord>& CrystallographyDatabase::all_structures() {
    static const std::vector<CrystalStructureRecord> tbl = build_crystal_table();
    return tbl;
}

const CrystalStructureRecord* CrystallographyDatabase::find_by_id(std::string_view id) noexcept {
    for (const auto& r : all_structures()) {
        if (r.id == id) return &r;
    }
    return nullptr;
}

std::vector<CrystalStructureRecord> CrystallographyDatabase::structures_for_element(int atomic_number) {
    std::vector<CrystalStructureRecord> out;
    for (const auto& r : all_structures()) {
        if (r.atomic_number == atomic_number) out.push_back(r);
    }
    return out;
}

std::string CrystallographyDatabase::to_json() {
    std::ostringstream out;
    out << "[\n";
    const auto& tbl = all_structures();
    for (size_t i = 0; i < tbl.size(); ++i) {
        const auto& r = tbl[i];
        out << "  {\"id\":\"" << r.id << "\""
            << ",\"atomic_number\":" << r.atomic_number
            << ",\"space_group\":" << r.space_group_number
            << ",\"a\":" << r.lattice.a_angstrom
            << ",\"apf\":" << r.atomic_packing_factor()
            << ",\"theoretical_density_g_cm3\":" << r.theoretical_density_g_cm3()
            << "}";
        if (i + 1 < tbl.size()) out << ",";
        out << "\n";
    }
    out << "]\n";
    return out.str();
}

core::base::ValidationReport CrystallographyDatabase::audit_densities(double max_rel_error_pct) {
    core::base::ValidationReport report;
    for (const auto& r : all_structures()) {
        if (r.space_group_number < 1 || r.space_group_number > 230) {
            report.add_error(r.id, "Space group number out of [1, 230]");
        }
        const double err = r.density_relative_error_pct();
        if (err > max_rel_error_pct) {
            report.add_error(r.id, "Theoretical density error " + std::to_string(err) +
                                   "% exceeds limit " + std::to_string(max_rel_error_pct) + "%");
        }
    }
    return report;
}

} // namespace shunya::matdb
