#pragma once

#include "shunya/core/base/result.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::matdb {

enum class CrystalSystem : uint8_t {
    Cubic,
    Tetragonal,
    Orthorhombic,
    Hexagonal,
    Trigonal,
    Monoclinic,
    Triclinic
};

enum class BravaisLattice : uint8_t {
    SimpleCubic,              // cP
    BodyCenteredCubic,        // cI
    FaceCenteredCubic,        // cF
    SimpleTetragonal,         // tP
    BodyCenteredTetragonal,   // tI
    SimpleOrthorhombic,       // oP
    BaseCenteredOrthorhombic, // oS
    BodyCenteredOrthorhombic, // oI
    FaceCenteredOrthorhombic, // oF
    SimpleHexagonal,          // hP
    Rhombohedral,             // hR
    SimpleMonoclinic,         // mP
    BaseCenteredMonoclinic,   // mS
    SimpleTriclinic           // aP
};

enum class StructurePrototype : uint8_t {
    SC,
    BCC,
    FCC,
    HCP,
    DiamondCubic,
    GraphiteHexagonal,
    Other
};

struct LatticeParameters {
    double a_angstrom{3.6149};
    double b_angstrom{3.6149};
    double c_angstrom{3.6149};
    double alpha_deg{90.0};
    double beta_deg{90.0};
    double gamma_deg{90.0};

    [[nodiscard]] double volume_angstrom3() const noexcept;
};

struct BasisSite {
    int atomic_number{29};
    double frac_x{0.0};
    double frac_y{0.0};
    double frac_z{0.0};
};

struct CrystalStructureRecord {
    std::string id;
    int atomic_number{29};
    std::string phase_name;
    CrystalSystem crystal_system{CrystalSystem::Cubic};
    BravaisLattice bravais_lattice{BravaisLattice::FaceCenteredCubic};
    StructurePrototype prototype{StructurePrototype::FCC};
    int space_group_number{225};
    std::string space_group_hm;
    LatticeParameters lattice;
    int atoms_per_unit_cell{4};
    double experimental_density_g_cm3{8.96};
    double reference_temp_k{298.0};
    bool is_standard_298k{true};
    std::vector<BasisSite> fractional_basis;

    [[nodiscard]] double unit_cell_volume_angstrom3() const noexcept;
    [[nodiscard]] double atomic_packing_factor() const noexcept;
    [[nodiscard]] double theoretical_density_g_cm3() const noexcept;
    [[nodiscard]] double density_relative_error_pct() const noexcept;
};

/**
 * @brief Registry of elemental crystal structures, 14 Bravais types, space groups (1..230), and 298 K lattice constants.
 */
class CrystallographyDatabase {
public:
    static const std::vector<CrystalStructureRecord>& all_structures();
    [[nodiscard]] static const CrystalStructureRecord* find_by_id(std::string_view id) noexcept;
    [[nodiscard]] static std::vector<CrystalStructureRecord> structures_for_element(int atomic_number);
    [[nodiscard]] static std::string to_json();
    [[nodiscard]] static core::base::ValidationReport audit_densities(double max_rel_error_pct = 1.5);
};

} // namespace shunya::matdb
