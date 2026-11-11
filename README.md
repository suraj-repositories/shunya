# Shunya — Chemistry-Based Material Engineering, Object Creation, Circuit Design & Simulation

Shunya is an integrated C++20 / Qt 6 material engineering, CAD/object modeling, circuit design, battery engineering, and multiphysics simulation workbench built on a zero-GUI-dependency scientific domain core (`core/base`, `core/domain`).

## End-to-End Phase 3 Workflow

**Chemical Elements (`data/elements.csv`) -> Natural Element Behavior -> Material Mixing Laboratory -> Physical Objects -> Functional Components -> Lithium Battery Engineering -> Interactive Circuit Design & Simulation -> 2D/3D Modeling & Multiphysics Simulation**

Key capabilities:
- **118-Element RFC-4180 Database (`ElementDatabase`)**: Preserves all 82 properties per element with data provenance, Excel/crystal-group normalization, and IUPAC Z=112–118 alias resolution.
- **Natural Element Behavior (`ElementBehaviorEngine`)**: Temperature/pressure phase evaluation, Clausius-Clapeyron boiling shift, thermal expansion, ideal-gas / thermally scaled transport properties, reactivity/corrosion/combustion warnings, and explicit handling of missing CSV fields.
- **Material Mixing Laboratory (`MaterialMixingLaboratory`)**: Six material formation scenarios (`PhysicalMixture`, `Alloy` with Hume-Rothery checks, `Solution`, `ChemicalCompound` with valence/stoichiometry validation, `LayeredMaterial`, `CompositeMaterial`) with temperature sweeps and promotion to reusable `MaterialRegistry` entries.
- **Object -> Component -> Battery -> Circuit Coupling (`shunya::eng`)**:
  - `PhysicalObjectManager`: Multi-region 2D/3D primitives with reactive mass/density/resistance recalculation on material edits.
  - `ComponentGenerator`: Qualification-enforced conversion into resistors, conductors, capacitors, inductors, switches, heating elements, and batteries.
  - `BatteryEngineer`: 7-region Lithium-ion cell builder (Cathode, Anode, Electrolyte, Separator, Cu/Al Collectors, Casing/Terminals) with Faraday + Dual-RC Thevenin ECM + lumped thermal simulation (`L2` fidelity disclosure).
  - `InteractiveCircuitDocument` & `CircuitSimulationRunner`: Undo/redo schematic editor, spatial Union-Find connectivity, ERC checks, DC Nodal MNA, and transient analysis (RC, RL, LED/Diode, Battery discharge).
  - `PhysicsSimulationRuntime`: Symplectic rigid-body dynamics, contact resolution, convective thermal balance, and thermal strain feedback.
- **Desktop Workbench (`shunya` / Qt 6 QML)**: Black-and-white monochrome Light & Dark themes, custom SVG vector icons (`shunya/icons/`), live 2D vector canvas & 3D mesh viewport, and multi-color scientific plotting.

## Repository Layout

- `core/base/` — Foundation types (`Uuid`, `Result`, `Error`, units, math, thread pool, config).
- `core/domain/` — Pure C++20 scientific kernel (`matdb`, `mat`, `eng`, `cad2d`, `cad3d`, `pcb`, `phy`, `chem`, `env`).
- `apps/cli/` — Headless CLI (`shunya-cli`) supporting benchmarks and `.shunya3.json` project execution.
- `shunya/` — Qt 6 Quick/QML desktop workbench (`appshunya`).
- `data/elements.csv` — Canonical 118-element chemical property dataset.
- `docs/phase3_scientific_models.md` — Detailed scientific model & limitation disclosure.
- `tests/` — Automated C++ unit, stress, E2E, and Qt UI verification suites.

## Building & Running Tests

Configure & build with Ninja and Qt 6 (Windows MinGW 64-bit example):

```powershell
cmake -B build/windows-ninja-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/windows-ninja-release
ctest --test-dir build/windows-ninja-release --output-on-failure
```

Headless CLI demo:

```powershell
./build/windows-ninja-release/apps/cli/shunya-cli --benchmark --seed-battery-demo
```
