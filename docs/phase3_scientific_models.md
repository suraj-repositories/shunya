# Shunya Phase 3 — Scientific Models, Provenance & Engineering Workflow

This document records the implemented scientific models, source data provenance, approximation levels, and known modeling boundaries in Shunya Phase 3 (`Chemical Elements -> Mixed Materials -> Physical Objects -> Components -> Lithium Battery -> Circuits -> 2D/3D Multiphysics`).

---

## 1. Chemical Element Database (`data/elements.csv` -> `ElementDatabase`)

- **Source File**: `data/elements.csv` (118 elements, 82 columns per record, loaded via RFC-4180 CSV parser in [`ElementDatabase`](../core/domain/include/shunya/matdb/element_database.hpp)).
- **Data Normalization Preserved Without Fabricating Missing Values**:
  - Excel-corrupted string fractions in `IonizationEnergies` / numeric fields (e.g., `"07-May"` -> `7/5 = 1.4`, `"05-Mar"` -> `5/3`) and multiline crystal space-group tokens (`"Fm_\n3m"` -> `"Fm-3m"`) are normalized cleanly while retaining `raw_fields` string access for every column.
  - Legacy element symbols (`Uut`/`Uup`/`Uus`/`Uuo` -> IUPAC `Nh`/`Fl`/`Mc`/`Lv`/`Ts`/`Og`) resolve via alias lookup.
  - Missing fields remain `std::nullopt` (`PropertyProvenance` tracks Measured CSV vs Estimated vs Missing — missing values are never fabricated).
- **Supported CSV Properties (82 Columns)**:
  - Identity & Periodic: `AtomicNumber`, `Element`, `Symbol`, `AtomicMass`, `Period`, `Group`, `Block`, `Type`, `Category`, `Valence`, `ElectronConfiguration`
  - Physical & Thermodynamic: `Density`, `Phase`, `MeltingPoint`, `BoilingPoint`, `AbsoluteMeltingPoint`, `AbsoluteBoilingPoint`, `CriticalPressure`, `CriticalTemperature`, `FusionHeat`, `VaporizationHeat`, `SpecificHeat`, `AdiabaticIndex`, `CombustionHeat`, `ThermalConductivity`, `ThermalExpansion`
  - Electrical, Magnetic, Elastic & Crystallography: `ElectricalConductivity`, `ElectricalType`, `Resistivity`, `SuperconductingPoint`, `MagneticType`, `MagneticSusceptibilityMass`, `MagneticSusceptibilityVolume`, `YoungModulus`, `BulkModulus`, `ShearModulus`, `BrinellHardness`, `MohsHardness`, `VickersHardness`, `SpeedOfSound`, `SpaceGroupName`, `SpaceGroupNumber`, `LatticeConstants`
  - Atomic, Optical, Nuclear & Chemical: `AtomicRadius`, `CovalentRadius`, `VanDerWaalsRadius`, `Electronegativity`, `FirstIonization`, `IonizationEnergies`, `RefractiveIndex`, `PoissonRatio`, `AbundanceEarthCrust`, `AbundanceHuman`, `AbundanceUniverse`, `DecayMode`, `HalfLife`, `Isotopes`, `Radioactive`, `Stable`

---

## 2. Natural Element Behavior (`ElementBehaviorEngine`)

Evaluates temperature ($T$) and pressure ($P$) dependent element state via [`ElementBehaviorEngine`](../core/domain/include/shunya/mat/element_behavior.hpp):

- **Phase & Transition Shifts**:
  - Reference `MeltingPoint` / `BoilingPoint` from `elements.csv`.
  - Clausius-Clapeyron boiling point shift using `VaporizationHeat` when present:
    $$\frac{1}{T_b(P)} = \frac{1}{T_{b,0}} - \frac{R \ln(P / P_0)}{\Delta H_{\text{vap}}}$$
    Flagged with `EpistemicStatus::Approximate` and `clamped` warnings below critical bounds (`CriticalTemperature`, `CriticalPressure`) when present.
- **Transport & Thermal Properties**:
  - Linear thermal expansion $\Delta L / L = \alpha \,(T - T_0)$ when `ThermalExpansion` is available; linear thermal density adjustment $\rho(T) \approx \rho_0 / (1 + 3\alpha \Delta T)$.
  - Ideal-gas law density $\rho = P M / (R T)$ when evaluated in gas phase; `AdiabaticIndex` preserved when available.
  - Electrical conductivity temperature scaling $\sigma(T) = \sigma_0 / (1 + \alpha_R (T - 293.15))$ for conductors and Arrhenius scaling $\sigma(T)$ for semiconductors/insulators, plus superconductivity transition check against `SuperconductingPoint`.
  - **Honest Provenance**: Missing properties return `EvaluatedProperty::available() == false` and appear in `ElementBehaviorReport::unsupported_properties`.

---

## 3. Material Mixing Laboratory (`MaterialMixingLaboratory`)

Supports 6 explicit formation scenarios in [`MaterialMixingLaboratory`](../core/domain/include/shunya/mat/material_mixing_lab.hpp):

1. `PhysicalMixture` — Volume/mass-weighted Voigt/Reuss bounds without assuming chemical reaction; flags reactivity incompatibility (e.g., Alkali metal + Oxidizer/Halogen) instead of silently mixing as inert.
2. `Alloy` — Solid metallic solution checked against **Hume-Rothery heuristics** ($\le 15\%$ atomic radius mismatch, electronegativity compatibility, valence factor) to distinguish probable solid solutions from immiscible/intermetallic risks.
3. `Solution` — Homogeneous liquid/electrolyte mixture tracking solute-solvent mass/mole fractions and ionic/electrical conductance contributions.
4. `ChemicalCompound` — **Valence & electronegativity stoichiometry validation** ($\Delta \chi$, formal charge balance). Avoids naive gas-density averaging for gas-solid compounds (e.g., $\text{Li} + \text{Fe} + \text{P} + \text{O}$ or $\text{LiPF}_6$) by applying bound-anion condensed density corrections and flagging approximate/user-override formation enthalpies.
5. `LayeredMaterial` — Series perpendicular ($1/k_\perp = \sum f_i / k_i$) vs parallel in-plane ($k_\parallel = \sum f_i k_i$) effective transport anisotropy.
6. `CompositeMaterial` — Matrix + reinforcement Rule-of-Mixtures (Voigt upper bound) and inverse Rule-of-Mixtures (Reuss lower bound) for elastic modulus, thermal conductivity, and electrical percolation checks.

Every evaluated mixture attaches `EpistemicStatus` tags (`ConfirmedScientific`, `ApproximateModel`, `UserAssumption`).

---

## 4. Object -> Functional Component -> Battery -> Circuit & Physics Coupling

- **Material-to-Object (`PhysicalObjectManager`)**:
  - Supports multi-region materials (`ObjectRegion`) on 2D/3D primitives (`Box`, `Cylinder`, `Sphere`, `Cone`, `CustomMesh`), computing exact volume $V$, surface area $A$, mass $m = \sum \rho_i V_i$, axial electrical resistance $R = L / (\sigma_{\text{eff}} A_c)$, thermal mass $C_{\text{th}} = \sum m_i c_{p,i}$, and phase melting checks (`is_solid_at_operating_temp`).
  - Live subscription via `EngineeringWorkspaceState`: mutating a `Material` density or conductivity automatically triggers `notify_material_modified()` -> recalculates every dependent `PhysicalObject` -> updates every `GeneratedComponentDefinition` -> syncs placed schematic components across all `InteractiveCircuitDocument`s.
- **Object-to-Component Qualification (`ComponentGenerator`)**:
  - Physical objects do NOT automatically become electronic parts: `Conductor`, `Wire`, `Resistor`, `HeatingElement`, and `Switch` require verified conductive paths and positive conductance; `Capacitor` enforces dielectric breakdown & plate-separation geometry; `Battery` enforces valid electrochemical stack specs.
- **Battery Engineering (`BatteryEngineer`)**:
  - 7 physical regions: Cathode (`LiFePO4` / active intercalation material), Anode (`Graphite`), Electrolyte (`LiPF6 Carbonate`), Separator (`Microporous PP/PE`), Anode Current Collector (`Copper`), Cathode Current Collector (`Aluminum`), and Structural Casing + Positive/Negative Terminals.
  - **Fidelity Disclosure (L2 Engineering Approximation)**:
    - Capacity $Q_{\text{nom}}$ derived via Faraday active-mass stoichiometry ($m_{\text{active}} \cdot q_{\text{spec}} / 1000$).
    - Internal resistance $R_0(T, \text{SOC})$ sums electronic foil/terminal resistances + ionic electrolyte resistance ($d / (\sigma_{\text{ionic}} A)$).
    - Dynamic discharge/charge transient uses a **Dual-RC Thevenin ECM** coupled to a lumped $m C_p \frac{dT}{dt} = I^2 R_0 - h A (T - T_{\text{amb}})$ thermal ODE — explicitly disclosed as an L2 engineering ECM approximation (not full 3D Newman P2D DFT).
    - Safety limit warnings fire for over-voltage ($> V_{\max}$), under-voltage ($< V_{\min}$), over-temperature ($> 353.15\,\text{K}$), and thermal runaway onset ($> 393.15\,\text{K}$).
- **Interactive Circuit (`InteractiveCircuitDocument` & `CircuitSimulationCoordinator`)**:
  - Spatial Union-Find geometric net resolution across terminals, multi-segment wires, and junctions; ERC checks for floating pins, missing ground, and shorted voltage sources; DC Nodal MNA solver (`solve_dc`) and Trapezoidal/Backward-Euler transient solver (`solve_transient`) handling switches, diodes/LEDs, RC charging, RL transients, and live Battery discharge coupling.
- **2D/3D Modeling & Multiphysics (`cad2d`, `cad3d`, `PhysicsSimulationRuntime`)**:
  - 2D vector primitives (`Rectangle`, `Circle`, `Line`, `Polygon`, `Polyline` + DXF/SVG I/O).
  - 3D watertight 2-manifold primitive & assembly meshes (`Box`, `Cylinder`, `Sphere`, `Cone`) with orbit/pan/zoom/ViewCube viewport (`Viewport3DItem`).
  - Symplectic rigid-body dynamics, ground/contact resolution, lumped Joule/source thermal balance, and thermal strain $\epsilon_{\text{th}} = \alpha (T - T_0)$ / axial stress feedback.
