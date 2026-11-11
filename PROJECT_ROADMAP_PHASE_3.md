# Shunya Phase-3 Master Development Roadmap: Chemistry-Based Material Engineering, Object Creation, Circuit Design & Simulation

**Document ID:** `PROJECT_ROADMAP_PHASE_3.md`  
**Platform Version Target:** `v3.0.0-alpha` through `v3.5.0-release`  
**Status:** `Completed (Phase 3 Verified)`  
**Baseline Audited:** `PROJECT_ROADMAP.md` (309/309 tasks) & `PROJECT_ROADMAP_PHASE_2.md` (162/162 leaf subtasks) verified against actual repository state (`shunya_unit_tests.exe`: 517/517 test cases, 162,484/162,484 checks passing; `appshunya.exe` Qt 6.12.0 build & launch verified).  
**Primary Chemistry Data Source:** `C:\Users\Shubham\Desktop\test\chemistry\elements.csv` (118 elements, 82 columns)

---

## Overall Progress & Status Legend

| Status | Meaning |
|---|---|
| `[ ]` | Not Started |
| `[~]` | In Progress |
| `[x]` | Completed |
| `[!]` | Blocked |

Parent-level completion status and progress percentages are **strictly calculated from child leaf subtasks** (`completed_leaf_subtasks / total_leaf_subtasks * 100%`).

| Module # | Module ID | Module Name | Priority | Total Subtasks | Completed | Progress | Status |
|---:|---|---|---|---:|---:|---:|---|
| 1 | `AUDIT` | Project Analysis and Phase 2 Verification | P0 | 6 | 6 | 100.0% | `[x]` |
| 2 | `ARCH` | Architecture and Infrastructure Improvements | P0 | 9 | 9 | 100.0% | `[x]` |
| 3 | `ELEMDB` | Chemical Element Database | P0 | 9 | 9 | 100.0% | `[x]` |
| 4 | `MATBEH` | Natural Element Behavior Engine | P0 | 9 | 9 | 100.0% | `[x]` |
| 5 | `MIXLAB` | Material Mixing Laboratory | P0 | 10 | 10 | 100.0% | `[x]` |
| 6 | `MATPROP` | Material Property Calculation Engine | P0 | 8 | 8 | 100.0% | `[x]` |
| 7 | `OBJMAT` | Element-to-Object Creation | P0 | 8 | 8 | 100.0% | `[x]` |
| 8 | `OBJGEO` | Object Geometry and Material Assignment | P0 | 8 | 8 | 100.0% | `[x]` |
| 9 | `OBJCMP` | Object-to-Circuit-Component Generation | P0 | 9 | 9 | 100.0% | `[x]` |
| 10 | `BAT` | Battery Engineering and Simulation | P0 | 9 | 9 | 100.0% | `[x]` |
| 11 | `CIRUI` | Circuit Construction Workspace | P0 | 9 | 9 | 100.0% | `[x]` |
| 12 | `CIRSIM` | Circuit Simulation Engine | P0 | 9 | 9 | 100.0% | `[x]` |
| 13 | `ED2D` | 2D Object Editor | P0 | 8 | 8 | 100.0% | `[x]` |
| 14 | `ED3D` | 3D Object Editor | P0 | 8 | 8 | 100.0% | `[x]` |
| 15 | `PHYSIM` | Physics and Environmental Simulation | P0 | 8 | 8 | 100.0% | `[x]` |
| 16 | `THEME` | UI Theme Redesign (Black & White) | P0 | 8 | 8 | 100.0% | `[x]` |
| 17 | `ICON` | Icon System Update | P0 | 6 | 6 | 100.0% | `[x]` |
| 18 | `TYPO` | Typography Update | P0 | 6 | 6 | 100.0% | `[x]` |
| 19 | `MENU` | Menu Item UI Redesign | P0 | 6 | 6 | 100.0% | `[x]` |
| 20 | `NAV` | Application Navigation Improvements | P0 | 8 | 8 | 100.0% | `[x]` |
| 21 | `PERSIST` | Data Persistence and Project Management | P0 | 8 | 8 | 100.0% | `[x]` |
| 22 | `ATEST` | Automated Testing | P0 | 8 | 8 | 100.0% | `[x]` |
| 23 | `E2E` | End-to-End Testing | P0 | 6 | 6 | 100.0% | `[x]` |
| 24 | `STAB` | Performance and Stability | P1 | 6 | 6 | 100.0% | `[x]` |
| 25 | `DOC` | Final Verification and Documentation | P0 | 6 | 6 | 100.0% | `[x]` |
| **TOTAL** | **ALL** | **Phase 3 Complete Platform** | **--** | **195** | **195** | **100.0%** | `[x]` |

---

## 1. Project Analysis and Phase 2 Verification (`AUDIT`)

**Module Objective:** Conduct an evidence-based audit of `PROJECT_ROADMAP.md`, `PROJECT_ROADMAP_PHASE_2.md`, the C++20 core libraries, the Qt 6 QML desktop application (`appshunya`), the headless CLI (`shunya-cli`), and the external chemistry dataset (`elements.csv`), separating verified working capabilities from disconnected stubs, duplicates, and UI gaps.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (6 / 6 subtasks)  
**Dependencies:** None (Entry Gate for Phase 3)

### Submodule AUDIT-01: Roadmap & Build Verification `[x]` (100.0% — 3/3)
- [x] **AUDIT-01-001: Audit Phase 1 & Phase 2 Roadmaps vs. Repository Build & Tests** `(100.0% — 3/3)`
  - [x] `AUDIT-01-001-01`: Read `PROJECT_ROADMAP.md` (309 tasks) and `PROJECT_ROADMAP_PHASE_2.md` (162 subtasks) and cross-check milestone status consistency. *(Finding: Phase 2 WBS claims 162/162 `[x]`, while Section 14 Milestone table M1–M5 still reads `[ ] Not Started` and cites 159 subtasks.)*
  - [x] `AUDIT-01-001-02`: Configure, build, and execute all CMake targets (`shunya_core_base`, `shunya_core_domain`, `shunya-cli`, `appshunya`, `shunya_unit_tests`) using Qt 6.12.0 MinGW 13.1.0 + Ninja. *(Verified: 455/455 build targets succeeded; `shunya_unit_tests.exe` passed 517/517 test cases, 162,484/162,484 assertions; `shunya-cli.exe --benchmark` passed 5/5 benchmarks; `appshunya.exe` launched cleanly in offscreen mode.)*
  - [x] `AUDIT-01-001-03`: Inspect `.github/workflows/ci.yml` and `CMakePresets.json` for CI gaps. *(Finding: CI builds only `shunya_unit_tests`, omitting `appshunya`, `shunya-cli`, and any GUI/QML tests.)*

### Submodule AUDIT-02: Codebase, Data & UI Gap Identification `[x]` (100.0% — 3/3)
- [x] **AUDIT-02-001: Audit Core Domain, Desktop UI Wiring, and `elements.csv` Integrity** `(100.0% — 3/3)`
  - [x] `AUDIT-02-001-01`: Audit element/material architecture across `core/domain`. *(Verified gaps: 5 fragmented element models exist—`PeriodicTableDatabase`, `Elements118Catalog`, `PeriodicTableExtended`, `PeriodicHeatmapFilter`, and `ChemicalElementDefinition`; `AlloyBuilder` hardcodes resistivity/CTE for only 10 elements; `MaterialBuilder::build_material` ignores constituent properties and returns hardcoded defaults; `CadImportPipeline::import_mock_cad` produces an 8-vertex mock box; `elements.csv` is not loaded anywhere.)*
  - [x] `AUDIT-02-001-02`: Inspect `C:\Users\Shubham\Desktop\test\chemistry\elements.csv` (118 rows × 82 columns) for schema, formatting, and scientific anomalies. *(Verified findings: RFC 4180 multiline quoted `Space_Group_Name` fields e.g. `"Fm_\n3m"`; Excel-mangled `Adiabatic_Index` values `"07-May"` [7/5=1.4] and `"05-Mar"` [5/3=1.6667]; Z=112–118 use legacy temporary symbols `Uub`–`Uuo` with missing `Atomic_Weight` for Z=117; scrambled `Electron_Configuration` strings e.g. He=`[Xe]4f56s2`; suspect `Specific_Heat` values e.g. Cu=`63` vs NIST `385` J/kg·K.)*
  - [x] `AUDIT-02-001-03`: Audit `shunya/` QML UI wiring, theme, icons, typography, and menus. *(Verified findings: 22 of 48 QML files—including `PeriodicTableWindow`, `AlloyComposer`, `MoleculeViewer`, `SchematicView`, `PcbRoutingCanvas`, `ChamberControlPanel`, `WindTunnelView`, `DockContainer`—are completely unreachable from `Main.qml`; `ShunyaStudioController::onTimerTick` only advances `clock_` without stepping physics/circuits; `MainMenuBar.qml` import/export actions pass hardcoded filenames like `"sample.dxf"` to log-only stubs; `Main.qml` embeds a hardcoded `PROJECT EXPLORER` sidebar; UI uses blue accents across 138 hex colors and 16 emoji/dingbat glyph hits.)*

**Implementation Details:**
- Detailed Phase 2 verification findings captured directly from CMake build logs, `shunya_unit_tests.exe` execution, Python static analysis of `shunya/*.qml`, and full CSV schema inspection.
- No functional Phase 2 C++ code was broken; all 517 existing core unit tests remain the regression floor.

**Testing Requirements & Acceptance Criteria:**
- Reproducible build and test baseline documented with exact command outputs and identified gaps before any Phase 3 implementation starts.

---

## 2. Architecture and Infrastructure Improvements (`ARCH`)

**Module Objective:** Refactor monolithic build targets, establish a unified engineering workflow model connecting Elements → Materials → Objects → Components → Circuits → Simulation, and wire real domain services into `appshunya` without violating `core/base` and `core/domain` UI-independence boundaries.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (9 / 9 subtasks)  
**Dependencies:** `AUDIT`

### Submodule ARCH-01: Modular Build & Dependency Hygiene `[x]` (100.0% — 3/3)
- [x] **ARCH-01-001: Decompose Monolithic `shunya_core_domain` & Optimize Linking** `(100.0% — 3/3)`
  - [x] `ARCH-01-001-01`: Split or organize `core/domain/CMakeLists.txt` into logical `OBJECT`/sub-library targets (`shunya_domain_core`, `shunya_domain_mat`, `shunya_domain_cad`, `shunya_domain_pcb`, `shunya_domain_sim`, `shunya_domain_perf`, `shunya_domain_eng`) while preserving existing public include paths and `shunya_core_domain` umbrella link target so archive/link times drop dramatically.
  - [x] `ARCH-01-001-02`: Add a dedicated Phase 3 engineering pipeline header/library layer (`shunya::eng`) linking materials, objects, components, batteries, and circuits via stable UUIDs (no duplicated material/geometry snapshots).
  - [x] `ARCH-01-001-03`: Ensure zero Qt headers leak into `core/base` or `core/domain` and extend `.github/workflows/ci.yml` to build `shunya-cli`, `shunya_unit_tests`, and Qt headless tests.

### Submodule ARCH-02: Unified Domain Project State & Live Simulation Bus `[x]` (100.0% — 3/3)
- [x] **ARCH-02-001: Replace Mocked Studio State with Live Project Pipeline Coordinator** `(100.0% — 3/3)`
  - [x] `ARCH-02-001-01`: Create `EngineeringWorkspaceState` (or `StudioProjectContext`) inside `core/domain` holding the active single project's element database handle, saved mixtures/materials, 2D/3D objects, generated components, battery definitions, circuit schematics, and simulation results.
  - [x] `ARCH-02-001-02`: Implement reactive dependency propagation (Observer/Signal or dirty-versioning) so mutating a Material or Element automatically invalidates and recalculates dependent Object mass/density/conductivity, Component parameters (R/C/L/Battery), and Circuit simulation readiness.
  - [x] `ARCH-02-001-03`: Connect `ShunyaStudioController` and `ShunyaMenuBarController` to `EngineeringWorkspaceState` so Run/Step/Pause/Reset executes real coupled circuit/battery/physics stepping and streams live telemetry into `ScientificPlot` and `OscilloscopePanel`.

### Submodule ARCH-03: Consolidated Material/Element Data Authority `[x]` (100.0% — 3/3)
- [x] **ARCH-03-001: Eliminate Fragmented Element Catalogs** `(100.0% — 3/3)`
  - [x] `ARCH-03-001-01`: Establish a single authoritative `ElementDatabase` populated from `elements.csv` with validated NIST/IUPAC cross-reference overlays, replacing hardcoded arrays in `PeriodicHeatmapFilter` and the 10-element `switch(z)` in `AlloyBuilder`.
  - [x] `ARCH-03-001-02`: Implement adapter bridges from `ElementDatabase` to `PeriodicTableDatabase`, `Elements118Catalog`, and `ShunyaPeriodicTableController` so all existing 517 Phase 2 tests stay green while consuming normalized data.
  - [x] `ARCH-03-001-03`: Add centralized `ProjectAssetRegistry` for UUID-keyed Materials, Objects, Components, and Circuits with referential integrity validation.

**Implementation Details:**
- Strict dependency direction: `core/base` → `core/domain` (no Qt) → `shunya` QML/C++ controllers.
- Preserve all existing headers in `core/domain/include/shunya/...`.

**Testing Requirements:**
- Unit tests verifying reactive invalidation (material change → object & component recalculation) and CMake clean build of all targets.

**Acceptance Criteria:**
- Zero regressions across all 517 baseline unit tests; incremental builds & links complete cleanly; no Qt dependency in `core/*`.

---

## 3. Chemical Element Database (`ELEMDB`)

**Module Objective:** Integrate `C:\Users\Shubham\Desktop\test\chemistry\elements.csv` as the primary chemical element data source with RFC 4180 CSV parsing, full preservation of all 82 CSV columns, validation/normalization, conflict transparency, and application-wide search/filter/sort/inspect capabilities.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (9 / 9 subtasks)  
**Dependencies:** `AUDIT`, `ARCH`

### Submodule ELEMDB-01: RFC 4180 CSV Ingestion & Schema Mapping `[x]` (100.0% — 3/3)
- [x] **ELEMDB-01-001: Parse and Preserve All 82 Properties from `elements.csv`** `(100.0% — 3/3)`
  - [x] `ELEMDB-01-001-01`: Implement a strict RFC 4180 CSV parser (`core/domain/include/shunya/matdb/csv_element_loader.hpp`) handling quoted fields with embedded newlines (e.g., `"Fm_\n3m"`), empty cells, scientific notation (`4.60E-05`), and UTF-8.
  - [x] `ELEMDB-01-001-02`: Define `ElementRecord` preserving every required and optional CSV column with explicit typed `std::optional<double>`, `std::optional<int>`, and `std::string` fields: Name, Symbol, Atomic_Number, Atomic_Weight, Density, Melting_Point, Boiling_Point, Phase, Absolute_Melting_Point, Absolute_Boiling_Point, Critical_Pressure, Critical_Temperature, Heat_of_Fusion, Heat_of_Vaporization, Heat_of_Combustion, Specific_Heat, Adiabatic_Index, Neel_Point, Thermal_Conductivity, Thermal_Expansion, Density_Liquid, Molar_Volume, Brinell/Mohs/Vickers_Hardness, Bulk/Shear/Young_Modulus, Poisson_Ratio, Refractive_Index, Speed_of_Sound, Valence, Electronegativity, ElectronAffinity, Autoignition_Point, Flashpoint, DOT/EU/NFPA/RTECS hazards, Block, Group, Period, Electron_Configuration, Color, Gas_phase, CAS/CID/Gmelin/NSC, Electrical_Type, Electrical_Conductivity, Resistivity, Superconducting_Point, Magnetic_Type, Curie_Point, Magnetic Susceptibilities, Abundances, Atomic/Covalent/VdW Radii, Space_Group_Name/Number, HalfLife, Lifetime, Decay_Mode, Quantum_Numbers, Neutron_Cross_Section/Mass_Absorption, Graph.Period, Graph.Group, plus raw/unparsed map preservation.
  - [x] `ELEMDB-01-001-03`: Bundle or resolve `elements.csv` deterministically (configurable path + embedded fallback copy / CMake asset deployment alongside executable) and verify all 118 rows load with zero crashes.

### Submodule ELEMDB-02: Data Validation, Normalization & Provenance `[x]` (100.0% — 3/3)
- [x] **ELEMDB-02-001: Handle Missing, Invalid, and Anomalous Source CSV Fields** `(100.0% — 3/3)`
  - [x] `ELEMDB-02-001-01`: Normalize Excel-mangled `Adiabatic_Index` values (`"07-May"` → `1.4` [7/5], `"05-Mar"` → `1.6666667` [5/3]) and clean newline artifacts in `Space_Group_Name` (`"Fm_\n3m"` → `"Fm-3m"`), tagging each corrected field with `NormalizationNote` provenance.
  - [x] `ELEMDB-02-001-02`: Cross-check `elements.csv` against IUPAC 2021 reference values (`Elements118Catalog`) for Z=112–118 aliases (`Uub`–`Uuo` vs `Cn`–`Og`), missing `Atomic_Weight` (Z=117), scrambled `Electron_Configuration` strings, and suspect `Specific_Heat` outliers, exposing a clear `PropertyProvenance` flag (`CsvPrimary`, `CsvNormalized`, `NistReferenceFallback`, `MissingUnsupported`).
  - [x] `ELEMDB-02-001-03`: Never fabricate missing scientific values silently: return `std::nullopt` / `Unsupported` with explicit diagnostics when a CSV property is absent (e.g., `Critical_Pressure` only exists for 21/118 elements; `Heat_of_Combustion` for 7/118).

### Submodule ELEMDB-03: Querying, Filtering, Sorting & Element Explorer UI `[x]` (100.0% — 3/3)
- [x] **ELEMDB-03-001: Central Query API & Interactive Elements Workspace** `(100.0% — 3/3)`
  - [x] `ELEMDB-03-001-01`: Implement `ElementDatabase::query(ElementQuery)` supporting substring search (name, symbol, alternate names, CAS number), filters (phase, block, group, period, electrical type, magnetic type, hazard class, property-present), and multi-column sorting (Z, mass, density, MP, BP, thermal/electrical conductivity, specific heat, modulus).
  - [x] `ELEMDB-03-001-02`: Upgrade `ShunyaPeriodicTableController` to expose all 82 CSV properties, search/filter/sort models, provenance badges, and unit-formatted values to QML.
  - [x] `ELEMDB-03-001-03`: Build/upgrade the `Elements` workspace view (`PeriodicTableWindow.qml` + searchable table view + full 82-property Element Inspector drawer with provenance indicators, Bohr orbital, isotope & crystal tabs) accessible from main navigation.

**Implementation Details:**
- Store raw CSV strings alongside parsed SI quantities so zero CSV information is lost.
- Distinguish clearly between `elements.csv` values and NIST/IUPAC cross-validated overrides.

**Testing Requirements:**
- Unit tests parsing the real `C:\Users\Shubham\Desktop\test\chemistry\elements.csv`, asserting 118 elements loaded, multiline quotes parsed, `"07-May"`/`"05-Mar"` normalized, Z=117 missing atomic weight handled gracefully, and search/filter/sort accuracy.

**Acceptance Criteria:**
- All 118 elements and all 82 CSV columns are accessible via C++ API and visible/searchable/sortable in the Elements UI with zero hardcoded element tables in UI code.

---

## 4. Natural Element Behavior Engine (`MATBEH`)

**Module Objective:** Model elements and materials as dynamic physical-chemical systems responding to temperature ($T$), pressure ($P$), and environment rather than static database rows, strictly distinguishing validated physics, documented approximations, and unsupported properties.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (9 / 9 subtasks)  
**Dependencies:** `ELEMDB`

### Submodule MATBEH-01: Thermodynamic State & Phase Transitions `[x]` (100.0% — 3/3)
- [x] **MATBEH-01-001: T/P-Dependent Phase, Density & Thermal Expansion Evaluator** `(100.0% — 3/3)`
  - [x] `MATBEH-01-001-01`: Implement `ElementBehaviorEngine::evaluate_state(element, T_k, P_pa)` computing phase (`Solid`, `MeltingTwoPhase`, `Liquid`, `BoilingTwoPhase`, `Gas`, `Supercritical` when $T > T_c$ and $P > P_c$ if critical data exists) using Clausius-Clapeyron / pressure-adjusted boiling/melting estimates where latent heat & molar volume data are available, clearly tagging model fidelity (`L1`/`L2`/`L3`).
  - [x] `MATBEH-01-001-02`: Compute temperature-dependent density: $\rho(T) = \rho_0 / (1 + 3\alpha \Delta T)$ in solid phase using CSV `Thermal_Expansion`, transitioning to `Density_Liquid` above $T_m$, and ideal-gas / Redlich-Kwong EOS ($\rho = PM / (ZRT)$) in gas phase.
  - [x] `MATBEH-01-001-03`: Compute enthalpy, sensible heat ($c_p \Delta T$), latent heat of fusion/vaporization transitions, and thermal strain ($\epsilon_{th} = \alpha \Delta T$) with explicit `Unsupported` flags when `Heat_of_Fusion` or `Thermal_Expansion` is missing.

### Submodule MATBEH-02: Transport & Electrical Behavior `[x]` (100.0% — 3/3)
- [x] **MATBEH-02-001: Temperature-Dependent Electrical & Thermal Transport** `(100.0% — 3/3)`
  - [x] `MATBEH-02-001-01`: Model metallic resistivity $\rho_e(T) = \rho_{e,0}[1 + \alpha_R (T - T_0)]$ and superconductivity transition ($\rho_e \to 0$ when $T < T_{sc}$ using CSV `Superconducting_Point` where available) and semiconductor negative temperature coefficient behavior when `Electrical_Type == "Semiconductor"`.
  - [x] `MATBEH-02-001-02`: Evaluate thermal conductivity $k(T)$ (solid/liquid transition scaling + Wiedemann-Franz cross-check warning for metals) and specific heat $c_p(T)$ (using Shomate polynomials where validated in `matdb` or documented constant-Cp approximation from CSV).
  - [x] `MATBEH-02-001-03`: Track magnetic phase transition at `Curie_Point` (ferromagnetic → paramagnetic) and `Neel_Point` (antiferromagnetic → paramagnetic) when present in CSV.

### Submodule MATBEH-03: Chemical Reactivity, Oxidation, Corrosion & Combustion `[x]` (100.0% — 3/3)
- [x] **MATBEH-03-001: Reactivity, Stability, Corrosion & Combustion Assessment** `(100.0% — 3/3)`
  - [x] `MATBEH-03-001-01`: Implement reactivity & stability rules from electronegativity, ionization energy, standard reduction potential / reactivity series (alkali metals with water/air, halogens, noble gas inertness), and radioactive half-life stability (`HalfLife` from CSV).
  - [x] `MATBEH-03-001-02`: Implement oxidation/corrosion susceptibility indicators (passivating oxide layer e.g. Al/Ti/Cr vs active rusting Fe vs noble Au/Pt, galvanic potential difference warning when dissimilar metals contact in electrolyte).
  - [x] `MATBEH-03-001-03`: Implement combustion & thermal hazard evaluation using CSV `Autoignition_Point`, `Flashpoint`, `Heat_of_Combustion`, and NFPA fire/health/reactivity ratings, surfacing warnings when $T \ge T_{\text{autoignition}}$ in oxidizing atmosphere.

**Implementation Details:**
- Every evaluated behavior result returns `BehaviorEvaluationReport` with `{value, unit, confidence: ConfirmedScientific | ApproximateModel | ExperimentalAssumption | Unsupported, explanation}`.

**Testing Requirements:**
- Unit tests verifying Li/Cu/Fe/Al/H2/He phase states across 10 K–4000 K, thermal expansion & resistivity scaling, superconducting transition for Al ($T_{sc} = 1.175$ K) / Pb, Li autoignition warning at $T > 452.15$ K, and unsupported property reporting for superheavy elements.

**Acceptance Criteria:**
- Elements dynamically respond to $T$ and $P$ changes in both solver APIs and UI inspectors with transparent scientific fidelity labels.

---

## 5. Material Mixing Laboratory (`MIXLAB`)

**Module Objective:** Build an interactive Material Mixing Laboratory where users combine elements and materials under controlled $T$/$P$ conditions across distinct formation modes (Physical Mixture, Alloy, Solution, Chemical Compound, Layered Material, Composite), enforce chemical validity rules rather than auto-forming invalid compounds, compare elements vs. mixture, and save/reopen/promote mixtures to reusable materials.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (10 / 10 subtasks)  
**Dependencies:** `ELEMDB`, `MATBEH`

### Submodule MIXLAB-01: Composition & Formation Rules Engine `[x]` (100.0% — 4/4)
- [x] **MIXLAB-01-001: Multi-Element Input, Proportion Balancing & Formation Scenarios** `(100.0% — 4/4)`
  - [x] `MIXLAB-01-001-01`: Implement `MixtureRecipe` supporting element/material selection by mass (kg/g), mass fraction (wt%), mole count (mol), mole fraction (at%), or volume fraction (vol%), with automatic normalization and conservation checks.
  - [x] `MIXLAB-01-001-02`: Implement 6 explicit formation scenarios (`PhysicalMixture`, `Alloy`, `Solution`, `ChemicalCompound`, `LayeredMaterial`, `CompositeMaterial`) with distinct structural parameters (e.g., layer thicknesses/orientations for `LayeredMaterial`, matrix + reinforcement for `CompositeMaterial`, solvent + solute for `Solution`).
  - [x] `MIXLAB-01-001-03`: Implement chemical & metallurgical formation validation rules: check valence/charge neutrality and electronegativity difference for `ChemicalCompound` (rejecting non-stoichiometric or inert combinations like He+Au as invalid compounds), check metallic-character compatibility and Hume-Rothery size/electronegativity heuristics for `Alloy` (flagging immiscibility/intermetallic warnings), and check phase/solubility compatibility for `Solution`.
  - [x] `MIXLAB-01-001-04`: Tag every calculated mixture property and reaction outcome with explicit epistemic status (`ConfirmedScientific`, `ApproximateSimulation`, `UserExperimentalAssumption`).

### Submodule MIXLAB-02: Experiment Persistence & Material Promotion `[x]` (100.0% — 3/3)
- [x] **MIXLAB-02-001: Save, Reopen, Modify & Promote Mixtures** `(100.0% — 3/3)`
  - [x] `MIXLAB-02-001-01`: Serialize/deserialize `MixtureExperiment` (inputs, ratios, $T$, $P$, formation mode, validation diagnostics, computed properties) inside the active project and standalone JSON files.
  - [x] `MIXLAB-02-001-02`: Support reopening and modifying saved experiments without breaking references, and comparing Base Elements vs. Resulting Mixture side-by-side (delta & % change table + chart).
  - [x] `MIXLAB-02-001-03`: Implement "Promote to Reusable Material" registering a live `MaterialDefinition` into the project's `MaterialRegistry` linked back to its source `MixtureExperiment` UUID.

### Submodule MIXLAB-03: Mixing Laboratory Interactive UI `[x]` (100.0% — 3/3)
- [x] **MIXLAB-03-001: Full Mixing Lab Workspace (Replacing Stub `AlloyComposer.qml`)** `(100.0% — 3/3)`
  - [x] `MIXLAB-03-001-01`: Build `ShunyaMixingLabController` exposing element/material picker, recipe rows (add/remove/adjust mass or % or moles), formation scenario selector, $T$/$P$ controls, validation diagnostics, comparison table, and saved experiments list.
  - [x] `MIXLAB-03-001-02`: Redesign `AlloyComposer.qml` / `MixingLaboratoryView.qml` into an interactive workspace with element browser integration, live property preview, element-vs-mixture comparison view, and epistemic badges (Confirmed / Approximate / Experimental Assumption).
  - [x] `MIXLAB-03-001-03`: Seed and verify scientifically meaningful starter templates (e.g., Steel / Fe-C alloy, Cu-Zn Brass, LiCoO2 / LiFePO4 cathode active material, LiPF6-EC/DMC electrolyte assumption, Al-6061 alloy) as editable recipes.

**Implementation Details:**
- Reuse `shunya::mat::FormulaParser`, `AlloyBuilder`, `CompositeHomogenizer`, and `matui::MaterialComparator` where applicable, fixing hardcoded fallbacks so all element properties come from `ElementDatabase`.

**Testing Requirements:**
- Unit tests for stoichiometry validation (valid H2O/LiFePO4 vs invalid He+Cu compound rejection), alloy mass↔mole conversions, Hume-Rothery warnings, experiment save/reopen round-trip, and material promotion.

**Acceptance Criteria:**
- Users can mix elements under custom $T$/$P$, see validated compatibility warnings, compare before/after properties, save/reopen experiments, and promote mixtures to project materials.

---

## 6. Material Property Calculation Engine (`MATPROP`)

**Module Objective:** Compute accurate effective physical, thermal, mechanical, and electrical properties of mixed materials, alloys, composites, solutions, and layered stackups as functions of composition, $T$, and $P$, propagating updates dynamically to all dependent objects and components.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (8 / 8 subtasks)  
**Dependencies:** `ELEMDB`, `MATBEH`, `MIXLAB`

### Submodule MATPROP-01: Mixing Laws & Homogenization Models `[x]` (100.0% — 4/4)
- [x] **MATPROP-01-001: Composition-Based Effective Property Calculators** `(100.0% — 4/4)`
  - [x] `MATPROP-01-001-01`: Compute mixture density via inverse rule-of-mixtures (volume additivity $\rho_{\text{mix}} = (\sum w_i / \rho_i)^{-1}$) for ideal mixtures/alloys, plus optional excess volume / contraction factor, and mass-weighted specific heat ($c_{p,\text{mix}} = \sum w_i c_{p,i}$, Kopp-Neumann approximation tagged as `ApproximateSimulation`).
  - [x] `MATPROP-01-001-02`: Compute effective mechanical moduli ($K, G, E, \nu$) using Voigt, Reuss, Hill average, and Hashin-Shtrikman bounds via `CompositeHomogenizer` fed by `ElementDatabase` moduli.
  - [x] `MATPROP-01-001-03`: Compute effective electrical conductivity/resistivity using Nordheim alloy scattering correction ($\rho_{\text{alloy}} = \sum x_i \rho_i + C x_i (1 - x_i)$ for solid solutions), percolation/parallel-series mixing for composites, and series/parallel sheet resistance for layered materials.
  - [x] `MATPROP-01-001-04`: Compute effective thermal conductivity (parallel/series/Maxwell-Eucken for dispersed phases, series thermal resistance for layered stackups) and thermal expansion (Turner/Voigt models).

### Submodule MATPROP-02: Condition Curves & Dependency Propagation `[x]` (100.0% — 4/4)
- [x] **MATPROP-02-001: $T$/$P$ Property Curves & Reactive Recalculation** `(100.0% — 4/4)`
  - [x] `MATPROP-02-001-01`: Generate `MaterialCurve` samples ($\rho(T), k(T), c_p(T), \rho_e(T)$) across user-specified temperature/pressure sweeps for plotting in `ScientificPlot`.
  - [x] `MATPROP-02-001-02`: Handle incomplete constituent data cleanly: if a constituent lacks a property (e.g., missing Young's modulus for a gas or exotic element), compute partial coverage metadata instead of silently inventing `1000` or `1.5e-7`.
  - [x] `MATPROP-02-001-03`: Connect `MaterialPropertyEngine` to `MaterialRegistry` and `MaterialDomainAdapter` so every simulation solver reads live homogenized properties.
  - [x] `MATPROP-02-001-04`: Emit `materialPropertiesChanged(materialId)` on composition or $T$/$P$ updates to trigger automatic downstream Object and Component recalculation.

**Implementation Details:**
- Replace hardcoded `element_resistivity_ohm_m` and `element_cte_per_k` switches in `core/domain/src/mat/alloy_builder.cpp` with queries to `ElementDatabase`.

**Testing Requirements:**
- Unit tests verifying Cu-Zn brass, Ti-6Al-4V, Al-Cu layered stackup, and missing-property partial reporting against analytical mixing bounds (Reuss $\le$ HS-lower $\le$ Hill $\le$ HS-upper $\le$ Voigt).

**Acceptance Criteria:**
- Changing an element fraction immediately updates density, resistivity, thermal conductivity, specific heat, and moduli with full provenance and zero hardcoded lookup switches.

---

## 7. Element-to-Object Creation (`OBJMAT`)

**Module Objective:** Enable users to create physical engineering objects from pure elements or mixed materials, linking geometry dimensions and environmental conditions directly to material definitions so mass, volume, density, and physical properties recalculate automatically when materials change.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (8 / 8 subtasks)  
**Dependencies:** `MATPROP`

### Submodule OBJMAT-01: Material-Backed Physical Object Data Model `[x]` (100.0% — 4/4)
- [x] **OBJMAT-01-001: PhysicalObject Schema & Live Material Linkage** `(100.0% — 4/4)`
  - [x] `OBJMAT-01-001-01`: Define `PhysicalObject` (`core/domain/include/shunya/eng/physical_object.hpp`) referencing geometry (`ObjectShapeKind`, extruded profile, or `SolidMesh3D`), optional multi-region partitions (regions/layers/faces), `material_id` UUID (never copying raw constants without link), and operating state ($T$, $P$).
  - [x] `OBJMAT-01-001-02`: Compute exact geometric volume ($V$), surface area ($A$), center of mass ($\mathbf{r}_{\text{cm}}$), inertia tensor ($I$), and total mass ($m = \sum_r \rho_r(T, P) V_r$) across single-material and multi-region objects.
  - [x] `OBJMAT-01-001-03`: Implement live reactive synchronization: whenever a linked `MaterialDefinition` in `MaterialRegistry` is edited, `PhysicalObjectManager::recalculate_for_material(material_id)` updates object density, mass, thermal mass ($m c_p$), and effective electrical/thermal conductances.
  - [x] `OBJMAT-01-001-04`: Provide standard engineering object presets/templates (Copper Wire, Iron Plate, Aluminum Frame, Steel Rod, Lithium Foil, Composite Housing, Insulating Spacer) built directly from `ElementDatabase`/`MaterialRegistry`.

### Submodule OBJMAT-02: Object Creation & Inspection UI `[x]` (100.0% — 4/4)
- [x] **OBJMAT-02-001: Objects Workspace & Object Builder Workflow** `(100.0% — 4/4)`
  - [x] `OBJMAT-02-001-01`: Implement `ShunyaObjectController` exposing object creation, editing, duplication, deletion, dimension configuration, material/region assignment, and temperature/pressure preview.
  - [x] `OBJMAT-02-001-02`: Build `ObjectsWorkspace.qml` allowing users to pick a material (pure element or mixed material), choose geometry type & dimensions, assign materials to whole object or regions, and preview calculated mass, volume, density, resistance, thermal capacitance, and phase warnings.
  - [x] `OBJMAT-02-001-03`: Integrate `ShunyaPropertyTreeModel` with the selected `PhysicalObject` (replacing the `"Switch Demo"` `"Resistor"`/`"Body"` demo toggle with live project object inspection).
  - [x] `OBJMAT-02-001-04`: Persist and reload all `PhysicalObject` instances and material bindings in the project file.

**Implementation Details:**
- Unify `PhysicalObject` with `device::DevicePart` and `SceneGraph` entities so 2D/3D editors and physics solvers share one source of truth.

**Testing Requirements:**
- Unit tests verifying Copper wire mass/resistance calculation from dimensions & Cu CSV density/resistivity, multi-region object center-of-mass/mass, and automatic recalculation when source material composition changes.

**Acceptance Criteria:**
- Editing a mixture in Mixing Lab automatically updates mass, density, and properties of every Object using that material.

---

## 8. Object Geometry and Material Assignment (`OBJGEO`)

**Module Objective:** Support multi-region material assignment on 2D/3D objects (faces, layers, volumes, internal cores/coatings), layered assemblies, and geometric/physical consistency validation.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (8 / 8 subtasks)  
**Dependencies:** `OBJMAT`, `ED2D`, `ED3D`

### Submodule OBJGEO-01: Multi-Region Geometry & Region Partitioning `[x]` (100.0% — 4/4)
- [x] **OBJGEO-01-001: Layered, Core-Shell & Sub-Volume Region Modeling** `(100.0% — 4/4)`
  - [x] `OBJGEO-01-001-01`: Extend `PhysicalObject` with `ObjectRegion` entries (volume fraction or explicit sub-geometry bounding/mesh/layer thickness, e.g., wire core + insulation jacket, plate substrate + coating, battery stack layers).
  - [x] `OBJGEO-01-001-02`: Implement `GeometryMaterialValidator` validating non-zero positive dimensions, non-overlapping or watertight sub-regions, valid `material_id` references, and phase consistency (e.g., warning if a structural solid region melts at operating $T$).
  - [x] `OBJGEO-01-001-03`: Compute directional/anisotropic effective properties for multi-region objects (axial vs radial resistance, through-plane vs in-plane thermal conductance).
  - [x] `OBJGEO-01-001-04`: Integrate `cad3d::SolidMesh3D` and `cad2d` profile extrusion so 2D sketches can become 3D material-assigned objects with accurate mesh volume & surface area.

### Submodule OBJGEO-02: Region Assignment & Assembly Mating UI `[x]` (100.0% — 4/4)
- [x] **OBJGEO-02-001: Interactive Region Material Assignment & Assembly Composer** `(100.0% — 4/4)`
  - [x] `OBJGEO-02-001-01`: Add region list & layer stackup editor inside the Objects workspace so users can assign distinct materials to each region/layer of an object.
  - [x] `OBJGEO-02-001-02`: Implement visual region coloring/hatching in 2D/3D previews with material legend tooltips (material name, phase, density, conductivity).
  - [x] `OBJGEO-02-001-03`: Support grouping multiple `PhysicalObject` parts into a `PhysicalAssembly` with contact interfaces (`DeviceContactInterface`: electrical contact resistance, thermal interface resistance, mechanical mate).
  - [x] `OBJGEO-02-001-04`: Save and restore multi-region assignments and assembly interfaces cleanly in project serialization.

**Testing Requirements:**
- Unit tests for insulated copper wire (Cu core + polymer/insulator jacket) volume/mass/axial-resistance vs radial-insulation, and assembly thermal/electrical contact interfaces.

**Acceptance Criteria:**
- Users can build multi-region objects and assemblies with distinct materials per region and inspect accurate composite physical/geometric metrics.

---

## 9. Object-to-Circuit-Component Generation (`OBJCMP`)

**Module Objective:** Convert created physical objects and assemblies into functional engineering/circuit components based on their material composition, electrical/thermal properties, geometry, terminals, and operating limits—never turning arbitrary shapes into valid electronics without physical qualification.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (9 / 9 subtasks)  
**Dependencies:** `OBJMAT`, `OBJGEO`

### Submodule OBJCMP-01: Physical Qualification & Parameter Extraction `[x]` (100.0% — 5/5)
- [x] **OBJCMP-01-001: Physics-Driven Component Qualification Rules** `(100.0% — 5/5)`
  - [x] `OBJCMP-01-001-01`: Define `GeneratedComponentDefinition` (`core/domain/include/shunya/eng/component_generator.hpp`) referencing source `object_id` UUID (no duplication of material/geometry), `ComponentKind` (`Resistor`, `ConductorWire`, `Switch`, `Capacitor`, `Inductor`, `Battery`, `Connector`, `HeatingElement`, `Motor`), terminal ports mapped to physical faces/endpoints, and live derived electrical/thermal parameters.
  - [x] `OBJCMP-01-001-02`: Implement physical parameter derivation formulas from object geometry + material properties:
    - `Resistor` / `ConductorWire` / `HeatingElement`: $R(T) = \rho_e(T) \cdot L / A_{\text{cross}}$, TCR $\alpha_R$, thermal mass $C_{\text{th}} = m c_p$, max current / power / melting limit warnings.
    - `Capacitor`: parallel-plate / coaxial geometry $C = \varepsilon_0 \varepsilon_r A / d$, dielectric breakdown voltage $V_{\max} = E_{\text{bd}} d$ (rejecting purely metallic short-circuited geometries without dielectric separator!).
    - `Inductor`: coil/solenoid turns & core permeability $L = \mu_0 \mu_r N^2 A / l$, winding resistance from conductor material.
    - `Switch` / `Connector`: contact resistance from contact area + material resistivity + ampacity thermal limit.
    - `Motor`: simplified electromechanical winding + rotor inertia/back-EMF parameters derived from coil material & rotor mass/inertia (explicitly tagged `L1`/`L2`).
  - [x] `OBJCMP-01-001-03`: Implement strict `ComponentQualificationValidator` rejecting physically invalid conversions with actionable diagnostics (e.g., trying to turn an insulator into a low-resistance wire, a conductor-only block into a capacitor without dielectric, or missing terminals).
  - [x] `OBJCMP-01-001-04`: Define terminal placement (`ComponentTerminal` with 2D/3D anchor position, polarity, domain, and target object region) and verify continuity between terminals through conductive regions.
  - [x] `OBJCMP-01-001-05`: Automatically recompute component electrical/thermal parameters whenever the underlying Object dimensions or Material composition changes.

### Submodule OBJCMP-02: Component Library & Conversion Wizard UI `[x]` (100.0% — 4/4)
- [x] **OBJCMP-02-001: Interactive Object-to-Component Converter & Library** `(100.0% — 4/4)`
  - [x] `OBJCMP-02-001-01`: Implement `ShunyaComponentController` managing conversion workflow, validation diagnostics, component library CRUD, and parameter preview.
  - [x] `OBJCMP-02-001-02`: Build `ComponentsWorkspace.qml` implementing the 9-step Component Creation Workflow (select Object → select Component Type → assign terminals/regions → configure type-specific parameters e.g. turns/area → inspect derived R/C/L/limits & validation report → generate & save to Component Library).
  - [x] `OBJCMP-02-001-03`: Expose all saved components from the Component Library directly in the Circuit Editor's component palette.
  - [x] `OBJCMP-02-001-04`: Persist `GeneratedComponentDefinition` items inside the project manifest/asset store with UUID links to their source Objects.

**Testing Requirements:**
- Unit tests verifying Copper wire → `ConductorWire` ($R = \rho L / A$ within $10^{-9}$), insulator block → rejected as `ConductorWire` with clear error, dielectric-sandwich → `Capacitor`, and live update of component $R$ when wire material or length changes.

**Acceptance Criteria:**
- An object must pass material/geometry/continuity validation to become a functional circuit component, and saved components appear immediately in the Circuit Editor.

---

## 10. Battery Engineering and Simulation (`BAT`)

**Module Objective:** Provide an end-to-end chemistry-to-circuit battery engineering system where users construct batteries (e.g., Lithium-ion) from material-assigned regions (anode, cathode, electrolyte, separator, current collectors, casing, terminals), validate compatibility, calculate electrochemical/thermal parameters, and simulate charging/discharging with live graphs and limit warnings.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (9 / 9 subtasks)  
**Dependencies:** `OBJCMP`, `ELEMDB`, `MATBEH`

### Submodule BAT-01: Battery Structural & Electrochemical Model `[x]` (100.0% — 5/5)
- [x] **BAT-01-001: Multi-Region Battery Assembly & Compatibility Validator** `(100.0% — 5/5)`
  - [x] `BAT-01-001-01`: Define `BatteryDesignSpec` referencing 7 physical regions/objects: `anode`, `cathode`, `electrolyte`, `separator`, `anode_current_collector`, `cathode_current_collector`, `casing`, plus `positive_terminal` and `negative_terminal`.
  - [x] `BAT-01-001-02`: Implement `BatteryCompatibilityValidator` enforcing physical & electrochemical sanity checks:
    - Anode/cathode must contain active redox/intercalation species (e.g., Li-containing cathode + Carbon/Graphite or Li anode) with valid redox potential difference ($E_{\text{cathode}} - E_{\text{anode}} > 0$).
    - Separator must be electrically insulating ($\sigma < 10^{-4}$ S/m) to prevent internal short circuit.
    - Current collectors must be electrically conductive (e.g., Cu on anode side, Al on cathode side) and checked for electrochemical stability/alloying warnings (e.g., Al-Li alloying warning if Al is used at low anode potential).
    - Casing material checked for structural integrity & thermal/chemical containment.
  - [x] `BAT-01-001-03`: Derive approximate battery parameters from geometry & material mass: active mass $m_{\text{active}}$, theoretical capacity $Q_{\text{Ah}}$ (via Faraday's constant $F = 96485.33$ C/mol and active lithium stoichiometry), OCV curve $V_{\text{oc}}(\text{SOC})$ from electrode chemistry, internal resistance $R_0(T)$ (collectors + electrolyte ionic conductivity $\sigma_{\text{ion}}$ + contact), thermal mass $C_{\text{th}} = \sum m_r c_{p,r}$, and surface cooling area.
  - [x] `BAT-01-001-04`: Upgrade `shunya::chem::BatteryCellRuntime` / `BatteryEngineer` to support both **charging** ($I < 0$, CC-CV limits, overvoltage/plating warnings) and **discharging** ($I > 0$), returning voltage, current, SOC, delivered/stored energy (Wh), capacity (Ah), instantaneous power (W), temperature (K/°C), and limit warnings (over-temperature, thermal runaway onset, over-current, under-voltage/over-voltage, melting).
  - [x] `BAT-01-001-05`: Explicitly label model fidelity (`L2 Engineering Approximation` — Dual-RC Thevenin ECM + Faraday capacity estimate + lumped thermal balance vs full Newman P2D DFT/FEM) in both API metadata and UI banners.

### Submodule BAT-02: Battery Component Integration & Simulation UI `[x]` (100.0% — 4/4)
- [x] **BAT-02-001: Battery Builder, Component Export & Charge/Discharge Graphs** `(100.0% — 4/4)`
  - [x] `BAT-02-001-01`: Implement `ShunyaBatteryController` connecting `BatteryDesignSpec`, material/object selection per region, parameter derivation, standalone charge/discharge simulation, and export as a `ComponentKind::Battery` into the Component Library.
  - [x] `BAT-02-001-02`: Build `BatteryBuilderView.qml` displaying the 7 battery layers/regions with assigned materials, geometry inputs, compatibility diagnostic checklist, derived electrical/thermal specs, and fidelity disclosure banner.
  - [x] `BAT-02-001-03`: Build interactive charge/discharge testbench view plotting multi-series color-coded graphs (Terminal Voltage V, Current A, Power W, Temperature °C, SOC %) using `ScientificPlot`, plus active safety warning banners when limits are exceeded.
  - [x] `BAT-02-001-04`: Ensure `Battery` components generated here can be placed directly into the Circuit Editor and simulated dynamically during transient circuit runs.

**Testing Requirements:**
- Unit tests verifying Li-ion battery assembly (LiFePO4/LiCoO2 cathode + Graphite anode + Cu/Al collectors + insulating separator) computes positive OCV, valid capacity, valid internal $R_0$, fails validation if separator is replaced with Copper (internal short), simulates CC discharge + CC charge, and triggers thermal warning at > 353 K / runaway onset > 393 K.

**Acceptance Criteria:**
- Users can build a lithium-based battery from elements → materials → objects → battery assembly, validate it, run charge/discharge curves with colored graphs, and insert it as a live Battery component in circuits.

---

## 11. Circuit Construction Workspace (`CIRUI`)

**Module Objective:** Build a complete, interactive schematic/circuit editor allowing users to place standard and object-generated components, wire terminals with junctions, edit properties, undo/redo, zoom/pan, validate topology, and save/load/duplicate circuits.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (9 / 9 subtasks)  
**Dependencies:** `OBJCMP`, `BAT`

### Submodule CIRUI-01: Interactive Circuit Canvas & Graph Model `[x]` (100.0% — 5/5)
- [x] **CIRUI-01-001: Interactive Circuit Document, Manipulation & Undo/Redo** `(100.0% — 5/5)`
  - [x] `CIRUI-01-001-01`: Create `InteractiveCircuitDocument` bridging `pcb::SchematicDocument` and `core::domain::CircuitNetlist` with component placement `(x, y, rotation_deg)`, terminal hit-testing, orthogonal/45° wire routing, explicit junction creation, and net naming.
  - [x] `CIRUI-01-001-02`: Implement `CommandStack` undo/redo commands (`AddComponentCmd`, `MoveComponentCmd`, `RotateComponentCmd`, `DeleteComponentCmd`, `AddWireCmd`, `EditPropertyCmd`) wired to Ctrl+Z / Ctrl+Y and the ribbon undo history dropdown.
  - [x] `CIRUI-01-001-03`: Support drag-and-drop / click-to-place from Component Palette (standard components: DC Voltage Source, AC Voltage Source, Resistor, Capacitor, Inductor, Diode, LED, Switch, Battery, Ground, Voltmeter, Ammeter, Wattmeter/Probe + user's Object-Generated Components and Batteries).
  - [x] `CIRUI-01-001-04`: Implement canvas zoom/pan, grid snapping, multi-selection, component rotation (90°), deletion (removing dangling wires cleanly), and inline component property editing via `PropertyInspector`.
  - [x] `CIRUI-01-001-05`: Support Save, Load, and Duplicate Circuit actions within the project.

### Submodule CIRUI-02: Circuit Validation & Visual Feedback `[x]` (100.0% — 4/4)
- [x] **CIRUI-02-001: Pre-Simulation Circuit Topology & ERC Diagnostics** `(100.0% — 4/4)`
  - [x] `CIRUI-02-001-01`: Wire `CircuitDiagnostics::inspect_circuit` and `pcb::ErcEngine` into the Circuit Editor to detect missing Ground (`"0"`), floating/unconnected terminals, shorted voltage sources/batteries, open circuits, and invalid parameter values ($\le 0$ resistance/capacitance).
  - [x] `CIRUI-02-001-02`: Render clear IEEE/IEC vector schematic symbols for every supported component (DC/AC sources, R, C, L, Diode, LED with emission arrows, Switch with open/closed state, Battery multi-cell symbol, Ground, Voltmeter/Ammeter), replacing generic boxes in `SchematicView.qml`.
  - [x] `CIRUI-02-001-03`: Display visual error/warning markers directly on offending terminals/components and list actionable remediation steps in an integrated diagnostics panel.
  - [x] `CIRUI-02-001-04`: Highlight live node voltages, branch currents, switch toggle interaction, and LED brightness/illumination state during simulation.

**Testing Requirements:**
- Tests for placing, moving, rotating, wiring, creating junctions, undoing/redoing every edit, duplicating circuits, and detecting floating nodes / missing ground / voltage source shorts.

**Acceptance Criteria:**
- Users can construct, edit, validate, undo/redo, save, load, and duplicate multi-component circuits interactively using both built-in and material-derived components.

---

## 12. Circuit Simulation Engine (`CIRSIM`)

**Module Objective:** Provide accurate DC, AC, and transient circuit simulation supporting KCL/KVL, series/parallel/mixed topologies, capacitors, inductors, diodes, LEDs, switches, measurement instruments, and live coupled battery components with meaningful error reporting.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (9 / 9 subtasks)  
**Dependencies:** `CIRUI`, `BAT`

### Submodule CIRSIM-01: Extended MNA & Component Models `[x]` (100.0% — 5/5)
- [x] **CIRSIM-01-001: Complete Circuit Component & Coupled Battery Solver Integration** `(100.0% — 5/5)`
  - [x] `CIRSIM-01-001-01`: Extend `CircuitNetlist` / `pcb::SpiceSimulationEngine` to natively support `LED` (calibrated forward voltage $V_f \approx 1.8$–$3.3$ V + Shockley exponential + power/current rating check), `Switch` (time-controlled or interactive toggle between $R_{\text{on}} = 10^{-3}\ \Omega$ and $R_{\text{off}} = 10^{9}\ \Omega$), and `MeasurementInstrument` (ideal Voltmeter $R = 10^{12}\ \Omega$, ideal Ammeter $V_{\text{sense}} = 0$ V branch current).
  - [x] `CIRSIM-01-001-02`: Couple `Battery` circuit components into the MNA transient loop: at each transient step, evaluate battery $V_{\text{oc}}(\text{SOC}) - V_{RC,1} - V_{RC,2}$ + internal $R_0(T)$ in the MNA stamp, solve exact circuit node voltages & battery terminal current $I_{\text{bat}}$, and advance `BatteryCellRuntime` thermal/SOC state by $\Delta t$.
  - [x] `CIRSIM-01-001-03`: Couple `PhysicalObject`-derived `Resistor`/`ConductorWire`/`HeatingElement` components so Joule heating $P = I^2 R(T)$ updates object temperature $T$ and temperature-dependent resistance $R(T)$ when electro-thermal coupling is active.
  - [x] `CIRSIM-01-001-04`: Verify KCL ($\sum I_{\text{node}} \approx 0$) and KVL ($\sum V_{\text{loop}} \approx 0$) conservation residuals on every solved DC and transient step, reporting residuals in telemetry.
  - [x] `CIRSIM-01-001-05`: Provide explicit, human-readable simulation error messages for singular matrices, floating subcircuits, voltage source loops, or non-convergence instead of silent NaN outputs.

### Submodule CIRSIM-02: Real-Time Simulation Execution & Telemetry Graphs `[x]` (100.0% — 4/4)
- [x] **CIRSIM-02-001: Interactive Run/Step/Transient Analysis & Multi-Channel Plotting** `(100.0% — 4/4)`
  - [x] `CIRSIM-02-001-01`: Replace `ShunyaStudioController`'s clock-only `onTimerTick` with real execution of `DcOperatingPointSolver`, `TransientSolver`, and coupled `BatteryCellRuntime` / electro-thermal stepping.
  - [x] `CIRSIM-02-001-02`: Stream real simulated node voltages, branch currents, resistances, powers, and component temperatures to `ShunyaOscilloscopeController` and `ScientificPlot` (removing hardcoded `loadDemoCapture` / `damped_sine` defaults when a circuit is active).
  - [x] `CIRSIM-02-001-03`: Verify capacitor charging/discharging ($\tau = RC$), inductor transient step ($\tau = L/R$), series/parallel/mixed resistor networks, diode/LED rectification, and battery-powered load transients.
  - [x] `CIRSIM-02-001-04`: Surface power dissipation, component over-limit warnings (exceeding rated current/power/breakdown/melting temperature), and KCL/KVL verification summaries in the Simulation workspace.

**Testing Requirements:**
- Analytical verification tests for series/parallel/mixed dividers, RC charge/discharge ($V_c(t) = V_0(1 - e^{-t/RC})$), RL current rise, LED forward drop, Switch toggle transient, and Battery discharging into a resistive load + LED inside MNA transient solver.

**Acceptance Criteria:**
- Constructing any valid circuit in the UI and clicking Run/Step produces physically accurate voltage, current, resistance, power, and temperature curves, while invalid circuits report actionable errors.

---

## 13. 2D Object Editor (`ED2D`)

**Module Objective:** Deliver a complete interactive 2D modeling editor integrated into the application workflow supporting shapes, paths, dimensions, layers, grouping, transforms, material assignment, and project persistence/reuse.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (8 / 8 subtasks)  
**Dependencies:** `ARCH`, `OBJMAT`

### Submodule ED2D-01: Complete 2D Drawing, Editing & Grouping Tools `[x]` (100.0% — 4/4)
- [x] **ED2D-01-001: Extend `ShunyaSketchCanvas2D` Beyond Line/Circle/Rect** `(100.0% — 4/4)`
  - [x] `ED2D-01-001-01`: Add Polygon, Polyline/Path, and Custom Bézier/Closed-Profile creation tools to `ShunyaSketchCanvas2D` and `SketchToolbar.qml` backed by `cad2d::Curve2D` and `cad2d::BezierSpline`.
  - [x] `ED2D-01-001-02`: Implement interactive selection, move, rotate, scale, duplicate, and exact numeric dimension editing (width, height, radius, coordinates, angle) via `PropertyInspector` and `CommandStack` undo/redo.
  - [x] `ED2D-01-001-03`: Wire `LayerPanel.qml` and `BlockLibraryPanel.qml` (`ShunyaLayerPanelController`) to support creating/deleting/reordering layers and grouping/ungrouping 2D entities (`cad2d::BlockManager`).
  - [x] `ED2D-01-001-04`: Attach material assignments (`material_id`) and thickness ($z$-depth) to closed 2D profiles so users can compute 2D/extruded area, volume, and mass directly.

### Submodule ED2D-02: 2D Reuse, Export & Persistence `[x]` (100.0% — 4/4)
- [x] **ED2D-02-001: Save/Reuse 2D Objects & Fix File Menu Import/Export** `(100.0% — 4/4)`
  - [x] `ED2D-02-001-01`: Implement saving and instantiating reusable 2D objects/blocks within the project's object catalog and converting closed 2D profiles to `PhysicalObject` or 3D extrusions.
  - [x] `ED2D-02-001-02`: Connect `MainMenuBar.qml` DXF and SVG import/export actions to real `FileDialog` pickers and `cad2d::DxfIo` / `cad2d::SvgIo` (replacing the hardcoded `"sample.dxf"` / `"schematic.svg"` stubs in `MainMenuBar.qml`).
  - [x] `ED2D-02-001-03`: Render material fills/patterns, selection handles, and live dimension annotations cleanly on `ShunyaSketchCanvas2D`.
  - [x] `ED2D-02-001-04`: Persist all 2D sketches, layers, blocks, and material assignments in project save/load.

**Testing Requirements:**
- Unit and controller tests for creating rectangle/circle/polygon/path, move/rotate/scale/duplicate, group/ungroup, layer visibility/locking, DXF/SVG round-trip, and material mass calculation.

**Acceptance Criteria:**
- Users can draw, edit, group, layer, assign materials to, save, and reuse 2D objects inside the `2D Editor` workspace.

---

## 14. 3D Object Editor (`ED3D`)

**Module Objective:** Deliver an interactive 3D modeling workspace supporting primitives, dimensions, transforms, material/texture assignment, grouping/assemblies, geometry inspection, and reuse, backed by `cad3d` meshes and a real 3D viewport.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (8 / 8 subtasks)  
**Dependencies:** `ARCH`, `OBJMAT`, `OBJGEO`

### Submodule ED3D-01: Interactive 3D Viewport & Primitives Engine `[x]` (100.0% — 4/4)
- [x] **ED3D-01-001: Real 3D Scene Renderer & Primitive Modeling** `(100.0% — 4/4)`
  - [x] `ED3D-01-001-01`: Replace the empty 2D background grid in `Main.qml` (`vpGridCanvas` + `GizmoOverlay`) with a real interactive 3D viewport item (`ShunyaViewport3DItem` rendering `cad3d::SolidMesh3D` primitives via Qt Quick 3D / QSG / software RHI rasterizer with depth sorting, PBR/shaded normals, wireframe/solid modes, and `ViewCube` / orbit / pan / zoom).
  - [x] `ED3D-01-001-02`: Implement parametric 3D primitive generators (`Cube/Box`, `Sphere`, `Cylinder`, `Cone`, `Torus/Tube`, and `Extrude from 2D`) producing watertight `SolidMesh3D` meshes with exact analytical/mesh volume, surface area, and bounding box.
  - [x] `ED3D-01-001-03`: Wire `GizmoOverlay` and `ShunyaViewportController` to actually move, rotate, and scale the selected 3D object in `SceneGraph` (not just a floating `target_position_` in `ShunyaViewportController`), with `CommandStack` undo/redo.
  - [x] `ED3D-01-001-04`: Support Boolean/grouping/assembly operations (combine/group objects into parent-child `SceneGraph` assemblies, CSG union/subtract/intersect where watertight).

### Submodule ED3D-02: 3D Material/Appearance Binding & Inspection `[x]` (100.0% — 4/4)
- [x] **ED3D-02-001: Material Assignment, Geometry Inspection & Model Reuse** `(100.0% — 4/4)`
  - [x] `ED3D-02-001-01`: Apply assigned `MaterialDefinition` optical/surface properties (element/material color, metallic vs dielectric roughness/reflectance via `cad3d::PbrMaterial`) to 3D objects in the viewport.
  - [x] `ED3D-02-001-02`: Implement Geometry & Physical Inspector panel showing vertex/triangle count, manifold status, bounding dimensions, volume, surface area, density, mass, center of mass, and inertia tensor.
  - [x] `ED3D-02-001-03`: Support saving/reusing 3D models and assemblies in the project library and importing/exporting OBJ/STL meshes via real file dialogs.
  - [x] `ED3D-02-001-04`: Persist 3D scene hierarchy, transforms, primitive parameters, meshes, and material bindings in project save/load.

**Testing Requirements:**
- Unit tests verifying watertight manifold properties and exact volume/surface area for Cube, Sphere, Cylinder, Cone; gizmo transform undo/redo; material-to-mass updates; and 3D scene serialization round-trip.

**Acceptance Criteria:**
- Users can create, transform, group, assign materials to, inspect, save, and reuse 3D primitives and assemblies in a live 3D viewport.

---

## 15. Physics and Environmental Simulation (`PHYSIM`)

**Module Objective:** Connect 2D and 3D material objects to real physics and environmental solvers (gravity, rigid-body motion, collision, force, heat transfer, thermal expansion, simplified deformation, and environmental conditions) rather than visual-only animations.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (8 / 8 subtasks)  
**Dependencies:** `MATBEH`, `ED2D`, `ED3D`, `CIRSIM`

### Submodule PHYSIM-01: Coupled Multi-Body, Thermal & Environmental Solvers `[x]` (100.0% — 4/4)
- [x] **PHYSIM-01-001: Object-Driven Physics & Environmental Simulation Runtime** `(100.0% — 4/4)`
  - [x] `PHYSIM-01-001-01`: Implement `PhysicsSimulationRuntime` synchronizing `PhysicalObject` instances with `phy::RigidBodyKinematics` / `mechanics::MechanicsWorld` (gravity vector, mass & inertia from material density, forces, impulses, and `phy::CollisionEngine` / `GjkEpa` contact collision with restitution & ground plane).
  - [x] `PHYSIM-01-001-02`: Couple `thermal::ThermalNetwork` / `phy::ThermalSolver3D` to project objects using material $k(T)$, $c_p(T)$, $\rho$, convection (`env::AmbientConvection`), radiation, and Joule heat sources from connected circuits/batteries.
  - [x] `PHYSIM-01-001-03`: Implement linear thermal expansion ($\Delta L = \alpha L_0 \Delta T$) and elastic stress/deformation preview ($\sigma = E \epsilon$, $\delta = F L / (E A)$) using object geometry and material Young's modulus / yield strength, flagging plastic yield or melting limits when exceeded.
  - [x] `PHYSIM-01-001-04`: Connect `env::AtmosphericModel` and `env::ClimaticChamber` (ambient temperature, pressure, gravity, convection) as configurable project simulation environment conditions, wiring `ChamberControlPanel.qml` to real C++ controls.

### Submodule PHYSIM-02: Real-Time Viewport & Telemetry Synchronization `[x]` (100.0% — 4/4)
- [x] **PHYSIM-02-001: Live 2D/3D Physics Playback & Field Visualization** `(100.0% — 4/4)`
  - [x] `PHYSIM-02-001-01`: Update 2D/3D object positions, orientations, thermal expansion dimensions, and temperature color overlays during active simulation ticks while keeping solver state strictly immutable from rendering.
  - [x] `PHYSIM-02-001-02`: Feed live mechanical (position, velocity, kinetic energy, contact force), thermal (object temperatures, heat flux), and environmental telemetry into `ScientificPlot` and `ColormapLegend`.
  - [x] `PHYSIM-02-001-03`: Support electro-thermo-mechanical co-simulation via `integ::MasterCoSimulationExecutive` so circuit current heats a physical object, raises its temperature, expands its length, and triggers temperature/phase alerts.
  - [x] `PHYSIM-02-001-04`: Disclose active fidelity tier (`L1`/`L2`/`L3`) and unsupported physics warnings (e.g., if an object lacks Young's modulus for deformation).

**Testing Requirements:**
- Unit & integration tests verifying gravity + bounce collision with conservation bounds, thermal conduction between two material-assigned objects (Cu vs Fe), thermal expansion ($\Delta L / L_0 = \alpha \Delta T$), and electro-thermal Joule heating of a resistor object.

**Acceptance Criteria:**
- Running a simulation drives real rigid-body motion, collisions, heat transfer, thermal expansion, and electrical coupling directly from material properties and geometry.

---

## 16. UI Theme Redesign (`THEME`)

**Module Objective:** Replace the existing blue-accented UI with a crisp, professional monochrome black-and-white visual system across both Light and Dark modes, while preserving clear, distinguishable scientific colors for graphs, simulation visualizations, and material/periodic indicators.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (8 / 8 subtasks)  
**Dependencies:** `ARCH`

### Submodule THEME-01: Centralized Monochrome Design Tokens `[x]` (100.0% — 4/4)
- [x] **THEME-01-001: Create `Theme.qml` Singleton & Eliminate Blue UI Accents** `(100.0% — 4/4)`
  - [x] `THEME-01-001-01`: Create a QML `pragma Singleton` `Theme.qml` (and optional C++ `ThemeManager`) supporting **Light Mode** (primary background `#FFFFFF`/`#FAFAFA`, surface `#F5F5F5`, elevated `#EFEFEF`, border `#D4D4D4`/`#E5E5E5`, primary text `#0A0A0A`, muted text `#525252`, primary button `#0A0A0A` bg / `#FFFFFF` text) and **Dark Mode** (primary background `#0A0A0A`/`#111111`, surface `#171717`, elevated `#202020`, border `#2E2E2E`/`#3A3A3A`, primary text `#FAFAFA`, muted text `#A3A3A3`, primary button `#FAFAFA` bg / `#0A0A0A` text).
  - [x] `THEME-01-001-02`: Add a persistent Light/Dark mode toggle in Settings / top bar (`Theme.isDarkMode`) and verify dynamic live switching without restart.
  - [x] `THEME-01-001-03`: Audit and replace all 138+ hardcoded hex colors across all 48+ QML files (`#00b4d8`, `#007acc`, `#3b82f6`, `#38bdf8`, `#58a6ff`, `#61afef`, `#2c3e50`, `#0f172a`, `#1e293b`, `#094771`, etc.) with `Theme.*` monochrome tokens.
  - [x] `THEME-01-001-04`: Restyle all buttons (primary solid contrast, secondary transparent/neutral border), inputs, checkboxes, comboboxes, sliders, scrollbars, cards, tabs, and dialogs using clean monochrome active/hover/focus/disabled states.

### Submodule THEME-02: Scientific Visualization Color Palette Preservation `[x]` (100.0% — 4/4)
- [x] **THEME-02-001: Dedicated Scientific Graph & Indicator Color System** `(100.0% — 4/4)`
  - [x] `THEME-02-001-01`: Define a colorblind-friendly, high-contrast scientific dataset palette in `Theme.chartColors` assigned deterministically by physical quantity (e.g., Voltage `#2563EB`, Current `#EA580C`, Power `#9333EA`, Temperature `#DC2626`, Density `#0D9488`, Pressure `#CA8A04`, SOC/Capacity `#16A34A`) readable on both white and near-black plot backgrounds.
  - [x] `THEME-02-001-02`: Upgrade `ScientificPlot.qml` and `OscilloscopePanel.qml` with multi-series legends, neutral monochrome grid lines, crisp axis labels, and interactive hover crosshair tooltips.
  - [x] `THEME-02-001-03`: Preserve semantic colors strictly for scientific data (periodic table heatmaps/family categories, thermal colormaps, schematic/PCB layers, phase/safety warning badges).
  - [x] `THEME-02-001-04`: Verify WCAG contrast ratios ($\ge 4.5:1$ body text, $\ge 3:1$ UI borders/controls) in both Light and Dark themes.

**Testing Requirements:**
- Automated QML token scan asserting zero legacy blue UI chrome hex codes remain outside scientific chart/colormap palettes; visual inspection across Light & Dark modes.

**Acceptance Criteria:**
- The entire application shell, panels, buttons, and forms use a consistent black-and-white monochrome design system in both Light and Dark modes, while graphs and scientific plots retain clear multi-color differentiation.

---

## 17. Icon System Update (`ICON`)

**Module Objective:** Replace all emoji/unicode-symbol hacks and inconsistent icon usages with a unified, crisp monochrome SVG vector icon system that adapts seamlessly to Light/Dark modes and active/inactive/hover/disabled states.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (6 / 6 subtasks)  
**Dependencies:** `THEME`

### Submodule ICON-01: Unified Vector SVG Icon Library `[x]` (100.0% — 3/3)
- [x] **ICON-01-001: Author SVG Icon Assets & `ShunyaIcon.qml` Component** `(100.0% — 3/3)`
  - [x] `ICON-01-001-01`: Create a consistent 24×24 viewBox, 1.75px stroke-width monochrome SVG icon set (embedded via Qt Resource `.qrc`) covering all navigation workspaces (Elements, Materials, Mixing Lab, Objects, Components, Battery, Circuits, 2D Editor, 3D Editor, Simulation, Settings), file/edit actions (New, Open, Save, Undo, Redo, Cut, Copy, Paste, Search, Close, Float, Pin), geometry tools (Select, Line, Circle, Rect, Polygon, Path, Move, Rotate, Scale, Extrude, Cube, Sphere, Cylinder, Cone), circuit tools (Wire, Junction, Resistor, Capacitor, Inductor, Source, Battery, Diode, LED, Switch, Ground, Probe), simulation controls (Run, Pause, Stop, Step, Reset), and status indicators (Info, Warning, Error, Success, Chevron).
  - [x] `ICON-01-001-02`: Build `ShunyaIcon.qml` wrapping Qt `Image` / `Shape` / `ColorOverlay` (or templated SVG stroke colorization) supporting standardized sizes (`xs: 12`, `sm: 16`, `md: 20`, `lg: 24`), active/inactive/hover/disabled tinting from `Theme`, and crisp HiDPI rendering.
  - [x] `ICON-01-001-03`: Add automated test / CI script scanning all `.qml` and `.cpp` UI files to ensure **zero emojis or pictographic dingbats** exist anywhere in the UI.

### Submodule ICON-02: Application-Wide Icon Replacement `[x]` (100.0% — 3/3)
- [x] **ICON-02-001: Replace Every Emoji and Text Glyph Across All UI Views** `(100.0% — 3/3)`
  - [x] `ICON-02-001-01`: Replace all 16+ audited emoji/symbol occurrences (`💾`, `🔍`, `📂`, `📄`, `🧊`, `⚡`, `📈`, `⚙`, `🗗`, `🗖`, `✦`, `▶`, `⏸`, `⏹`, `⏭`, `↺`, `↻`, `✕`, `●`, `○`, `➔`) in `Main.qml`, `RibbonBar.qml`, `RibbonGroup.qml`, `CommandPalette.qml`, `PropertyInspector.qml`, `DockContainer.qml`, and `UndoHistoryDropdown.qml` with `ShunyaIcon`.
  - [x] `ICON-02-001-02`: Add consistent `ShunyaIcon` entries to all navigation items, menus, toolbars, notifications, empty states, and status bars.
  - [x] `ICON-02-001-03`: Verify visual clarity, stroke consistency, alignment, and contrast of every icon in both Light and Dark modes.

**Testing Requirements:**
- Automated regex test asserting 0 emoji/dingbat codepoints in `shunya/**/*.{qml,cpp,hpp}` and verifying every referenced SVG resource loads cleanly (`status === Image.Ready`).

**Acceptance Criteria:**
- Zero emojis appear anywhere in the application; every interactive area uses aligned, crisp monochrome SVG icons.

---

## 18. Typography Update (`TYPO`)

**Module Objective:** Standardize typography across all QML views, tables, forms, menus, and charts using a modern UI font + monospace technical data font with well-defined weights, sizes, and line heights.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (6 / 6 subtasks)  
**Dependencies:** `THEME`

### Submodule TYPO-01: Typography Scale & Font Integration `[x]` (100.0% — 3/3)
- [x] **TYPO-01-001: Embed Modern UI & Technical Monospace Fonts and Scale Tokens** `(100.0% — 3/3)`
  - [x] `TYPO-01-001-01`: Bundle/load a clean modern sans-serif UI font (e.g., Inter / Roboto / Segoe UI Variable fallback) and crisp tabular monospace font (e.g., JetBrains Mono / Consolas fallback) via `QFontDatabase::addApplicationFont` at startup and set the application default font in `main.cpp`.
  - [x] `TYPO-01-001-02`: Define semantic typography tokens in `Theme.qml`: `fontDisplay` (20px/600), `fontH1` (16px/600), `fontH2` (14px/600), `fontH3` (12px/600), `fontBody` (13px/400, lineHeight 1.4), `fontLabel` (11px/500), `fontCaption` (10px/400), and `fontMono` (12px/400, tabular figures for numerical/telemetry/formula data).
  - [x] `TYPO-01-001-03`: Configure consistent `lineHeight`, `elide`, and kerning policies for tables, inspector rows, menu items, and plot labels.

### Submodule TYPO-02: Application-Wide Typography Normalization `[x]` (100.0% — 3/3)
- [x] **TYPO-02-001: Audit & Normalize All QML Text Elements** `(100.0% — 3/3)`
  - [x] `TYPO-02-001-01`: Replace ad-hoc `font.pixelSize` (ranging arbitrarily from 8px to 18px) and raw `"Monospace"` / `"sans-serif"` overrides across all QML components with `Theme` typography tokens.
  - [x] `TYPO-02-001-02`: Apply tabular monospace typography to all numerical readouts (properties, coordinates, element weights, circuit voltages/currents, simulation clocks, logs).
  - [x] `TYPO-02-001-03`: Verify text legibility, zero truncation/clipping, and consistent visual hierarchy across 1280×800 and 1920×1080 viewports.

**Acceptance Criteria:**
- Uniform UI and monospace fonts, consistent type scale, and clean readability in every panel, table, form, menu, and graph.

---

## 19. Menu Item UI Redesign (`MENU`)

**Module Objective:** Redesign all application menu bars, dropdown menus, context menus, and popups so they match the monochrome theme, have consistent padding/icon alignment/keyboard navigation/focus indicators, and ensure every menu item executes a real working action.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (6 / 6 subtasks)  
**Dependencies:** `THEME`, `ICON`, `TYPO`

### Submodule MENU-01: Styled Menu, Dropdown & Context Menu Primitives `[x]` (100.0% — 3/3)
- [x] **MENU-01-001: Unified Monochrome MenuBar, Menu, MenuItem & Submenu Styling** `(100.0% — 3/3)`
  - [x] `MENU-01-001-01`: Create custom styled `ShunyaMenuBar`, `ShunyaMenu`, `ShunyaMenuItem`, and `ShunyaMenuSeparator` components with consistent 32px item height, 12px horizontal padding, fixed 16px icon slot, aligned shortcut text column, clean 1px neutral border, and zero clipped/overflowing submenus.
  - [x] `MENU-01-001-02`: Implement distinct visual states for normal, hover, active/selected, disabled, and keyboard focus-visible rings (`Tab` / arrow keys / `Enter` / `Esc`).
  - [x] `MENU-01-001-03`: Add reusable right-click context menus (`ShunyaContextMenu`) for 2D/3D objects, components, circuit elements, and material rows (Edit, Duplicate, Assign Material, Convert to Component, Delete).

### Submodule MENU-02: Functional Menu Action Wiring `[x]` (100.0% — 3/3)
- [x] **MENU-02-001: Eliminate Placeholder/No-Op Menu Items in `MainMenuBar.qml`** `(100.0% — 3/3)`
  - [x] `MENU-02-001-01`: Wire `Edit` menu actions (`Undo`, `Redo`, `Cut`, `Copy`, `Paste`, `Preferences...`) to real `CommandStack`, clipboard/duplication actions, and the Project Settings / Preferences view (no log-only stubs).
  - [x] `MENU-02-001-02`: Wire `File → Import` and `File → Export` items to real `FileDialog` pickers and actual `cad2d` (DXF/SVG), `cad3d` (OBJ/STL), `pcb` (SPICE netlist), and CSV/JSON exporters (removing hardcoded `"sample.dxf"`, `"telemetry.csv"` placeholders).
  - [x] `MENU-02-001-03`: Update `View`, `Tools`, `Simulation`, and `Help` menus to reflect the new workflow navigation workspaces, Light/Dark theme toggle, and actual keyboard shortcuts.

**Testing Requirements:**
- Automated & manual verification of every menu item click, shortcut trigger, disabled state, submenu placement, and keyboard navigation path.

**Acceptance Criteria:**
- Zero dead or log-only menu items; every dropdown and context menu renders cleanly in Light and Dark modes with full keyboard support.

---

## 20. Application Navigation Improvements (`NAV`)

**Module Objective:** Redesign the application layout around the single-open-project engineering workflow (Elements → Materials → Mixing Laboratory → Objects → Components → Circuits → 2D Editor → 3D Editor → Simulation → Project Settings), explicitly **removing any multi-project or file-manager sidebar**.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (8 / 8 subtasks)  
**Dependencies:** `THEME`, `ICON`, `TYPO`, `MENU`

### Submodule NAV-01: Remove File-Manager Sidebar & Build Workflow Rail `[x]` (100.0% — 4/4)
- [x] **NAV-01-001: Streamlined Single-Project Navigation Architecture** `(100.0% — 4/4)`
  - [x] `NAV-01-001-01`: Remove the old hardcoded `PROJECT EXPLORER` left sidebar from `Main.qml` and ensure no file-manager or multi-project tree sidebar exists anywhere in the UI.
  - [x] `NAV-01-001-02`: Implement a compact, collapsible left/top **Workflow Navigation Rail** (`WorkflowNavRail.qml`) prioritizing the active workspace and presenting the 10 core engineering stages in logical order:
    1. `Elements`
    2. `Materials`
    3. `Mixing Laboratory`
    4. `Objects`
    5. `Components` (including Battery Builder sub-view or integrated entry)
    6. `Circuits`
    7. `2D Editor`
    8. `3D Editor`
    9. `Simulation`
    10. `Project Settings`
  - [x] `NAV-01-001-03`: Replace the disconnected 5-preset toolbar and redundant ribbon boilerplate with a single unified contextual header/toolbar bound cleanly to the active workflow stage, while preserving Command Palette (`Ctrl+K`), Undo/Redo, Theme Toggle, and Simulation controls.
  - [x] `NAV-01-001-04`: Wire every previously orphaned QML capability (`PeriodicTableWindow`, `AlloyComposer`/`MixingLab`, `SchematicView`, `SketchCanvas`, `3D Viewport`, `OscilloscopePanel`, `ScientificPlot`, `ChamberControlPanel`) into its corresponding workflow workspace so **zero QML views are unreachable**.

### Submodule NAV-02: Workspace Focus, Inspectors & Diagnostics Drawer `[x]` (100.0% — 4/4)
- [x] **NAV-02-001: Context-Aware Inspector & Non-Intrusive Diagnostics** `(100.0% — 4/4)`
  - [x] `NAV-02-001-01`: Make the right-hand `PropertyInspector` contextual (auto-showing selected Element / Mixture / Object / Component / Circuit Node / Simulation Settings properties, and collapsing cleanly when not needed to maximize canvas area).
  - [x] `NAV-02-001-02`: Add a compact single-project title/dirty-indicator pill in the header (`ProjectName *`) plus quick-switch breadcrumbs (e.g., `Objects → Copper_Wire → Resistor_R1`) showing end-to-end data lineage.
  - [x] `NAV-02-001-03`: Refine the bottom collapsible Diagnostics & Simulation Log drawer and status bar (monochrome styling, error/warning filter pills, clear button, export log).
  - [x] `NAV-02-001-04`: Build the `Project Settings` workspace (project name/author/description, fidelity policy, default ambient $T$/$P$/gravity, solver dt/tolerances, theme & units preferences, `elements.csv` source path).

**Testing Requirements:**
- Navigation reachability test verifying all 10 workflow workspaces open smoothly, preserve state when switching tabs, and contain zero file-manager/multi-project sidebars.

**Acceptance Criteria:**
- Clean, distraction-free workspace centered on the active engineering stage with single-click flow from Elements → Materials → Mixing Lab → Objects → Components → Circuits → 2D/3D → Simulation → Project Settings.

---

## 21. Data Persistence and Project Management (`PERSIST`)

**Module Objective:** Persist the complete single-project engineering graph—saved mixtures, custom materials, physical objects, generated components, battery designs, 2D sketches, 3D assemblies, circuits, and simulation settings—atomically in `.shunya` project files with dirty-state protection.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (8 / 8 subtasks)  
**Dependencies:** `ARCH`, `MIXLAB`, `OBJMAT`, `OBJCMP`, `BAT`, `CIRUI`, `ED2D`, `ED3D`

### Submodule PERSIST-01: Unified Project Bundle Schema (`v3.0`) `[x]` (100.0% — 4/4)
- [x] **PERSIST-01-001: Complete Serialization & Deserialization Round-Trip** `(100.0% — 4/4)`
  - [x] `PERSIST-01-001-01`: Extend `Project` / `EngineeringWorkspaceState` serialization (`engineering_pipeline.cpp`) to store and reload: project manifest & environment settings, `MixtureExperiment` recipes, promoted `MaterialDefinition` records, `PhysicalObject` + region assignments, `GeneratedComponentDefinition` + `BatteryDesignSpec` records, and `InteractiveCircuitDocument` netlists/layouts.
  - [x] `PERSIST-01-001-02`: Preserve UUID cross-references across save/load so loaded objects remain linked to their materials, components remain linked to their objects, and circuits remain linked to their components.
  - [x] `PERSIST-01-001-03`: Add schema version migration (`v0.1.0-dev` / Phase 2 projects → `v3.0.0`) via `ProjectMigrator` so older `.shunya` manifests still open safely without data loss.
  - [x] `PERSIST-01-001-04`: Implement atomic save (`.tmp` write + flush + rename), backup retention on overwrite, and clear error reporting on permission/disk/corrupt-JSON failures.

### Submodule PERSIST-02: Single-Project Lifecycle & Dirty State Guards `[x]` (100.0% — 4/4)
- [x] **PERSIST-02-001: Complete New/Open/Save/SaveAs/Close Lifecycle** `(100.0% — 4/4)`
  - [x] `PERSIST-02-001-01`: Ensure every edit across Mixing Lab, Objects, Components, Battery, Circuits, 2D, 3D, and Settings marks the active project dirty (`isProjectDirty == true`) and updates window title/status.
  - [x] `PERSIST-02-001-02`: Fix `UnsavedChangesGuardDialog.qml` and `ShunyaMenuBarController` so Save / Discard / Cancel flows execute the deferred pending action (`new`, `open`, `close`, `exit`) reliably.
  - [x] `PERSIST-02-001-03`: Support importing/exporting individual reusable assets (export/import Material JSON, Component JSON, Circuit netlist) into the current project.
  - [x] `PERSIST-02-001-04`: Add a built-in "Load Example: Lithium Battery Engineering Workflow" starter template in `NewProjectWizard.qml` that seeds or guides the user through the full workflow cleanly.

**Testing Requirements:**
- Round-trip persistence tests saving a complete multi-stage project (mixture + material + wire object + battery + circuit + 2D/3D objects) to disk, clearing memory, reopening, and verifying identical UUIDs, properties, and simulation results.

**Acceptance Criteria:**
- Saving and reopening a project restores 100% of engineering data, linkages, and simulation readiness without loss or broken UUIDs.

---

## 22. Automated Testing (`ATEST`)

**Module Objective:** Implement comprehensive unit, integration, edge-case, and automated Qt/QML UI verification suites covering every Phase 3 backend engine, controller, button, menu item, form, and canvas.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (8 / 8 subtasks)  
**Dependencies:** Modules `ELEMDB` through `PERSIST`

### Submodule ATEST-01: Core Engine & Edge-Case Unit Test Suites `[x]` (100.0% — 4/4)
- [x] **ATEST-01-001: Exhaustive Domain & Edge-Case Verification** `(100.0% — 4/4)`
  - [x] `ATEST-01-001-01`: Add unit tests for `elements.csv` ingestion (`test_p3_elements_csv.cpp`), natural element behavior (`test_p3_element_behavior.cpp`), and mixing lab & property calculation engine (`test_p3_mixing_lab.cpp`).
  - [x] `ATEST-01-001-02`: Add unit tests for element-to-object creation & reactive updates (`test_p3_objects.cpp`), object-to-component & battery compatibility/charge/discharge (`test_p3_components_battery.cpp`), and extended circuit/LED/Switch/Battery simulation (`test_p3_circuits.cpp`).
  - [x] `ATEST-01-001-03`: Add exhaustive edge-case tests: empty inputs, negative/NaN/Inf values, missing CSV properties, invalid compound stoichiometry (He+Au), zero-area/negative-dimension geometry, floating/unconnected circuit terminals, missing ground, voltage source short, extreme $T$ (1 K to 10,000 K) and $P$ ($10^{-3}$ Pa to $10^{9}$ Pa), large object/element datasets (1,000+ objects), and corrupt JSON/missing file error paths.
  - [x] `ATEST-01-001-04`: Ensure all 517 existing Phase 2 tests + all new Phase 3 tests pass with 0 failures and 0 compiler warnings under `-Wall -Wextra -Wpedantic`.

### Submodule ATEST-02: Automated Qt/QML Controller & UI Test Suite `[x]` (100.0% — 4/4)
- [x] **ATEST-02-001: Headless Qt Quick Test & Controller Verification (`appshunya_tests`)** `(100.0% — 4/4)`
  - [x] `ATEST-02-001-01`: Add a CTest-registered Qt Quick / C++ UI test target (`tests/ui/test_shunya_ui.cpp` or `Qt6::QuickTest` + `QSignalSpy`) running offscreen (`QT_QPA_PLATFORM=offscreen`) to instantiate `Main.qml` and all workflow views with zero QML warnings or binding loops.
  - [x] `ATEST-02-001-02`: Programmatically test every navigation item, menu action, toolbar button, dialog (`NewProjectWizard`, `UnsavedChangesGuardDialog`, `CommandPalette`), search/filter input, and theme switcher (Light ↔ Dark), verifying loading/success/error/disabled states and duplicate-click safety.
  - [x] `ATEST-02-001-03`: Programmatically test QML visual invariants: zero emoji codepoints, zero legacy blue UI chrome colors, consistent `Theme` typography tokens, and all `ShunyaIcon` SVG sources loaded.
  - [x] `ATEST-02-001-04`: Programmatically test interactive 2D canvas, 3D viewport, Circuit Editor, Mixing Lab, and Simulation controls via Qt events/controller slots.

**Acceptance Criteria:**
- Both `shunya_unit_tests` and automated Qt UI tests pass 100% via `ctest` with comprehensive edge-case coverage.

---

## 23. End-to-End Testing (`E2E`)

**Module Objective:** Verify the complete connected engineering workflow end-to-end—from raw chemical elements in `elements.csv` through material mixing, physical object assembly, battery/component conversion, circuit construction, multiphysics simulation, graphing, and project persistence.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (6 / 6 subtasks)  
**Dependencies:** `ATEST`

### Submodule E2E-01: Full Battery-From-Elements Workflow Verification `[x]` (100.0% — 3/3)
- [x] **E2E-01-001: Automated End-to-End Workflow Test (Section 9 Scenario)** `(100.0% — 3/3)`
  - [x] `E2E-01-001-01`: Implement an automated end-to-end test (`tests/unit/test_p3_e2e_battery_workflow.cpp` + UI workflow test) executing all 19 steps of the Section 9 Lithium Battery workflow: query Li, Fe, Cu, Al, C, O, P from `elements.csv` → mix electrode/electrolyte/alloy materials in Mixing Lab under $T$/$P$ → save & promote materials → create anode, cathode, separator, electrolyte, Cu/Al current collectors, and Al/composite casing objects → verify mass/density/conductivities → assemble battery & convert to `Battery` component + create Cu wire & resistive load components → place Battery, Switch, Copper Wire, Resistor, LED, Ground, and Voltage/Current probes in Circuit Editor → validate circuit → run transient discharge & charge simulation → assert voltage, current, power, SOC, and temperature curves + reactive recalculation when wire/battery material changes.
  - [x] `E2E-01-001-02`: Save the entire end-to-end project to disk, reload it into a fresh `EngineeringWorkspaceState`, re-run the simulation, and assert deterministic numerical equivalence.
  - [x] `E2E-01-001-03`: Run interactive visual/manual walkthrough of `appshunya.exe` in both Light and Dark modes, capturing and inspecting screenshots of all 10 workspaces, graphs, menus, icons, and battery simulation results.

### Submodule E2E-02: Secondary Cross-Domain Workflows `[x]` (100.0% — 3/3)
- [x] **E2E-02-001: Additional Connected Engineering Scenarios** `(100.0% — 3/3)`
  - [x] `E2E-02-001-01`: Verify End-to-End 2D Sketch → Extruded 3D Copper/Aluminum Busbar Object → Heating Element Component → Circuit Joule Heating → 3D Thermal Conduction & Thermal Expansion workflow.
  - [x] `E2E-02-001-02`: Verify End-to-End Negative/Failure Path Workflow: attempt invalid compound mixing, attempt converting insulator to low-R wire, attempt simulating battery with conductive separator, attempt running ungrounded circuit → confirm clear errors and zero crashes.
  - [x] `E2E-02-001-03`: Verify headless CLI (`shunya-cli.exe`) can load and simulate a Phase 3 project file containing materials, batteries, and circuits.

**Acceptance Criteria:**
- All 19 steps of the Battery-from-Elements workflow and secondary workflows pass deterministically in both headless tests and live desktop UI.

---

## 24. Performance and Stability (`STAB`)

**Module Objective:** Ensure responsive UI interaction (60 FPS viewport/plotting), fast startup and CSV loading, bounded memory usage on large object/circuit counts, and zero crashes or memory leaks.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (6 / 6 subtasks)  
**Dependencies:** `E2E`

### Submodule STAB-01: Runtime Performance & Memory Budgets `[x]` (100.0% — 3/3)
- [x] **STAB-01-001: Profiling & Stress Testing** `(100.0% — 3/3)`
  - [x] `STAB-01-001-01`: Benchmark `elements.csv` parsing + `ElementDatabase` indexing (< 20 ms cold load) and 1,000-material / 1,000-object reactive recalculation (< 50 ms).
  - [x] `STAB-01-001-02`: Benchmark transient circuit + battery + physics simulation stepping and lock-free/decimated plot updates (`ScientificPlot` with 100k+ points without UI thread stalls).
  - [x] `STAB-01-001-03`: Fix any discovered bottlenecks, duplicate allocations, or slow QML model resets (use incremental model updates instead of full resets during simulation ticks).

### Submodule STAB-02: Robustness, Error Recovery & Clean Shutdown `[x]` (100.0% — 3/3)
- [x] **STAB-02-001: Stress, Fuzz & Resource Safety Verification** `(100.0% — 3/3)`
  - [x] `STAB-02-001-01`: Run stress tests on rapid UI workspace switching, repeated simulation Start/Pause/Stop/Reset cycles, rapid undo/redo bursts, and malformed project/CSV file loading.
  - [x] `STAB-02-001-02`: Verify clean shutdown of `AppContext`, worker `ThreadPool`, simulation timers, and Qt resources with zero hanging background threads or uncaught exceptions.
  - [x] `STAB-02-001-03`: Verify Release build (`windows-ninja-release`) compiles cleanly with optimizations (`-O2`/`-O3`) and passes 100% of unit, UI, and E2E tests.

**Acceptance Criteria:**
- Cold startup + CSV load < 500 ms, interactive simulation & plotting stays smooth, and stress tests pass in both Debug and Release builds.

---

## 25. Final Verification and Documentation (`DOC`)

**Module Objective:** Complete final whole-system verification, update all roadmap statuses and technical documentation, and verify clean Git history across all Phase 3 deliverables.  
**Completion Status:** `[x]` Completed | **Progress:** `100.0%` (6 / 6 subtasks)  
**Dependencies:** `STAB`

### Submodule DOC-01: Final System & Visual Audit `[x]` (100.0% — 3/3)
- [x] **DOC-01-001: Comprehensive Acceptance Sign-Off** `(100.0% — 3/3)`
  - [x] `DOC-01-001-01`: Re-run full `ctest` suite (`shunya_unit_tests`, `appshunya` UI tests, E2E tests) and `shunya-cli --benchmark` after final edits with 0 failures and 0 warnings.
  - [x] `DOC-01-001-02`: Verify every item in Section 18 Quality Standards & Acceptance Criteria (elements CSV, mixing lab, objects, components, battery, circuits, 2D/3D, physics, B&W theme, SVG icons, typography, menus, no file-manager sidebar).
  - [x] `DOC-01-001-03`: Fix any discrepancies between Phase 2 roadmap milestone tables and actual repository state if needed, and verify `PROJECT_ROADMAP_PHASE_3.md` reflects 100% accurate leaf-derived percentages.

### Submodule DOC-02: Engineering Documentation & Repository Hygiene `[x]` (100.0% — 3/3)
- [x] **DOC-02-001: Scientific Model Disclosure & User/Developer Docs** `(100.0% — 3/3)`
  - [x] `DOC-02-001-01`: Document all `elements.csv` normalizations/anomalies, mixture calculation equations, electrochemical battery model assumptions (Fidelity L1/L2), and supported vs unsupported physics behaviors in `docs/phase3_scientific_models.md`.
  - [x] `DOC-02-001-02`: Update `README.md` / developer build & run instructions covering Qt 6.12+, `elements.csv` placement, CLI usage, and E2E workflow guide.
  - [x] `DOC-02-001-03`: Verify clean `git status`, properly scoped atomic commits per task, and zero untracked temporary artifacts.

**Acceptance Criteria:**
- All tests pass, all documentation matches implemented code, and `PROJECT_ROADMAP_PHASE_3.md` reports 100.0% verified completion.

---

*End of PROJECT_ROADMAP_PHASE_3.md — Living Master Engineering Roadmap*
