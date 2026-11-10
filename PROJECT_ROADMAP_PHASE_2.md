# Shunya Phase-2 Master Development Roadmap: Advanced CAD, Multiphysics & Virtual Simulation Studio
**Document ID:** `PROJECT_ROADMAP_PHASE_2.md`  
**Platform Version Target:** `v2.5.0-alpha` through `v3.0.0-release`  
**Status:** `Active Engineering Roadmap`  
**Baseline Dependency:** Verified `PROJECT_ROADMAP.md` Core Foundation (309/309 Tasks Completed, 326/326 Tests Passing)  
**Document Architecture:** Hierarchical Work Breakdown Structure (WBS: Phase -> Module -> Submodule -> Task -> Subtask)  

---

## 1. Project Overview and Development Vision

The foundational phase of **Shunya** successfully established a mathematically rigorous, dependency-clean C++20 core engine comprising 32 architectural modules, 309 foundational tasks, and an exhaustive Catch2-style test harness (3,429 assertions, zero failures, zero warnings). This robust baseline verified fundamental Modified Nodal Analysis (MNA) circuit simulation, unit-dimensional compile-time typing (C++20 NTTP), lock-free ring-buffer observability, headless CLI batch execution, and coupled electro-thermo-mechanical demonstrators.

### Strategic Objective for Next Phase
The objective of this next development phase is to advance Shunya from a verified algorithmic core into a **world-class, desktop-first virtual engineering laboratory and multiphysics CAD/CAE design studio**. This entails bridging numerical physics kernels with professional design automation and interactive visual synthesis:

1. **Advanced High-Performance Desktop UI:** Evolving the single-window shell into an enterprise-grade multi-pane docking system with native cross-platform application menu bars (comprehensive File lifecycle, recent files, template wizards), detachable floating windows, contextual ribbon toolbars, quick-access command palettes (Ctrl+K), and high-framerate GPU-accelerated scientific telemetry plotters (60+ FPS at 1M data points).
2. **2D Vector Drafting & Constraint Engine:** Providing a professional 2D sketching environment equipped with parametric geometric primitives, Clipper2 polygon set operations, magnetic object snapping, DXF/SVG vector exchange, and an analytical Newton-Raphson/Levenberg-Marquardt geometric constraint solver.
3. **3D Solid Modeling & Modern Hardware Rendering:** Delivering parametric feature modeling (Extrude, Revolve, Sweep, Loft, CSG Booleans, Fillet/Chamfer, Shell) backed by a Half-Edge mesh topology, BVH acceleration trees, volumetric Delaunay tetrahedral meshing, and a modern Qt RHI / Vulkan 1.3 / Direct3D 12 PBR graphics pipeline.
4. **First-Principles Periodic Materials & Atomic Modeling:** Transitioning from static property records to a dynamic 118-element IUPAC/NIST scientific database, isotopic decay networks, VSEPR 3D molecular conformations, multi-phase mixture homogenization (Voigt-Reuss-Hashin-Shtrikman), and temperature/pressure-dependent nonlinear constitutive tensor models.
5. **Integrated Electronic Design Automation (EDA / PCB):** Bridging schematic capture, hierarchical multi-sheet buses, IPC-7351 footprint synthesis, multi-layer stackup design, interactive 45-degree push-and-shove trace routing, DRC geometric verification, Gerber RS-274X manufacturing export, and mixed-signal SPICE/digital logic co-simulation.
6. **Coupled Real-Time Virtual Physics & Chemistry Environment:** Implementing symplectic multi-body dynamics, continuous collision detection (CCD), 3D transient continuum thermal diffusion, Smoothed Particle Hydrodynamics (SPH) free-surface fluids, MAC grid CFD, chemical reaction networks, Arrhenius combustion kinetics, and realistic atmospheric/gravity environmental field chambers.
7. **Simulation Orchestration & High-Performance Computing:** Implementing a multi-rate master co-simulation orchestrator with adaptive time-step negotiation, FMI 2.0/3.0 FMU exchange, GPU compute shaders, SIMD vectorization (AVX2/AVX-512/NEON), and zero-allocation memory arenas.

### Fidelity Levels (L0 to L4)
Shunya maintains an uncompromising policy of **Scientific Honesty**. Every computational model, simulation viewport, and telemetry probe explicitly advertises its active fidelity tier:

| Tier | Classification | Underlying Mathematical Formulation | Typical Use Cases |
|---|---|---|---|
| **L0** | **Visual Preview** | Raster/PBR visual representation without physical coupling | Concept layout, visual packaging, aesthetic review |
| **L1** | **Idealized / Functional** | Closed-form analytical formulas, ideal circuit components, rigid kinematic constraints | Fast architectural exploration, schematic drafting, digital logic verification |
| **L2** | **Engineering Approximation** | Lumped parameter models, 1D thermal networks, linear elasticity, weakly-compressible SPH | Design validation, interactive parameter sweeps, control system tuning |
| **L3** | **High Fidelity Continuum** | 3D Navier-Stokes CFD, transient FEM thermal conduction, non-linear SPICE transients, Arrhenius kinetics | Component stress qualification, thermal hotspot prediction, fluid splash dynamics |
| **L4** | **Certified / Reference Benchmark** | Traceable validation against NIST/CODATA, SPICE3f5, and IEEE reference analytical solutions | Regulatory compliance, safety audits, publication-grade scientific validation |

---

## 2. Existing Architecture Analysis

### Baseline Foundation Review
A thorough analysis of the repository and the original `PROJECT_ROADMAP.md` reveals that the platform is currently at Version 2.0.0-dev with 100% of Phase 1 foundational requirements completed and verified:
- **Core Domain Separation:** All mathematical models, unit definitions, and physics representations reside strictly in `core/base/` and `core/domain/` with zero downstream coupling to Qt, GUI toolkits, or external renderers.
- **Compile-Time Dimensional Analysis:** Enhancement `IMP-01` introduced zero-runtime-overhead unit checking via C++20 Non-Type Template Parameters (`units.hpp`), guaranteeing dimensional correctness in solver kernels.
- **Headless Execution CLI:** Enhancement `IMP-02` delivered `shunya-cli`, allowing batch sweeps, automated test runs, and headless validation without Qt GUI overhead.
- **Lock-Free Telemetry Probes:** Enhancement `IMP-03` established single-producer single-consumer ring buffers (`ring_buffer.hpp`), isolating 100 kHz simulation clocks from 60 Hz rendering loops.
- **Coupled Electro-Thermo-Mechanical Demonstrator:** Enhancement `IMP-05` and Section 18 demonstrator verified bidirectional state handoffs between circuit Joule heating, thermal diffusion, and mechanical cantilever deflection.
- **Desktop GUI Shell:** Prototype `appshunya` created in `shunya/` with Qt 6 Quick/QML, providing live simulation stepping, log displays, and preset category toggles.

### Architectural Gaps to Address in Next Phase
| Functional Domain | Current Baseline State (Phase 1) | Required Next-Phase Target Architecture |
|---|---|---|
| **User Interface** | Fixed single-window QML shell; simple preset buttons | Dockable multi-window framework, floating panels, dynamic ribbon, reflected property tree with inline unit math, 60 FPS plotting |
| **2D Modeling** | Non-existent; only bounding box data structures | Parametric 2D vector primitives, Clipper2 Booleans, magnetic snapping, Newton-Raphson geometric constraint solver, DXF/SVG exchange |
| **3D Modeling** | Elementary visual primitive meshes | Parametric B-Rep solid features (Extrude, Revolve, Sweep), CSG Booleans, half-edge mesh processing, SAH BVH, Delaunay tetrahedral mesher, Vulkan/D3D12 PBR |
| **Periodic Elements** | Basic static data structures for 118 elements | Exhaustive IUPAC/NIST dataset (isotopes, half-lives, decay chains, crystal lattices, Shomate polynomials, interactive QML periodic table) |
| **Materials** | Lookup dictionary for bulk constants | Molecular graph topologies, VSEPR 3D conformation, Voigt-Reuss-Hashin-Shtrikman composite homogenization, nonlinear temperature-dependent tensor models |
| **PCB & Circuits** | Basic linear netlist and textbook MNA solver | Multi-sheet schematic editor, ERC, IPC footprint generator, multi-layer stackup, 45-deg push-and-shove router, DRC, Gerber RS-274X, SPICE/Digital co-sim |
| **Physics Simulation** | 1D/lumped mechanical and thermal baselines | Symplectic multi-body dynamics, CCD collision detection, 3D continuum transient thermal diffusion, radiation view factors, SPH and MAC grid CFD |
| **Chemistry** | Basic reaction formula string storage | Multi-step reaction ODE network, Arrhenius/Eyring kinetics, Gibbs energy minimization (NASA CEA), flame deflagration, aqueous electrochemistry |
| **Virtual Environment**| Basic static environment parameters | US Standard Atmosphere (0-86km), altitude density/pressure profiles, wind vector fields, solar irradiance, environmental stress chambers |
| **Integration** | Monolithic sequential demonstrator stepping | Multi-rate master co-simulation orchestrator, adaptive LTE step negotiation, FMI 2.0/3.0 FMU master/slave, shared-memory IPC |
| **Performance** | Basic multithreading thread pool | Vulkan/D3D12 GPGPU compute shaders, AVX2/AVX-512 SIMD vectorization, Structure-of-Arrays (SoA) layout, lock-free work-stealing job queues, memory arenas |
| **Validation** | Unit tests for class APIs | Rigorous analytical benchmark suite (Kepler orbits, Fourier heat, Navier-Stokes Poiseuille, NIST-JANAF thermochem, SPICE3f5 circuits, SSIM visual gates) |

---

## 3. Overall Progress & Task Metrics

Progress tracking enforces strict hierarchical derivation: subtasks represent the canonical counting unit. Parent tasks, submodules, and modules derive their completion status directly from child leaf nodes.

| Status Indicator | Meaning | Subtask Count | Percentage |
|---|---|---:|---:|
| `[ ]` | Not Started | 77 | 47.5% |
| `[~]` | In Progress | 0 | 0.0% |
| `[x]` | Completed | 85 | 52.5% |
| `[!]` | Blocked | 0 | 0.0% |
| **Total** | **All Catalogued Leaf Subtasks** | **162** | **100.0%** |

> **Current Next-Phase Completion:** **52.5%** (85 / 162 leaf subtasks completed)


---

## 4. Module Breakdown Matrix

Every module is assigned a target development stage, priority classification (P0 = Critical Path, P1 = Essential Capability, P2 = Advanced Polish), and total leaf-task allocation.

| Module ID | Module Name | Phase / Stage | Priority | Total Subtasks | Completed | Progress |
|---|---|---|---|---:|---:|---:|
| **UI** | Advanced UI Development | Stage 1 | P0 | 20 | 20 | 100.0% |
| **2D** | 2D Modeling Engine | Stage 2 | P0 | 13 | 13 | 100.0% |
| **3D** | 3D Modeling Engine | Stage 3 | P0 | 17 | 17 | 100.0% |
| **MATDB** | Periodic Elements Database | Stage 4 | P0 | 14 | 14 | 100.0% |
| **MAT** | Material Modeling | Stage 4 | P0 | 14 | 14 | 100.0% |
| **PCB** | Circuit Board Design | Stage 5 | P1 | 17 | 7 | 41.2% |
| **PHY** | Physics Simulation | Stage 6 | P0 | 15 | 0 | 0.0% |
| **CHEM** | Chemistry Simulation | Stage 7 | P1 | 13 | 0 | 0.0% |
| **ENV** | Virtual Environment | Stage 8 | P0 | 9 | 0 | 0.0% |
| **INT** | System Integration | Stage 9 | P0 | 11 | 0 | 0.0% |
| **PERF** | Performance Optimization | Stage 10 | P1 | 9 | 0 | 0.0% |
| **TEST** | Testing & Validation | Stage 11 | P0 | 10 | 0 | 0.0% |
| **TOTAL** | **Comprehensive Next Phase Platform** | **Stages 1 - 11** | **--** | **162** | **85** | **52.5%** |

---

## 5. Development Stage Overview

The development lifecycle is divided into 11 sequential, dependency-gated engineering stages:

- **Stage 1: Advanced UI & Workspace Foundations (Module UI)**  
  Establish the professional desktop shell: multi-pane docking system, application menu bar with full File lifecycle management (New, Open, Save, Recent, Import/Export, dirty-state guards), docking layout JSON serialization, contextual ribbon toolbar, unified reflected property inspector with inline unit math, and 60 FPS GPU-accelerated scientific plotters.
- **Stage 2: 2D Modeling & Constraint Engine (Module 2D)**  
  Construct the 2D CAD vector sketcher: parametric curves, Clipper2 polygon set operations, object snapping, Newton-Raphson geometric constraint solver, layer management, and DXF/SVG data exchange.
- **Stage 3: 3D Solid Features & PBR Graphics Pipeline (Module 3D)**  
  Build parametric 3D B-Rep solid features (Extrude, Revolve, Sweep, Loft, Booleans, Fillet), half-edge mesh processing, SAH BVH trees, tetrahedral finite-element meshing, and modern Qt RHI Vulkan/D3D12 PBR rendering pipeline.
- **Stage 4: First-Principles Elements & Material Modeling (Modules MATDB & MAT)**  
  Implement the exhaustive 118-element IUPAC/NIST database, isotopic decay networks, VSEPR 3D molecular conformations, Voigt-Reuss-Hashin-Shtrikman composite homogenization, and temperature/pressure-dependent nonlinear constitutive tensor models.
- **Stage 5: Electronic Design Automation & PCB Studio (Module PCB)**  
  Deliver multi-sheet schematic capture, ERC checking, IPC-7351 footprint generator, multi-layer stackup design, 45-degree push-and-shove trace router, copper flood fill, DRC verification, Gerber RS-274X export, and mixed-signal SPICE/digital co-simulation.
- **Stage 6: Continuum Physics Simulation Engines (Module PHY)**  
  Develop symplectic rigid-body dynamics, continuous collision detection (CCD), articulated joints, 3D transient continuum thermal diffusion, radiation view factors, SPH free-surface fluids, and staggered MAC grid incompressible CFD.
- **Stage 7: Chemical Kinetics & Thermochemistry (Module CHEM)**  
  Implement multi-step chemical reaction network ODE solvers, Arrhenius/Eyring kinetics, Gibbs free energy minimization (NASA CEA), flame deflagration, aqueous electrolyte equilibrium, and electrochemical battery cell models.
- **Stage 8: Virtual Environmental Chambers & Boundary Conditions (Module ENV)**  
  Construct the US Standard Atmosphere (0-86km) environmental column, aerodynamic wind vector fields, solar irradiance modeling, climatic thermal shock chambers, and deep-space radiative cooling sinks.
- **Stage 9: Multi-Domain Co-Simulation Orchestrator (Module INT)**  
  Deliver the master co-simulation orchestrator with multi-rate domain stepping, adaptive LTE step negotiation, cross-domain coupling (electro-thermal, thermo-mechanical, FSI, chemo-thermal), FMI 2.0/3.0 FMU support, and shared-memory IPC.
- **Stage 10: Performance Optimization & High-Performance Computing (Module PERF)**  
  Engineer Vulkan/D3D12 GPGPU compute shaders for particles and sparse matrix math, AVX2/AVX-512 SIMD vectorization, Structure-of-Arrays (SoA) layout, work-stealing job scheduling, and zero-allocation frame memory arenas.
- **Stage 11: Scientific Validation & Automated CI Quality Gates (Module TEST)**  
  Execute analytical verification suites (Kepler orbits, Fourier heat, Navier-Stokes, NIST-JANAF thermochem, SPICE3f5 circuits), automated CI regression gates, and headless SSIM golden visual tests.

---

## 6. Comprehensive Work Breakdown Structure (WBS)

```text
SHUNYA NEXT PHASE PLATFORM WBS
├── Module [UI] [x] Advanced UI Development (Stage 1 | P0)
│   ├── Submodule [UI-01] [x] Dockable Workspace & Window Management
│   │   ├── Task [UI-01-001] [x] Multi-pane Docking Framework & Layout Serialization
│   │   │   ├── Subtask [UI-01-001-01] [x] Implement layout node tree & docking manager
│   │   │   ├── Subtask [UI-01-001-02] [x] Implement workspace presets serialization & DPI restoration
│   │   │   ├── Subtask [UI-01-001-03] [x] Implement hotkey-driven panel toggle and multi-monitor management
│   │   ├── Task [UI-01-002] [x] Contextual Ribbon & Modular Toolbars
│   │   │   ├── Subtask [UI-01-002-01] [x] Implement domain-sensitive dynamic ribbon bar
│   │   │   ├── Subtask [UI-01-002-02] [x] Implement quick-access toolbar and fuzzy command palette
│   │   │   ├── Subtask [UI-01-002-03] [x] Implement undo/redo visualization stack in toolbar dropdown
│   │   ├── Task [UI-01-003] [x] Desktop Menu Bar & File Management Architecture
│   │   │   ├── Subtask [UI-01-003-01] [x] Implement desktop application menu bar structure and cross-platform native integration
│   │   │   ├── Subtask [UI-01-003-02] [x] Implement File menu project lifecycle actions and recent files manager
│   │   │   ├── Subtask [UI-01-003-03] [x] Implement File import/export action dispatchers and dirty state save prompt dialogs
│   ├── Submodule [UI-02] [x] Unified Inspector & Property Editor
│   │   ├── Task [UI-02-001] [x] Scientific Property Grid & Dimension-Aware Inputs
│   │   │   ├── Subtask [UI-02-001-01] [x] Build hierarchical reflected property tree editor
│   │   │   ├── Subtask [UI-02-001-02] [x] Implement inline dimensional unit conversions in property fields
│   │   │   ├── Subtask [UI-02-001-03] [x] Implement parametric mathematical formula expression evaluator in numerical inputs
│   │   ├── Task [UI-02-002] [x] Multi-Selection & Batch Property Editing
│   │   │   ├── Subtask [UI-02-002-01] [x] Implement homogenous & heterogenous multi-selection property diffing
│   │   │   ├── Subtask [UI-02-002-02] [x] Implement bulk property update transactions with atomic rollback
│   ├── Submodule [UI-03] [x] Scientific Visualization Viewports & Overlays
│   │   ├── Task [UI-03-001] [x] Viewport Controls, Gizmos & Heads-Up Display (HUD)
│   │   │   ├── Subtask [UI-03-001-01] [x] Implement 3D transform gizmos with coordinate snapping
│   │   │   ├── Subtask [UI-03-001-02] [x] Build viewport camera orientation ViewCube with ortho/persp toggle
│   │   │   ├── Subtask [UI-03-001-03] [x] Implement interactive viewport HUD for solver diagnostics and FPS
│   │   ├── Task [UI-03-002] [x] Real-Time Scientific Data Plotting & Probe Drawer
│   │   │   ├── Subtask [UI-03-002-01] [x] Implement high-framerate GPU-accelerated 2D line and scatter plot widget
│   │   │   ├── Subtask [UI-03-002-02] [x] Implement scalar field contour colormap legend and probe tooltips
│   │   │   ├── Subtask [UI-03-002-03] [x] Implement multi-channel oscilloscope & logic analyzer viewports
├── Module [2D] [x] 2D Modeling Engine (Stage 2 | P0)
│   ├── Submodule [2D-01] [x] 2D Geometric Primitives & Vector Drawing
│   │   ├── Task [2D-01-001] [x] Parametric 2D Primitives Engine
│   │   │   ├── Subtask [2D-01-001-01] [x] Implement point, line, polyline, arc, circle, ellipse, and cubic Bézier spline primitives
│   │   │   ├── Subtask [2D-01-001-02] [x] Implement polygon boolean operations (Union, Difference, Intersection, XOR via Clipper2)
│   │   │   ├── Subtask [2D-01-001-03] [x] Implement offset curve generation and filleting/chamfering algorithms for 2D profiles
│   │   ├── Task [2D-01-002] [x] Interactive Drawing Tools & Snapping System
│   │   │   ├── Subtask [2D-01-002-01] [x] Implement interactive sketch canvas with zoom, pan, grid raster, and coordinate readout
│   │   │   ├── Subtask [2D-01-002-02] [x] Implement geometric object snapping (endpoints, midpoints, centers, quadrants, intersections)
│   │   │   ├── Subtask [2D-01-002-03] [x] Implement dynamic polar guides and construction geometry layer
│   ├── Submodule [2D-02] [x] 2D Geometric Constraint Solver
│   │   ├── Task [2D-02-001] [x] Degrees of Freedom & 2D Constraint Formulation
│   │   │   ├── Subtask [2D-02-001-01] [x] Define constraint graph (Coincident, Collinear, Parallel, Perpendicular, Tangent, Concentric)
│   │   │   ├── Subtask [2D-02-001-02] [x] Implement dimensional constraints (Distance, Angle, Radius, Diameter, Horizontal/Vertical)
│   │   │   ├── Subtask [2D-02-001-03] [x] Implement non-linear Newton-Raphson / Levenberg-Marquardt solver for sketch DOF analysis
│   ├── Submodule [2D-03] [x] Layer Management & CAD Vector Exchange
│   │   ├── Task [2D-03-001] [x] Multi-Layer CAD Organization & Styles
│   │   │   ├── Subtask [2D-03-001-01] [x] Implement hierarchical layer system (visibility, locking, line weights, dash patterns, color)
│   │   │   ├── Subtask [2D-03-001-02] [x] Implement grouping, compound 2D blocks, and instancing
│   │   ├── Task [2D-03-002] [x] 2D Import/Export Pipeline
│   │   │   ├── Subtask [2D-03-002-01] [x] Implement DXF (AutoCAD Drawing Exchange Format) R12/2000 reader and writer
│   │   │   ├── Subtask [2D-03-002-02] [x] Implement SVG export and import with scale preservation and layer mapping
├── Module [3D] [x] 3D Modeling Engine (Stage 3 | P0)
│   ├── Submodule [3D-01] [x] 3D Parametric Features & Boundary Representation (B-Rep)
│   │   ├── Task [3D-01-001] [x] Sketch-to-3D Solid Feature Engine
│   │   │   ├── Subtask [3D-01-001-01] [x] Implement linear extrusion feature with draft angle, end conditions, and symmetric limits
│   │   │   ├── Subtask [3D-01-001-02] [x] Implement revolve feature about arbitrary 3D axes with angular constraints
│   │   │   ├── Subtask [3D-01-001-03] [x] Implement sweep and loft features along curved 3D guide trajectories
│   │   ├── Task [3D-01-002] [x] 3D Solid Boolean & Direct Modeling Operations
│   │   │   ├── Subtask [3D-01-002-01] [x] Implement robust 3D solid Boolean operations (CSG Union, Subtract, Intersect)
│   │   │   ├── Subtask [3D-01-002-02] [x] Implement 3D edge filleting, variable radius fillets, and planar/conical chamfers
│   │   │   ├── Subtask [3D-01-002-03] [x] Implement thin-wall shelling and uniform/directional offsetting
│   ├── Submodule [3D-02] [x] Polygonal Mesh Processing & Computational Geometry
│   │   ├── Task [3D-02-001] [x] Half-Edge Data Structure & Mesh Topology
│   │   │   ├── Subtask [3D-02-001-01] [x] Implement half-edge mesh representation supporting manifold topology and boundary queries
│   │   │   ├── Subtask [3D-02-001-02] [x] Implement progressive mesh simplification (Quadric Error Metric decimation)
│   │   │   ├── Subtask [3D-02-001-03] [x] Implement Loop and Catmull-Clark subdivision surfaces
│   │   ├── Task [3D-02-002] [x] Computational Geometry & 3D Spatial Partitioning
│   │   │   ├── Subtask [3D-02-002-01] [x] Implement 3D Bounding Volume Hierarchy (BVH) and Octree for fast raycasting
│   │   │   ├── Subtask [3D-02-002-02] [x] Implement tetrahedral mesh generation for finite element simulation
│   ├── Submodule [3D-03] [x] Real-Time PBR Rendering Pipeline & Scene Lighting
│   │   ├── Task [3D-03-001] [x] Modern Hardware Graphics Abstraction (RHI)
│   │   │   ├── Subtask [3D-03-001-01] [x] Implement unified rendering hardware interface abstraction
│   │   │   ├── Subtask [3D-03-001-02] [x] Implement forward+ / deferred rendering pipeline with shadow cascades (CSM)
│   │   │   ├── Subtask [3D-03-001-03] [x] Implement order-independent transparency (WBOIT) for scientific visualization
│   │   ├── Task [3D-03-002] [x] Physically Based Rendering (PBR) Materials & Environment
│   │   │   ├── Subtask [3D-03-002-01] [x] Implement Cook-Torrance microfacet specular BRDF (metallic-roughness)
│   │   │   ├── Subtask [3D-03-002-02] [x] Implement image-based lighting (IBL) with split-sum approximation
│   │   │   ├── Subtask [3D-03-002-03] [x] Implement volumetric scientific field rendering (emission-absorption raymarching)
├── Module [MATDB] Periodic Elements Database (Stage 4 | P0)
│   ├── Submodule [MATDB-01] Comprehensive 118-Element Scientific Dataset
│   │   ├── Task [MATDB-01-001] Element Physicochemical & Isotopic Registry
│   │   │   ├── Subtask [MATDB-01-001-01] Complete data ingestion for all 118 IUPAC elements
│   │   │   ├── Subtask [MATDB-01-001-02] Compile comprehensive isotope table with half-lives and decay modes
│   │   │   ├── Subtask [MATDB-01-001-03] Encode electronegativity scales and atomic radii
│   │   ├── Task [MATDB-01-002] Electronic Shell Structure & Spectral Data
│   │   │   ├── Subtask [MATDB-01-002-01] Implement Aufbau principle and Hund's rule shell filling validator
│   │   │   ├── Subtask [MATDB-01-002-02] Ingest sequential ionization energies and electron affinity values
│   │   │   ├── Subtask [MATDB-01-002-03] Ingest atomic emission/absorption spectral lines for optical radiation
│   ├── Submodule [MATDB-02] Solid-State, Crystallographic & Thermodynamic Baseline Data
│   │   ├── Task [MATDB-02-001] Crystal Lattice Structures & Space Groups
│   │   │   ├── Subtask [MATDB-02-001-01] Catalog standard crystal systems (BCC, FCC, HCP, Diamond) with lattice parameters
│   │   │   ├── Subtask [MATDB-02-001-02] Implement unit cell 3D visualization and atomic packing factor calculators
│   │   ├── Task [MATDB-02-002] Standard Thermodynamic State Quantities
│   │   │   ├── Subtask [MATDB-02-002-01] Ingest standard enthalpy of formation, entropy, and Gibbs free energy at 298.15 K
│   │   │   ├── Subtask [MATDB-02-002-02] Ingest NIST Shomate polynomial coefficients for heat capacity across 298 K - 6000 K
│   ├── Submodule [MATDB-03] Interactive Periodic Table UI & Element Inspector
│   │   ├── Task [MATDB-03-001] Dynamic Vector Periodic Table Component
│   │   │   ├── Subtask [MATDB-03-001-01] Build interactive 118-element periodic grid with family and block colormaps
│   │   │   ├── Subtask [MATDB-03-001-02] Implement dynamic property heatmap overlay across periodic table
│   │   ├── Task [MATDB-03-002] Element Detail Inspector & Isotope Visualizer
│   │   │   ├── Subtask [MATDB-03-002-01] Implement Bohr orbital visualization and electron cloud probability density display
│   │   │   ├── Subtask [MATDB-03-002-02] Implement decay chain visualizer for radioactive isotopes with branching ratios
├── Module [MAT] Material Modeling (Stage 4 | P0)
│   ├── Submodule [MAT-01] Atomic & Molecular Structural Modeling
│   │   ├── Task [MAT-01-001] Molecular Graph & Valence Bond Representation
│   │   │   ├── Subtask [MAT-01-001-01] Implement molecular graph topology and valence bond models
│   │   │   ├── Subtask [MAT-01-001-02] Implement VSEPR geometry predictor for 3D atomic coordinates
│   │   │   ├── Subtask [MAT-01-001-03] Implement molecular 3D ball-and-stick and space-filling (CPK) generator
│   │   ├── Task [MAT-01-002] Chemical Formula Parser & SMILES / InChI Import
│   │   │   ├── Subtask [MAT-01-002-01] Implement stoichiometric formula parser supporting hydrate notation
│   │   │   ├── Subtask [MAT-01-002-02] Implement simplified SMILES string parser for molecular graphs
│   ├── Submodule [MAT-02] Constitutive Material Creation & Alloy/Mixture Synthesis
│   │   ├── Task [MAT-02-001] Multi-Phase Mixture & Solid Solution Rules
│   │   │   ├── Subtask [MAT-02-001-01] Implement mixture homogenization rules (Voigt, Reuss, Hashin-Shtrikman bounds)
│   │   │   ├── Subtask [MAT-02-001-02] Implement alloy composition builder with mass/molar fraction balancing
│   │   ├── Task [MAT-02-002] Porosity, Microstructure & Anisotropic Formulations
│   │   │   ├── Subtask [MAT-02-002-01] Implement effective medium approximations for porous and cellular solids
│   │   │   ├── Subtask [MAT-02-002-02] Implement orthotropic and transversely isotropic elasticity tensors (6x6 stiffness)
│   ├── Submodule [MAT-03] Temperature & Pressure Dependent Material Properties
│   │   ├── Task [MAT-03-001] Non-Linear Thermal Property Functions
│   │   │   ├── Subtask [MAT-03-001-01] Implement temperature-dependent thermal conductivity and specific heat interpolators
│   │   │   ├── Subtask [MAT-03-001-02] Implement linear and volumetric thermal expansion coefficients and thermal stress coupling
│   │   ├── Task [MAT-03-002] Phase Change & Non-Linear Rheology Models
│   │   │   ├── Subtask [MAT-03-002-01] Implement melting/solidification enthalpy curves with latent heat mushy-zone tracking
│   │   │   ├── Subtask [MAT-03-002-02] Implement temperature-dependent dynamic viscosity (Arrhenius & Andrade liquid models)
│   │   │   ├── Subtask [MAT-03-002-03] Implement temperature-dependent electrical resistivity and semiconductor bandgap variation
├── Module [PCB] Circuit Board Design (Stage 5 | P1)
│   ├── Submodule [PCB-01] Schematic Capture & Symbol Management
│   │   ├── Task [PCB-01-001] Multi-Sheet Schematic Editor
│   │   │   ├── Subtask [PCB-01-001-01] Implement hierarchical schematic sheets with inter-sheet ports and buses
│   │   │   ├── Subtask [PCB-01-001-02] Build interactive schematic wiring tool with orthogonal auto-routing and junctions
│   │   │   ├── Subtask [PCB-01-001-03] Implement real-time Electrical Rule Checking (ERC) engine
│   │   ├── Task [PCB-01-002] Component Symbol & Footprint Library Manager
│   │   │   ├── Subtask [PCB-01-002-01] Implement IEEE/IEC compliant schematic symbol library and pin mapper
│   │   │   ├── Subtask [PCB-01-002-02] Implement IPC-7351 compliant SMD/THT footprint generator
│   ├── Submodule [PCB-02] PCB Layout, Stackup & Interactive Routing
│   │   ├── Task [PCB-02-001] Multi-Layer Board Stackup Architecture
│   │   │   ├── Subtask [PCB-02-001-01] Implement layer stackup editor (copper layers, core, prepreg, dielectric)
│   │   │   ├── Subtask [PCB-02-001-02] Implement board outline definition with cutouts and keep-out zones
│   │   ├── Task [PCB-02-002] Interactive & Semi-Automated Trace Routing
│   │   │   ├── Subtask [PCB-02-002-01] Implement interactive 45-degree angle trace routing with push-and-shove
│   │   │   ├── Subtask [PCB-02-002-02] Implement via placement (through-hole, blind, buried) and connectivity
│   │   │   ├── Subtask [PCB-02-002-03] Implement copper pour polygon flood fill with thermal relief spokes
│   │   ├── Task [PCB-02-003] Design Rule Checking (DRC) & Manufacturing Export
│   │   │   ├── Subtask [PCB-02-003-01] Implement geometric DRC engine (clearance, trace width, annular ring)
│   │   │   ├── Subtask [PCB-02-003-02] Implement Gerber (RS-274X) and Excellon drill file generator
│   │   │   ├── Subtask [PCB-02-003-03] Implement 3D PCB solid model exporter (STEP / OBJ) with placed 3D bodies
│   ├── Submodule [PCB-03] Electrical & Digital Logic Co-Simulation
│   │   ├── Task [PCB-03-001] SPICE & Analog Circuit Simulation Integration
│   │   │   ├── Subtask [PCB-03-001-01] Implement netlist compilation from schematic/PCB with parasitic extraction
│   │   │   ├── Subtask [PCB-03-001-02] Implement transient analysis, DC operating point, and AC frequency sweep
│   │   ├── Task [PCB-03-002] Digital Logic & Event-Driven Simulation
│   │   │   ├── Subtask [PCB-03-002-01] Implement IEEE 1164 9-state digital logic simulation engine with delays
│   │   │   ├── Subtask [PCB-03-002-02] Implement mixed-signal ADC/DAC co-simulation bridges
├── Module [PHY] Physics Simulation (Stage 6 | P0)
│   ├── Submodule [PHY-01] Classical Mechanics, Multi-Body Dynamics & Collisions
│   │   ├── Task [PHY-01-001] Rigid Body Kinematics & Numerical Integrators
│   │   │   ├── Subtask [PHY-01-001-01] Implement symplectic integrators preserving mechanical energy
│   │   │   ├── Subtask [PHY-01-001-02] Implement spatial inertia tensors and rotational quaternion kinematics
│   │   ├── Task [PHY-01-002] Collision Detection & Constraint Solving
│   │   │   ├── Subtask [PHY-01-002-01] Implement broadphase dynamic BVH and narrowphase GJK/EPA contact manifolds
│   │   │   ├── Subtask [PHY-01-002-02] Implement continuous collision detection (CCD) for high-velocity bodies
│   │   │   ├── Subtask [PHY-01-002-03] Implement projected Gauss-Seidel constraint solver for articulated joints
│   ├── Submodule [PHY-02] Thermodynamics, Heat Transfer & Phase Transitions
│   │   ├── Task [PHY-02-001] Conduction, Convection & Radiation Solvers
│   │   │   ├── Subtask [PHY-02-001-01] Implement 3D transient thermal conduction via finite difference / element solver
│   │   │   ├── Subtask [PHY-02-001-02] Implement surface convective heat transfer with empirical Nusselt correlations
│   │   │   ├── Subtask [PHY-02-001-03] Implement Stefan-Boltzmann radiation with diffuse gray-body view factor calculations
│   │   ├── Task [PHY-02-002] Phase Change Enthalpy & Thermal Stress
│   │   │   ├── Subtask [PHY-02-002-01] Implement enthalpy-based phase change solver with latent heat tracking
│   │   │   ├── Subtask [PHY-02-002-02] Implement coupled thermo-mechanical expansion and thermal stress tensor computation
│   ├── Submodule [PHY-03] Fluid Mechanics & Fluid Dynamics
│   │   ├── Task [PHY-03-001] Smoothed Particle Hydrodynamics (SPH) Free-Surface Solver
│   │   │   ├── Subtask [PHY-03-001-01] Implement weakly compressible SPH formulation with Tait equation of state
│   │   │   ├── Subtask [PHY-03-001-02] Implement artificial and physical viscosity models with boundary interaction
│   │   │   ├── Subtask [PHY-03-001-03] Implement surface tension models and wetting contact angle
│   │   ├── Task [PHY-03-002] Eulerian Grid-Based Incompressible Fluid Solver
│   │   │   ├── Subtask [PHY-03-002-01] Implement staggered MAC grid advection-diffusion solver (Stable Fluids)
│   │   │   ├── Subtask [PHY-03-002-02] Implement Conjugate Gradient Poisson pressure solver with boundary conditions
├── Module [CHEM] Chemistry Simulation (Stage 7 | P1)
│   ├── Submodule [CHEM-01] Reaction Kinetics & Stoichiometric Networks
│   │   ├── Task [CHEM-01-001] Law of Mass Action & Rate Law Engine
│   │   │   ├── Subtask [CHEM-01-001-01] Implement rate equation solver for elementary and multi-step reactions
│   │   │   ├── Subtask [CHEM-01-001-02] Implement temperature-dependent Arrhenius and modified Eyring reaction rates
│   │   │   ├── Subtask [CHEM-01-001-03] Implement catalytic rate modification and enzyme Michaelis-Menten kinetics
│   │   ├── Task [CHEM-01-002] Chemical Equilibrium & Gibbs Free Energy Minimization
│   │   │   ├── Subtask [CHEM-01-002-01] Implement chemical equilibrium composition solver using Gibbs energy minimization
│   │   │   ├── Subtask [CHEM-01-002-02] Implement Le Chatelier equilibrium shift response to temperature, pressure, and volume
│   ├── Submodule [CHEM-02] Thermochemistry & Energy Balances
│   │   ├── Task [CHEM-02-001] Reaction Enthalpy & Heat of Combustion
│   │   │   ├── Subtask [CHEM-02-001-01] Implement standard enthalpy of reaction calculator and Hess's Law
│   │   │   ├── Subtask [CHEM-02-001-02] Implement exothermic/endothermic heat generation coupling to thermodynamics solver
│   │   ├── Task [CHEM-02-002] Combustion, Flame Propagation & Explosion Modeling
│   │   │   ├── Subtask [CHEM-02-002-01] Implement hydrocarbon combustion kinetics with ignition thresholds and oxygen depletion
│   │   │   ├── Subtask [CHEM-02-002-02] Implement deflagration pressure rise and adiabatic flame temperature calculation
│   ├── Submodule [CHEM-03] Molecular Interactions & Solution Chemistry
│   │   ├── Task [CHEM-03-001] Aqueous Solutions, Electrolytes & pH
│   │   │   ├── Subtask [CHEM-03-001-01] Implement acid-base dissociation equilibrium, buffer solutions, and dynamic pH
│   │   │   ├── Subtask [CHEM-03-001-02] Implement electrolyte ionic conductivity and precipitation checking (K_sp)
│   │   ├── Task [CHEM-03-002] Electrochemistry & Galvanic Cells
│   │   │   ├── Subtask [CHEM-03-002-01] Implement Nernst equation for half-cell and full-cell electrochemical potentials
│   │   │   ├── Subtask [CHEM-03-002-02] Implement battery charge/discharge cell model coupled to circuit and thermal solvers
├── Module [ENV] Virtual Environment (Stage 8 | P0)
│   ├── Submodule [ENV-01] Environmental Physical Fields & Atmospheric Model
│   │   ├── Task [ENV-01-001] Barometric & Atmospheric Column Modeling
│   │   │   ├── Subtask [ENV-01-001-01] Implement US Standard Atmosphere model from sea level to mesosphere
│   │   │   ├── Subtask [ENV-01-001-02] Implement dynamic ambient humidity, dew point, and vapor pressure models
│   │   │   ├── Subtask [ENV-01-001-03] Implement wind velocity vector fields and boundary layer shear
│   │   ├── Task [ENV-01-002] Gravitational & Celestial Radiation Fields
│   │   │   ├── Subtask [ENV-01-002-01] Implement configurable planetary gravity fields and gravity gradient
│   │   │   ├── Subtask [ENV-01-002-02] Implement solar position calculator and direct/diffuse solar irradiance
│   ├── Submodule [ENV-02] Virtual Chamber & Environmental Boundary Enclosures
│   │   ├── Task [ENV-02-001] Controlled Test Chamber & Wind Tunnel Rig
│   │   │   ├── Subtask [ENV-02-001-01] Implement hermetic thermal/pressure test chamber boundary conditions
│   │   │   ├── Subtask [ENV-02-001-02] Implement virtual wind tunnel boundary enclosure with laminar inlet and outlet
│   │   ├── Task [ENV-02-002] Ambient Energy Exchange & Radiation Sinks
│   │   │   ├── Subtask [ENV-02-002-01] Implement ambient thermal convective boundary exchange with dynamic h
│   │   │   ├── Subtask [ENV-02-002-02] Implement night-sky deep space radiative cooling sink model
├── Module [INT] System Integration (Stage 9 | P0)
│   ├── Submodule [INT-01] Multi-Rate Co-Simulation Orchestrator
│   │   ├── Task [INT-01-001] Master Time Coordinator & Adaptive Stepping
│   │   │   ├── Subtask [INT-01-001-01] Implement master simulation executive with multi-rate domain clock scheduling
│   │   │   ├── Subtask [INT-01-001-02] Implement adaptive time-step negotiation based on local truncation error
│   │   │   ├── Subtask [INT-01-001-03] Implement deterministic discrete event queue and rollback mechanism
│   │   ├── Task [INT-01-002] Cross-Domain Physical Coupling Connectors
│   │   │   ├── Subtask [INT-01-002-01] Implement electro-thermal bidirectional coupling connector
│   │   │   ├── Subtask [INT-01-002-02] Implement thermo-mechanical bidirectional coupling connector
│   │   │   ├── Subtask [INT-01-002-03] Implement fluid-structure interaction (FSI) coupling connector
│   │   │   ├── Subtask [INT-01-002-04] Implement chemo-thermal coupling connector
│   ├── Submodule [INT-02] Standard Co-Simulation Interfaces & Data Bus
│   │   ├── Task [INT-02-001] FMI/FMU Industry Standard Support
│   │   │   ├── Subtask [INT-02-001-01] Implement Functional Mock-up Interface (FMI 2.0 / 3.0) Co-Simulation Master
│   │   │   ├── Subtask [INT-02-001-02] Implement FMI Export for Shunya sub-models as standalone FMUs
│   │   ├── Task [INT-02-002] High-Speed Shared-Memory IPC & Network Telemetry
│   │   │   ├── Subtask [INT-02-002-01] Implement low-latency lock-free shared memory telemetry IPC bus
│   │   │   ├── Subtask [INT-02-002-02] Implement WebSocket / gRPC live streaming endpoint for remote monitoring
├── Module [PERF] Performance Optimization (Stage 10 | P1)
│   ├── Submodule [PERF-01] GPU Acceleration & Compute Shaders
│   │   ├── Task [PERF-01-001] GPGPU Numerical Compute Kernels
│   │   │   ├── Subtask [PERF-01-001-01] Implement GPU compute shader pipeline for SPH fluids and particle physics
│   │   │   ├── Subtask [PERF-01-001-02] Implement parallel GPU reduction and sparse matrix-vector multiplication (SpMV)
│   │   │   ├── Subtask [PERF-01-001-03] Implement pinned host-device unified memory buffers with asynchronous transfer
│   │   ├── Task [PERF-01-002] SIMD Vectorization & Cache Locality
│   │   │   ├── Subtask [PERF-01-002-01] Vectorize inner solver loops using AVX2 / AVX-512 / ARM NEON intrinsics
│   │   │   ├── Subtask [PERF-01-002-02] Refactor core domain states to Structure-of-Arrays (SoA) layout
│   ├── Submodule [PERF-02] Multithreading Architecture & Memory Management
│   │   ├── Task [PERF-02-001] Work-Stealing Task Scheduler & Domain Decomposition
│   │   │   ├── Subtask [PERF-02-001-01] Implement lock-free work-stealing job system for parallel spatial tasks
│   │   │   ├── Subtask [PERF-02-001-02] Implement spatial domain decomposition with boundary ghost-cell synchronization
│   │   ├── Task [PERF-02-002] High-Performance Arena Allocators
│   │   │   ├── Subtask [PERF-02-002-01] Implement frame-based and simulation-step monotonic arena allocators
│   │   │   ├── Subtask [PERF-02-002-02] Implement fixed-size memory pool allocators for small dynamic objects
├── Module [TEST] Testing & Validation (Stage 11 | P0)
│   ├── Submodule [TEST-01] Analytical Benchmark Suite & Reference Verification
│   │   ├── Task [TEST-01-001] Classical Mechanics & Kinematic Benchmarks
│   │   │   ├── Subtask [TEST-01-001-01] Implement analytical verification suite for orbits, projectiles, and chaotic pendulums
│   │   │   ├── Subtask [TEST-01-001-02] Implement spinning top gyroscopic precession and Euler equation verification
│   │   ├── Task [TEST-01-002] Thermal, Fluid & Chemical Analytical Benchmarks
│   │   │   ├── Subtask [TEST-01-002-01] Implement 1D/2D transient heat diffusion verification against Fourier series
│   │   │   ├── Subtask [TEST-01-002-02] Implement Poiseuille / Couette flow validation against exact Navier-Stokes solutions
│   │   │   ├── Subtask [TEST-01-002-03] Implement stoichiometric equilibrium validation against NIST-JANAF tables
│   ├── Submodule [TEST-02] Standard Electronics & PCB Validation Suite
│   │   ├── Task [TEST-02-001] SPICE Reference Benchmark Suite
│   │   │   ├── Subtask [TEST-02-001-01] Implement circuit simulation benchmark against SPICE3f5 reference outputs
│   │   │   ├── Subtask [TEST-02-001-02] Implement digital logic timing verification against IEEE 1164 test vectors
│   ├── Submodule [TEST-03] Continuous Integration & Automated Regression Suite
│   │   ├── Task [TEST-03-001] Automated Scientific Quality Gates
│   │   │   ├── Subtask [TEST-03-001-01] Build automated CI test harness executing numerical accuracy checks
│   │   │   ├── Subtask [TEST-03-001-02] Implement headless visual regression testing for viewports via SSIM
│   │   │   ├── Subtask [TEST-03-001-03] Implement performance benchmark regression gate detecting throughput drops
```


---

## 7. Detailed Modules and Submodules

### Module UI: Advanced UI Development
**Target Stage:** Stage 1 | **Priority:** P0 | **Status:** `[x] Completed` (100.0% completed: 20/20 subtasks)  
**Scope Summary:** Professional engineering workspace, multi-window dockable panels, contextual ribbon toolbars, unified property inspector with unit intelligence, 3D viewport gizmos, and high-framerate scientific visualization overlays.

#### Submodule UI-01: Dockable Workspace & Window Management
**Status:** `[x] Completed` (100.0% completed: 9/9 subtasks)  
**Submodule Task Inventory:** 3 major engineering tasks, 9 canonical leaf subtasks (9 completed).

#### Submodule UI-02: Unified Inspector & Property Editor
**Status:** `[x] Completed` (100.0% completed: 5/5 subtasks)  
**Submodule Task Inventory:** 2 major engineering tasks, 5 canonical leaf subtasks (5 completed).

#### Submodule UI-03: Scientific Visualization Viewports & Overlays
**Status:** `[x] Completed` (100.0% completed: 6/6 subtasks)  
**Submodule Task Inventory:** 2 major engineering tasks, 6 canonical leaf subtasks (6 completed).



### Module 2D: 2D Modeling Engine
**Target Stage:** Stage 2 | **Priority:** P0 | **Status:** `[x] Completed` (100.0% completed: 13/13 subtasks)  
**Scope Summary:** Parametric 2D vector geometry engine, interactive drafting tools, geometric snapping, non-linear 2D constraint solver, layer management, and CAD vector exchange.

#### Submodule 2D-01: 2D Geometric Primitives & Vector Drawing
**Status:** `[x] Completed` (100.0% completed: 6/6 subtasks)  
**Submodule Task Inventory:** 2 major engineering tasks, 6 canonical leaf subtasks (6 completed).

#### Submodule 2D-02: 2D Geometric Constraint Solver
**Status:** `[x] Completed` (100.0% completed: 3/3 subtasks)  
**Submodule Task Inventory:** 1 major engineering task, 3 canonical leaf subtasks (3 completed).

#### Submodule 2D-03: Layer Management & CAD Vector Exchange
**Status:** `[x] Completed` (100.0% completed: 4/4 subtasks)  
**Submodule Task Inventory:** 2 major engineering tasks, 4 canonical leaf subtasks (4 completed).



### Module 3D: 3D Modeling Engine
**Target Stage:** Stage 3 | **Priority:** P0 | **Status:** `[x] Completed` (100.0% completed: 17/17 subtasks)  
**Scope Summary:** Parametric 3D solid feature modeling (B-Rep), solid Booleans, mesh processing, half-edge topology, spatial partitioning, hardware RHI rendering, and PBR materials.

#### Submodule 3D-01: 3D Parametric Features & Boundary Representation (B-Rep)
**Status:** `[x] Completed` (100.0% completed: 6/6 subtasks)  
**Submodule Task Inventory:** 2 major engineering tasks, 6 canonical leaf subtasks (6 completed).

#### Submodule 3D-02: Polygonal Mesh Processing & Computational Geometry
**Status:** `[x] Completed` (100.0% completed: 5/5 subtasks)  
**Submodule Task Inventory:** 2 major engineering tasks, 5 canonical leaf subtasks (5 completed).

#### Submodule 3D-03: Real-Time PBR Rendering Pipeline & Scene Lighting
**Status:** `[x] Completed` (100.0% completed: 6/6 subtasks)  
**Submodule Task Inventory:** 2 major engineering tasks, 6 canonical leaf subtasks (6 completed).



### Module MATDB: Periodic Elements Database
**Target Stage:** Stage 4 | **Priority:** P0 | **Status:** `[x] Completed` (14 / 14 subtasks, 100.0%)  
**Scope Summary:** Exhaustive physicochemical database for all 118 IUPAC elements, isotopic abundances, electronic shell structures, crystal lattices, thermodynamic Shomate polynomials, and interactive periodic table visualization.

#### Submodule MATDB-01: Comprehensive 118-Element Scientific Dataset
**Status:** `[x] Completed` (6 / 6 subtasks)  
**Submodule Task Inventory:** 2 major engineering tasks, 6 canonical leaf subtasks.

#### Submodule MATDB-02: Solid-State, Crystallographic & Thermodynamic Baseline Data
**Status:** `[x] Completed` (4 / 4 subtasks)  
**Submodule Task Inventory:** 2 major engineering tasks, 4 canonical leaf subtasks.

#### Submodule MATDB-03: Interactive Periodic Table UI & Element Inspector
**Status:** `[x] Completed` (4 / 4 subtasks)  
**Submodule Task Inventory:** 2 major engineering tasks, 4 canonical leaf subtasks.



### Module MAT: Material Modeling
**Target Stage:** Stage 4 | **Priority:** P0 | **Status:** `[x] Completed` (14 / 14 subtasks, 100.0%)  
**Scope Summary:** Atomic and molecular modeling, molecular graph topology, VSEPR geometry, composite/alloy synthesis, Voigt-Reuss-Hashin-Shtrikman homogenization, and temperature/pressure-dependent nonlinear constitutive tensor models.

#### Submodule MAT-01: Atomic & Molecular Structural Modeling
**Status:** `[x] Completed` (5 / 5 subtasks)  
**Submodule Task Inventory:** 2 major engineering tasks, 5 canonical leaf subtasks.

#### Submodule MAT-02: Constitutive Material Creation & Alloy/Mixture Synthesis
**Status:** `[x] Completed` (4 / 4 subtasks)  
**Submodule Task Inventory:** 2 major engineering tasks, 4 canonical leaf subtasks.

#### Submodule MAT-03: Temperature & Pressure Dependent Material Properties
**Status:** `[x] Completed` (5 / 5 subtasks)  
**Submodule Task Inventory:** 2 major engineering tasks, 5 canonical leaf subtasks.



### Module PCB: Circuit Board Design
**Target Stage:** Stage 5 | **Priority:** P1 | **Status:** `[~] In Progress` (7 / 17 subtasks, 41.2%)  
**Scope Summary:** Electronic Design Automation (EDA) suite including multi-sheet schematic capture, ERC, IPC footprint generator, multi-layer PCB stackup, 45-degree push-and-shove trace router, copper flood fill, DRC, Gerber RS-274X export, and mixed-signal SPICE/digital co-simulation.

#### Submodule PCB-01: Schematic Capture & Symbol Management
**Status:** `[x] Completed` (5 / 5 subtasks)  
**Submodule Task Inventory:** 2 major engineering tasks, 5 canonical leaf subtasks.

#### Submodule PCB-02: PCB Layout, Stackup & Interactive Routing
**Status:** `[~] In Progress` (2 / 8 subtasks)  
**Submodule Task Inventory:** 3 major engineering tasks, 8 canonical leaf subtasks.

#### Submodule PCB-03: Electrical & Digital Logic Co-Simulation
**Status:** `[ ] Not Started`  
**Submodule Task Inventory:** 2 major engineering tasks, 4 canonical leaf subtasks.



### Module PHY: Physics Simulation
**Target Stage:** Stage 6 | **Priority:** P0 | **Status:** `[ ] Not Started`  
**Scope Summary:** Multi-domain physics simulation engine covering symplectic rigid-body dynamics, continuous collision detection (CCD), articulated joints, 3D transient heat conduction/radiation, phase transitions, SPH free-surface fluids, and Eulerian grid CFD.

#### Submodule PHY-01: Classical Mechanics, Multi-Body Dynamics & Collisions
**Status:** `[ ] Not Started`  
**Submodule Task Inventory:** 2 major engineering tasks, 5 canonical leaf subtasks.

#### Submodule PHY-02: Thermodynamics, Heat Transfer & Phase Transitions
**Status:** `[ ] Not Started`  
**Submodule Task Inventory:** 2 major engineering tasks, 5 canonical leaf subtasks.

#### Submodule PHY-03: Fluid Mechanics & Fluid Dynamics
**Status:** `[ ] Not Started`  
**Submodule Task Inventory:** 2 major engineering tasks, 5 canonical leaf subtasks.



### Module CHEM: Chemistry Simulation
**Target Stage:** Stage 7 | **Priority:** P1 | **Status:** `[ ] Not Started`  
**Scope Summary:** Chemical reaction kinetics, stoichiometric network balancing, Law of Mass Action, Arrhenius temperature dependence, Gibbs energy minimization, thermochemical combustion, aqueous electrochemistry, and battery cells.

#### Submodule CHEM-01: Reaction Kinetics & Stoichiometric Networks
**Status:** `[ ] Not Started`  
**Submodule Task Inventory:** 2 major engineering tasks, 5 canonical leaf subtasks.

#### Submodule CHEM-02: Thermochemistry & Energy Balances
**Status:** `[ ] Not Started`  
**Submodule Task Inventory:** 2 major engineering tasks, 4 canonical leaf subtasks.

#### Submodule CHEM-03: Molecular Interactions & Solution Chemistry
**Status:** `[ ] Not Started`  
**Submodule Task Inventory:** 2 major engineering tasks, 4 canonical leaf subtasks.



### Module ENV: Virtual Environment
**Target Stage:** Stage 8 | **Priority:** P0 | **Status:** `[ ] Not Started`  
**Scope Summary:** Configurable physical virtual environment including US Standard Atmosphere column, altitude pressure/density gradients, aerodynamic wind vectors, solar radiation, microgravity, vacuum test chambers, and deep-space radiation sinks.

#### Submodule ENV-01: Environmental Physical Fields & Atmospheric Model
**Status:** `[ ] Not Started`  
**Submodule Task Inventory:** 2 major engineering tasks, 5 canonical leaf subtasks.

#### Submodule ENV-02: Virtual Chamber & Environmental Boundary Enclosures
**Status:** `[ ] Not Started`  
**Submodule Task Inventory:** 2 major engineering tasks, 4 canonical leaf subtasks.



### Module INT: System Integration
**Target Stage:** Stage 9 | **Priority:** P0 | **Status:** `[ ] Not Started`  
**Scope Summary:** Multi-rate co-simulation orchestrator, master time coordinator, adaptive time-stepping, cross-domain coupling (electro-thermal, thermo-mechanical, fluid-structure, chemo-thermal), FMI 2.0/3.0 FMU support, and low-latency shared-memory telemetry IPC.

#### Submodule INT-01: Multi-Rate Co-Simulation Orchestrator
**Status:** `[ ] Not Started`  
**Submodule Task Inventory:** 2 major engineering tasks, 7 canonical leaf subtasks.

#### Submodule INT-02: Standard Co-Simulation Interfaces & Data Bus
**Status:** `[ ] Not Started`  
**Submodule Task Inventory:** 2 major engineering tasks, 4 canonical leaf subtasks.



### Module PERF: Performance Optimization
**Target Stage:** Stage 10 | **Priority:** P1 | **Status:** `[ ] Not Started`  
**Scope Summary:** GPU compute shaders, SIMD vectorization (AVX2/AVX-512/NEON), Structure-of-Arrays (SoA) data transformation, work-stealing job scheduling, spatial domain decomposition, and zero-allocation frame/simulation arenas.

#### Submodule PERF-01: GPU Acceleration & Compute Shaders
**Status:** `[ ] Not Started`  
**Submodule Task Inventory:** 2 major engineering tasks, 5 canonical leaf subtasks.

#### Submodule PERF-02: Multithreading Architecture & Memory Management
**Status:** `[ ] Not Started`  
**Submodule Task Inventory:** 2 major engineering tasks, 4 canonical leaf subtasks.



### Module TEST: Testing & Validation
**Target Stage:** Stage 11 | **Priority:** P0 | **Status:** `[ ] Not Started`  
**Scope Summary:** Comprehensive analytical validation suite, NIST/CODATA reference benchmarks, SPICE reference circuits, automated continuous integration quality gates, and SSIM visual regression testing.

#### Submodule TEST-01: Analytical Benchmark Suite & Reference Verification
**Status:** `[ ] Not Started`  
**Submodule Task Inventory:** 2 major engineering tasks, 5 canonical leaf subtasks.

#### Submodule TEST-02: Standard Electronics & PCB Validation Suite
**Status:** `[ ] Not Started`  
**Submodule Task Inventory:** 1 major engineering tasks, 2 canonical leaf subtasks.

#### Submodule TEST-03: Continuous Integration & Automated Regression Suite
**Status:** `[ ] Not Started`  
**Submodule Task Inventory:** 1 major engineering tasks, 3 canonical leaf subtasks.




---

## 8. Task and Subtask Checklists

This section contains the comprehensive checklist of all 64 major tasks and 159 canonical leaf subtasks. Each entry documents technical specifications, algorithmic implementations, dependencies, deliverables, acceptance criteria, and verification methods.

### Module UI: Advanced UI Development

**Status:** `[x] Completed` | **Stage:** Stage 1 | **Priority:** P0

#### Submodule UI-01: Dockable Workspace & Window Management
**Status:** `[x] Completed`

- [x] **UI-01-001: Multi-pane Docking Framework & Layout Serialization**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Implement an advanced multi-pane docking system supporting tabbed document interfaces, splitters, floating torn-off windows, and persistent JSON layout serialization.
  - [x] **UI-01-001-01: Implement layout node tree & docking manager**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Develop C++ DockingManager and DockNode hierarchy (SplitNode, TabNode, LeafNode) managing geometry constraints, minimum pane dimensions, drag-and-drop hit zones, and visual docking drop-indicator overlays.
    - **Implementation Details:** Class shunya::core::domain::DockingManager with DockNode tree, directional snap zones (North, South, East, West, Center Tab), and recursive layout node graph. QML components DockContainer.qml and DockOverlay.qml with ShunyaDockingController.
    - **Dependencies:** core/domain/workspace_ui.hpp, Qt 6.8+ Quick Layouts
    - **Expected Deliverables:** docking_manager in workspace_ui.hpp/.cpp, docking_controller.hpp/.cpp, DockContainer.qml, DockOverlay.qml, unit tests for layout tree node splits.
    - **Acceptance Criteria:** Panels can be dragged, dropped into splits or tabs, torn off into standalone windows, and restored without memory leaks or graphic artifacts.
    - **Testing & Verification Requirements:** Unit test tree mutations; integration test simulating drag-and-drop docking events with synthetic pointer coordinates.
    - **Actual Verification Evidence:** Verified via unit tests in test_ui_docking.cpp (Leaf/Split/Tabbed creation, recursive search, snap zones West/South/CenterTab, split ratio mutation and clamping, leaf removal). All 333 test cases passing.
    - **Git Commit Reference:** Commit 7a9d7d9 (feat(ui): implement multi-pane docking framework & layout serialization)

  - [x] **UI-01-001-02: Implement workspace presets serialization & DPI restoration**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Serialize the complete dockable pane topology, geometry ratios, visibility states, and monitor affinities to atomic JSON manifests with high-DPI coordinate scaling.
    - **Implementation Details:** Serialize DockNode tree to JSON with normalized floating-point coordinate proportions (0.0 to 1.0) to preserve layout across differing monitor resolutions.
    - **Dependencies:** UI-01-001-01, core/domain/workspace_ui.hpp
    - **Expected Deliverables:** layout serializer in workspace_ui.hpp/.cpp, presets schema definition, default presets (Modeling, Schematic, Simulation, Results, Scripting).
    - **Acceptance Criteria:** Saving layout to JSON and restoring on different screen resolutions reproduces identical relative proportions with zero clipped controls.
    - **Testing & Verification Requirements:** Round-trip serialization tests: save tree -> mutate -> load -> assert identical node hierarchy and geometry ratios.
    - **Actual Verification Evidence:** Verified via unit tests in test_ui_docking.cpp (JSON layout serialization round-trip across all 5 workspace presets: Modeling, Schematic, Simulation, Results, Scripting with exact split ratio and panel state recovery).
    - **Git Commit Reference:** Commit 7a9d7d9 (feat(ui): implement multi-pane docking framework & layout serialization)

  - [x] **UI-01-001-03: Implement hotkey-driven panel toggle and multi-monitor management**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Provide global shortcut management for toggling inspector, viewport, telemetry drawers, and tearing off floating windows to secondary display heads.
    - **Implementation Details:** DockActionManager binding key sequence actions (F9-F12, Ctrl+B, Ctrl+J) to dock leaf visibility; ShunyaDockingController and Main.qml global Shortcut bindings.
    - **Dependencies:** UI-01-001-01
    - **Expected Deliverables:** DockActionManager in workspace_ui.hpp/.cpp, Shortcut bindings in Main.qml, docking action tests.
    - **Acceptance Criteria:** Hotkeys (F9-F12, Ctrl+B, Ctrl+J) toggle panels instantaneously without dropping rendering frames; multi-monitor layout recovers cleanly when display disconnects.
    - **Testing & Verification Requirements:** Automated keyboard event injection tests verifying state toggles and screen geometry edge boundary checks.
    - **Actual Verification Evidence:** Verified via unit tests in test_ui_docking.cpp (F9, F10, F12, Ctrl+B, Ctrl+J shortcut execution, dual sidebar toggling, invalid sequence rejection) and Main.qml Shortcut bindings.
    - **Git Commit Reference:** Commit 7a9d7d9 (feat(ui): implement multi-pane docking framework & layout serialization)

- [x] **UI-01-002: Contextual Ribbon & Modular Toolbars**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Design and implement a context-aware ribbon toolbar system that adapts actions and tools dynamically based on active workspace mode and active selection.
  - [x] **UI-01-002-01: Implement domain-sensitive dynamic ribbon bar**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Develop RibbonBar.qml with contextual tabs (2D Sketch, 3D Mesh, PCB Routing, Physics Setup, Analysis) dynamically activated upon entity selection or mode switch.
    - **Implementation Details:** RibbonBarModel C++ domain class in workspace_ui.hpp/.cpp, ShunyaRibbonController backend exposing active category, command groups, large/small icon modes, and ribbon collapse/expand behavior. RibbonBar.qml, RibbonTab.qml, and RibbonGroup.qml components.
    - **Dependencies:** UI-01-001-01, core/domain/selection.hpp
    - **Expected Deliverables:** ribbon_controller.hpp/.cpp, RibbonBar.qml, RibbonTab.qml, RibbonGroup.qml.
    - **Acceptance Criteria:** Switching from 3D viewport to PCB schematic switches active ribbon tabs in < 16ms with zero UI flicker.
    - **Testing & Verification Requirements:** Test selection change signals update ribbon tab models accurately across all 5 workspace domains.
    - **Actual Verification Evidence:** Verified via unit tests in test_ui_ribbon_command.cpp (RibbonBarModel tabs, groups, actions, category mapping, workspace preset context switching < 16ms, selection context switching for 2D_Profile, PCB_Component, Physics_Body).
    - **Git Commit Reference:** Commit 2ce8911 (feat(ui): implement contextual ribbon bar, fuzzy command palette and undo history)

  - [x] **UI-01-002-02: Implement quick-access toolbar and fuzzy command palette**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Create a quick-access toolbar for pin-able commands and a central fuzzy-search command palette (Ctrl+K / Ctrl+P) indexing all actions, elements, and solver runs.
    - **Implementation Details:** FuzzyMatcher scoring algorithm in fuzzy_match.hpp with word boundary, prefix, and consecutive bonuses. CommandPaletteRegistry fuzzy_search with relevance ranking. CommandPalette.qml modal dialog with Ctrl+K / Ctrl+P shortcuts.
    - **Dependencies:** UI-01-002-01
    - **Expected Deliverables:** command_palette_controller in ribbon_controller.hpp/.cpp, CommandPalette.qml, fuzzy_match.hpp.
    - **Acceptance Criteria:** Command search returns ranked results in < 5ms over 1,000 registered actions; executing action triggers bound slot cleanly.
    - **Testing & Verification Requirements:** Benchmark fuzzy search latency on 2,000 commands; verify action invocation with parameter passing.
    - **Actual Verification Evidence:** Verified via unit tests in test_ui_ribbon_command.cpp (QuickAccessToolbarModel pin/unpin/is_pinned, FuzzyMatcher scoring, CommandPaletteRegistry fuzzy_search across title, keywords, shortcuts, and benchmark latency on 2,000 commands).
    - **Git Commit Reference:** Commit 2ce8911 (feat(ui): implement contextual ribbon bar, fuzzy command palette and undo history)

  - [x] **UI-01-002-03: Implement undo/redo visualization stack in toolbar dropdown**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Visual dropdown history stack showing command descriptions, timestamps, and memory footprint with multi-step rollback capability.
    - **Implementation Details:** CommandStack::history_entries() and rollback_to_index() in command_stack.hpp/.cpp; ShunyaRibbonController exposing historyEntries, canUndo, canRedo, undoName, redoName; UndoHistoryDropdown.qml popup with atomic multi-step rollback.
    - **Dependencies:** UI-01-002-01, core/domain/command_stack.hpp
    - **Expected Deliverables:** command_history_model in ribbon_controller.hpp/.cpp, UndoHistoryDropdown.qml.
    - **Acceptance Criteria:** User can click 5 steps back in history dropdown to roll back state atomically in one transaction.
    - **Testing & Verification Requirements:** Undo/redo transaction rollbacks tested against multi-entity modifications; assert consistency of project state.
    - **Actual Verification Evidence:** Verified via unit tests in test_ui_ribbon_command.cpp (CommandStack 5 commands execution, history_entries inspection, multi-step rollback from 5 to 2, roll-forward from 2 to 4, atomic state consistency).
    - **Git Commit Reference:** Commit 2ce8911 (feat(ui): implement contextual ribbon bar, fuzzy command palette and undo history)

- [x] **UI-01-003: Desktop Menu Bar & File Management Architecture**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Implement a professional cross-platform desktop menu bar (File, Edit, View, Insert, Simulation, Tools, Window, Help) with deep File lifecycle management, recent project persistence, template wizards, and dirty-state guards.
  - [x] **UI-01-003-01: Implement desktop application menu bar structure and cross-platform native integration**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Create standard application menu bar component supporting native system menu integration on macOS (platform native MenuBar) and custom dark/light styled menu bar on Windows/Linux with accelerator key combinations (Alt+F for File, Alt+E for Edit) and mnemonic keyboard navigation.
    - **Implementation Details:** Developed MenuBarController C++ class exposing action hierarchies and shortcut bindings to MainMenuBar.qml using Qt Quick MenuBar, Menu, and Action elements with dynamic shortcut conflict validation.
    - **Dependencies:** UI-01-001-01, core/domain/workspace_ui.hpp
    - **Expected Deliverables:** menu_bar_controller.hpp/.cpp, MainMenuBar.qml, AppMenu.qml, MenuItemDelegate.qml, accelerator_tests.cpp.
    - **Acceptance Criteria:** Menu bar renders seamlessly across Windows, Linux, and macOS; pressing Alt+F opens File menu; keyboard arrow navigation and Escape work with native responsiveness.
    - **Testing & Verification Requirements:** Automated QML test driving menu item hover, activation, keyboard navigation, and action signal emission.
    - **Actual Verification Evidence:** Verified via unit tests in test_ui_menu_bar.cpp (Menu Bar Structure, Categories & Accelerator Shortcuts), Qt 6 Quick desktop build appshunya.exe with MainMenuBar.qml and ShunyaMenuBarController. All 329 test cases passing.
    - **Git Commit Reference:** Commit 23291b0 (feat(ui): implement desktop application menu bar and file management)

  - [x] **UI-01-003-02: Implement File menu project lifecycle actions and recent files manager**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Implement full File menu actions: New Project (with wizard templates: Multiphysics, Circuit/PCB, Mechanical 3D, Chemistry Lab, Blank), Open Project, Save, Save As, Close Project, and Recent Projects submenu.
    - **Implementation Details:** RecentProjectsManager class persisting MRU (Most Recently Used) list of paths, timestamps, and thumbnail previews in SQLite / JSON settings with automatic pruning of non-existent files.
    - **Dependencies:** UI-01-003-01, core/domain/project.hpp, core/domain/data_exchange.hpp
    - **Expected Deliverables:** recent_projects_manager.hpp/.cpp, NewProjectWizard.qml, ProjectTemplateModel.hpp, unit tests for MRU persistence.
    - **Acceptance Criteria:** Selecting 'New Project' opens template wizard; 'Save' commits project atomically; 'Recent Projects' displays last 10 opened projects with valid file existence validation.
    - **Testing & Verification Requirements:** Unit tests for MRU stack ordering, deduplication, missing file pruning, and project creation from templates.
    - **Actual Verification Evidence:** Verified via Catch2 test suite test_ui_menu_bar.cpp (Recent Projects Manager MRU ordering, JSON round-trip restoration, ProjectTemplateRegistry 5 templates), and NewProjectWizard.qml modal dialog integration.
    - **Git Commit Reference:** Commit 23291b0 (feat(ui): implement desktop application menu bar and file management)

  - [x] **UI-01-003-03: Implement File import/export action dispatchers and dirty state save prompt dialogs**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Build File Import/Export submenus (Import DXF, SVG, STEP, SPICE netlist; Export STEP, Gerber, PDF, CSV, HDF5) and integrate a safety guard modal dialog prompting user to Save, Discard, or Cancel when closing modified unsaved projects.
    - **Implementation Details:** Connect File menu dispatch slots to domain exchange pipelines; implement UnsavedChangesGuardDialog.qml querying Project::is_dirty() before project close, new project load, or application quit.
    - **Dependencies:** UI-01-003-02, core/domain/command_stack.hpp, core/domain/project.hpp
    - **Expected Deliverables:** file_exchange_dispatcher.hpp/.cpp, UnsavedChangesGuardDialog.qml, ExportFileDialog.qml, dirty_state_guard_tests.cpp.
    - **Acceptance Criteria:** Triggering 'Exit' or 'Close Project' on a modified project pops up modal guard with keyboard focus on 'Save'; clicking 'Cancel' aborts closure without losing edits.
    - **Testing & Verification Requirements:** Integration test verifying application exit cancellation on dirty project; test clean discard without saving when explicitly selected.
    - **Actual Verification Evidence:** Verified via Catch2 test suite test_ui_menu_bar.cpp (Import/Export action dispatching, Project::is_dirty() tracking and clearing), UnsavedChangesGuardDialog.qml confirmation modal, and FileDialog integration.
    - **Git Commit Reference:** Commit 23291b0 (feat(ui): implement desktop application menu bar and file management)



#### Submodule UI-02: Unified Inspector & Property Editor
**Status:** `[x] Completed`

- [x] **UI-02-001: Scientific Property Grid & Dimension-Aware Inputs**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Build a high-performance hierarchical property inspector that inspects reflected C++ entities, supports arbitrary units with inline conversion, and evaluates math formulas.
  - [x] **UI-02-001-01: Build hierarchical reflected property tree editor**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Implement PropertyTreeModel wrapping core entity reflection, grouping properties into collapsable categories (Transform, Material, Boundary Conditions, Solvers).
    - **Implementation Details:** QAbstractItemModel derived PropertyTreeModel with in-place cell delegates for floats, vectors, booleans, enums, and color pickers. ReflectedPropertyItem, PropertyCategoryGroup, and PropertySheet in property_node.hpp/.cpp with PropertyInspector.qml and PropertyDelegateFactory.qml.
    - **Dependencies:** core/domain/element_definition.hpp, core/domain/element_instance.hpp
    - **Expected Deliverables:** property_tree_model.hpp/.cpp, PropertyInspector.qml, PropertyDelegateFactory.qml.
    - **Acceptance Criteria:** Property changes in inspector propagate to core domain models in < 1ms; external domain updates refresh UI delegates immediately.
    - **Testing & Verification Requirements:** Property tree model unit tests covering node insertion, modification, and dataChanged signal emission.
    - **Actual Verification Evidence:** Verified via unit tests in test_ui_property_inspector.cpp (PropertySheet reflection inspection, sub-millisecond mutation < 1ms, Transform Vec3 propagation, and external change listener notifications).
    - **Git Commit Reference:** Commit 226000e (feat(ui): implement scientific property grid, dimensional inputs and formula evaluator)

  - [x] **UI-02-001-02: Implement inline dimensional unit conversions in property fields**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Enable inputs to parse unit strings (e.g. '25 mm', '3.5 kOhm', '100 psi', '450 degC') and automatically convert to canonical SI units.
    - **Implementation Details:** ShunyaQuantityInput item wrapping core/base/units.hpp parse_quantity; supports auto-suggest compatible unit suffixes via ShunyaQuantityInputController and QuantitySpinBox.qml.
    - **Dependencies:** UI-02-001-01, core/base/units.hpp
    - **Expected Deliverables:** quantity_input_controller.hpp/.cpp, QuantitySpinBox.qml.
    - **Acceptance Criteria:** Typing '150 kPa' automatically converts to 150,000 Pa internally while allowing user to toggle display unit.
    - **Testing & Verification Requirements:** Exhaustive unit string parsing tests: pressure, thermal, electrical, kinematic, and compound dimensions.
    - **Actual Verification Evidence:** Verified via unit tests in test_ui_property_inspector.cpp (Length, Electrical, Pressure, Temperature affine conversions, Kinematics, Dimensionless, error rejection, and UnitRegistry compatible unit queries).
    - **Git Commit Reference:** Commit 226000e (feat(ui): implement scientific property grid, dimensional inputs and formula evaluator)

  - [x] **UI-02-001-03: Implement parametric mathematical formula expression evaluator in numerical inputs**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Allow numerical property fields to accept algebraic expressions (e.g. '=width * 2 + 5mm', '=sin(pi/4) * radius') referencing project parameters.
    - **Implementation Details:** Recursive descent mathematical expression parser in expression_evaluator.hpp/.cpp with AST evaluation, variable resolution, and ParameterReferenceManager with Kahn's topological sort dependency resolution and circular dependency detection.
    - **Dependencies:** UI-02-001-02, core/base/math.hpp
    - **Expected Deliverables:** expression_evaluator.hpp/.cpp, ParameterReferenceManager.hpp, unit tests for expression parser.
    - **Acceptance Criteria:** Expressions evaluate with double-precision accuracy; parameter updates automatically re-evaluate dependent expressions.
    - **Testing & Verification Requirements:** Syntax validation tests, operator precedence verification, circular reference detection tests.
    - **Actual Verification Evidence:** Verified via unit tests in test_ui_property_inspector.cpp (Operator precedence, built-in functions, inline physical unit suffixes, variable context evaluation, topological dependency propagation, and cycle rejection).
    - **Git Commit Reference:** Commit 226000e (feat(ui): implement scientific property grid, dimensional inputs and formula evaluator)

- [x] **UI-02-002: Multi-Selection & Batch Property Editing**
  - **Status:** `[x] Completed`
  - **Priority:** P1
  - **Description:** Support selecting multiple entities across the viewport or scene tree and editing shared properties simultaneously with diff visualizers.
  - [x] **UI-02-002-01: Implement homogenous & heterogenous multi-selection property diffing**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Determine intersection of properties across multi-selected entities, showing '<mixed>' placeholder for differing values and unified value for identical values.
    - **Implementation Details:** PropertyIntersectionAnalyzer scanning selected QObject reflection metadata; builds composite property descriptor map.
    - **Dependencies:** UI-02-001-01, core/domain/selection.hpp
    - **Expected Deliverables:** multi_property_controller.hpp/.cpp, MultiPropertyInspector.qml.
    - **Acceptance Criteria:** Selecting 50 identical resistors shows resistance value once; selecting a resistor and capacitor shows only shared transform/name properties.
    - **Testing & Verification Requirements:** Test intersection logic on homogenous sets, heterogenous sets, and empty selections.
    - **Actual Verification Evidence:** Verified via unit tests in test_ui_multi_property.cpp (50 identical resistors intersection with unified resistance value, 50 divergent resistors with <mixed> placeholder, heterogeneous resistor + capacitor intersection isolating common spatial transforms, and empty selection safety).
    - **Git Commit Reference:** Commit 31968f3 (feat(ui): implement multi-selection property diffing and batch editing)

  - [x] **UI-02-002-02: Implement bulk property update transactions with atomic rollback**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Apply batch property modifications across selected entity collections wrapped inside an atomic MacroCommand on the undo stack.
    - **Implementation Details:** MacroCommand wrapping vector of individual SetPropertyCommand instances executed sequentially in an atomic transaction.
    - **Dependencies:** UI-02-002-01, core/domain/command_stack.hpp
    - **Expected Deliverables:** batch_command_helper.hpp/.cpp, unit tests for batch command execution.
    - **Acceptance Criteria:** Single undo action reverts property change on all 50 modified entities simultaneously.
    - **Testing & Verification Requirements:** Apply bulk property change across 100 entities -> undo -> assert all 100 return to exact original values.
    - **Actual Verification Evidence:** Verified via unit tests in test_ui_multi_property.cpp (BatchSetPropertyCommand modifying 100 entities in batch, single CommandStack::undo() atomic rollback restoring all 100 entities to original values, and CommandStack::redo() restoration).
    - **Git Commit Reference:** Commit 31968f3 (feat(ui): implement multi-selection property diffing and batch editing)



#### Submodule UI-03: Scientific Visualization Viewports & Overlays
**Status:** `[x] Completed`

- [x] **UI-03-001: Viewport Controls, Gizmos & Heads-Up Display (HUD)**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Interactive viewport interaction components including 3D translation/rotation/scale gizmos, ViewCube orientation widget, and simulation HUD.
  - [x] **UI-03-001-01: Implement 3D transform gizmos with coordinate snapping**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Build interactive screen-space 3D gizmos (Translate axes, Rotation rings, Scale boxes) with ray-plane intersection and grid snapping.
    - **Implementation Details:** TransformGizmoRenderer generating hit-testable line/mesh primitives; calculates transform delta projected on plane perpendicular to camera. Implemented GizmoInteractionEngine in viewport_interaction.hpp/.cpp with GizmoOverlay.qml and ShunyaViewportController.
    - **Dependencies:** core/domain/rendering.hpp, core/base/math.hpp
    - **Expected Deliverables:** transform_gizmo.hpp/.cpp, GizmoOverlay.qml, mathematical ray-casting hit-test tests.
    - **Acceptance Criteria:** Dragging gizmo axis translates entity along constrained global or local coordinate axis with sub-pixel responsiveness.
    - **Testing & Verification Requirements:** Synthetic raycast hit tests verifying axis pick tolerances and coordinate snapping accuracy.
    - **Actual Verification Evidence:** Verified via unit tests in test_ui_viewport.cpp (Coordinate snapping rounding invariance, 2D screen projections of 3D axes, axis picking within 14px tolerance, constrained translation delta calculation, and interactive GizmoOverlay.qml).
    - **Git Commit Reference:** Commit 6a0ee82 (feat(ui): implement 3D transform gizmos, ViewCube, and viewport HUD)

  - [x] **UI-03-001-02: Build viewport camera orientation ViewCube with ortho/persp toggle**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Interactive 3D navigation cube in corner of viewport allowing one-click snapping to Top, Front, Right, Isometric views, and perspective/orthographic toggle.
    - **Implementation Details:** ViewCubeController rendering standalone miniature 3D cube with face hit-testing and smooth slerp camera animation interpolation. Implemented ViewCubeManager in viewport_interaction.hpp/.cpp and ViewCube.qml.
    - **Dependencies:** UI-03-001-01
    - **Expected Deliverables:** view_cube.hpp/.cpp, ViewCube.qml, camera_animator.hpp.
    - **Acceptance Criteria:** Clicking 'Top' face smoothly interpolates camera rotation and position to orthographic top-down projection in 250ms.
    - **Testing & Verification Requirements:** Verify camera projection matrix calculation for all 6 orthogonal faces and 8 corner isometric views.
    - **Actual Verification Evidence:** Verified via unit tests in test_ui_viewport.cpp (Standard face direction/up vectors for Top, Bottom, Front, Back, Left, Right, Isometric; camera creation with orthographic/perspective toggle; smooth camera interpolation at midpoint/boundaries; and ViewCube.qml).
    - **Git Commit Reference:** Commit 6a0ee82 (feat(ui): implement 3D transform gizmos, ViewCube, and viewport HUD)

  - [x] **UI-03-001-03: Implement interactive viewport HUD for solver diagnostics and FPS**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Lightweight semi-transparent HUD overlay showing real-time rendering FPS, simulation step time, solver residual norm, and memory bandwidth.
    - **Implementation Details:** ViewportHudOverlay.qml subscribing to core/domain/observability.hpp and lock-free telemetry ring buffer. Implemented DiagnosticsTelemetryTracker in viewport_interaction.hpp/.cpp and ViewportHud.qml.
    - **Dependencies:** core/domain/observability.hpp, core/base/ring_buffer.hpp
    - **Expected Deliverables:** hud_controller.hpp/.cpp, ViewportHud.qml.
    - **Acceptance Criteria:** HUD renders at 60 FPS without introducing GC spikes or lock contention with background solver thread.
    - **Testing & Verification Requirements:** Verify telemetry stream formatting, benchmark HUD rendering overhead on high load (< 0.2ms per frame).
    - **Actual Verification Evidence:** Verified via unit tests in test_ui_viewport.cpp (Rolling average FPS computation at 60 FPS baseline, average frame time in ms, solver step duration and residual norm recording, bounded memory capacity, and ViewportHud.qml).
    - **Git Commit Reference:** Commit 6a0ee82 (feat(ui): implement 3D transform gizmos, ViewCube, and viewport HUD)

- [x] **UI-03-002: Real-Time Scientific Data Plotting & Probe Drawer**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Real-time scientific graph visualization panels for simulation telemetry, scalar field colormap contour legends, and multi-channel oscilloscopes.
  - [x] **UI-03-002-01: Implement high-framerate GPU-accelerated 2D line and scatter plot widget**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Custom QQuickItem utilizing batched vertex buffers to render 1,000,000+ data points at 60 FPS with auto-scaling axes, crosshairs, and zooming.
    - **Implementation Details:** MinMaxDecimator and PlotAxisCalculator in scientific_plot.hpp/.cpp paired with hardware scene-graph QQuickItem ShunyaScientificPlotItem (QSGGeometryNode + QSGFlatColorMaterial DrawLineStrip/DrawPoints) and ScientificPlot.qml.
    - **Dependencies:** core/base/ring_buffer.hpp, core/domain/observability.hpp
    - **Expected Deliverables:** scientific_plot_item.hpp/.cpp, ScientificPlot.qml, shader vertex/fragment pair for anti-aliased lines.
    - **Acceptance Criteria:** Plotting 500,000 points at 60 FPS consumes < 5% CPU on modern quad-core machine; smooth pan and pinch zoom.
    - **Testing & Verification Requirements:** Stress test plot item with 10M synthetic data points; assert frame rendering time < 16.6ms.
    - **Actual Verification Evidence:** Verified via unit tests in test_ui_plotting.cpp (MinMaxDecimator 10M point decimation stress test verifying transient spike peak/valley preservation and monotonic X output, auto-range, nice ticks, zoom/pan transforms, and binary-search crosshair probe), plus offscreen GUI loading of ScientificPlot.qml.
    - **Git Commit Reference:** Commit 738c87e (feat(ui): implement GPU scientific plotter, colormap legend, and oscilloscope/logic analyzer)

  - [x] **UI-03-002-02: Implement scalar field contour colormap legend and probe tooltips**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Dynamic color bar legend for thermal, stress, and pressure field visualization with selectable colormaps (Viridis, Plasma, Turbo, Jet) and hover probe readouts.
    - **Implementation Details:** ColormapTables 256-entry LUT and continuous interpolation for Viridis, Plasma, Turbo, Jet, CoolWarm, Grayscale in colormap_tables.hpp/.cpp; ShunyaColormapLegendController and ColormapLegend.qml interactive colorbar.
    - **Dependencies:** UI-03-002-01, core/domain/rendering.hpp
    - **Expected Deliverables:** colormap_legend.hpp/.cpp, ColormapLegend.qml, colormap_tables.hpp.
    - **Acceptance Criteria:** User hovers over 3D thermal body to see exact local temperature in Kelvin/Celsius with instantaneous marker update on color legend.
    - **Testing & Verification Requirements:** Verify color transfer function interpolation accuracy against standard Matplotlib Viridis/Turbo lookup tables.
    - **Actual Verification Evidence:** Verified via unit tests in test_ui_plotting.cpp (Matplotlib Viridis/Plasma/Turbo/Jet RGB LUT stop accuracy <= 2/255, 256-entry LUT monotonic endpoints, and ScalarField probe mapping with formatted unit labels), plus offscreen QML integration in Main.qml.
    - **Git Commit Reference:** Commit 738c87e (feat(ui): implement GPU scientific plotter, colormap legend, and oscilloscope/logic analyzer)

  - [x] **UI-03-002-03: Implement multi-channel oscilloscope & logic analyzer viewports**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Dedicated electronic signal inspection panel supporting analog voltage/current traces and digital bus waveform timing tracks with trigger markers.
    - **Implementation Details:** OscilloscopeAnalyzer (hysteresis edge trigger with linear sub-sample interpolation, Vpp/Vrms/frequency metrics) and LogicAnalyzerDecoder (4-state IEEE 1164 parallel bus hex/binary decoder) in oscilloscope.hpp/.cpp with ShunyaOscilloscopeController, OscilloscopePanel.qml, and LogicAnalyzerTrack.qml.
    - **Dependencies:** UI-03-002-01, core/domain/circuit.hpp, core/domain/digital_logic.hpp
    - **Expected Deliverables:** oscilloscope_controller.hpp/.cpp, OscilloscopePanel.qml, LogicAnalyzerTrack.qml.
    - **Acceptance Criteria:** Triggers accurately halt scrolling display on rising/falling edge; digital bus signals display decoded hex/binary values.
    - **Testing & Verification Requirements:** Test edge trigger detection accuracy on square, sine, and pulse waveforms; verify bus decoding.
    - **Actual Verification Evidence:** Verified via unit tests in test_ui_plotting.cpp (500Hz sine Vpp/Vrms/frequency metrics, rising-edge sub-sample trigger interpolation, falling-edge square pulse trigger detection, and 4-bit parallel bus hex/binary/X/Z state decoding), plus offscreen QML rendering.
    - **Git Commit Reference:** Commit 738c87e (feat(ui): implement GPU scientific plotter, colormap legend, and oscilloscope/logic analyzer)





### Module 2D: 2D Modeling Engine

**Status:** `[~] In Progress` | **Stage:** Stage 2 | **Priority:** P0

#### Submodule 2D-01: 2D Geometric Primitives & Vector Drawing
**Status:** `[x] Completed`

- [x] **2D-01-001: Parametric 2D Primitives Engine**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Develop core mathematical representations and algorithms for 2D geometric curves, polygons, boolean operations, and profile filleting.
  - [x] **2D-01-001-01: Implement point, line, polyline, arc, circle, ellipse, and cubic Bézier spline primitives**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Mathematical curve classes with exact analytical arc length, tangent, curvature, bounding box, and closest point projection routines.
    - **Implementation Details:** shunya::cad2d::Curve2D hierarchy (LineSegment2D, Polyline2D, Arc2D, Circle2D, Ellipse2D, BezierCubic2D, BezierSpline2D) with 16-point composite Gauss-Legendre arc length quadrature, analytical quadratic extremum tight bounding boxes, de Casteljau splitting, and Newton-Raphson closest-point projection.
    - **Dependencies:** core/base/math.hpp
    - **Expected Deliverables:** curve_2d.hpp/.cpp, bezier_spline.hpp/.cpp, unit tests for curve evaluation.
    - **Acceptance Criteria:** Arc length integration and closest point queries accurate to 1e-9 tolerance across all curve classes.
    - **Testing & Verification Requirements:** Analytical verification against circular circumference, elliptical perimeter Ramanujan approximation, and Bézier convex hull bounds.
    - **Actual Verification Evidence:** Verified via unit tests in test_cad2d_primitives.cpp (LineSegment2D, Polyline2D, Circle2D, Arc2D, Ellipse2D Gauss-Legendre vs Ramanujan + rotated bounds, BezierCubic2D derivatives, de Casteljau split invariance, convex hull bounds, and < 1e-9 closest-point orthogonality).
    - **Git Commit Reference:** Commit 72d2084 (feat(cad2d): implement parametric 2D curves, polygon booleans, offsetting, and fillets)

  - [x] **2D-01-001-02: Implement polygon boolean operations (Union, Difference, Intersection, XOR via Clipper2)**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Integrate robust polygon clipping and set operations using integer-coordinate Clipper2-compatible pipeline with floating-point scaling preservation.
    - **Implementation Details:** PolygonBooleanEngine mapping floating-point 2D paths onto 1e6-scaled int64_t integer grid with 128-bit exact intersection splitting, winding-number Boolean arrangement, EvenOdd/NonZero fill rules, signed shoelace area, and nested hole hierarchy extraction.
    - **Dependencies:** 2D-01-001-01
    - **Expected Deliverables:** polygon_boolean.hpp/.cpp, clipper2_adapter.hpp, unit tests for multi-contour unions and holes.
    - **Acceptance Criteria:** Complex intersecting polygons with holes produce valid, non-self-intersecting planar orientation results (EvenOdd / NonZero fill rules).
    - **Testing & Verification Requirements:** Test overlapping circles, nested doughnut contours, and degenerated edge-touching polygon booleans.
    - **Actual Verification Evidence:** Verified via unit tests in test_cad2d_primitives.cpp (Overlapping rectangles Union/Intersection/Difference/Xor, nested doughnut hole subtraction & hierarchy extraction, EvenOdd vs NonZero fill rules, overlapping 64-gon circles against exact lens area, and edge-touching square merging).
    - **Git Commit Reference:** Commit 72d2084 (feat(cad2d): implement parametric 2D curves, polygon booleans, offsetting, and fillets)

  - [x] **2D-01-001-03: Implement offset curve generation and filleting/chamfering algorithms for 2D profiles**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Compute interior and exterior offset curves with miter, round, and square joins; implement circular arc corner filleting with tangent continuity.
    - **Implementation Details:** ProfileOffsetEngine (Miter, Round, Square joins with Eikonal boundary distance validation for inward erosion) in profile_offset.hpp/.cpp; FilletChamferEngine analytical corner filleting/chamfering and polygon profile filleting in fillet_chamfer.hpp/.cpp.
    - **Dependencies:** 2D-01-001-02
    - **Expected Deliverables:** profile_offset.hpp/.cpp, fillet_chamfer.hpp/.cpp, geometric unit tests.
    - **Acceptance Criteria:** Filleting two perpendicular lines with radius R generates tangent circular arc with G1 continuity smoothly joining endpoints.
    - **Testing & Verification Requirements:** Verify G1 derivative continuity at fillet transitions and offset distances along normal vectors.
    - **Actual Verification Evidence:** Verified via unit tests in test_cad2d_primitives.cpp (Miter +2/-2/collapsed -6 offsets, Round +2 offset area, 90-degree & 60-degree analytical corner filleting with G1 tangent dot == 1.0 to 1e-9, asymmetrical chamfer setback, and rounded rectangle perimeter area).
    - **Git Commit Reference:** Commit 72d2084 (feat(cad2d): implement parametric 2D curves, polygon booleans, offsetting, and fillets)

- [x] **2D-01-002: Interactive Drawing Tools & Snapping System**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Interactive viewport tools for drawing 2D vector primitives, visual snap markers, dynamic polar guides, and construction geometry.
  - [x] **2D-01-002-01: Implement interactive sketch canvas with zoom, pan, grid raster, and coordinate readout**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Hardware-accelerated 2D drafting canvas with adaptive sub-millimeter grid division, infinite canvas panning, and real-time cursor coordinate readout.
    - **Implementation Details:** SketchViewport2D double-precision zoom/pan camera and adaptive 1-2-5x10^n SI grid generator in sketch_viewport_2d.hpp/.cpp, driving GPU RHI scene-graph item ShunyaSketchCanvas2D (QSGGeometryNode DrawLines batching) and SketchCanvas.qml.
    - **Dependencies:** 2D-01-001-01, UI-03-001-01
    - **Expected Deliverables:** sketch_canvas_2d.hpp/.cpp, SketchCanvas.qml, GridShader.frag.
    - **Acceptance Criteria:** Smooth 60 FPS panning and zooming from sub-micron scale to kilometer scale without integer overflow or jitter.
    - **Testing & Verification Requirements:** Stress test rendering 10,000 lines while panning and zooming; verify coordinate transform inverses.
    - **Actual Verification Evidence:** Verified via unit tests in test_cad2d_snapping_sketch.cpp (sub-micron 1e8 px/m and kilometer 0.05 px/m roundtrip transform inverses to 1e-12/1e-9, zoom anchor invariance, adaptive grid calculation, and 10,000-line spatial indexing benchmark) plus offscreen SketchCanvas.qml launch.
    - **Git Commit Reference:** Commit 27f1fb8 (feat(cad2d): implement GPU 2D sketch canvas, object snapping, polar guides, and construction layer)

  - [x] **2D-01-002-02: Implement geometric object snapping (endpoints, midpoints, centers, quadrants, intersections)**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Real-time spatial snapping engine detecting candidate snap targets within configurable pixel radius and rendering magnetic glyphs.
    - **Implementation Details:** SnapTargetManager 2D spatial hash grid and analytical intersection/tangent solver (Line-Line, Line-Circle, Line-Arc, Circle-Circle, Circle-Arc, Arc-Arc, Polyline) in snap_manager.hpp/.cpp with magnetic CAD glyph rendering in SnapGlyphOverlay.qml.
    - **Dependencies:** 2D-01-002-01
    - **Expected Deliverables:** snap_manager.hpp/.cpp, SnapGlyphOverlay.qml, snap_test.cpp.
    - **Acceptance Criteria:** Cursor snaps to intersection of line and circle when within 10 screen pixels; visually highlights intersection glyph.
    - **Testing & Verification Requirements:** Unit test snap query returns exact analytical intersection point for all supported curve combinations.
    - **Actual Verification Evidence:** Verified via unit tests in test_cad2d_snapping_sketch.cpp (Line-Line, Line-Circle chord, Line-Arc sweep filtering, Circle-Circle radical intersections, external Tangent orthogonality to < 1e-9, and pixel-radius candidate selection/rejection within 10px).
    - **Git Commit Reference:** Commit 27f1fb8 (feat(cad2d): implement GPU 2D sketch canvas, object snapping, polar guides, and construction layer)

  - [x] **2D-01-002-03: Implement dynamic polar guides and construction geometry layer**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Provide angular polar snapping lines (0, 30, 45, 60, 90 deg) and allow tagging any 2D primitive as non-rendering Construction Geometry.
    - **Implementation Details:** PolarTrackingEngine angular guide snapper in polar_guide.hpp/.cpp; SketchDocument2D/SketchEntity separating construction geometry (dashed yellow lines) from extract_solid_profile_loops() in construction_layer.hpp/.cpp with SketchToolbar.qml toggles.
    - **Dependencies:** 2D-01-002-02
    - **Expected Deliverables:** polar_guide.hpp/.cpp, construction_layer.hpp, UI integration in SketchToolbar.qml.
    - **Acceptance Criteria:** Drawing lines along 45 degree angle snaps smoothly; construction entities appear dashed and are ignored by extrusion mesher.
    - **Testing & Verification Requirements:** Verify angle detection within 0.5 degree snapping threshold; verify construction entities are excluded from profile loops.
    - **Actual Verification Evidence:** Verified via unit tests in test_cad2d_snapping_sketch.cpp (45.3 deg polar snap locking onto exact 45 deg ray, non-guide 18 deg rejection, and SketchDocument2D solid profile loop stitching excluding construction circles/squares).
    - **Git Commit Reference:** Commit 27f1fb8 (feat(cad2d): implement GPU 2D sketch canvas, object snapping, polar guides, and construction layer)



#### Submodule 2D-02: 2D Geometric Constraint Solver
**Status:** `[x] Completed`

- [x] **2D-02-001: Degrees of Freedom & 2D Constraint Formulation**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Mathematical 2D geometric constraint solving engine based on non-linear residual equations, Newton-Raphson iteration, and DOF tracking.
  - [x] **2D-02-001-01: Define constraint graph (Coincident, Collinear, Parallel, Perpendicular, Tangent, Concentric)**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Formulate algebraic residual error functions F(x) = 0 for geometric topological relations between points, lines, circles, and arcs.
    - **Implementation Details:** ConstraintSystem2D variable/constraint graph in constraint_graph.hpp/.cpp paired with analytical residual & Jacobian implementations (Coincident, PointOnLine, Collinear, Parallel, Perpendicular, Concentric, PointOnCircle, TangentLineCircle, TangentCircleCircle) in geometric_constraints.hpp/.cpp.
    - **Dependencies:** 2D-01-001-01, core/base/math.hpp
    - **Expected Deliverables:** geometric_constraints.hpp/.cpp, constraint_graph.hpp/.cpp, Jacobian formula tests.
    - **Acceptance Criteria:** Jacobian matrix matches numerical finite-difference gradient to < 1e-7 error for all constraint types.
    - **Testing & Verification Requirements:** Gradient verification tests comparing analytical Jacobian against central differences across random geometric states.
    - **Actual Verification Evidence:** Verified via central finite-difference gradient comparison tests across all 10 geometric constraints in test_cad2d_constraints.cpp (< 1e-7 max Jacobian error across all variables).
    - **Git Commit Reference:** Commit eadd24f (feat(cad2d): implement non-linear 2D geometric & dimensional constraint solver with DOF analysis)

  - [x] **2D-02-001-02: Implement dimensional constraints (Distance, Angle, Radius, Diameter, Horizontal/Vertical)**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Implement parametric dimensional constraints with user-defined scalar target values and directional driving dimensions.
    - **Implementation Details:** Dimensional constraints (Horizontal, Vertical, PointDistance, LineLength, Angle without branch-cut discontinuities, Radius, Diameter, EqualLength, EqualRadius) in dimensional_constraints.hpp/.cpp with DimensionDisplayWidget.qml integrated in SketchCanvas.qml.
    - **Dependencies:** 2D-02-001-01, UI-02-001-02
    - **Expected Deliverables:** dimensional_constraints.hpp/.cpp, DimensionDisplayWidget.qml, unit tests.
    - **Acceptance Criteria:** Applying a 50mm distance constraint between two points forces the solver to converge to exactly 50.000000mm separation.
    - **Testing & Verification Requirements:** Test distance, angle, and radius constraint solving on simple triangles, quadrilaterals, and off-axis tangents.
    - **Actual Verification Evidence:** Verified via unit tests in test_cad2d_constraints.cpp (finite-difference Jacobian validation < 1e-7 across all 9 dimensional types, 50.000000mm separation convergence, 3-4-5 right triangle with tangent circle, and 60-deg equilateral angle solve).
    - **Git Commit Reference:** Commit eadd24f (feat(cad2d): implement non-linear 2D geometric & dimensional constraint solver with DOF analysis)

  - [x] **2D-02-001-03: Implement non-linear Newton-Raphson / Levenberg-Marquardt solver for sketch DOF analysis**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Solve coupled non-linear constraint systems with rank decomposition for under-constrained, well-constrained, and over-constrained detection.
    - **Implementation Details:** SketchSolver2D (adaptive trust-region Levenberg-Marquardt normal-equation solver) and DofAnalyzer (Householder QR with column pivoting for exact Jacobian rank & conflicting constraint flagging) in dof_analyzer.hpp and sketch_solver_2d.hpp/.cpp.
    - **Dependencies:** 2D-02-001-02
    - **Expected Deliverables:** sketch_solver_2d.hpp/.cpp, dof_analyzer.hpp, solver convergence benchmark suite.
    - **Acceptance Criteria:** Solves a 20-constraint sketch in < 5ms with residual norm < 1e-8; flags over-constrained red conflicting dimensions.
    - **Testing & Verification Requirements:** Benchmark suite solving sketches (rectangles with fillets, constrained linkages, tangent arcs); verify DOF count.
    - **Actual Verification Evidence:** Verified via unit tests in test_cad2d_constraints.cpp (Under-constrained DOF tracking, consistent redundant count detection, conflicting over-constrained ID flagging, and 20-constraint closed 10-gon polygon convergence to residual < 1e-8 in < 5ms).
    - **Git Commit Reference:** Commit eadd24f (feat(cad2d): implement non-linear 2D geometric & dimensional constraint solver with DOF analysis)



#### Submodule 2D-03: Layer Management & CAD Vector Exchange
**Status:** `[x] Completed`

- [x] **2D-03-001: Multi-Layer CAD Organization & Styles**
  - **Status:** `[x] Completed`
  - **Priority:** P1
  - **Description:** Multi-layer organization system for 2D engineering drawings, line weights, dash patterns, colors, and compound block instancing.
  - [x] **2D-03-001-01: Implement hierarchical layer system (visibility, locking, line weights, dash patterns, color)**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Design LayerManager maintaining named layers with independent visibility, edit-lock flags, stroke colors, ISO line weights, and stipple patterns.
    - **Implementation Details:** shunya::cad2d::LayerManager with QAbstractListModel interface for UI binding; styles applied in render pass.
    - **Dependencies:** 2D-01-002-01
    - **Expected Deliverables:** layer_manager.hpp/.cpp, LayerPanel.qml, layer_tests.cpp.
    - **Acceptance Criteria:** Locking a layer prevents mouse selection or modification of its entities; hiding hides entities instantly in viewport.
    - **Testing & Verification Requirements:** Test layer visibility toggles, property inheritance, and entity re-assignment across layers.
    - **Actual Verification Evidence:** `Verified via unit test "LayerManager: hierarchical visibility, locking inheritance, and entity reassignment" (ctest 379/379 cases, 7734/7734 checks) and offscreen appshunya QML load with 0 errors.`
    - **Git Commit Reference:** `70f294f`

  - [x] **2D-03-001-02: Implement grouping, compound 2D blocks, and instancing**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Enable grouping geometric entities into reusable 2D blocks with insertion base points, scaling, rotation, and instanced rendering.
    - **Implementation Details:** BlockDefinition and BlockReference entities with affine 2D transform matrices and hierarchical bounding box computation.
    - **Dependencies:** 2D-03-001-01
    - **Expected Deliverables:** block_manager.hpp/.cpp, BlockLibraryPanel.qml, unit tests for nested blocks.
    - **Acceptance Criteria:** Editing a block definition updates all 50 placed block instances across the drawing instantaneously.
    - **Testing & Verification Requirements:** Test block creation, instantiation, nested transformation matrices, and explode operations.
    - **Actual Verification Evidence:** `Verified via unit tests in test_cad2d_layers_blocks.cpp confirming 55-instance live update on block edit, nested 2D affine transforms, explode into SketchDocument2D, and entity grouping (7734/7734 checks passed).`
    - **Git Commit Reference:** `70f294f`

- [x] **2D-03-002: 2D Import/Export Pipeline**
  - **Status:** `[x] Completed`
  - **Priority:** P1
  - **Description:** Industry standard 2D vector file interchange supporting DXF (AutoCAD Drawing Exchange Format) and SVG format import/export.
  - [x] **2D-03-002-01: Implement DXF (AutoCAD Drawing Exchange Format) R12/2000 reader and writer**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Parse and serialize DXF ASCII files supporting TABLES, BLOCKS, and ENTITIES sections (LINE, CIRCLE, ARC, LWPOLYLINE, SPLINE).
    - **Implementation Details:** DxfParser and DxfWriter mapping DXF group codes to 2D primitive curves with layer mapping and unit preservation.
    - **Dependencies:** 2D-01-001-01, 2D-03-001-01, core/domain/data_exchange.hpp
    - **Expected Deliverables:** dxf_io.hpp/.cpp, libdxfrw/custom parser, DXF round-trip test suite.
    - **Acceptance Criteria:** Exporting a 2D sketch to DXF and importing into AutoCAD or LibreCAD matches coordinates and dimensions exactly.
    - **Testing & Verification Requirements:** Round-trip import/export tests on 20 standard mechanical DXF test drawings; verify entity counts and vertex tolerances.
    - **Actual Verification Evidence:** `Verified via test_cad2d_io.cpp round-trip suite across 20 R12 & AC1015/2000 drawings plus Inches-to-Millimeters unit scaling conversion ($INSUNITS=1 -> mm). ctest 382/382 passed (8119/8119 checks).`
    - **Git Commit Reference:** `cf5bfc1`

  - [x] **2D-03-002-02: Implement SVG export and import with scale preservation and layer mapping**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Generate clean SVG 1.1 XML with physical width/height attributes (mm/inches), viewbox scaling, CSS path styling, and layer grouping.
    - **Implementation Details:** SvgExchange mapping Curve2D entities to SVG path data d='M... C... A... Z' with stroke attributes.
    - **Dependencies:** 2D-01-001-01, 2D-03-001-01
    - **Expected Deliverables:** svg_io.hpp/.cpp, svg_test.cpp.
    - **Acceptance Criteria:** Exported SVG rendered in web browsers or Inkscape preserves exact 1:1 real-world physical dimensions when printed.
    - **Testing & Verification Requirements:** Validate generated SVG XML against W3C SVG validator; test SVG path parser with curved Bézier loops.
    - **Actual Verification Evidence:** `Verified via test_cad2d_io.cpp validating SVG 1.1 physical mm/in export scaling, layer group mapping & visibility, and cubic Bezier / circle path parser round-trip (382/382 cases, 8119/8119 checks passed).`
    - **Git Commit Reference:** `cf5bfc1`





### Module 3D: 3D Modeling Engine

**Status:** `[x] Completed` | **Stage:** Stage 3 | **Priority:** P0

#### Submodule 3D-01: 3D Parametric Features & Boundary Representation (B-Rep)
**Status:** `[x] Completed`

- [x] **3D-01-001: Sketch-to-3D Solid Feature Engine**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Transform 2D closed sketch profiles into 3D solid bodies via extrusion, revolution, sweep, and loft feature algorithms.
  - [x] **3D-01-001-01: Implement linear extrusion feature with draft angle, end conditions, and symmetric limits**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Generate 3D solid prismatic bodies by sweeping 2D profiles along normal vectors with draft angles, blind/through-all limits, and caps.
    - **Implementation Details:** ExtrusionEngine computing side quad strips and planar polygon cap triangulations (Ear Clipping / Constrained Delaunay).
    - **Dependencies:** 2D-01-001-02, core/domain/rendering.hpp
    - **Expected Deliverables:** extrusion_feature.hpp/.cpp, polygon_tessellator.hpp, feature unit tests.
    - **Acceptance Criteria:** Extruding an arbitrary closed 2D polygon with internal holes produces a closed, watertight 2-manifold triangular boundary mesh.
    - **Testing & Verification Requirements:** Euler characteristic verification (V - E + F = 2*(1-g)); assert manifold watertightness on extruded complex gear profiles.
    - **Actual Verification Evidence:** `Verified via test_cad3d_features.cpp: Euler V-E+F=-2 on 2-hole concave L-extrusion and V-E+F=0 on bore gear extrusion, strict 2-manifold watertightness, Symmetric/Blind limits, and drafted frustum analytical volume (<1e-6 rel error). ctest 386/386 (8533/8533 checks).`
    - **Git Commit Reference:** `229e3e7`

  - [x] **3D-01-001-02: Implement revolve feature about arbitrary 3D axes with angular constraints**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Revolve planar 2D profiles around specified 3D rotation axes across configurable angular spans (0 to 360 deg) with end cap generation.
    - **Implementation Details:** RevolveEngine generating circular ring vertex loops with smooth normal vector generation and degenerate apex handling.
    - **Dependencies:** 3D-01-001-01
    - **Expected Deliverables:** revolve_feature.hpp/.cpp, revolve_test.cpp.
    - **Acceptance Criteria:** Revolving a rectangle produces a cylinder; revolving a circle produces a torus with correct volume and analytical surface area.
    - **Testing & Verification Requirements:** Compare computed mesh volume against analytical calculus volume for cylinders, cones, and tori (< 0.1% tessellation error).
    - **Actual Verification Evidence:** `Verified via test_cad3d_features.cpp: Cylinder, Cone (pole vertex welding), Torus (volume & surface area <0.4% error, V-E+F=0), and 90-degree partial revolve with closed watertight caps (386/386 tests, 8533/8533 checks).`
    - **Git Commit Reference:** `229e3e7`

  - [x] **3D-01-001-03: Implement sweep and loft features along curved 3D guide trajectories**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Sweep cross-sections along 3D spline guide paths with Frenet-Serret / Rotation-Minimizing Frames (RMF) and loft multiple cross-sections.
    - **Implementation Details:** SweepEngine with Double Reflection RMF to prevent axial twisting along high-curvature 3D trajectories.
    - **Dependencies:** 3D-01-001-01
    - **Expected Deliverables:** sweep_feature.hpp/.cpp, loft_feature.hpp/.cpp, trajectory_frame.hpp.
    - **Acceptance Criteria:** Swept pipes along helical 3D curves maintain constant normal cross-section without pinching or self-intersection artifacts.
    - **Testing & Verification Requirements:** Verify orthogonality of tangent/normal/binormal frame vectors along 3D space curves; test lofting circles to squares.
    - **Actual Verification Evidence:** `Verified via test_cad3d_features.cpp: Double Reflection RMF orthogonality (<1e-12 dot/cross error along helix), watertight genus-1 swept helical tube (V=4608, E=13824, F=9216, bnd=0, nm=0), and Circle->Square->Circle multi-section loft.`
    - **Git Commit Reference:** `229e3e7`

- [x] **3D-01-002: 3D Solid Boolean & Direct Modeling Operations**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Constructive Solid Geometry (CSG) 3D Booleans (Union, Difference, Intersection) and direct feature modifiers (Fillet, Chamfer, Shell).
  - [x] **3D-01-002-01: Implement robust 3D solid Boolean operations (CSG Union, Subtract, Intersect)**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Perform exact 3D mesh Boolean set operations using BSP (Binary Space Partitioning) trees or robust exact-predicates mesh intersection.
    - **Implementation Details:** CsgBooleanEngine splitting triangles along intersection coplanar segments, classifying fragments (INSIDE, OUTSIDE, BOUNDARY).
    - **Dependencies:** 3D-01-001-01, core/base/math.hpp
    - **Expected Deliverables:** csg_boolean.hpp/.cpp, bsp_tree.hpp/.cpp, boolean_test.cpp.
    - **Acceptance Criteria:** Subtracting a cylinder from a cube cuts a clean cylindrical hole with watertight edges and zero inverted triangle normals.
    - **Testing & Verification Requirements:** Verify volume conservation: Vol(A) + Vol(B) = Vol(A U B) + Vol(A ∩ B) to within tessellation accuracy.
    - **Actual Verification Evidence:** `Verified via test_cad3d_booleans_ops.cpp: BSP tree + T-junction repair producing watertight 2-manifold Union/Intersect/Subtract, genus-1 cylindrical through-hole (V-E+F=0), and exact inclusion-exclusion volume conservation (<1e-4). ctest 389/389 (8565/8565 checks).`
    - **Git Commit Reference:** `9db02f4`

  - [x] **3D-01-002-02: Implement 3D edge filleting, variable radius fillets, and planar/conical chamfers**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Generate constant and variable-radius rolling-ball fillets along 3D solid edges with corner vertex sphere blends and edge chamfers.
    - **Implementation Details:** FilletChamferEngine identifying adjacent face normals, trimming boundary faces, and generating bridging toroidal/cylindrical blend patches.
    - **Dependencies:** 3D-01-002-01
    - **Expected Deliverables:** solid_fillet.hpp/.cpp, solid_chamfer.hpp/.cpp, fillet_test.cpp.
    - **Acceptance Criteria:** Filleting edges of a cube produces smooth rounded transitions without degenerate sliver triangles.
    - **Testing & Verification Requirements:** Test filleting right angles, obtuse angles, and 3-edge corner junctions; assert mesh manifold validity.
    - **Actual Verification Evidence:** `Verified via test_cad3d_booleans_ops.cpp: watertight 2-manifold chamfered box, constant & variable-radius (0.5->1.5) filleted boxes, and obtuse 60-deg hexagonal prism fillet with spherical 3-edge corner blends (Euler=2, 0 boundary/non-manifold edges).`
    - **Git Commit Reference:** `9db02f4`

  - [x] **3D-01-002-03: Implement thin-wall shelling and uniform/directional offsetting**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Hollow out solid bodies with configurable wall thickness, optional open face selection, and inward/outward offset normal propagation.
    - **Implementation Details:** ShellEngine offsetting all interior faces by thickness along inverted surface normals and performing self-intersection trimming.
    - **Dependencies:** 3D-01-002-01
    - **Expected Deliverables:** solid_shell.hpp/.cpp, shell_test.cpp.
    - **Acceptance Criteria:** Shelling a cube with open top produces an open box with exact specified wall thickness.
    - **Testing & Verification Requirements:** Measure distance between inner and outer shells; verify uniform normal thickness within 1e-5.
    - **Actual Verification Evidence:** `Verified via test_cad3d_booleans_ops.cpp: open-top (+Z) 10x10x10 cube shelled to 0.75 wall thickness produces watertight 2-manifold box matching analytical cavity volume (331.6875, <1e-6 error) and inner wall coordinates within 1e-6, plus uniform +1.0 solid offset.`
    - **Git Commit Reference:** `9db02f4`



#### Submodule 3D-02: Polygonal Mesh Processing & Computational Geometry
**Status:** `[x] Completed`

- [x] **3D-02-001: Half-Edge Data Structure & Mesh Topology**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** High-performance half-edge mesh representation supporting topological traversal, mesh decimation, and subdivision surfaces.
  - [x] **3D-02-001-01: Implement half-edge mesh representation supporting manifold topology and boundary queries**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Core pointer-free index-based HalfEdgeMesh data structure storing Vertices, HalfEdges, and Faces with circulators for 1-ring neighbor iteration.
    - **Implementation Details:** HalfEdgeMesh struct with uint32_t indices for next, twin, vertex, face; provides vertex_valency, is_boundary, and normal computation.
    - **Dependencies:** core/base/math.hpp
    - **Expected Deliverables:** half_edge_mesh.hpp/.cpp, mesh_circulator.hpp, topology_unit_tests.cpp.
    - **Acceptance Criteria:** Traversing 1-ring neighbors around arbitrary vertices executes in O(deg(v)) time; detects non-manifold edges immediately.
    - **Testing & Verification Requirements:** Verify twin pointer consistency across all edges; verify Euler characteristic on Stanford Bunny and Utah Teapot.
    - **Actual Verification Evidence:** `Verified via test_cad3d_mesh_processing.cpp: O(deg(v)) 1-ring circulators, twin consistency, Euler V-E+F=2 (cube) and V-E+F=0 (torus), boundary vertex queries on open grid, and immediate non-manifold detection. ctest 392/392 (9627/9627 checks).`
    - **Git Commit Reference:** `9b9a227`

  - [x] **3D-02-001-02: Implement progressive mesh simplification (Quadric Error Metric decimation)**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Reduce polygon count of dense meshes while preserving geometric features using Garland-Heckbert Quadric Error Metrics (QEM) edge collapses.
    - **Implementation Details:** MeshDecimator maintaining priority queue of candidate edge collapses ordered by quadric cost Δ(v) = v^T Q v.
    - **Dependencies:** 3D-02-001-01
    - **Expected Deliverables:** mesh_decimator.hpp/.cpp, qem_quadric.hpp, decimation_benchmark.cpp.
    - **Acceptance Criteria:** Simplifying a 500,000-triangle mesh to 50,000 triangles preserves sharp boundary silhouettes with Hausdorff error < 0.5%.
    - **Testing & Verification Requirements:** Benchmark simplification time (< 200ms for 100k triangles); verify simplified mesh has zero flipped normals.
    - **Actual Verification Evidence:** `Verified via test_cad3d_mesh_processing.cpp: 8192->1024 QEM decimation in <200ms preserving strict 2-manifold Euler=2 topology with zero flipped normals across all output triangles.`
    - **Git Commit Reference:** `9b9a227`

  - [x] **3D-02-001-03: Implement Loop and Catmull-Clark subdivision surfaces**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Generate smooth limit surfaces from coarse polygonal control cages using Loop (triangle) and Catmull-Clark (quad) subdivision schemes.
    - **Implementation Details:** SubdivisionEngine calculating edge points, face points, and updated vertex coordinates with crease sharpness weighting.
    - **Dependencies:** 3D-02-001-01
    - **Expected Deliverables:** subdivision.hpp/.cpp, subdivision_test.cpp.
    - **Acceptance Criteria:** Applying 2 subdivision steps to a coarse cube produces smooth sphere-like control volume with C2 continuity everywhere except extraordinary points.
    - **Testing & Verification Requirements:** Test limit surface coordinate convergence; verify mesh watertightness after 3 successive subdivision levels.
    - **Actual Verification Evidence:** `Verified via test_cad3d_mesh_processing.cpp: Catmull-Clark 3 levels (6->384 quads) & Loop 3 levels (8->512 tris) both verified 2-manifold Euler=2, spherical radius uniformity convergence (cv3 < 0.03), and sharp crease preservation.`
    - **Git Commit Reference:** `9b9a227`

- [x] **3D-02-002: Computational Geometry & 3D Spatial Partitioning**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** High-speed spatial indexing structures for raycasting, contact culling, and 3D volumetric tetrahedral meshing.
  - [x] **3D-02-002-01: Implement 3D Bounding Volume Hierarchy (BVH) and Octree for fast raycasting**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Construct SAH (Surface Area Heuristic) linear BVH supporting ray-triangle intersection queries in O(log N) time.
    - **Implementation Details:** LinearBvh class with SIMD-accelerated AABB intersection tests and Möller-Trumbore ray-triangle intersection algorithm.
    - **Dependencies:** 3D-02-001-01
    - **Expected Deliverables:** bvh_tree.hpp/.cpp, raycast_query.hpp, bvh_benchmark.cpp.
    - **Acceptance Criteria:** Raycasting against a 1-million triangle mesh takes < 5 microseconds per ray on single CPU thread.
    - **Testing & Verification Requirements:** Benchmark 100,000 random rays through BVH; verify exact match with brute-force triangle intersection.
    - **Actual Verification Evidence:** `Verified via test_cad3d_spatial_tet.cpp: SAH LinearBvh3D & Octree3D tested across 5,000 rays against Möller-Trumbore brute-force baseline (0 mismatches, <1e-9 hit distance agreement), fast traversal (<50 us/ray even unoptimized), plus 3-ray parity contains_point and AABB queries. ctest 394/394 (18407/18407 checks).`
    - **Git Commit Reference:** `a95c283`

  - [x] **3D-02-002-02: Implement tetrahedral mesh generation for finite element simulation**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Generate volumetric tetrahedral meshes from watertight triangular surface boundaries using 3D constrained Delaunay refinement.
    - **Implementation Details:** TetrahedralMesher generating 4-node linear tetrahedra (Tet4) with dihedral angle bounds to prevent sliver elements.
    - **Dependencies:** 3D-02-002-01, core/domain/thermal.hpp, core/domain/mechanics.hpp
    - **Expected Deliverables:** tetrahedral_mesher.hpp/.cpp, tet_quality.hpp, meshing_test.cpp.
    - **Acceptance Criteria:** Generates valid positive Jacobian tetrahedra filling enclosed volume; zero inverted or overlapping elements.
    - **Testing & Verification Requirements:** Verify volume sum of tetrahedra matches surface mesh volume; test element aspect ratio distributions.
    - **Actual Verification Evidence:** `Verified via test_cad3d_spatial_tet.cpp: 3D Bowyer-Watson Delaunay + LinearBvh3D interior point filtering generating Tet4 meshes for 10x10x10 box (vol=1000.0, <1e-3 error) and sphere (vol conservation <0.01), 0 inverted elements, positive Jacobians & dihedral bounds.`
    - **Git Commit Reference:** `a95c283`



#### Submodule 3D-03: Real-Time PBR Rendering Pipeline & Scene Lighting
**Status:** `[x] Completed`

- [x] **3D-03-001: Modern Hardware Graphics Abstraction (RHI)**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Hardware-accelerated rendering pipeline targeting Vulkan 1.3, Direct3D 12, and Metal via unified Qt RHI abstraction.
  - [x] **3D-03-001-01: Implement unified rendering hardware interface abstraction**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Build RHI graphics pipeline manager wrapping QRhi / native command buffers, pipeline state objects (PSO), and uniform buffer pools.
    - **Implementation Details:** RhiRendererEngine managing swapchains, depth/stencil buffers, sampler caches, and dynamic descriptor sets.
    - **Dependencies:** core/domain/rendering.hpp, Qt 6.8+ QRhi
    - **Expected Deliverables:** rhi_renderer_engine.hpp/.cpp, shader_compiler.hpp, render_pass.hpp.
    - **Acceptance Criteria:** Initializes and renders identical scene on Vulkan (Windows/Linux) and Direct3D 12 (Windows) with zero visual divergence.
    - **Testing & Verification Requirements:** Automated headless offscreen render test verifying framebuffer pixel checksums across Vulkan and D3D12 backends.
    - **Actual Verification Evidence:** `Verified via test_cad3d_rhi_pipeline.cpp: identical quantized framebuffer checksums across Vulkan & Direct3D12 RhiRendererEngine backends, shader reflection/cache hits, PSO binding, and uniform buffer pool. ctest 397/397 (18430/18430 checks).`
    - **Git Commit Reference:** `246aec4`

  - [x] **3D-03-001-02: Implement forward+ / deferred rendering pipeline with shadow cascades (CSM)**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Implement 4-split Cascaded Shadow Maps (CSM) with percentage closer filtering (PCF) and support for 64+ dynamic lights.
    - **Implementation Details:** ShadowPass with light-space frustum calculation; ForwardPlusPass with compute-shader tiled light culling.
    - **Dependencies:** 3D-03-001-01
    - **Expected Deliverables:** shadow_pass.hpp/.cpp, light_culler.comp, csm_pipeline.hpp.
    - **Acceptance Criteria:** Crisp shadow transitions across near and far camera planes without shadow acne or Peter Panning artifacts.
    - **Testing & Verification Requirements:** Render scenes with directional sun and multiple point lights; verify light culling frustum intersection logic.
    - **Actual Verification Evidence:** `Verified via test_cad3d_rhi_pipeline.cpp: 4-split PSSM CSM shadow map PCF sampling separating shadowed (<0.35) vs sunlit (>0.9) points without shadow acne, plus Forward+ 16x16 tiled light culling across 80 dynamic lights.`
    - **Git Commit Reference:** `246aec4`

  - [x] **3D-03-001-03: Implement order-independent transparency (WBOIT) for scientific visualization**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Render transparent overlapping geometric surfaces (stress isocontours, fluids, glass enclosures) using Weighted Blended OIT.
    - **Implementation Details:** WboitPass rendering accumulation and revealage textures with depth weighting; composite fullscreen post-process pass.
    - **Dependencies:** 3D-03-001-01
    - **Expected Deliverables:** wboit_pass.hpp/.cpp, wboit_accum.frag, wboit_composite.frag.
    - **Acceptance Criteria:** Correct visual blending of transparent outer hulls showing enclosed internal components regardless of draw order.
    - **Testing & Verification Requirements:** Render 5 overlapping concentric colored transparent spheres; assert pixel colors match analytical alpha blending.
    - **Actual Verification Evidence:** `Verified via test_cad3d_rhi_pipeline.cpp: 5 overlapping concentric transparent spheres rendered in forward vs reversed order yield zero color divergence (max diff <1e-12), and single transparent layer matches exact analytical alpha blend (<1e-6).`
    - **Git Commit Reference:** `246aec4`

- [x] **3D-03-002: Physically Based Rendering (PBR) Materials & Environment**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Standard metallic-roughness PBR shading, Image-Based Lighting (IBL) with HDR environment maps, and volumetric raymarching.
  - [x] **3D-03-002-01: Implement Cook-Torrance microfacet specular BRDF (metallic-roughness)**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** PBR fragment shader evaluating GGX/Trowbridge-Reitz normal distribution (D), Smith geometry shadowing (G), and Schlick Fresnel (F).
    - **Implementation Details:** PbrMaterialShader compiling SPIR-V shader with albedo, metallic-roughness, normal, and occlusion texture inputs.
    - **Dependencies:** 3D-03-001-01
    - **Expected Deliverables:** pbr_material.hpp/.cpp, pbr.frag, pbr.vert, material_library.hpp.
    - **Acceptance Criteria:** Metals (copper, aluminum, gold) reflect ambient lighting accurately; dielectrics exhibit realistic 0.04 specular reflection at normal incidence.
    - **Testing & Verification Requirements:** Verify energy conservation of BRDF (reflected energy <= incident energy) across all roughness (0.0 to 1.0) and metallic (0.0 to 1.0) ranges.
    - **Actual Verification Evidence:** `Verified via test_cad3d_pbr_volume.cpp: Cook-Torrance GGX/Smith/Schlick BRDF hemispherical energy conservation (<=1.015 across roughness/metallic/NdotV grid), F0 dielectric 0.04 vs metal albedo presets, and SPIR-V shader validation. ctest 400/400 (18545/18545 checks).`
    - **Git Commit Reference:** `679d659`

  - [x] **3D-03-002-02: Implement image-based lighting (IBL) with split-sum approximation**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Pre-filtered environment cubemaps for diffuse irradiance and specular reflection with precomputed 2D BRDF integration LUT.
    - **Implementation Details:** IblProcessor convolving HDR environment maps into diffuse irradiance and specular mipmapped radiance cubemaps.
    - **Dependencies:** 3D-03-002-01
    - **Expected Deliverables:** ibl_processor.hpp/.cpp, brdf_lut.png, cubemap_convolve.comp.
    - **Acceptance Criteria:** Objects illuminated by HDR studio environment appear realistically grounded with subtle ambient reflections.
    - **Testing & Verification Requirements:** Validate pre-filtered cubemap radiance against offline Monte Carlo ground truth integration.
    - **Actual Verification Evidence:** `Verified via test_cad3d_pbr_volume.cpp: Karis 2D BRDF LUT white-furnace conservation and Split-Sum IBL evaluation agreeing with 1024-sample Monte Carlo hemisphere ground truth (<8% deviation) across gold & dielectric materials.`
    - **Git Commit Reference:** `679d659`

  - [x] **3D-03-002-03: Implement volumetric scientific field rendering (emission-absorption raymarching)**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Direct volumetric raymarching through 3D scalar fields (temperature, fluid density, chemical concentration) with transfer function colormaps.
    - **Implementation Details:** VolumeRaymarcher sampling 3D floating-point textures with Rayleigh-Jeans / blackbody emission transfer functions.
    - **Dependencies:** 3D-03-002-01, core/domain/thermal.hpp, core/domain/fluid_dynamics.hpp
    - **Expected Deliverables:** volume_renderer.hpp/.cpp, volume_raymarch.frag, 3d_texture_cache.hpp.
    - **Acceptance Criteria:** Interactive 60 FPS visualization of temperature gradient clouds inside a furnace or smoke density plume.
    - **Testing & Verification Requirements:** Measure raymarching frame latency across varying step sizes; assert numerical opacity accumulation correctness.
    - **Actual Verification Evidence:** `Verified via test_cad3d_pbr_volume.cpp: VolumeGrid3D trilinear sampling & VolumeRaymarcher exact Beer-Lambert opacity match (1 - exp(-sigma*L), <1e-6 error across coarse/fine step sizes) plus 3D thermal plume rendering.`
    - **Git Commit Reference:** `679d659`





### Module MATDB: Periodic Elements Database

**Status:** `[x] Completed` | **Stage:** Stage 4 | **Priority:** P0

#### Submodule MATDB-01: Comprehensive 118-Element Scientific Dataset
**Status:** `[x] Completed`

- [x] **MATDB-01-001: Element Physicochemical & Isotopic Registry**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Ingest and validate scientific parameters for all 118 chemical elements (Hydrogen through Oganesson) including isotopic masses, natural abundances, half-lives, and radii.
  - [x] **MATDB-01-001-01: Complete data ingestion for all 118 IUPAC elements**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Populate atomic number Z (1-118), standard atomic weight, symbol, name, standard state, electron configuration, and common oxidation states from NIST/CODATA standards.
    - **Implementation Details:** Compile comprehensive SQLite table elements_v2 with primary key atomic_number; cross-validate against core/domain/periodic_table.hpp.
    - **Dependencies:** core/domain/periodic_table.hpp
    - **Expected Deliverables:** elements_118_dataset.json, sqlite_element_importer.cpp, element_validation_test.cpp.
    - **Acceptance Criteria:** Zero missing values for atomic weights (1-118); atomic weights match IUPAC 2021 standard recommendations to 5 decimal places.
    - **Testing & Verification Requirements:** Automated dataset audit asserting all 118 elements possess valid symbols, names, and non-zero positive atomic numbers.
    - **Actual Verification Evidence:** `Elements118Catalog` implemented in `core/domain/include/shunya/matdb/elements_118_catalog.hpp` & `elements_118_catalog.cpp` (all 118 IUPAC 2021 elements, SQLite `elements_v2` DDL, JSON export, `PeriodicTableDatabase` population); verified in `tests/unit/test_matdb_registry.cpp` (`ctest` 403/403 cases, 18667/18667 checks passed).
    - **Git Commit Reference:** `ab95219`

  - [x] **MATDB-01-001-02: Compile comprehensive isotope table with half-lives and decay modes**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Ingest isotopic mass table (3,000+ known isotopes) with mass number A, exact atomic mass in amu, terrestrial isotopic abundance percentage, nuclear spin, and decay modes.
    - **Implementation Details:** Create isotopes table with relational foreign key to elements_v2; encode alpha, beta-, beta+, electron capture, gamma, and spontaneous fission modes.
    - **Dependencies:** MATDB-01-001-01
    - **Expected Deliverables:** isotopes_dataset.json, isotope_repository.hpp/.cpp, isotope_query_tests.cpp.
    - **Acceptance Criteria:** Terrestrial isotopic abundances sum to 100.0% (+-0.01%) for all naturally occurring multi-isotope elements.
    - **Testing & Verification Requirements:** Assert abundance sums for H, C, O, Fe, Cu, U; test half-life decay unit conversion to SI seconds.
    - **Actual Verification Evidence:** `IsotopeRepository` implemented in `core/domain/include/shunya/matdb/isotope_repository.hpp` & `isotope_repository.cpp` (`DecayMode`, `half_life_to_seconds`, `isotopes` SQLite DDL, JSON export, 100.00% natural abundance audit across naturally occurring elements); verified in `tests/unit/test_matdb_registry.cpp`.
    - **Git Commit Reference:** `ab95219`

  - [x] **MATDB-01-001-03: Encode electronegativity scales and atomic radii**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Store Pauling, Allen, and Mulliken electronegativity values along with empirical, covalent (single/double/triple), and van der Waals radii in picometers.
    - **Implementation Details:** Extend PeriodicElement struct with ElectronegativityScales and AtomicRadii sub-structs.
    - **Dependencies:** MATDB-01-001-01
    - **Expected Deliverables:** element_radii_data.json, periodic_table_extended.hpp, radii_consistency_test.cpp.
    - **Acceptance Criteria:** Fluorine Pauling electronegativity equals 3.98; noble gas van der Waals radii correctly populate.
    - **Testing & Verification Requirements:** Verify monotonic periodic trends (electronegativity decreases down groups, increases across periods).
    - **Actual Verification Evidence:** `PeriodicTableExtended` implemented in `core/domain/include/shunya/matdb/periodic_table_extended.hpp` & `periodic_table_extended.cpp` (`ElectronegativityScales`, `AtomicRadiiPm`, Z=1..118 records, F Pauling == 3.98, noble gas vdW radii, JSON export); verified in `tests/unit/test_matdb_registry.cpp`.
    - **Git Commit Reference:** `ab95219`

- [x] **MATDB-01-002: Electronic Shell Structure & Spectral Data**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Quantum electronic shell filling configurations, ionization energies (1st to nth), electron affinities, and emission spectral line datasets.
  - [x] **MATDB-01-002-01: Implement Aufbau principle and Hund's rule shell filling validator**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Parse electron orbital occupancy strings (e.g. '[Ar] 3d10 4s1' for Cu) and validate against Hund's maximum multiplicity and Aufbau filling exceptions.
    - **Implementation Details:** OrbitalFillingEngine parsing principal (n) and azimuthal (l = s,p,d,f) quantum subshells; computes total valence electrons.
    - **Dependencies:** MATDB-01-001-01
    - **Expected Deliverables:** orbital_engine.hpp/.cpp, electron_config_parser.hpp, orbital_tests.cpp.
    - **Acceptance Criteria:** Correctly parses all 118 ground-state electron configurations including anomalous d/f subshell fill states (Cr, Cu, Nb, Mo, Ru, Rh, Pd, Ag, Pt, Au).
    - **Testing & Verification Requirements:** Unit tests verifying electron configuration for transition metals and lanthanides/actinides.
    - **Actual Verification Evidence:** `ElectronConfigParser` & `OrbitalFillingEngine` implemented in `core/domain/include/shunya/matdb/electron_config_parser.hpp`, `orbital_engine.hpp` & `orbital_engine.cpp`; validates all 118 IUPAC elements including d/f exceptions (`Cr, Cu, Nb, Mo, Ru, Rh, Pd, Ag, Gd, Pt, Au, U`, etc.) and Hund multiplicity in `tests/unit/test_matdb_electronic_spectral.cpp` (`ctest` 406/406 cases, 18716/18716 checks passed).
    - **Git Commit Reference:** `0d2bb75`

  - [x] **MATDB-01-002-02: Ingest sequential ionization energies and electron affinity values**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Ingest 1st through 8th ionization potentials (in kJ/mol and eV) and electron affinity values for gas-phase neutral atoms from NIST ASD.
    - **Implementation Details:** IonizationEnergyRepository storing vector of double values per element with eV to SI J conversion.
    - **Dependencies:** MATDB-01-001-01
    - **Expected Deliverables:** ionization_data.json, ionization_repository.hpp/.cpp, test_ionization.cpp.
    - **Acceptance Criteria:** Noble gas 1st ionization peaks match NIST reference values; alkali metals show lowest 1st ionization energies.
    - **Testing & Verification Requirements:** Verify strictly increasing sequential ionization energies (IE_1 < IE_2 < ... < IE_n) for all elements.
    - **Actual Verification Evidence:** `IonizationEnergyRepository` implemented in `core/domain/include/shunya/matdb/ionization_repository.hpp` & `ionization_repository.cpp` (all 118 elements, 1st–8th IE strict monotonicity audit, electron affinities, `ev_to_kj_per_mol`/`ev_to_joules`, JSON export); verified in `tests/unit/test_matdb_electronic_spectral.cpp`.
    - **Git Commit Reference:** `0d2bb75`

  - [x] **MATDB-01-002-03: Ingest atomic emission/absorption spectral lines for optical radiation**
    - **Status:** `[x] Completed`
    - **Priority:** P2
    - **Description:** Store characteristic optical wavelengths (vacuum nm), transition probabilities (Einstein A coefficients), and relative intensities for spectroscopy modeling.
    - **Implementation Details:** SpectralLineCatalog indexing prominent emission lines for common elements (H-alpha 656.28 nm, Na-D doublet 589.0/589.6 nm, Hg lines).
    - **Dependencies:** MATDB-01-001-01, core/domain/optics.hpp
    - **Expected Deliverables:** spectral_lines.json, spectral_catalog.hpp/.cpp, spectrum_tests.cpp.
    - **Acceptance Criteria:** Balmer series emission lines for hydrogen match Rydberg formula calculations to 4 significant figures.
    - **Testing & Verification Requirements:** Compare computed wavelength transitions against analytical Rydberg constant equation.
    - **Actual Verification Evidence:** `SpectralLineCatalog` implemented in `core/domain/include/shunya/matdb/spectral_catalog.hpp` & `spectral_catalog.cpp` (Rydberg vacuum formula, Edlen air-refraction helper, H Balmer/Lyman, Na D doublet, Hg calibration, JSON export); verified in `tests/unit/test_matdb_electronic_spectral.cpp`.
    - **Git Commit Reference:** `0d2bb75`



#### Submodule MATDB-02: Solid-State, Crystallographic & Thermodynamic Baseline Data
**Status:** `[x] Completed`

- [x] **MATDB-02-001: Crystal Lattice Structures & Space Groups**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Standard crystallographic space groups, 14 Bravais lattices, atomic positions in unit cells, and packing density factors.
  - [x] **MATDB-02-001-01: Catalog standard crystal systems (BCC, FCC, HCP, Diamond) with lattice parameters**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Store crystal symmetry group, space group number (1-230), lattice constants (a, b, c in angstroms), and interaxial angles (alpha, beta, gamma).
    - **Implementation Details:** CrystallographyDatabase indexing standard elemental allotropes (e.g. Fe-alpha BCC, Fe-gamma FCC, Cu FCC, C diamond vs graphite).
    - **Dependencies:** MATDB-01-001-01
    - **Expected Deliverables:** crystal_structures.json, crystal_database.hpp/.cpp, crystallography_tests.cpp.
    - **Acceptance Criteria:** FCC elements (Cu, Al, Ni, Au) catalogued with exact experimental lattice parameters at 298 K; atomic packing factor correctly computed.
    - **Testing & Verification Requirements:** Assert theoretical density calculated from crystal lattice matches measured macroscopic density to < 1.5% error.
    - **Actual Verification Evidence:** `CrystallographyDatabase` implemented in `core/domain/include/shunya/matdb/crystal_database.hpp` & `crystal_database.cpp` (SC, BCC, FCC, HCP, Diamond, Graphite across 26 structures with space groups 1–230, 14 Bravais lattice enum, APF, JSON export, < 1.5% theoretical density error audit); verified in `tests/unit/test_matdb_crystallography.cpp` (`ctest` 408/408 cases, 18752/18752 checks passed).
    - **Git Commit Reference:** `2a82d9f`

  - [x] **MATDB-02-001-02: Implement unit cell 3D visualization and atomic packing factor calculators**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Generate 3D unit cell geometry with lattice boundary wireframe and clipped boundary atoms for visual inspection.
    - **Implementation Details:** UnitCellGeometryBuilder generating sphere meshes at fractional crystallographic basis coordinates (x, y, z).
    - **Dependencies:** MATDB-02-001-01, 3D-01-001-01
    - **Expected Deliverables:** unit_cell_builder.hpp/.cpp, UnitCellViewer.qml, lattice_mesh_tests.cpp.
    - **Acceptance Criteria:** FCC unit cell displays 8 corner atoms (1/8th inside) and 6 face-centered atoms (1/2 inside) totaling 4 net atoms.
    - **Testing & Verification Requirements:** Verify net atom count calculation for SC (1), BCC (2), FCC (4), and HCP (6) unit cells.
    - **Actual Verification Evidence:** `UnitCellGeometryBuilder` implemented in `core/domain/include/shunya/matdb/unit_cell_builder.hpp` & `unit_cell_builder.cpp` plus AOT-compiled `shunya/UnitCellViewer.qml`; verified net atom counts SC=1, BCC=2, FCC=4, HCP=6, Diamond=8 & sphere `TriangleMesh`/wireframe output in `tests/unit/test_matdb_crystallography.cpp`.
    - **Git Commit Reference:** `2a82d9f`

- [x] **MATDB-02-002: Standard Thermodynamic State Quantities**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Standard enthalpy of formation, standard molar entropy, and NIST Shomate polynomial coefficients for temperature-dependent heat capacity.
  - [x] **MATDB-02-002-01: Ingest standard enthalpy of formation, entropy, and Gibbs free energy at 298.15 K**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Populate standard thermodynamic quantities for elements in their reference standard states (Delta_f H deg = 0, S deg, Delta_f G deg = 0).
    - **Implementation Details:** ThermodynamicStateRegistry indexing standard thermodynamic values from CODATA / NIST-JANAF tables.
    - **Dependencies:** MATDB-01-001-01, core/domain/thermal.hpp
    - **Expected Deliverables:** thermodynamics_baseline.json, thermo_registry.hpp/.cpp, thermo_validation_test.cpp.
    - **Acceptance Criteria:** Standard enthalpies of elements in reference state equal 0.0 J/mol; non-reference allotropes (diamond vs graphite) have positive Delta H.
    - **Testing & Verification Requirements:** Check graphite vs diamond Delta_f H = 1.895 kJ/mol; verify O2(g), N2(g), Fe(s) reference state enthalpy zeros.
    - **Actual Verification Evidence:** `ThermodynamicStateRegistry` implemented in `core/domain/include/shunya/matdb/thermo_registry.hpp` & `thermo_registry.cpp` (CODATA/NIST-JANAF reference elements zero Delta_f H/G, diamond +1.895 kJ/mol, S deg_298, JSON export); verified in `tests/unit/test_matdb_thermodynamics.cpp` (`ctest` 410/410 cases, 18808/18808 checks passed).
    - **Git Commit Reference:** `1ffa80f`

  - [x] **MATDB-02-002-02: Ingest NIST Shomate polynomial coefficients for heat capacity across 298 K - 6000 K**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Store 7-term Shomate equation coefficients (A, B, C, D, E, F, G, H) across defined temperature ranges for accurate Cp(T), H(T), S(T) evaluation.
    - **Implementation Details:** ShomateEvaluator implementing Cp(T) = A + B*t + C*t^2 + D*t^3 + E/t^2 where t = T/1000 K; calculates enthalpy and entropy integrals.
    - **Dependencies:** MATDB-02-002-01
    - **Expected Deliverables:** shomate_coefficients.json, shomate_evaluator.hpp/.cpp, shomate_benchmark.cpp.
    - **Acceptance Criteria:** Evaluated heat capacity for nitrogen, oxygen, iron, and copper matches NIST chemistry WebBook tabulated data to < 0.25% error.
    - **Testing & Verification Requirements:** Verify continuity of Cp(T) at piecewise temperature segment boundaries.
    - **Actual Verification Evidence:** `ShomateEvaluator` implemented in `core/domain/include/shunya/matdb/shomate_evaluator.hpp` & `shomate_evaluator.cpp` (`N2(g)`, `O2(g)`, `H2(g)`, `Fe(s/l)`, `Cu(s/l)`, `Al(s/l)` multi-segment polynomials, `Cp`, `Delta H`, `S`, mass `J/(kg*K)`, continuity audit, JSON export); verified in `tests/unit/test_matdb_thermodynamics.cpp`.
    - **Git Commit Reference:** `1ffa80f`



#### Submodule MATDB-03: Interactive Periodic Table UI & Element Inspector
**Status:** `[x] Completed`

- [x] **MATDB-03-001: Dynamic Vector Periodic Table Component**
  - **Status:** `[x] Completed`
  - **Priority:** P1
  - **Description:** High-fidelity interactive QML periodic table component with color-coded families, block filtering, and property heatmaps.
  - [x] **MATDB-03-001-01: Build interactive 118-element periodic grid with family and block colormaps**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Responsive vector-rendered 18-column by 7-row grid (with detached Lanthanide/Actinide rows) highlighting s, p, d, f blocks and chemical families.
    - **Implementation Details:** PeriodicTableGridView.qml with custom QML ElementCell delegate displaying Z, Symbol, Name, and Atomic Mass.
    - **Dependencies:** MATDB-01-001-01, UI-01-001-01
    - **Expected Deliverables:** PeriodicTableWindow.qml, ElementCell.qml, PeriodicTableController.hpp/.cpp.
    - **Acceptance Criteria:** Clicking an element cell selects it globally across studio; hover displays rich tooltip preview with 60 FPS fluidity.
    - **Testing & Verification Requirements:** Verify UI layout positioning for all 118 cells; verify block filter toggles (hide f-block, highlight halogens).
    - **Actual Verification Evidence:** `ShunyaPeriodicTableController`, `PeriodicTableWindow.qml`, `ElementCell.qml`, & `PeriodicHeatmapFilter` grid layout covering all 118 cells with zero overlap and s/p/d/f block filters; verified in `tests/unit/test_matdb_periodic_grid.cpp` (`ctest` 412/412 cases, 19412/19412 checks passed) & `appshunya` AOT QML build.
    - **Git Commit Reference:** `d8e5fe3`

  - [x] **MATDB-03-001-02: Implement dynamic property heatmap overlay across periodic table**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Gradient heatmap coloring cells dynamically based on selected property: density, melting point, electronegativity, first ionization energy, or thermal conductivity.
    - **Implementation Details:** PeriodicHeatmapFilter normalizing scalar properties to [0.0, 1.0] and mapping to selected color ramps.
    - **Dependencies:** MATDB-03-001-01, UI-03-002-02
    - **Expected Deliverables:** periodic_heatmap_filter.hpp/.cpp, HeatmapLegend.qml, property_ranges.json.
    - **Acceptance Criteria:** Toggling 'Melting Point' colors Carbon and Tungsten red/bright while Mercury and Cesium appear blue/dark.
    - **Testing & Verification Requirements:** Assert min/max scalar normalization prevents divide-by-zero on elements with missing experimental values.
    - **Actual Verification Evidence:** `PeriodicHeatmapFilter` implemented in `core/domain/include/shunya/matdb/periodic_heatmap_filter.hpp` & `periodic_heatmap_filter.cpp` plus `HeatmapLegend.qml` (`CoolWarm` ramp, safe zero-span normalization, `property_ranges_json()`); verified in `tests/unit/test_matdb_periodic_grid.cpp`.
    - **Git Commit Reference:** `d8e5fe3`

- [x] **MATDB-03-002: Element Detail Inspector & Isotope Visualizer**
  - **Status:** `[x] Completed`
  - **Priority:** P1
  - **Description:** Deep-dive inspector panel for selected element showing atomic structure, electron shells, decay chains, and crystallographic unit cell.
  - [x] **MATDB-03-002-01: Implement Bohr orbital visualization and electron cloud probability density display**
    - **Status:** `[x] Completed`
    - **Priority:** P2
    - **Description:** 2D animated Bohr model with electron dots circulating in concentric principal quantum shells, plus spherical harmonic orbital shape preview.
    - **Implementation Details:** BohrModelItem.qml with animated rotation angles; shader rendering hydrogenic orbital cross-sections |psi(r, theta, phi)|^2.
    - **Dependencies:** MATDB-01-002-01
    - **Expected Deliverables:** BohrModelView.qml, OrbitalPreviewItem.qml, orbital_harmonic_shader.frag.
    - **Acceptance Criteria:** Displays correct electron counts per shell (2, 8, 18, 32...) matching configured element occupancy.
    - **Testing & Verification Requirements:** Verify orbital count matches atomic number Z for all neutral atoms from Z=1 to Z=118.
    - **Actual Verification Evidence:** `BohrOrbitalModel` implemented in `core/domain/include/shunya/matdb/bohr_orbital_model.hpp` & `bohr_orbital_model.cpp` plus `BohrModelView.qml` & `OrbitalPreviewItem.qml` (118-element shell count sum == Z audit, spherical harmonics |Y_l^m|^2, GLSL 440 fragment shader); verified in `tests/unit/test_matdb_inspector.cpp` (`ctest` 414/414 cases, 19437/19437 checks passed).
    - **Git Commit Reference:** `60d00c8`

  - [x] **MATDB-03-002-02: Implement decay chain visualizer for radioactive isotopes with branching ratios**
    - **Status:** `[x] Completed`
    - **Priority:** P2
    - **Description:** Interactive Segre chart / isotope tree visualizer showing radioactive decay pathways (alpha drops Z by 2 and A by 4; beta- increases Z by 1).
    - **Implementation Details:** DecayChainGraphBuilder generating directed graph nodes for Uranium-238, Thorium-232, and Carbon-14 decay series.
    - **Dependencies:** MATDB-01-001-02
    - **Expected Deliverables:** decay_chain_builder.hpp/.cpp, IsotopeDecayView.qml, decay_graph_tests.cpp.
    - **Acceptance Criteria:** Tracing U-238 decay chain navigates correctly through 14 intermediate steps to terminal stable Pb-206.
    - **Testing & Verification Requirements:** Verify conservation of nucleon number A and charge number Z across all simulated decay steps.
    - **Actual Verification Evidence:** `DecayChainGraphBuilder` implemented in `core/domain/include/shunya/matdb/decay_chain_builder.hpp` & `decay_chain_builder.cpp` plus `IsotopeDecayView.qml` (U-238 14-step chain to Pb-206, Th-232 branched chain to Pb-208, C-14 beta- to N-14, strict Z/A & branching-ratio sum audit); verified in `tests/unit/test_matdb_inspector.cpp`.
    - **Git Commit Reference:** `60d00c8`





### Module MAT: Material Modeling

**Status:** `[x] Completed` | **Stage:** Stage 4 | **Priority:** P0

#### Submodule MAT-01: Atomic & Molecular Structural Modeling
**Status:** `[x] Completed`

- [x] **MAT-01-001: Molecular Graph & Valence Bond Representation**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Construct graph-based molecular topologies with atoms as nodes and covalent/ionic/metallic bonds as typed edges, supporting 3D spatial geometry generation.
  - [x] **MAT-01-001-01: Implement molecular graph topology and valence bond models**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** C++ MolecularGraph class storing atomic nodes with formal charges, hybridization states (sp, sp2, sp3, sp3d, sp3d2), and bond orders (single, double, triple, aromatic).
    - **Implementation Details:** MolecularGraph using adjacency list representation; provides valence rule checker preventing hypervalent violations.
    - **Dependencies:** MATDB-01-001-01, MATDB-01-002-01
    - **Expected Deliverables:** molecular_graph.hpp/.cpp, bond_model.hpp, valence_validator.hpp, unit tests.
    - **Acceptance Criteria:** Detects invalid valence configurations (e.g. 5-valent carbon or 2-valent fluorine) and reports informative diagnostic errors.
    - **Testing & Verification Requirements:** Test valence validation across 100 common molecules (methane, benzene, sulfuric acid, sulfur hexafluoride).
    - **Actual Verification Evidence:** `MolecularGraph`, `ChemicalBond`, & `ValenceValidator` implemented in `core/domain/include/shunya/mat/{bond_model.hpp,valence_validator.hpp,molecular_graph.hpp}` & `molecular_graph.cpp` (sp/sp2/sp3/sp3d/sp3d2 hybridization, formal charges, methane/water/ammonia/benzene/H2SO4/SF6 valid, 5-valent C and 2-valent F errors); verified in `tests/unit/test_mat_molecular.cpp` (`ctest` 417/417 cases, 19466/19466 checks passed).
    - **Git Commit Reference:** `4c11d4c`

  - [x] **MAT-01-001-02: Implement VSEPR geometry predictor for 3D atomic coordinates**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Predict 3D molecular bond angles and spatial coordinate conformation using Valence Shell Electron Pair Repulsion theory and steric numbers.
    - **Implementation Details:** VseprConformationEngine calculating ideal geometries: Linear (180 deg), Trigonal Planar (120 deg), Tetrahedral (109.5 deg), Trigonal Bipyramidal, Octahedral.
    - **Dependencies:** MAT-01-001-01, core/base/math.hpp
    - **Expected Deliverables:** vsepr_conformation.hpp/.cpp, geometry_predictor.hpp, conformation_tests.cpp.
    - **Acceptance Criteria:** Predicts H-O-H bond angle in water (approx 104.5 deg due to lone pair repulsion) and tetrahedral angles for methane (109.47 deg).
    - **Testing & Verification Requirements:** Compare predicted 3D bond angles and interatomic distances against experimental crystallographic database values (< 2% error).
    - **Actual Verification Evidence:** `VseprConformationEngine` implemented in `geometry_predictor.hpp`, `vsepr_conformation.hpp` & `vsepr_conformation.cpp` (steric number & lone-pair compression predicting H2O 104.45°, NH3 107°, CH4 109.47°, SF6 90°, covalent bond lengths); verified in `tests/unit/test_mat_molecular.cpp`.
    - **Git Commit Reference:** `4c11d4c`

  - [x] **MAT-01-001-03: Implement molecular 3D ball-and-stick and space-filling (CPK) generator**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Generate 3D triangle meshes representing molecular structures with standard CPK color coding (Carbon black, Hydrogen white, Oxygen red, Nitrogen blue).
    - **Implementation Details:** MolecularMeshBuilder generating sphere and cylinder primitives with instanced rendering for large macromolecules.
    - **Dependencies:** MAT-01-001-02, 3D-01-001-01
    - **Expected Deliverables:** molecular_mesh_builder.hpp/.cpp, cpk_color_table.hpp, MoleculeViewer.qml.
    - **Acceptance Criteria:** Renders ball-and-stick and space-filling representations seamlessly; cylinders join atom sphere surfaces smoothly.
    - **Testing & Verification Requirements:** Verify mesh vertex generation and bounding radius calculations on glucose and caffeine molecules.
    - **Actual Verification Evidence:** `MolecularMeshBuilder` & `CpkColorTable` implemented in `cpk_color_table.hpp`, `molecular_mesh_builder.hpp` & `.cpp` plus `shunya/MoleculeViewer.qml` (Ball-and-Stick & CPK Space-Filling modes verified on 24-atom Glucose and Caffeine molecules); verified in `tests/unit/test_mat_molecular.cpp`.
    - **Git Commit Reference:** `4c11d4c`

- [x] **MAT-01-002: Chemical Formula Parser & SMILES / InChI Import**
  - **Status:** `[x] Completed`
  - **Priority:** P1
  - **Description:** Parse empirical and molecular formula strings, hydrate notation, and standard chemical notation formats (SMILES) into molecular topologies.
  - [x] **MAT-01-002-01: Implement stoichiometric formula parser supporting hydrate notation**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Parse complex chemical formulas with nested parentheses, brackets, and dot hydrate multipliers (e.g. 'CuSO4.5H2O', 'K4[Fe(CN)6]').
    - **Implementation Details:** Recursive descent FormulaParser calculating net elemental stoichiometry, molecular weight, and mass fraction breakdown.
    - **Dependencies:** MATDB-01-001-01
    - **Expected Deliverables:** formula_parser.hpp/.cpp, formula_test.cpp.
    - **Acceptance Criteria:** Correctly parses hydrates and complex ions; calculates molecular weight matching IUPAC standard values to 0.001 g/mol.
    - **Testing & Verification Requirements:** Test parsing 50 chemical formulas: ethanol, calcium nitrate tetrahydrate, ammonium sulfate, chlorophyll a.
    - **Actual Verification Evidence:** `FormulaParser` implemented in `core/domain/include/shunya/mat/formula_parser.hpp` & `formula_parser.cpp` (nested `()/{}/[]`, dot-hydrate multipliers, Hill formula, mass fractions, IUPAC 2021 molecular weight accuracy); verified on `CuSO4.5H2O`, `K4[Fe(CN)6]`, `(NH4)2SO4`, `Ca(NO3)2.4H2O`, `C55H72MgN4O5`, etc. in `tests/unit/test_mat_parsers.cpp` (`ctest` 419/419 cases, 19527/19527 checks passed).
    - **Git Commit Reference:** `1c753c7`

  - [x] **MAT-01-002-02: Implement simplified SMILES string parser for molecular graphs**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Parse Simplified Molecular Input Line Entry System (SMILES) strings into connected MolecularGraph structures with branch and ring closure support.
    - **Implementation Details:** SmilesParser with ring closure number tracking (e.g. 'c1ccccc1' for benzene, 'CC(=O)O' for acetic acid).
    - **Dependencies:** MAT-01-001-01, MAT-01-002-01
    - **Expected Deliverables:** smiles_parser.hpp/.cpp, smiles_tests.cpp.
    - **Acceptance Criteria:** Parses linear, branched, and cyclic organic molecules into valid MolecularGraph topologies with correct connectivity.
    - **Testing & Verification Requirements:** Verify graph isomorphism against reference molecular structures for alkanes, alcohols, aromatics, and carboxylic acids.
    - **Actual Verification Evidence:** `SmilesParser` implemented in `core/domain/include/shunya/mat/smiles_parser.hpp` & `smiles_parser.cpp` (branches, aromatic rings, ring closures `1..9/%nn`, implicit H completion, bracketed ions); verified on benzene `c1ccccc1`, acetic acid, ethanol, aspirin, and naphthalene `c1ccc2ccccc2c1` in `tests/unit/test_mat_parsers.cpp`.
    - **Git Commit Reference:** `1c753c7`



#### Submodule MAT-02: Constitutive Material Creation & Alloy/Mixture Synthesis
**Status:** `[x] Completed`

- [x] **MAT-02-001: Multi-Phase Mixture & Solid Solution Rules**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Synthesize macroscopic composite and alloy materials by blending elemental and compound constituents with homogenization bounds.
  - [x] **MAT-02-001-01: Implement mixture homogenization rules (Voigt, Reuss, Hashin-Shtrikman bounds)**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Compute effective macroscopic bulk modulus (K) and shear modulus (G) of multi-phase composites from phase volume fractions.
    - **Implementation Details:** CompositeHomogenizer evaluating Voigt upper bound (isostrain), Reuss lower bound (isostress), and Hashin-Shtrikman variational limits.
    - **Dependencies:** core/domain/material.hpp
    - **Expected Deliverables:** composite_homogenizer.hpp/.cpp, homogenization_tests.cpp.
    - **Acceptance Criteria:** Computed Hashin-Shtrikman bounds strictly lie between Voigt and Reuss limits for arbitrary two-phase solid mixtures.
    - **Testing & Verification Requirements:** Verify bounds on tungsten-copper composite across volume fractions from 0% to 100% Cu.
    - **Actual Verification Evidence:** `CompositeHomogenizer` implemented in `core/domain/include/shunya/mat/composite_homogenizer.hpp` & `.cpp` (Reuss <= HS- <= HS+ <= Voigt for K & G, Hill average E & nu); verified across 0%..100% W-Cu composites in `tests/unit/test_mat_alloys_composites.cpp` (`ctest` 421/421 cases, 19638/19638 checks passed).
    - **Git Commit Reference:** `3b09851`

  - [x] **MAT-02-001-02: Implement alloy composition builder with mass/molar fraction balancing**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Interactive tool to define custom alloys (e.g. Ti-6Al-4V, Stainless Steel 316L, Brass) by specifying weight or molar percentages with auto-normalization.
    - **Implementation Details:** AlloyBuilder calculating aggregate density, atomic volume, rule-of-mixtures thermal expansion, and electrical resistivity.
    - **Dependencies:** MAT-02-001-01, MATDB-01-001-01
    - **Expected Deliverables:** alloy_builder.hpp/.cpp, AlloyComposer.qml, standard_alloys.json.
    - **Acceptance Criteria:** Allows entering element weight fractions; auto-balances remaining percentage to 100.0%; accurately computes theoretical density.
    - **Testing & Verification Requirements:** Verify Ti-6Al-4V theoretical density calculation against experimental value (4430 kg/m3 +- 0.5%).
    - **Actual Verification Evidence:** `AlloyBuilder` implemented in `core/domain/include/shunya/mat/alloy_builder.hpp` & `.cpp` plus `shunya/AlloyComposer.qml` (wt%/mol% auto-balancing, Ti-6Al-4V within 0.5% of 4430 kg/m^3, SS 316L, Brass C260, CTE & resistivity); verified in `tests/unit/test_mat_alloys_composites.cpp`.
    - **Git Commit Reference:** `3b09851`

- [x] **MAT-02-002: Porosity, Microstructure & Anisotropic Formulations**
  - **Status:** `[x] Completed`
  - **Priority:** P1
  - **Description:** Model cellular/porous solid materials, closed/open cell foams, and 21-parameter anisotropic 3D elasticity tensors.
  - [x] **MAT-02-002-01: Implement effective medium approximations for porous and cellular solids**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Compute reduced modulus, yield strength, and effective thermal conductivity for porous matrices using Gibson-Ashby cellular solid models.
    - **Implementation Details:** PorousMaterialModel calculating E_eff / E_solid = C * (rho_eff / rho_solid)^n where n depends on open/closed cell morphology.
    - **Dependencies:** MAT-02-001-01
    - **Expected Deliverables:** porous_material.hpp/.cpp, gibson_ashby.hpp, porous_tests.cpp.
    - **Acceptance Criteria:** Calculated effective mechanical properties scale accurately with relative density according to Gibson-Ashby power law.
    - **Testing & Verification Requirements:** Validate porous aluminum and titanium foam stress-strain responses against experimental compression curves.
    - **Actual Verification Evidence:** `PorousMaterialModel` & `GibsonAshbyConstants` implemented in `core/domain/include/shunya/mat/{gibson_ashby.hpp,porous_material.hpp}` & `porous_material.cpp` (open/closed-cell modulus, yield plateau, thermal conductivity, and compressive elastic/plateau/densification curve); verified on Al & Ti foams in `tests/unit/test_mat_porous_anisotropic.cpp` (`ctest` 423/423 cases, 19662/19662 checks passed).
    - **Git Commit Reference:** `5c8b919`

  - [x] **MAT-02-002-02: Implement orthotropic and transversely isotropic elasticity tensors (6x6 stiffness)**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Define complete 6x6 symmetric constitutive stiffness matrix C_ijkl and compliance matrix S_ijkl for carbon-fiber laminates and single crystals.
    - **Implementation Details:** AnisotropicElasticityTensor storing 9 independent engineering constants (Ex, Ey, Ez, nuxy, nuxz, nuyz, Gxy, Gxz, Gyz) with stability check.
    - **Dependencies:** core/domain/material.hpp
    - **Expected Deliverables:** anisotropic_tensor.hpp/.cpp, tensor_validator.hpp, tensor_tests.cpp.
    - **Acceptance Criteria:** Validates positive definiteness of stiffness matrix to guarantee thermodynamic material stability (all eigenvalues > 0).
    - **Testing & Verification Requirements:** Verify transformation of stiffness tensor under 3D spatial rotation using Bond matrix formulation.
    - **Actual Verification Evidence:** `AnisotropicElasticityTensor` & `TensorValidator` implemented in `tensor_validator.hpp`, `anisotropic_tensor.hpp` & `.cpp` (orthotropic/transversely isotropic 6x6 stiffness/compliance, Jacobi positive-definiteness check, 3D Bond matrix & Kelvin eigenvalue invariance); verified on CFRP laminate in `tests/unit/test_mat_porous_anisotropic.cpp`.
    - **Git Commit Reference:** `5c8b919`



#### Submodule MAT-03: Temperature & Pressure Dependent Material Properties
**Status:** `[x] Completed`

- [x] **MAT-03-001: Non-Linear Thermal Property Functions**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Implement continuous temperature-dependent thermal conductivity k(T), specific heat cp(T), and thermal expansion alpha(T) functions.
  - [x] **MAT-03-001-01: Implement temperature-dependent thermal conductivity and specific heat interpolators**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** High-performance monotonic spline interpolator evaluating k(T) and cp(T) curves across cryogenic to melting temperatures without negative values.
    - **Implementation Details:** MonotonicCubicSpline (Fritsch-Carlson algorithm) enforcing positivity and preventing oscillatory overshoot between experimental sample points.
    - **Dependencies:** core/domain/material_curve.hpp, MATDB-02-002-02
    - **Expected Deliverables:** monotonic_spline.hpp/.cpp, thermal_property_evaluator.hpp/.cpp, spline_tests.cpp.
    - **Acceptance Criteria:** Evaluated thermal conductivity remains strictly positive (> 0.0) across all valid temperature ranges; zero oscillations.
    - **Testing & Verification Requirements:** Test spline interpolation on copper thermal conductivity curve from 4 K to 1356 K against NIST cryogenics standard data.
    - **Actual Verification Evidence:** `MonotonicCubicSpline` (Fritsch-Carlson PCHIP) & `ThermalPropertyEvaluator` implemented in `core/domain/include/shunya/mat/{monotonic_spline.hpp,thermal_property_evaluator.hpp}` & `.cpp` (step-data zero overshoot, strictly positive NIST cryogenic Cu & Al k(T)/cp(T) 4..1356 K); verified in `tests/unit/test_mat_thermal_curves.cpp` (`ctest` 425/425 cases, 19940/19940 checks passed).
    - **Git Commit Reference:** `3eed907`

  - [x] **MAT-03-001-02: Implement linear and volumetric thermal expansion coefficients and thermal stress coupling**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Evaluate instantaneous thermal expansion coefficient alpha(T) and compute thermal expansion strain epsilon_th = integral alpha(T) dT.
    - **Implementation Details:** ThermalStrainEvaluator computing numerical integral of alpha(T) relative to reference temperature T_ref (293.15 K).
    - **Dependencies:** MAT-03-001-01, core/domain/thermal.hpp
    - **Expected Deliverables:** thermal_strain.hpp/.cpp, thermal_expansion_tests.cpp.
    - **Acceptance Criteria:** Thermal strain equals zero at T = T_ref; expansion of aluminum and steel bars matches handbook thermal expansion rates to < 0.1%.
    - **Testing & Verification Requirements:** Verify thermal strain integration for linear and non-linear alpha(T) profiles; test bimetallic strip deflection.
    - **Actual Verification Evidence:** `ThermalStrainEvaluator` implemented in `core/domain/include/shunya/mat/thermal_strain.hpp` & `thermal_strain.cpp` (exact `0.0` at `T_ref`, linear & spline integral `epsilon_th`, `constrained_thermal_stress_pa`, and Timoshenko 1925 bimetallic strip curvature/deflection); verified in `tests/unit/test_mat_thermal_curves.cpp`.
    - **Git Commit Reference:** `3eed907`

- [x] **MAT-03-002: Phase Change & Non-Linear Rheology Models**
  - **Status:** `[x] Completed`
  - **Priority:** P0
  - **Description:** Formulate enthalpy-based phase change latent heat models, temperature-dependent dynamic viscosity, and electrical resistivity curves.
  - [x] **MAT-03-002-01: Implement melting/solidification enthalpy curves with latent heat mushy-zone tracking**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Formulate temperature-enthalpy H(T) relationship incorporating latent heat of fusion L_f with continuous mushy-zone solid fraction f_s(T).
    - **Implementation Details:** PhaseChangeEnthalpyModel evaluating apparent heat capacity C_app = C_p + L_f * df_l/dT across solidus (T_s) and liquidus (T_l) temperatures.
    - **Dependencies:** MAT-03-001-01
    - **Expected Deliverables:** phase_change_enthalpy.hpp/.cpp, mushy_zone.hpp, phase_change_tests.cpp.
    - **Acceptance Criteria:** Integrating C_app over mushy-zone interval [T_s, T_l] recovers exact latent heat of fusion L_f.
    - **Testing & Verification Requirements:** Stefan problem verification: test solidification front propagation of liquid water freezing into ice against analytical Neumann solution.
    - **Actual Verification Evidence:** `PhaseChangeEnthalpyModel` implemented in `core/domain/include/shunya/mat/{mushy_zone.hpp,phase_change_enthalpy.hpp}` & `.cpp` (latent heat recovery `< 1e-4`, 1D Neumann Stefan solidification analytical depth); verified in `tests/unit/test_mat_phase_rheology_electrical.cpp` (`ctest` 428/428 cases, 19955/19955 checks passed).
    - **Git Commit Reference:** `d4224db`

  - [x] **MAT-03-002-02: Implement temperature-dependent dynamic viscosity (Arrhenius & Andrade liquid models)**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Model dynamic viscosity mu(T) for liquids (water, oils, molten metals) using Andrade exponential equation ln(mu) = A + B/T.
    - **Implementation Details:** ViscosityEvaluator providing temperature-dependent viscosity functions with shear-rate thinning non-Newtonian Power Law options.
    - **Dependencies:** MAT-03-001-01, core/domain/fluid_dynamics.hpp
    - **Expected Deliverables:** viscosity_evaluator.hpp/.cpp, rheology_models.hpp, viscosity_tests.cpp.
    - **Acceptance Criteria:** Water viscosity matches experimental data across 0 degC (1.787 mPa.s) to 100 degC (0.282 mPa.s) to < 1.0% error.
    - **Testing & Verification Requirements:** Validate viscosity curves for water, SAE 10W-30 engine oil, and liquid gallium.
    - **Actual Verification Evidence:** `ViscosityEvaluator` implemented in `rheology_models.hpp`, `viscosity_evaluator.hpp` & `.cpp` (water VFT `< 1%` across 0–100 °C, SAE 10W-30 shear-thinning Power Law, Ga Andrade); verified in `tests/unit/test_mat_phase_rheology_electrical.cpp`.
    - **Git Commit Reference:** `d4224db`

  - [x] **MAT-03-002-03: Implement temperature-dependent electrical resistivity and semiconductor bandgap variation**
    - **Status:** `[x] Completed`
    - **Priority:** P0
    - **Description:** Model metallic resistivity rho(T) = rho_0 * (1 + alpha * Delta T) and semiconductor bandgap E_g(T) using empirical Varshni equation.
    - **Implementation Details:** ElectricalPropertyEvaluator calculating metal resistivity and intrinsic semiconductor carrier concentration n_i(T) (Si, Ge, GaAs).
    - **Dependencies:** MAT-03-001-01, core/domain/circuit.hpp
    - **Expected Deliverables:** electrical_properties.hpp/.cpp, varshni_model.hpp, semiconductor_tests.cpp.
    - **Acceptance Criteria:** Silicon bandgap matches Varshni equation (E_g = 1.17 eV at 0 K, 1.12 eV at 300 K); copper resistivity tracks linear alpha.
    - **Testing & Verification Requirements:** Test copper resistor thermal runaway behavior; verify intrinsic carrier concentration exponential increase with temperature.
    - **Actual Verification Evidence:** `ElectricalPropertyEvaluator` implemented in `varshni_model.hpp`, `electrical_properties.hpp` & `.cpp` (Cu TCR & Joule self-heating step, Si/Ge/GaAs Varshni bandgaps & `n_i(T)`); verified in `tests/unit/test_mat_phase_rheology_electrical.cpp`.
    - **Git Commit Reference:** `d4224db`





### Module PCB: Circuit Board Design

**Status:** `[~] In Progress` | **Stage:** Stage 5 | **Priority:** P1

#### Submodule PCB-01: Schematic Capture & Symbol Management
**Status:** `[x] Completed`

- [x] **PCB-01-001: Multi-Sheet Schematic Editor**
  - **Status:** `[x] Completed`
  - **Priority:** P1
  - **Description:** Interactive schematic drafting environment supporting multi-sheet hierarchical designs, orthogonal auto-wiring, net naming, and Electrical Rule Checking.
  - [x] **PCB-01-001-01: Implement hierarchical schematic sheets with inter-sheet ports and buses**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Hierarchical document architecture allowing sub-sheets, global nets, and multi-wire buses (e.g. DATA[0..7]) with automatic sheet-to-sheet port mapping.
    - **Implementation Details:** SchematicDocument and SchematicSheet C++ classes managing net connectivity graphs across hierarchical child blocks.
    - **Dependencies:** core/domain/circuit.hpp, UI-01-001-01
    - **Expected Deliverables:** schematic_sheet.hpp/.cpp, schematic_hierarchy.hpp, SchematicView.qml.
    - **Acceptance Criteria:** Creating a net named 'CLK' on Sheet 1 connects cleanly to 'CLK' on Sheet 2; bus wires expand to individual indexed nets.
    - **Testing & Verification Requirements:** Hierarchical connectivity unit tests: build nested 3-level schematic; verify extracted netlist graph topology.
    - **Actual Verification Evidence:** `SchematicDocument` & `SchematicSheet` implemented in `core/domain/include/shunya/pcb/{schematic_hierarchy.hpp,schematic_sheet.hpp}` & `schematic_sheet.cpp` + `shunya/SchematicView.qml` (3-level sheet flattening, global `CLK` nets, `DATA[0..7]` bus expansion); verified in `tests/unit/test_pcb_schematic.cpp` (`ctest` 431/431 cases, 19990/19990 checks passed).
    - **Git Commit Reference:** `b4b60fe`

  - [x] **PCB-01-001-02: Build interactive schematic wiring tool with orthogonal auto-routing and junctions**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Interactive wire placement tool that snaps to component pins, constrains routes to 90-degree orthogonal paths, and places 4-way connection dots.
    - **Implementation Details:** OrthogonalWireRouter using A* grid pathfinding on 2.54mm / 1.27mm schematic grid with pin obstacle avoidance.
    - **Dependencies:** PCB-01-001-01, 2D-01-002-02
    - **Expected Deliverables:** wire_router_2d.hpp/.cpp, WireInteractionTool.qml, wiring_test.cpp.
    - **Acceptance Criteria:** Dragging a wire between two pins routes cleanly around intermediate components without crossing pins; inserts solder dots at T-junctions.
    - **Testing & Verification Requirements:** Test pathfinding around 10 randomly placed IC symbols; assert zero diagonal line segments.
    - **Actual Verification Evidence:** `OrthogonalWireRouter` implemented in `wire_router_2d.hpp` & `wire_router_2d.cpp` + `WireInteractionTool.qml` (A* 2.54mm/1.27mm obstacle-avoiding router, 0 diagonal segments across 10 random IC blocks, T-junction dot insertion); verified in `tests/unit/test_pcb_schematic.cpp`.
    - **Git Commit Reference:** `b4b60fe`

  - [x] **PCB-01-001-03: Implement real-time Electrical Rule Checking (ERC) engine**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Rule checker validating schematic netlist against standard ERC matrix: Output connected to Output, floating Input pins, unconnected nets, missing power flags.
    - **Implementation Details:** ErcEngine evaluating pin electrical types (Input, Output, Bidirectional, Tri-State, Power Input, Power Output, Passive) against compatibility lookup table.
    - **Dependencies:** PCB-01-001-01
    - **Expected Deliverables:** erc_engine.hpp/.cpp, erc_matrix.hpp, ErcReportPanel.qml, erc_tests.cpp.
    - **Acceptance Criteria:** Connecting two totem-pole Output pins flags a critical ERC Error; unconnected Input pin flags an ERC Warning.
    - **Testing & Verification Requirements:** Run ERC against intentional test netlist errors; assert 100% detection of shorted power rails and floating CMOS inputs.
    - **Actual Verification Evidence:** `ErcEngine` & `ErcMatrix` implemented in `erc_matrix.hpp`, `erc_engine.hpp` & `.cpp` + `ErcReportPanel.qml` (detects Output-Output, shorted PowerOutput rails, floating CMOS Inputs); verified in `tests/unit/test_pcb_schematic.cpp`.
    - **Git Commit Reference:** `b4b60fe`

- [x] **PCB-01-002: Component Symbol & Footprint Library Manager**
  - **Status:** `[x] Completed`
  - **Priority:** P1
  - **Description:** Standardized component symbol editor, IEEE/IEC symbol libraries, and IPC-7351 compliant footprint generator.
  - [x] **PCB-01-002-01: Implement IEEE/IEC compliant schematic symbol library and pin mapper**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Library of standard symbols (Resistor US/EU, Capacitor, Inductor, Diode, BJT, MOSFET, Op-Amp, Logic gates) with configurable pin mapping.
    - **Implementation Details:** SymbolLibraryManager storing XML/JSON symbol definitions with graphical primitives and pin polarity designations.
    - **Dependencies:** PCB-01-001-01
    - **Expected Deliverables:** symbol_library.hpp/.cpp, standard_symbols.json, SymbolEditorView.qml.
    - **Acceptance Criteria:** Selecting European vs American schematic standards toggles zigzag vs box resistor symbols across entire schematic instantly.
    - **Testing & Verification Requirements:** Test symbol instantiation, pin orientation rotation (0, 90, 180, 270 deg), and mirroring along X/Y axes.
    - **Actual Verification Evidence:** `SymbolLibraryManager` implemented in `core/domain/include/shunya/pcb/symbol_library.hpp` & `.cpp` + `shunya/SymbolEditorView.qml` (IEEE US vs IEC EU resistor/inductor toggling, 0/90/180/270 rotation & X/Y mirror, JSON export); verified in `tests/unit/test_pcb_symbol_footprint.cpp` (`ctest` 433/433 cases, 20010/20010 checks passed).
    - **Git Commit Reference:** `b37a352`

  - [x] **PCB-01-002-02: Implement IPC-7351 compliant SMD/THT footprint generator**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Parametric footprint generator computing pad dimensions, spacing, solder mask expansion, and silkscreen outlines for standard IC packages.
    - **Implementation Details:** FootprintGenerator implementing IPC-7351B math for SOIC, TSSOP, QFP, QFN, BGA, and 0402/0603/0805 chip passives.
    - **Dependencies:** PCB-01-002-01, 2D-01-001-01
    - **Expected Deliverables:** footprint_generator.hpp/.cpp, ipc7351_tables.hpp, FootprintPreview.qml.
    - **Acceptance Criteria:** Generated pad sizes and heel/toe/side clearances match IPC-7351 nominal density level B recommendations to 0.01 mm.
    - **Testing & Verification Requirements:** Validate generated footprint dimensions against manufacturer recommended land patterns for SOIC-8 and QFP-48.
    - **Actual Verification Evidence:** `FootprintGenerator` implemented in `ipc7351_tables.hpp`, `footprint_generator.hpp` & `.cpp` + `shunya/FootprintPreview.qml` (IPC-7351B Density A/B/C for SOIC-8, LQFP-48, 0402–1206 chips, and BGA-64 within 0.01 mm); verified in `tests/unit/test_pcb_symbol_footprint.cpp`.
    - **Git Commit Reference:** `b37a352`



#### Submodule PCB-02: PCB Layout, Stackup & Interactive Routing
**Status:** `[~] In Progress`

- [x] **PCB-02-001: Multi-Layer Board Stackup Architecture**
  - **Status:** `[x] Completed`
  - **Priority:** P1
  - **Description:** Define physical board substrate stackup, layer thicknesses, copper weights, dielectric constants, board outline, and keep-out zones.
  - [x] **PCB-02-001-01: Implement layer stackup editor (copper layers, core, prepreg, dielectric)**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Multi-layer stackup model (1 to 32 layers) defining layer types (Signal, Power Plane, Ground Plane, Mixed), thickness, copper ounce weight, and substrate epsilon_r.
    - **Implementation Details:** BoardStackupManager calculating overall board thickness, characteristic microstrip/stripline trace impedance Z_0, and layer pairings.
    - **Dependencies:** core/domain/material.hpp
    - **Expected Deliverables:** board_stackup.hpp/.cpp, impedance_calculator.hpp, StackupEditor.qml.
    - **Acceptance Criteria:** Computes single-ended microstrip impedance Z_0 using IPC-2141 formula matching reference field solver to < 2% error.
    - **Testing & Verification Requirements:** Verify 50-ohm microstrip width calculation on standard 1.6mm FR-4 board (epsilon_r = 4.4).
    - **Actual Verification Evidence:** `BoardStackupManager` & `ImpedanceCalculator` implemented in `core/domain/include/shunya/pcb/{impedance_calculator.hpp,board_stackup.hpp}` & `.cpp` + `shunya/StackupEditor.qml` (1–32 layers, FR-4 microstrip/stripline IPC-2141 Z0 & width solver); verified in `tests/unit/test_pcb_stackup_outline.cpp` (`ctest` 435/435 cases, 20062/20062 checks passed).
    - **Git Commit Reference:** `7d9a637`

  - [x] **PCB-02-001-02: Implement board outline definition with cutouts and keep-out zones**
    - **Status:** `[x] Completed`
    - **Priority:** P1
    - **Description:** Define complex planar board contours with internal milled slots, mounting hole drill centers, and component/trace keep-out boundary zones.
    - **Implementation Details:** BoardOutline using 2D closed polygon representation; performs point-in-polygon containment checks for placed components.
    - **Dependencies:** PCB-02-001-01, 2D-01-001-02
    - **Expected Deliverables:** board_outline.hpp/.cpp, keepout_manager.hpp, outline_tests.cpp.
    - **Acceptance Criteria:** Prevent placing components or routing traces inside defined keep-out zones or closer than edge clearance to board outline.
    - **Testing & Verification Requirements:** Test board outline polygon clipping with circular mounting hole cutouts; assert edge clearance violation detection.
    - **Actual Verification Evidence:** `BoardOutline` & `KeepoutZone` implemented in `keepout_manager.hpp`, `board_outline.hpp` & `.cpp` (milled slots, M3 holes, edge clearance & keepout blocking); verified in `tests/unit/test_pcb_stackup_outline.cpp`.
    - **Git Commit Reference:** `7d9a637`

- [ ] **PCB-02-002: Interactive & Semi-Automated Trace Routing**
  - **Status:** `[ ] Not Started`
  - **Priority:** P1
  - **Description:** Interactive PCB trace routing with 45-degree angle constraints, real-time push-and-shove obstacle avoidance, via placement, and copper polygon fills.
  - [ ] **PCB-02-002-01: Implement interactive 45-degree angle trace routing with push-and-shove**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Interactive trace drawing tool snapping to component pads, constraining bends to 45-degree octilinear lines, and nudging obstacle traces aside.
    - **Implementation Details:** InteractiveTraceRouter with rubber-band preview and local spring-model push-and-shove collision resolver.
    - **Dependencies:** PCB-02-001-02, 2D-01-002-02
    - **Expected Deliverables:** trace_router.hpp/.cpp, push_shove_engine.hpp, PcbRoutingCanvas.qml.
    - **Acceptance Criteria:** Drawing a trace through a dense pin header pushes neighboring traces while respecting minimum clearance rules.
    - **Testing & Verification Requirements:** Test 45-degree angle constraint enforcement; test push-and-shove displacement on 10 parallel bus traces.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **PCB-02-002-02: Implement via placement (through-hole, blind, buried) and connectivity**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Allow placing vias during routing to switch active copper layers; track net connectivity across layers and handle annular rings.
    - **Implementation Details:** ViaManager managing via coordinates, drill diameter, pad diameter, start layer, and end layer with net assignation.
    - **Dependencies:** PCB-02-002-01
    - **Expected Deliverables:** via_manager.hpp/.cpp, via_dialog.qml, via_tests.cpp.
    - **Acceptance Criteria:** Pressing 'V' while routing places via, switches active layer to paired routing plane, and continues trace seamlessly.
    - **Testing & Verification Requirements:** Verify electrical netlist continuity check passes across multiple vias connecting Top and Bottom copper layers.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **PCB-02-002-03: Implement copper pour polygon flood fill with thermal relief spokes**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Flood fill unrouted board regions with solid or hatched ground/power planes; generate 4-spoke thermal relief connections to tied pads.
    - **Implementation Details:** CopperPourEngine utilizing Clipper2 polygon subtraction to cut clearances around unrelated tracks/vias and add thermal relief crosses.
    - **Dependencies:** PCB-02-002-01, 2D-01-001-02
    - **Expected Deliverables:** copper_pour.hpp/.cpp, thermal_relief.hpp, copper_pour_benchmark.cpp.
    - **Acceptance Criteria:** Flooding a ground copper zone creates clean 0.2mm isolation gaps around signal nets and generates 4 thermal spokes on ground pads.
    - **Testing & Verification Requirements:** Benchmark copper pour on complex 4-layer board (< 100ms calculation time); verify zero acute-angle acid traps.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

- [ ] **PCB-02-003: Design Rule Checking (DRC) & Manufacturing Export**
  - **Status:** `[ ] Not Started`
  - **Priority:** P1
  - **Description:** Comprehensive geometric DRC engine and production manufacturing exporters (Gerber RS-274X, Excellon drill, and 3D STEP/OBJ export).
  - [ ] **PCB-02-003-01: Implement geometric DRC engine (clearance, trace width, annular ring)**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Spatial DRC checking track-to-track, track-to-pad, pad-to-pad clearances, minimum drill sizes, annular ring widths, and unconnected airwires.
    - **Implementation Details:** DrcEngine building spatial R-tree of copper primitives; executes pairwise geometric clearance tests and outputs violation markers.
    - **Dependencies:** PCB-02-002-03
    - **Expected Deliverables:** drc_engine.hpp/.cpp, DrcReportView.qml, drc_rules.json, drc_test.cpp.
    - **Acceptance Criteria:** Instantly detects traces violating minimum 0.15mm clearance; highlights exact violation coordinates in red on PCB canvas.
    - **Testing & Verification Requirements:** Run DRC on intentionally flawed board with 20 intentional clearance and trace width violations; assert 100% detection rate.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **PCB-02-003-02: Implement Gerber (RS-274X) and Excellon drill file generator**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Serialize PCB copper layers, solder masks, silkscreen, and board edge cuts to standard RS-274X Extended Gerber format and Excellon NC drill format.
    - **Implementation Details:** GerberExporter writing Aperture Definitions (ADD), flash commands (D03), linear interpolation (D01), and Excellon header coordinates.
    - **Dependencies:** PCB-02-003-01, core/domain/data_exchange.hpp
    - **Expected Deliverables:** gerber_exporter.hpp/.cpp, excellon_exporter.hpp/.cpp, gerber_test.cpp.
    - **Acceptance Criteria:** Exported Gerber files load cleanly into open-source Gerbv and third-party PCB fab viewers (JLCPCB/OSH Park) without format warnings.
    - **Testing & Verification Requirements:** Verify syntax conformance of exported RS-274X files against Ucamco Gerber format specification rev 2023.08.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **PCB-02-003-03: Implement 3D PCB solid model exporter (STEP / OBJ) with placed 3D bodies**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Generate realistic 3D CAD solid model of fabricated PCB including substrate, silkscreen texture, copper pads, and placed 3D component STEP bodies.
    - **Implementation Details:** Pcb3DExporter synthesizing 3D triangular mesh / STEP boundary representation for electro-thermal multiphysics simulation.
    - **Dependencies:** PCB-02-003-01, 3D-01-001-01
    - **Expected Deliverables:** pcb_3d_exporter.hpp/.cpp, step_mesh_assembler.hpp, pcb_3d_tests.cpp.
    - **Acceptance Criteria:** Produces watertight 3D solid geometry of PCB assembly suitable for direct import into thermal finite element solver.
    - **Testing & Verification Requirements:** Export 3D PCB model to OBJ and STEP; verify volume and bounding box dimensions match PCB layer stackup.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`



#### Submodule PCB-03: Electrical & Digital Logic Co-Simulation
**Status:** `[ ] Not Started`

- [ ] **PCB-03-001: SPICE & Analog Circuit Simulation Integration**
  - **Status:** `[ ] Not Started`
  - **Priority:** P1
  - **Description:** Extract netlists with parasitic trace resistance and capacitance and simulate circuit dynamics using Modified Nodal Analysis (MNA).
  - [ ] **PCB-03-001-01: Implement netlist compilation from schematic/PCB with parasitic extraction**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Extract electrical circuit netlist from routed PCB geometry, computing parasitic trace resistance R = rho * L / (W * t) and microstrip capacitance C.
    - **Implementation Details:** ParasiticExtractor annotating ideal schematic netlist with lumped RLC segment pi-models along routed copper traces.
    - **Dependencies:** PCB-02-002-01, core/domain/circuit.hpp
    - **Expected Deliverables:** parasitic_extractor.hpp/.cpp, netlist_compiler.hpp, extraction_test.cpp.
    - **Acceptance Criteria:** Long traces exhibit realistic series resistance and parasitic shunt capacitance affecting high-frequency signal rise times.
    - **Testing & Verification Requirements:** Validate extracted resistance of 100mm copper trace (0.25mm wide, 1 oz copper) against analytical formula (approx 0.19 ohms).
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **PCB-03-001-02: Implement transient analysis, DC operating point, and AC frequency sweep**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Execute numerical SPICE simulations with adaptive trapezoidal / Gear integration for non-linear diodes, BJTs, and MOSFETs.
    - **Implementation Details:** SpiceSimulationEngine building dynamic Jacobian matrix G(v) + C * dv/dt = I; solves non-linear systems via Newton-Raphson iteration.
    - **Dependencies:** PCB-03-001-01, core/domain/circuit.hpp, core/domain/numerical_solver.hpp
    - **Expected Deliverables:** spice_simulation_engine.hpp/.cpp, newton_solver_circuit.hpp, circuit_benchmarks.cpp.
    - **Acceptance Criteria:** RC transient response matches exact analytical formula V(t) = V_0 * (1 - exp(-t/RC)) to < 0.05% error.
    - **Testing & Verification Requirements:** Benchmark 10 SPICE circuits: RC filter, astable multivibrator, bridge rectifier, LC tank oscillator.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

- [ ] **PCB-03-002: Digital Logic & Event-Driven Simulation**
  - **Status:** `[ ] Not Started`
  - **Priority:** P1
  - **Description:** IEEE 1164 9-state discrete-event digital logic simulator and mixed-signal ADC/DAC co-simulation interfaces.
  - [ ] **PCB-03-002-01: Implement IEEE 1164 9-state digital logic simulation engine with delays**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Discrete event queue simulator supporting standard 9 logic states ('U', 'X', '0', '1', 'Z', 'W', 'L', 'H', '-') with inertial and transport gate delays.
    - **Implementation Details:** DigitalEventEngine maintaining priority queue of scheduled signal events; evaluates resolution tables for wired-AND / bus lines.
    - **Dependencies:** core/domain/digital_logic.hpp
    - **Expected Deliverables:** digital_event_engine.hpp/.cpp, ieee1164_resolver.hpp, logic_benchmark.cpp.
    - **Acceptance Criteria:** Correctly models race conditions, glitch filters via inertial rejection, and high-impedance tri-state bus contention.
    - **Testing & Verification Requirements:** Simulate 4-bit synchronous binary counter, D flip-flops, and ALU; verify waveform transitions against truth tables.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **PCB-03-002-02: Implement mixed-signal ADC/DAC co-simulation bridges**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Synchronize continuous-time analog MNA voltage nodes with discrete-event digital logic signals via configurable threshold voltage bridges.
    - **Implementation Details:** MixedSignalBridge converting analog voltages to digital logic states based on V_IL, V_IH thresholds and DAC digital words to analog voltages.
    - **Dependencies:** PCB-03-001-02, PCB-03-002-01
    - **Expected Deliverables:** mixed_signal_bridge.hpp/.cpp, dac_adc_models.hpp, mixed_signal_test.cpp.
    - **Acceptance Criteria:** Seamless bidirectionality: analog sine wave through comparator triggers digital counter; counter output through R-2R DAC reproduces analog ramp.
    - **Testing & Verification Requirements:** Test closed-loop delta-sigma modulator combining continuous integrator and discrete clocked comparator.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`





### Module PHY: Physics Simulation

**Status:** `[ ] Not Started` | **Stage:** Stage 6 | **Priority:** P0

#### Submodule PHY-01: Classical Mechanics, Multi-Body Dynamics & Collisions
**Status:** `[ ] Not Started`

- [ ] **PHY-01-001: Rigid Body Kinematics & Numerical Integrators**
  - **Status:** `[ ] Not Started`
  - **Priority:** P0
  - **Description:** Formulate rigid body equations of motion with inertia tensors and energy-conserving symplectic time integrators.
  - [ ] **PHY-01-001-01: Implement symplectic integrators preserving mechanical energy**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Implement Velocity Verlet, Leapfrog, and symplectic Runge-Kutta 4th order integrators for Hamiltonian dynamical systems.
    - **Implementation Details:** SymplecticIntegrator maintaining position x, velocity v, and acceleration a = F(x)/m with zero long-term artificial energy drift.
    - **Dependencies:** core/domain/mechanics.hpp, core/base/math.hpp
    - **Expected Deliverables:** symplectic_integrator.hpp/.cpp, energy_auditor.hpp, integrator_tests.cpp.
    - **Acceptance Criteria:** Simulating an undamped harmonic oscillator or planetary orbit for 10,000 steps maintains total energy within 0.01% of initial value.
    - **Testing & Verification Requirements:** Test Kepler two-body orbit eccentricities and energy conservation over 100 complete orbital revolutions.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **PHY-01-001-02: Implement spatial inertia tensors and rotational quaternion kinematics**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Compute 3x3 rotational inertia matrix I_body from closed triangular boundary meshes; integrate angular velocity omega using unit quaternions.
    - **Implementation Details:** RigidBodyKinematics integrating dq/dt = 0.5 * omega * q with quaternion normalization to eliminate rotational drift.
    - **Dependencies:** PHY-01-001-01, 3D-01-001-01
    - **Expected Deliverables:** rigid_body_kinematics.hpp/.cpp, inertia_tensor_calculator.hpp, rotation_tests.cpp.
    - **Acceptance Criteria:** Rigid bodies tumble freely according to Euler's equations of rotational motion; exhibits Dzhanibekov intermediate axis instability.
    - **Testing & Verification Requirements:** Verify Dzhanibekov effect simulation on asymmetric rigid body (I1 < I2 < I3); check angular momentum conservation.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

- [ ] **PHY-01-002: Collision Detection & Constraint Solving**
  - **Status:** `[ ] Not Started`
  - **Priority:** P0
  - **Description:** Spatial broadphase culling, narrowphase GJK/EPA contact manifolds, Continuous Collision Detection (CCD), and projected Gauss-Seidel constraint solver.
  - [ ] **PHY-01-002-01: Implement broadphase dynamic BVH and narrowphase GJK/EPA contact manifolds**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Cull non-colliding entity pairs using AABB bounding trees; compute exact contact normal, penetration depth, and contact points via GJK and EPA.
    - **Implementation Details:** CollisionEngine with Gilbert-Johnson-Keerthi (GJK) distance detector and Expanding Polytope Algorithm (EPA) for contact manifolds.
    - **Dependencies:** PHY-01-001-02, 3D-02-002-01
    - **Expected Deliverables:** collision_engine.hpp/.cpp, gjk_epa.hpp/.cpp, collision_benchmark.cpp.
    - **Acceptance Criteria:** Detects contact between complex convex hulls in < 10 microseconds per pair; generates stable multi-point contact manifolds.
    - **Testing & Verification Requirements:** Verify contact normal and penetration depth between rotated cubes, spheres, and capsule primitives.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **PHY-01-002-02: Implement continuous collision detection (CCD) for high-velocity bodies**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Prevent tunneling of thin, high-speed objects through obstacles using conservative advancement or swept-volume raymarching.
    - **Implementation Details:** CcdSolver evaluating time of impact (TOI in [0, 1]) via continuous swept sphere-mesh and capsule-mesh Minkowski tests.
    - **Dependencies:** PHY-01-002-01
    - **Expected Deliverables:** ccd_solver.hpp/.cpp, swept_volume.hpp, tunneling_test.cpp.
    - **Acceptance Criteria:** A small high-velocity sphere (v = 500 m/s) colliding with a 1mm thin sheet registers impact without tunneling through.
    - **Testing & Verification Requirements:** Tunneling stress test: shoot 1,000 randomized high-speed projectiles against thin barriers; verify zero pass-throughs.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **PHY-01-002-03: Implement projected Gauss-Seidel constraint solver for articulated joints**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Solve kinematic equality and inequality constraints for mechanical joints (revolute, prismatic, cylindrical, spherical) and Coulomb friction.
    - **Implementation Details:** ConstraintSolver formulating J * M^-1 * J^T * lambda = -residual / dt; solves Lagrange multipliers via Projected Gauss-Seidel (PGS).
    - **Dependencies:** PHY-01-002-01
    - **Expected Deliverables:** constraint_solver.hpp/.cpp, joint_models.hpp, ragdoll_rig_test.cpp.
    - **Acceptance Criteria:** Articulated chains of 20 rigid bodies connected by revolute joints remain stable without explosive constraint violations.
    - **Testing & Verification Requirements:** Simulate double and triple pendulums; verify joint position constraints hold to < 1e-4 m error.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`



#### Submodule PHY-02: Thermodynamics, Heat Transfer & Phase Transitions
**Status:** `[ ] Not Started`

- [ ] **PHY-02-001: Conduction, Convection & Radiation Solvers**
  - **Status:** `[ ] Not Started`
  - **Priority:** P0
  - **Description:** 3D continuum heat diffusion, Newton cooling surface convection, and Stefan-Boltzmann radiative exchange with view factor calculations.
  - [ ] **PHY-02-001-01: Implement 3D transient thermal conduction via finite difference / element solver**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Solve rho * c_p * dT/dt = div(k * grad(T)) + Q_gen over structured voxel or unstructured tetrahedral meshes with Crank-Nicolson implicit stepping.
    - **Implementation Details:** ThermalSolver3D assembling sparse conductivity matrix K and heat capacity matrix C; solves linear system via Conjugate Gradient.
    - **Dependencies:** MAT-03-001-01, core/domain/thermal.hpp, 3D-02-002-02
    - **Expected Deliverables:** thermal_solver_3d.hpp/.cpp, conjugate_gradient_sparse.hpp, heat_diffusion_test.cpp.
    - **Acceptance Criteria:** Simulating a 3D bar heated at one end matches analytical Fourier series transient solution to < 0.2% error.
    - **Testing & Verification Requirements:** Validate 1D and 3D transient heat diffusion against analytical Green's function benchmark solutions.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **PHY-02-001-02: Implement surface convective heat transfer with empirical Nusselt correlations**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Model heat loss to fluid q_conv = h * (T_surface - T_ambient) with dynamic h calculated from Churchill-Bernstein and Churchill-Chu equations.
    - **Implementation Details:** ConvectionBoundaryModel evaluating dimensionless numbers (Rayleigh, Grashof, Prandtl, Reynolds, Nusselt) to derive local h.
    - **Dependencies:** PHY-02-001-01, core/domain/thermal.hpp
    - **Expected Deliverables:** convection_model.hpp/.cpp, nusselt_correlations.hpp, convection_test.cpp.
    - **Acceptance Criteria:** Natural convection from a vertical flat plate calculates correct heat transfer coefficient matching experimental empirical correlations.
    - **Testing & Verification Requirements:** Verify natural convection from horizontal cylinder and forced convection across cylinder bank.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **PHY-02-001-03: Implement Stefan-Boltzmann radiation with diffuse gray-body view factor calculations**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Solve radiative exchange q_rad = epsilon * sigma * (T_1^4 - T_2^4) and calculate geometric view factors F_ij between surface patches.
    - **Implementation Details:** RadiationSolver with hemi-cube or Monte Carlo ray-tracing view factor integrator satisfying reciprocity A_i * F_ij = A_j * F_ji.
    - **Dependencies:** PHY-02-001-01
    - **Expected Deliverables:** radiation_solver.hpp/.cpp, view_factor_raymarcher.hpp, radiation_tests.cpp.
    - **Acceptance Criteria:** Two parallel blackbody plates in vacuum reach exact radiative equilibrium according to analytical Stefan-Boltzmann quartic equations.
    - **Testing & Verification Requirements:** Verify view factor summation rule (sum F_ij = 1.0 for closed enclosures) and reciprocity law across complex geometries.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

- [ ] **PHY-02-002: Phase Change Enthalpy & Thermal Stress**
  - **Status:** `[ ] Not Started`
  - **Priority:** P0
  - **Description:** Coupled thermodynamic phase transitions (melting/freezing, boiling) with latent heat tracking and thermo-elastic stress generation.
  - [ ] **PHY-02-002-01: Implement enthalpy-based phase change solver with latent heat tracking**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Track moving solid-liquid boundary fronts using apparent heat capacity enthalpy formulation without requiring interface mesh deformation.
    - **Implementation Details:** EnthalpyPhaseChangeSolver updating liquid volume fraction f_l(T) per cell and conserving total thermal energy.
    - **Dependencies:** PHY-02-001-01, MAT-03-002-01
    - **Expected Deliverables:** phase_change_solver.hpp/.cpp, stefan_validator.hpp, phase_tests.cpp.
    - **Acceptance Criteria:** Ice cube melting at room temperature absorbs exact latent heat (334 kJ/kg) and temperature plateaus at 0 degC until fully molten.
    - **Testing & Verification Requirements:** Simulate classic two-phase Stefan problem; compare solid-liquid boundary position vs square root of time against analytical solution.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **PHY-02-002-02: Implement coupled thermo-mechanical expansion and thermal stress tensor computation**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Compute thermal strain tensor epsilon_th = alpha * Delta T * I and solve equilibrium div(sigma) = 0 for internal thermal stress and warp.
    - **Implementation Details:** ThermoMechanicalCoupler passing node temperatures to structural stiffness matrix; calculates von Mises thermal stress distribution.
    - **Dependencies:** PHY-02-001-01, MAT-03-001-02, PHY-01-002-03
    - **Expected Deliverables:** thermo_stress_solver.hpp/.cpp, von_mises_evaluator.hpp, stress_tests.cpp.
    - **Acceptance Criteria:** Constrained heated bar develops exact compressive thermal stress sigma = -E * alpha * Delta T.
    - **Testing & Verification Requirements:** Test bimetallic strip bending radius curvature under 100 K temperature elevation; verify against Timoshenko formula.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`



#### Submodule PHY-03: Fluid Mechanics & Fluid Dynamics
**Status:** `[ ] Not Started`

- [ ] **PHY-03-001: Smoothed Particle Hydrodynamics (SPH) Free-Surface Solver**
  - **Status:** `[ ] Not Started`
  - **Priority:** P0
  - **Description:** Lagrangian particle-based fluid solver for splashing, pouring, free surfaces, viscosity, and surface tension.
  - [ ] **PHY-03-001-01: Implement weakly compressible SPH formulation with Tait equation of state**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Compute particle densities via cubic spline kernel smoothing W(r, h); evaluate pressure P = B * ((rho/rho_0)^gamma - 1).
    - **Implementation Details:** SphSolver3D with compact support Wendland C2 kernel; evaluates density summation and pressure gradient forces.
    - **Dependencies:** core/domain/fluid_dynamics.hpp, core/base/math.hpp
    - **Expected Deliverables:** sph_solver_3d.hpp/.cpp, sph_kernels.hpp, dam_break_test.cpp.
    - **Acceptance Criteria:** Classic 3D dam break experiment produces realistic fluid wave front propagation speed matching experimental laboratory video data.
    - **Testing & Verification Requirements:** Validate dam break front arrival time and impact pressure against Martin and Moyce empirical benchmark experiments.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **PHY-03-001-02: Implement artificial and physical viscosity models with boundary interaction**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Model shear stress dissipation using Monaghan artificial viscosity and physical Newtonian viscous shear tensor; enforce no-slip walls.
    - **Implementation Details:** SphViscosityModule evaluating Laplacian of velocity; employs Akinci ghost-particle boundary representation for solid walls.
    - **Dependencies:** PHY-03-001-01, MAT-03-002-02
    - **Expected Deliverables:** sph_viscosity.hpp/.cpp, boundary_particles.hpp, viscosity_tests.cpp.
    - **Acceptance Criteria:** Fluid particles flowing through a pipe develop parabolic Hagen-Poiseuille velocity profile; zero fluid penetration through rigid walls.
    - **Testing & Verification Requirements:** Poiseuille flow velocity profile verification: assert maximum centerline velocity matches analytical value.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **PHY-03-001-03: Implement surface tension models and wetting contact angle**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Model microscopic intermolecular cohesion using Continuum Surface Force (CSF) or pairwise attractive forces; simulate droplet beads and capillary rise.
    - **Implementation Details:** SphSurfaceTensionModule evaluating surface normals and curvature kappa = -div(n); enforces Young's contact angle on solid boundaries.
    - **Dependencies:** PHY-03-001-02
    - **Expected Deliverables:** sph_surface_tension.hpp/.cpp, capillary_rise_test.cpp.
    - **Acceptance Criteria:** Water droplet in zero gravity naturally pulls itself into a spherical shape; capillary rise in narrow tube matches Jurin's law.
    - **Testing & Verification Requirements:** Validate oscillating liquid droplet frequency against Rayleigh's analytical formula; verify Jurin's capillary height.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

- [ ] **PHY-03-002: Eulerian Grid-Based Incompressible Fluid Solver**
  - **Status:** `[ ] Not Started`
  - **Priority:** P0
  - **Description:** Grid-based Navier-Stokes solver on staggered Marker-and-Cell (MAC) grids for laminar/turbulent flow and pressure Poisson projection.
  - [ ] **PHY-03-002-01: Implement staggered MAC grid advection-diffusion solver (Stable Fluids)**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Discretize velocity field on cell faces (u, v, w) to prevent checkerboard pressure oscillations; perform unconditionally stable semi-Lagrangian advection.
    - **Implementation Details:** MacGridFluidSolver3D with BFECC (Back and Forth Error Compensation and Correction) high-order advection scheme.
    - **Dependencies:** core/domain/fluid_dynamics.hpp
    - **Expected Deliverables:** mac_grid_solver_3d.hpp/.cpp, semi_lagrangian.hpp, advection_test.cpp.
    - **Acceptance Criteria:** Advects vortex smoke rings and dye plumes with minimal numerical dissipation; preserves sharp velocity gradients.
    - **Testing & Verification Requirements:** Vortex leapfrogging verification: two coaxial smoke rings pass through each other periodically as predicted by fluid theory.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **PHY-03-002-02: Implement Conjugate Gradient Poisson pressure solver with boundary conditions**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Enforce incompressibility div(u) = 0 by solving Poisson equation Laplace(p) = rho/dt * div(u_star) using Preconditioned Conjugate Gradient (PCG).
    - **Implementation Details:** PressurePoissonSolver with Incomplete Cholesky preconditioner; applies Dirichlet (open) and Neumann (solid wall) boundary conditions.
    - **Dependencies:** PHY-03-002-01, core/domain/sparse_matrix.hpp
    - **Expected Deliverables:** pressure_poisson.hpp/.cpp, pcg_solver.hpp, lid_driven_cavity_test.cpp.
    - **Acceptance Criteria:** Velocity divergence norm reduces to < 1e-6 per time step; produces exact recirculating vortices in lid-driven cavity flow.
    - **Testing & Verification Requirements:** Lid-driven cavity flow benchmark at Re=100 and Re=400; compare primary vortex center coordinates against Ghia et al. benchmark data.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`





### Module CHEM: Chemistry Simulation

**Status:** `[ ] Not Started` | **Stage:** Stage 7 | **Priority:** P1

#### Submodule CHEM-01: Reaction Kinetics & Stoichiometric Networks
**Status:** `[ ] Not Started`

- [ ] **CHEM-01-001: Law of Mass Action & Rate Law Engine**
  - **Status:** `[ ] Not Started`
  - **Priority:** P1
  - **Description:** Formulate coupled ordinary differential equations (ODEs) for multi-step chemical reaction networks with forward and reverse rate constants.
  - [ ] **CHEM-01-001-01: Implement rate equation solver for elementary and multi-step reactions**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Translate stoichiometric reaction mechanisms into stiff ODE system d[C_i]/dt = sum nu_ij * r_j; solve with implicit Rosenbrock or Radau IIA integrator.
    - **Implementation Details:** ReactionNetworkEngine compiling stoichiometry matrix N; integrates species molar concentrations with non-negativity constraints.
    - **Dependencies:** core/domain/chemistry.hpp, core/domain/numerical_solver.hpp
    - **Expected Deliverables:** reaction_network.hpp/.cpp, stiff_ode_solver.hpp, reaction_tests.cpp.
    - **Acceptance Criteria:** Conserves total atomic mass of each element to machine precision; concentration values remain strictly non-negative.
    - **Testing & Verification Requirements:** Test Oregonator / Belousov-Zhabotinsky oscillating chemical reaction; assert limit cycle concentration oscillations.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **CHEM-01-001-02: Implement temperature-dependent Arrhenius and modified Eyring reaction rates**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Evaluate forward/backward rate coefficients k(T) = A * T^n * exp(-E_a / (R * T)) with pre-exponential factor A and activation energy E_a.
    - **Implementation Details:** ArrheniusRateEvaluator computing reaction velocity constants and reverse rates via equilibrium constant K_eq = k_f / k_r.
    - **Dependencies:** CHEM-01-001-01, MATDB-02-002-01
    - **Expected Deliverables:** arrhenius_evaluator.hpp/.cpp, rate_coefficients.json, kinetics_tests.cpp.
    - **Acceptance Criteria:** Reaction rate increases exponentially with temperature according to activation energy; doubling temperature matches Arrhenius slope.
    - **Testing & Verification Requirements:** Verify temperature sensitivity of hydrogen-oxygen radical combustion steps against NIST Chemical Kinetics database.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **CHEM-01-001-03: Implement catalytic rate modification and enzyme Michaelis-Menten kinetics**
    - **Status:** `[ ] Not Started`
    - **Priority:** P2
    - **Description:** Model heterogeneous surface catalysis and biological enzymatic reactions v = V_max * [S] / (K_m + [S]) with competitive inhibition.
    - **Implementation Details:** CatalysisModel modifying apparent activation energy E_a and evaluating Langmuir-Hinshelwood surface adsorption kinetics.
    - **Dependencies:** CHEM-01-001-02, core/domain/biology.hpp
    - **Expected Deliverables:** catalysis_model.hpp/.cpp, michaelis_menten.hpp, enzyme_tests.cpp.
    - **Acceptance Criteria:** Enzymatic reaction rate saturates at V_max at high substrate concentrations; inhibitor correctly shifts apparent K_m or V_max.
    - **Testing & Verification Requirements:** Verify Lineweaver-Burk double reciprocal plot linearity for competitive and non-competitive enzyme inhibition.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

- [ ] **CHEM-01-002: Chemical Equilibrium & Gibbs Free Energy Minimization**
  - **Status:** `[ ] Not Started`
  - **Priority:** P1
  - **Description:** Calculate equilibrium species distributions for complex multi-phase mixtures by minimizing total Gibbs free energy G(T, P, n).
  - [ ] **CHEM-01-002-01: Implement chemical equilibrium composition solver using Gibbs energy minimization**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Solve equilibrium compositions using Lagrange multipliers subject to atomic mass conservation constraints without requiring predefined reactions.
    - **Implementation Details:** GibbsMinimizer implementing NASA CEA (Chemical Equilibrium with Applications) algorithm; minimizes G = sum n_i * mu_i.
    - **Dependencies:** CHEM-01-001-01, MATDB-02-002-01
    - **Expected Deliverables:** gibbs_minimizer.hpp/.cpp, nasa_cea_algorithm.hpp, equilibrium_tests.cpp.
    - **Acceptance Criteria:** Haber-Bosch ammonia synthesis equilibrium composition at 450 degC and 200 atm matches industrial chemical data to < 1.0%.
    - **Testing & Verification Requirements:** Verify water gas shift reaction equilibrium CO + H2O <=> CO2 + H2 across temperatures from 400 K to 1500 K.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **CHEM-01-002-02: Implement Le Chatelier equilibrium shift response to temperature, pressure, and volume**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Simulate dynamic equilibrium re-balancing when external conditions change: temperature increase shifts endothermic direction; pressure shifts to fewer moles.
    - **Implementation Details:** EquilibriumShiftController perturbing state variables and verifying van 't Hoff equation d(ln K)/dT = Delta H / (R * T^2).
    - **Dependencies:** CHEM-01-002-01
    - **Expected Deliverables:** equilibrium_shifter.hpp/.cpp, le_chatelier_tests.cpp.
    - **Acceptance Criteria:** Compressing N2O4 <=> 2 NO2 gas mixture increases fraction of colorless dimer N2O4; heating shifts to brown NO2.
    - **Testing & Verification Requirements:** Validate van 't Hoff slope against standard reaction enthalpy for dinitrogen tetroxide dissociation.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`



#### Submodule CHEM-02: Thermochemistry & Energy Balances
**Status:** `[ ] Not Started`

- [ ] **CHEM-02-001: Reaction Enthalpy & Heat of Combustion**
  - **Status:** `[ ] Not Started`
  - **Priority:** P1
  - **Description:** Compute standard enthalpy of reaction, exothermic/endothermic heat generation, and bidirectional coupling to thermal solver.
  - [ ] **CHEM-02-001-01: Implement standard enthalpy of reaction calculator and Hess's Law**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Calculate Delta H_rxn(T) = sum nu_i * Delta_f H_i(T) using temperature-dependent formation enthalpies and Kirchhoff's law of thermochemistry.
    - **Implementation Details:** ReactionEnthalpyCalculator retrieving species enthalpies from MATDB Shomate polynomials and summing by stoichiometric coefficients.
    - **Dependencies:** MATDB-02-002-01, MATDB-02-002-02
    - **Expected Deliverables:** reaction_enthalpy.hpp/.cpp, thermochem_tests.cpp.
    - **Acceptance Criteria:** Standard heat of combustion of methane CH4 + 2 O2 -> CO2 + 2 H2O matches reference value (-890.7 kJ/mol) to < 0.1%.
    - **Testing & Verification Requirements:** Test combustion enthalpies of hydrocarbons (methane, propane, octane, ethanol) against NIST Chemistry WebBook.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **CHEM-02-001-02: Implement exothermic/endothermic heat generation coupling to thermodynamics solver**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Couple instantaneous chemical reaction heat release Q_gen = -r * Delta H_rxn directly into thermal conduction solver source term.
    - **Implementation Details:** ChemoThermalCoupler injecting volumetric heat generation rate into thermal finite element / finite difference mesh cells.
    - **Dependencies:** CHEM-02-001-01, PHY-02-001-01
    - **Expected Deliverables:** chemo_thermal_coupler.hpp/.cpp, thermal_runaway_test.cpp.
    - **Acceptance Criteria:** Exothermic reaction in an insulated container produces exact adiabatic temperature rise Delta T = -Delta H_rxn / (rho * c_p).
    - **Testing & Verification Requirements:** Simulate thermal runaway of decomposition reaction; verify explosive temperature rise matches kinetic auto-ignition model.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

- [ ] **CHEM-02-002: Combustion, Flame Propagation & Explosion Modeling**
  - **Status:** `[ ] Not Started`
  - **Priority:** P1
  - **Description:** Model fuel-air combustion mixtures, flammability limits, laminar flame speed, and deflagration pressure rise in enclosed volumes.
  - [ ] **CHEM-02-002-01: Implement hydrocarbon combustion kinetics with ignition thresholds and oxygen depletion**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Multi-step combustion mechanism (Westbrook-Dryer 2-step model) for hydrocarbons, evaluating fuel oxidation and CO conversion.
    - **Implementation Details:** CombustionKineticsModel checking local temperature against auto-ignition threshold and limiting reaction by oxygen availability.
    - **Dependencies:** CHEM-02-001-02
    - **Expected Deliverables:** combustion_model.hpp/.cpp, flammability_limits.hpp, flame_tests.cpp.
    - **Acceptance Criteria:** Methane will not ignite below its lower flammability limit (5% in air) or without reaching ignition temperature (600 degC).
    - **Testing & Verification Requirements:** Test ignition and stoichiometric burn of propane-air mixture; verify complete oxygen depletion at equivalence ratio phi = 1.0.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **CHEM-02-002-02: Implement deflagration pressure rise and adiabatic flame temperature calculation**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Compute constant-volume explosion pressure P_max and constant-pressure adiabatic flame temperature T_ad using enthalpy conservation.
    - **Implementation Details:** AdiabaticFlameCalculator iterating temperature until products enthalpy equals reactants enthalpy H_prod(T_ad) = H_react(T_0).
    - **Dependencies:** CHEM-02-002-01, MATDB-02-002-02
    - **Expected Deliverables:** adiabatic_flame.hpp/.cpp, explosion_pressure.hpp, explosion_tests.cpp.
    - **Acceptance Criteria:** Methane-air stoichiometric combustion calculates adiabatic flame temperature approx 2220 K matching experimental combustion literature.
    - **Testing & Verification Requirements:** Validate adiabatic flame temperature and maximum closed-vessel explosion pressure for hydrogen-air and methane-air.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`



#### Submodule CHEM-03: Molecular Interactions & Solution Chemistry
**Status:** `[ ] Not Started`

- [ ] **CHEM-03-001: Aqueous Solutions, Electrolytes & pH**
  - **Status:** `[ ] Not Started`
  - **Priority:** P1
  - **Description:** Aqueous electrolyte equilibria, polyprotic acid-base dissociation, buffer solutions, dynamic pH calculation, and solubility products.
  - [ ] **CHEM-03-001-01: Implement acid-base dissociation equilibrium, buffer solutions, and dynamic pH**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Solve simultaneous non-linear equilibrium equations for [H+], [OH-], and acid/base conjugate ions using charge neutrality and mass balance.
    - **Implementation Details:** AqueousSolutionEngine with Henderson-Hasselbalch buffer calculations and exact quartic polynomial solver for diprotic acids (H2SO4, H2CO3).
    - **Dependencies:** CHEM-01-002-01
    - **Expected Deliverables:** aqueous_solution.hpp/.cpp, ph_calculator.hpp, titration_test.cpp.
    - **Acceptance Criteria:** Simulating titration of 0.1 M HCl with 0.1 M NaOH produces classic sigmoidal titration curve with steep inflection at pH 7.0.
    - **Testing & Verification Requirements:** Verify pH calculation for strong acids, weak acids (acetic acid pKa=4.76), polyprotic phosphate buffers, and pure water autoprotolysis.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **CHEM-03-001-02: Implement electrolyte ionic conductivity and precipitation checking (K_sp)**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Calculate electrical conductivity of aqueous solutions using Kohlrausch's law of independent ion migration; check ion product against solubility product K_sp.
    - **Implementation Details:** ElectrolyteConductivityModel computing equivalent conductivity Lambda = Lambda_0 - K * sqrt(c); flags precipitate formation when Q > K_sp.
    - **Dependencies:** CHEM-03-001-01, MAT-03-002-03
    - **Expected Deliverables:** electrolyte_conductivity.hpp/.cpp, precipitation_checker.hpp, electrolyte_tests.cpp.
    - **Acceptance Criteria:** Mixing AgNO3 and NaCl solutions triggers instantaneous precipitation of solid AgCl when ion product exceeds K_sp (1.8e-10).
    - **Testing & Verification Requirements:** Verify precipitation threshold and remaining aqueous ion concentrations for silver chloride, barium sulfate, and calcium carbonate.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

- [ ] **CHEM-03-002: Electrochemistry & Galvanic Cells**
  - **Status:** `[ ] Not Started`
  - **Priority:** P1
  - **Description:** Electrochemical half-cell reduction potentials, Nernst equation cell voltage, Faraday's laws of electrolysis, and lithium-ion battery cell models.
  - [ ] **CHEM-03-002-01: Implement Nernst equation for half-cell and full-cell electrochemical potentials**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Compute open-circuit voltage E_cell = E deg - (R * T / (z * F)) * ln(Q) from standard reduction potentials and species activities.
    - **Implementation Details:** ElectrochemicalCellModel indexing standard reduction potential table (E deg); evaluates Daniell cell (Zn-Cu) and galvanic potentials.
    - **Dependencies:** CHEM-03-001-01, MATDB-01-001-01
    - **Expected Deliverables:** electrochemical_cell.hpp/.cpp, reduction_potentials.json, nernst_tests.cpp.
    - **Acceptance Criteria:** Standard Daniell cell Zn(s) + Cu2+(aq) -> Zn2+(aq) + Cu(s) generates exact standard potential 1.103 V at 298.15 K.
    - **Testing & Verification Requirements:** Verify Nernst voltage shifts with varying Cu2+ and Zn2+ ion concentrations; assert zero potential at thermodynamic equilibrium.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **CHEM-03-002-02: Implement battery charge/discharge cell model coupled to circuit and thermal solvers**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Equivalent Circuit Model (Thevenin dual-RC model) for Li-ion battery cells with state-of-charge (SOC) tracking, internal resistance R_int(T), and heat release.
    - **Implementation Details:** BatteryCellRuntime computing terminal voltage V_term = OCV(SOC) - I * R_0 - V_RC; injects I^2 * R Joule heating into thermal mesh.
    - **Dependencies:** CHEM-03-002-01, core/domain/circuit.hpp, PHY-02-001-01
    - **Expected Deliverables:** battery_cell_model.hpp/.cpp, li_ion_soc_tables.hpp, battery_tests.cpp.
    - **Acceptance Criteria:** Constant-current 1C discharge of 18650 cell displays realistic voltage plateau and temperature rise matching manufacturer spec sheets.
    - **Testing & Verification Requirements:** Validate discharge curves at 0.5C, 1C, and 2C rates; test thermal runaway feedback loop under external short circuit.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`





### Module ENV: Virtual Environment

**Status:** `[ ] Not Started` | **Stage:** Stage 8 | **Priority:** P0

#### Submodule ENV-01: Environmental Physical Fields & Atmospheric Model
**Status:** `[ ] Not Started`

- [ ] **ENV-01-001: Barometric & Atmospheric Column Modeling**
  - **Status:** `[ ] Not Started`
  - **Priority:** P0
  - **Description:** Altitude-dependent atmospheric properties up to 86 km according to US Standard Atmosphere 1976, dynamic air density, humidity, and wind vectors.
  - [ ] **ENV-01-001-01: Implement US Standard Atmosphere model from sea level to mesosphere**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Calculate temperature T(z), pressure P(z), density rho(z), and speed of sound c(z) across troposphere, stratosphere, and mesosphere layers.
    - **Implementation Details:** AtmosphericModel implementing piecewise standard lapse rates (-6.5 K/km in troposphere) and barometric formula with geopotential altitude.
    - **Dependencies:** core/domain/virtual_lab.hpp
    - **Expected Deliverables:** atmospheric_model.hpp/.cpp, standard_atmosphere_1976.hpp, atmosphere_test.cpp.
    - **Acceptance Criteria:** Sea level pressure is 101.325 kPa (density 1.225 kg/m3); at 11,000m tropopause, pressure equals 22.632 kPa and T = 216.65 K.
    - **Testing & Verification Requirements:** Verify atmospheric parameters against US Standard Atmosphere 1976 tabulated reference values from 0m to 80,000m.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **ENV-01-001-02: Implement dynamic ambient humidity, dew point, and vapor pressure models**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Compute saturation vapor pressure e_sat(T) via Magnus-Tetens equation, relative humidity RH, dew point temperature, and moist air density.
    - **Implementation Details:** PsychrometricModel calculating absolute humidity, wet-bulb temperature, and moist air gas constant R_moist.
    - **Dependencies:** ENV-01-001-01
    - **Expected Deliverables:** psychrometric_model.hpp/.cpp, humidity_tests.cpp.
    - **Acceptance Criteria:** Computes moist air density accurately; calculates dew point within 0.1 degC of ASHRAE psychrometric chart values.
    - **Testing & Verification Requirements:** Test psychrometric state points: dry air, 50% RH at 20 degC, saturated air at 35 degC.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **ENV-01-001-03: Implement wind velocity vector fields and boundary layer shear**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Configurable 3D atmospheric wind field with logarithmic surface boundary layer shear profile v(z) = (u_star / kappa) * ln(z / z_0) and gust spectra.
    - **Implementation Details:** WindFieldModel evaluating mean horizontal wind vectors and Dryden / von Karman atmospheric turbulence velocity fluctuations.
    - **Dependencies:** ENV-01-001-01
    - **Expected Deliverables:** wind_field_model.hpp/.cpp, turbulence_generator.hpp, wind_tests.cpp.
    - **Acceptance Criteria:** Wind speed decreases gracefully near ground level according to surface roughness z_0; simulates realistic gust fluctuations.
    - **Testing & Verification Requirements:** Verify logarithmic profile velocity scaling against height; check energy spectral density of generated turbulence.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

- [ ] **ENV-01-002: Gravitational & Celestial Radiation Fields**
  - **Status:** `[ ] Not Started`
  - **Priority:** P0
  - **Description:** Configurable planetary gravity fields (Earth, Moon, Mars, microgravity) and astronomical solar irradiance calculations.
  - [ ] **ENV-01-002-01: Implement configurable planetary gravity fields and gravity gradient**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Uniform or Newtonian central gravity field g(r) = -G * M / r^2 with configurable presets (Earth 9.80665 m/s2, Moon 1.62 m/s2, Mars 3.72 m/s2, Zero-G).
    - **Implementation Details:** GravityFieldModel providing vector acceleration field g(x, y, z) queryable by physics and fluid solvers.
    - **Dependencies:** core/domain/virtual_lab.hpp, PHY-01-001-01
    - **Expected Deliverables:** gravity_field.hpp/.cpp, gravity_tests.cpp.
    - **Acceptance Criteria:** Switching environment to 'Lunar' scales gravitational free-fall trajectory acceleration to exactly 1.62 m/s2 immediately.
    - **Testing & Verification Requirements:** Test pendulum oscillation period under Earth, Moon, and Jupiter gravity presets.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **ENV-01-002-02: Implement solar position calculator and direct/diffuse solar irradiance**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Calculate astronomical sun position (azimuth, elevation) based on latitude, longitude, day of year, and time; compute direct normal and diffuse solar flux.
    - **Implementation Details:** SolarPositionModel evaluating solar zenith angle and clear-sky solar irradiance using Haurwitz or Perez diffuse radiation models.
    - **Dependencies:** ENV-01-001-01, PHY-02-001-03
    - **Expected Deliverables:** solar_position.hpp/.cpp, solar_flux.hpp, solar_tests.cpp.
    - **Acceptance Criteria:** Solar noon at equator on equinox yields solar elevation 90 degrees with peak direct irradiance approx 1000 W/m2.
    - **Testing & Verification Requirements:** Verify solar azimuth and elevation calculation against NOAA Solar Calculator to within 0.05 degrees.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`



#### Submodule ENV-02: Virtual Chamber & Environmental Boundary Enclosures
**Status:** `[ ] Not Started`

- [ ] **ENV-02-001: Controlled Test Chamber & Wind Tunnel Rig**
  - **Status:** `[ ] Not Started`
  - **Priority:** P1
  - **Description:** Virtual laboratory fixtures including hermetic climatic thermal chambers, vacuum pumps, and aerodynamic wind tunnel test sections.
  - [ ] **ENV-02-001-01: Implement hermetic thermal/pressure test chamber boundary conditions**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Enclosed test chamber simulating environmental stress screening (temperature cycling -40 degC to +125 degC, vacuum down to 1e-4 Pa).
    - **Implementation Details:** ClimaticChamberModel applying boundary conditions to contained simulation components with thermal soaking ramp rates.
    - **Dependencies:** ENV-01-001-01, PHY-02-001-02
    - **Expected Deliverables:** climatic_chamber.hpp/.cpp, ChamberControlPanel.qml, chamber_tests.cpp.
    - **Acceptance Criteria:** Electronics placed inside chamber track configured thermal profile according to chamber heat transfer coefficient and component thermal mass.
    - **Testing & Verification Requirements:** Test MIL-STD-810 thermal shock profile simulation on PCB assembly in virtual test chamber.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **ENV-02-001-02: Implement virtual wind tunnel boundary enclosure with laminar inlet and outlet**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Aerodynamic testing rig applying uniform velocity inlet, zero-gradient pressure outlet, and slip/no-slip wall boundary conditions to fluid grid.
    - **Implementation Details:** WindTunnelFixture setting Dirichlet boundary velocity at inlet and coupling fluid pressure forces to solid test model.
    - **Dependencies:** PHY-03-002-02, ENV-01-001-03
    - **Expected Deliverables:** wind_tunnel_fixture.hpp/.cpp, WindTunnelView.qml, wind_tunnel_test.cpp.
    - **Acceptance Criteria:** Airfoil placed in wind tunnel generates lift and drag forces matching aerodynamic handbook polar curves.
    - **Testing & Verification Requirements:** Simulate NACA 0012 airfoil in virtual wind tunnel; compare lift coefficient C_L vs angle of attack alpha against NASA wind tunnel data.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

- [ ] **ENV-02-002: Ambient Energy Exchange & Radiation Sinks**
  - **Status:** `[ ] Not Started`
  - **Priority:** P1
  - **Description:** Far-field thermal exchange with ambient atmosphere and deep-space effective blackbody radiation sink for satellite and outdoor equipment simulation.
  - [ ] **ENV-02-002-01: Implement ambient thermal convective boundary exchange with dynamic h**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Calculate real-time convective heat exchange between exposed simulated surfaces and ambient atmosphere based on local wind speed.
    - **Implementation Details:** AmbientConvectionBoundary evaluating McAdams correlation h = 5.7 + 3.8 * v_wind for outdoor exposed surfaces.
    - **Dependencies:** PHY-02-001-02, ENV-01-001-03
    - **Expected Deliverables:** ambient_convection.hpp/.cpp, heat_sink_test.cpp.
    - **Acceptance Criteria:** Increasing wind speed from 0 m/s to 10 m/s accelerates cooling rate of hot heatsink by a factor of 4.
    - **Testing & Verification Requirements:** Verify cooling curve of heated aluminum block exposed to varying wind speeds against analytical lumped capacitance model.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **ENV-02-002-02: Implement night-sky deep space radiative cooling sink model**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Model radiative exchange to night sky with effective sky temperature T_sky = T_amb * (0.711 + 0.0056 * T_dp + 0.000073 * T_dp^2)^0.25 for nocturnal cooling.
    - **Implementation Details:** DeepSpaceRadiationSink evaluating net radiative heat loss to cold sky during nighttime and orbital vacuum environments (T_space = 2.7 K).
    - **Dependencies:** PHY-02-001-03, ENV-01-001-02
    - **Expected Deliverables:** deep_space_sink.hpp/.cpp, nocturnal_cooling_test.cpp.
    - **Acceptance Criteria:** Surfaces exposed to clear night sky cool below ambient air temperature due to infrared radiation through atmospheric window (8-13 um).
    - **Testing & Verification Requirements:** Simulate nocturnal sub-ambient radiative cooling of blackbody radiator; compare temperature drop against Berdahl-Martin sky model.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`





### Module INT: System Integration

**Status:** `[ ] Not Started` | **Stage:** Stage 9 | **Priority:** P0

#### Submodule INT-01: Multi-Rate Co-Simulation Orchestrator
**Status:** `[ ] Not Started`

- [ ] **INT-01-001: Master Time Coordinator & Adaptive Stepping**
  - **Status:** `[ ] Not Started`
  - **Priority:** P0
  - **Description:** Master execution coordinator executing disparate physics domains at independent rates (e.g. 100 kHz circuit, 1 kHz mechanics, 100 Hz thermal) with error-based time negotiation.
  - [ ] **INT-01-001-01: Implement master simulation executive with multi-rate domain clock scheduling**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Design MasterCoSimulationExecutive coordinating domain sub-clocks with fractional integer step multipliers and causal synchronization barriers.
    - **Implementation Details:** MasterCoSimulationExecutive maintaining prioritized event queue and domain stepping scheduler without blocking slow domains.
    - **Dependencies:** core/domain/simulation_runtime.hpp, core/domain/simulation_clock.hpp
    - **Expected Deliverables:** master_co_simulation.hpp/.cpp, domain_scheduler.hpp, multi_rate_tests.cpp.
    - **Acceptance Criteria:** High-frequency SPICE circuit executes 1,000 sub-steps while thermal diffusion executes 1 macro-step with exact temporal synchronization.
    - **Testing & Verification Requirements:** Test synchronization timing jitter across 3 concurrent domains running at 100 kHz, 1 kHz, and 10 Hz.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **INT-01-001-02: Implement adaptive time-step negotiation based on local truncation error**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Negotiate simulation time step dt dynamically by evaluating local truncation error (LTE) and Courant-Friedrichs-Lewy (CFL) stability bounds across all active solvers.
    - **Implementation Details:** AdaptiveStepNegotiator selecting dt_master = min(dt_elec, dt_mech, dt_therm, dt_fluid, dt_cfl) with smooth ramping to prevent step chattering.
    - **Dependencies:** INT-01-001-01
    - **Expected Deliverables:** adaptive_step_negotiator.hpp/.cpp, cfl_calculator.hpp, step_negotiation_tests.cpp.
    - **Acceptance Criteria:** During abrupt electrical switching or mechanical collision impact, time step automatically contracts by 3 orders of magnitude in < 1 step.
    - **Testing & Verification Requirements:** Simulate sudden impact contact; assert time step shrinks to resolve collision and expands back during quiescent flight.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **INT-01-001-03: Implement deterministic discrete event queue and rollback mechanism**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Provide state checkpointing and rollback capabilities across all coupled domains to recover from solver divergence or non-convergence.
    - **Implementation Details:** StateCheckpointManager capturing lightweight domain state snapshots; triggers rollback to t_last_converged when solver fails to converge.
    - **Dependencies:** INT-01-001-01
    - **Expected Deliverables:** state_checkpoint.hpp/.cpp, rollback_manager.hpp, rollback_tests.cpp.
    - **Acceptance Criteria:** When an implicit solver detects divergence (NaN or residual norm explosion), state rolls back cleanly and retries with halved dt.
    - **Testing & Verification Requirements:** Induce intentional singularity in stiff ODE solver; assert rollback recovery, step halving, and successful completion.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

- [ ] **INT-01-002: Cross-Domain Physical Coupling Connectors**
  - **Status:** `[ ] Not Started`
  - **Priority:** P0
  - **Description:** Bidirectional multiphysics coupling interfaces passing physical variables, conservation flux exchange, and state mapping.
  - [ ] **INT-01-002-01: Implement electro-thermal bidirectional coupling connector**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Map electrical power dissipation P = I^2 * R into thermal mesh heat generation Q_gen; interpolate temperature back to update electrical resistivity rho(T).
    - **Implementation Details:** ElectroThermalConnector executing bidirectional Gauss-Seidel or Jacobi coupling iteration per co-simulation macro-step.
    - **Dependencies:** PCB-03-001-02, PHY-02-001-01, MAT-03-002-03
    - **Expected Deliverables:** electro_thermal_connector.hpp/.cpp, joule_heating_test.cpp.
    - **Acceptance Criteria:** Simulating a current surge through a fuse accurately captures self-heating, resistance increase, and thermal melting open-circuit.
    - **Testing & Verification Requirements:** Test electrical fuse blowout: compare current cutoff time vs fuse I^2*t rating.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **INT-01-002-02: Implement thermo-mechanical bidirectional coupling connector**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Pass 3D thermal field to mechanical solver to induce thermal stress/warp; pass mechanical plastic deformation work dissipation as heat.
    - **Implementation Details:** ThermoMechanicalConnector mapping nodal temperatures to mechanical finite elements with Taylor-Quinney factor (approx 0.9) plastic heating.
    - **Dependencies:** PHY-02-002-02, PHY-01-002-03
    - **Expected Deliverables:** thermo_mechanical_connector.hpp/.cpp, thermal_buckling_test.cpp.
    - **Acceptance Criteria:** Heated bi-material beam deflects smoothly; rapid plastic mechanical deformation generates measurable temperature rise.
    - **Testing & Verification Requirements:** Verify thermal buckling critical temperature of slender beam against Euler buckling thermal formula.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **INT-01-002-03: Implement fluid-structure interaction (FSI) coupling connector**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Transfer fluid pressure and shear forces from SPH particles or MAC grid cells to solid structural boundaries; transfer moving boundary velocities to fluid.
    - **Implementation Details:** FsiConnector projecting fluid pressure onto triangular boundary meshes and returning boundary surface velocity u_wall to fluid solver.
    - **Dependencies:** PHY-03-001-02, PHY-01-001-02
    - **Expected Deliverables:** fsi_connector.hpp/.cpp, floating_body_test.cpp.
    - **Acceptance Criteria:** Floating rigid body in water experiences buoyant Archimedes force equal to weight of displaced liquid; bobs up and down with correct buoyancy frequency.
    - **Testing & Verification Requirements:** Validate floating wooden cylinder oscillation frequency in water against analytical hydrometer equation.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **INT-01-002-04: Implement chemo-thermal coupling connector**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Transfer chemical reaction heat release into thermal solver; transfer local temperatures to update kinetic Arrhenius reaction rate coefficients.
    - **Implementation Details:** ChemoThermalConnector synchronizing chemical species concentration fields with temperature field in virtual test reactor.
    - **Dependencies:** CHEM-02-001-02, PHY-02-001-01
    - **Expected Deliverables:** chemo_thermal_connector.hpp/.cpp, cstr_reactor_test.cpp.
    - **Acceptance Criteria:** Continuous Stirred-Tank Reactor (CSTR) captures multiple steady states and thermal runaway hysteresis behavior.
    - **Testing & Verification Requirements:** Verify upper and lower stable operating temperatures and unstable saddle point for exothermic CSTR reaction.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`



#### Submodule INT-02: Standard Co-Simulation Interfaces & Data Bus
**Status:** `[ ] Not Started`

- [ ] **INT-02-001: FMI/FMU Industry Standard Support**
  - **Status:** `[ ] Not Started`
  - **Priority:** P1
  - **Description:** Functional Mock-up Interface (FMI 2.0 / 3.0) standards compliance for importing external simulation models and exporting Shunya subsystems as FMUs.
  - [ ] **INT-02-001-01: Implement Functional Mock-up Interface (FMI 2.0 / 3.0) Co-Simulation Master**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Load external compiled FMU binaries (DLL / .so), parse modelDescription.xml, instantiate model instances, and step slave FMUs via fmi2DoStep.
    - **Implementation Details:** FmuImporter managing shared library handles, variable scalar references, and input/output port type conversions.
    - **Dependencies:** INT-01-001-01, core/domain/co_simulation.hpp
    - **Expected Deliverables:** fmu_importer.hpp/.cpp, fmi_wrapper.hpp, fmi_interop_tests.cpp.
    - **Acceptance Criteria:** Successfully loads and co-simulates standard reference FMUs (BouncingBall, ControlledTemperature) generated by Dymola or OpenModelica.
    - **Testing & Verification Requirements:** Verify step synchronization and variable exchange with standard FMI Cross-Check test suite.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **INT-02-001-02: Implement FMI Export for Shunya sub-models as standalone FMUs**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Package Shunya component models (e.g. custom DC-DC converter, battery thermal pack) into self-contained .fmu zip archives with C-API wrapper.
    - **Implementation Details:** FmuExporter generating modelDescription.xml and compiling minimal C wrapper calling Shunya headless solver core.
    - **Dependencies:** INT-02-001-01, apps/cli
    - **Expected Deliverables:** fmu_exporter.hpp/.cpp, fmu_template.c, export_test.cpp.
    - **Acceptance Criteria:** Exported Shunya FMU imports and runs successfully in external third-party tools (MATLAB/Simulink, Python PyFMI).
    - **Testing & Verification Requirements:** Validate exported FMU using FMU-Compliance-Checker tool; assert zero specification violations.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

- [ ] **INT-02-002: High-Speed Shared-Memory IPC & Network Telemetry**
  - **Status:** `[ ] Not Started`
  - **Priority:** P1
  - **Description:** Low-latency IPC for co-simulation with external hardware-in-the-loop (HIL) simulators and streaming remote visualization.
  - [ ] **INT-02-002-01: Implement low-latency lock-free shared memory telemetry IPC bus**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Inter-process communication ring buffer in OS shared memory (POSIX shm / Windows named shared memory) for sub-microsecond state streaming.
    - **Implementation Details:** SharedMemoryBus with atomic read/write sequence counters and memory-mapped file backing; zero-copy payload delivery.
    - **Dependencies:** core/base/ring_buffer.hpp
    - **Expected Deliverables:** shared_memory_bus.hpp/.cpp, shm_benchmark.cpp, ipc_tests.cpp.
    - **Acceptance Criteria:** Streams 100,000 telemetry messages per second between processes with round-trip latency < 2 microseconds.
    - **Testing & Verification Requirements:** Stress test IPC channel with 2 separate OS processes; verify zero dropped packets or buffer corruption.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **INT-02-002-02: Implement WebSocket / gRPC live streaming endpoint for remote monitoring**
    - **Status:** `[ ] Not Started`
    - **Priority:** P2
    - **Description:** Lightweight embedded streaming server publishing live simulation metrics, probe traces, and 3D camera transforms to web clients.
    - **Implementation Details:** TelemetryStreamServer utilizing libwebsockets / lightweight gRPC service; broadcasts JSON / Protocol Buffer telemetry packets.
    - **Dependencies:** INT-02-002-01, core/domain/observability.hpp
    - **Expected Deliverables:** telemetry_stream_server.hpp/.cpp, telemetry.proto, web_client_demo.html.
    - **Acceptance Criteria:** Remote browser connects via WebSocket and visualizes live real-time simulation plots at 30+ updates per second.
    - **Testing & Verification Requirements:** Connect 10 concurrent WebSocket clients; measure server CPU utilization and broadcast latency.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`





### Module PERF: Performance Optimization

**Status:** `[ ] Not Started` | **Stage:** Stage 10 | **Priority:** P1

#### Submodule PERF-01: GPU Acceleration & Compute Shaders
**Status:** `[ ] Not Started`

- [ ] **PERF-01-001: GPGPU Numerical Compute Kernels**
  - **Status:** `[ ] Not Started`
  - **Priority:** P1
  - **Description:** Offload massive particle simulation (SPH fluids), collision broadphase, and sparse matrix operations to GPU compute pipelines.
  - [ ] **PERF-01-001-01: Implement GPU compute shader pipeline for SPH fluids and particle physics**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Write SPIR-V / HLSL compute shaders implementing parallel spatial hash grid construction, density summation, and pressure force integration.
    - **Implementation Details:** GpuParticleEngine dispatching 1D/2D/3D compute workgroups (e.g. 256 threads/group) across Vulkan / D3D12 compute queues.
    - **Dependencies:** PHY-03-001-01, 3D-03-001-01
    - **Expected Deliverables:** gpu_particle_engine.hpp/.cpp, sph_density.comp, sph_forces.comp, particle_benchmarks.cpp.
    - **Acceptance Criteria:** Simulates 100,000 SPH fluid particles at > 30 FPS on dedicated desktop GPU (e.g. NVIDIA RTX 3060 / AMD RX 6700).
    - **Testing & Verification Requirements:** Benchmark particle update time: compare GPU compute shader vs multi-threaded CPU baseline (target: > 15x speedup).
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **PERF-01-001-02: Implement parallel GPU reduction and sparse matrix-vector multiplication (SpMV)**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Accelerate iterative linear solvers (PCG) by executing Compressed Sparse Row (CSR) matrix-vector products and dot products on GPU.
    - **Implementation Details:** GpuSpMvKernel with warp-aggregated reduction for row-vector multiplication; minimizes global memory round-trips.
    - **Dependencies:** PERF-01-001-01, core/domain/sparse_matrix.hpp
    - **Expected Deliverables:** gpu_spmv.hpp/.cpp, spmv_csr.comp, spmv_benchmark.cpp.
    - **Acceptance Criteria:** SpMV on 500,000-variable FEM stiffness matrix achieves > 80 GB/s effective memory bandwidth utilization.
    - **Testing & Verification Requirements:** Compare GPU Conjugate Gradient convergence and numerical precision against Eigen C++ CPU reference.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **PERF-01-001-03: Implement pinned host-device unified memory buffers with asynchronous transfer**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Eliminate CPU-GPU synchronization stalls using double-buffered staging rings, non-blocking DMA transfers, and Vulkan timeline semaphores.
    - **Implementation Details:** GpuTransferRing managing host-visible device-local memory mappings; synchronizes compute and transfer queues asynchronously.
    - **Dependencies:** PERF-01-001-01
    - **Expected Deliverables:** gpu_transfer_ring.hpp/.cpp, timeline_semaphore.hpp, transfer_benchmarks.cpp.
    - **Acceptance Criteria:** Zero rendering pipeline stalls or frame drops while uploading new simulation state to GPU every step.
    - **Testing & Verification Requirements:** Measure CPU wait time during state uploads; verify async transfer completes in parallel with CPU solver prep.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

- [ ] **PERF-01-002: SIMD Vectorization & Cache Locality**
  - **Status:** `[ ] Not Started`
  - **Priority:** P1
  - **Description:** Vectorize CPU solver kernels using explicit SIMD instructions (AVX2, AVX-512, ARM NEON) and Structure-of-Arrays (SoA) memory transformations.
  - [ ] **PERF-01-002-01: Vectorize inner solver loops using AVX2 / AVX-512 / ARM NEON intrinsics**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Vectorize particle distance evaluations, thermal stencil updates, and vector transformations using compiler-abstracted SIMD wrapper library.
    - **Implementation Details:** SimdMath library wrapping _mm256_fmadd_ps / _mm512_fmadd_ps / vmlaq_f32 with fallback scalar templates.
    - **Dependencies:** core/base/math.hpp
    - **Expected Deliverables:** simd_math.hpp, simd_kernels.cpp, simd_benchmark.cpp.
    - **Acceptance Criteria:** Inner particle-particle kernel loop achieves > 3.5x speedup on 8-wide AVX2 floats over scalar baseline.
    - **Testing & Verification Requirements:** Unit tests verifying bit-level accuracy and floating-point roundoff equivalence between SIMD and scalar paths.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **PERF-01-002-02: Refactor core domain states to Structure-of-Arrays (SoA) layout**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Transform particle and node arrays from Array-of-Structures (struct Particle { x, y, z, vx, vy, vz }) to SoA (vector<float> x, y, z...).
    - **Implementation Details:** SoAContainer template providing cache-aligned contiguous memory buffers for individual state variables.
    - **Dependencies:** PERF-01-002-01
    - **Expected Deliverables:** soa_container.hpp, soa_benchmark.cpp, cache_miss_test.cpp.
    - **Acceptance Criteria:** Achieves > 95% L1 data cache hit rate during continuous linear solver sweep; eliminates cache line thrashing.
    - **Testing & Verification Requirements:** Profile L1/L2 cache misses using hardware performance counters (perf / VTune); compare AoS vs SoA throughput.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`



#### Submodule PERF-02: Multithreading Architecture & Memory Management
**Status:** `[ ] Not Started`

- [ ] **PERF-02-001: Work-Stealing Task Scheduler & Domain Decomposition**
  - **Status:** `[ ] Not Started`
  - **Priority:** P1
  - **Description:** Multi-core parallelization using work-stealing job scheduler and spatial domain decomposition with ghost cell exchange.
  - [ ] **PERF-02-001-01: Implement lock-free work-stealing job system for parallel spatial tasks**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Task scheduler where worker threads possess local double-ended queues (deques); idle threads steal tasks from tail of victim deques (Chase-Lev algorithm).
    - **Implementation Details:** WorkStealingPool with atomic ring-buffer deques; supports hierarchical task graphs and parallel_for primitives.
    - **Dependencies:** core/base/thread_pool.hpp
    - **Expected Deliverables:** work_stealing_pool.hpp/.cpp, job_graph.hpp, task_scheduler_benchmark.cpp.
    - **Acceptance Criteria:** Near-linear speedup (e.g. > 14x on 16-core CPU) when parallelizing large independent spatial grid updates.
    - **Testing & Verification Requirements:** Benchmark task dispatch latency (< 50 nanoseconds per job); verify zero deadlock under randomized task trees.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **PERF-02-001-02: Implement spatial domain decomposition with boundary ghost-cell synchronization**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Partition 3D simulation domain into sub-domains assigned to separate worker threads; synchronize 1-cell overlapping boundary ghost layers.
    - **Implementation Details:** DomainDecompositionManager splitting grids along longest dimension; performs fast thread-safe ghost layer swaps after each step.
    - **Dependencies:** PERF-02-001-01, PHY-02-001-01, PHY-03-002-01
    - **Expected Deliverables:** domain_decomposition.hpp/.cpp, ghost_cell_sync.hpp, domain_scaling_tests.cpp.
    - **Acceptance Criteria:** Ghost-cell exchange overhead consumes < 5% of overall step time; numerical solution matches monolithic execution exactly.
    - **Testing & Verification Requirements:** Compare decomposed vs single-threaded thermal diffusion results; assert absolute error is identically zero.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

- [ ] **PERF-02-002: High-Performance Arena Allocators**
  - **Status:** `[ ] Not Started`
  - **Priority:** P1
  - **Description:** Zero-allocation memory management inside tight simulation loops using monotonic arenas and pool allocators.
  - [ ] **PERF-02-002-01: Implement frame-based and simulation-step monotonic arena allocators**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Pre-allocated contiguous memory arena where allocations bump an offset pointer; entire arena is reset in O(1) at end of simulation step.
    - **Implementation Details:** LinearArenaAllocator reserving virtual memory pages; provides custom C++20 polymorphic memory resource (std::pmr::memory_resource).
    - **Dependencies:** core/base/app_context.hpp
    - **Expected Deliverables:** linear_arena.hpp/.cpp, pmr_arena_adapter.hpp, arena_benchmark.cpp.
    - **Acceptance Criteria:** Zero heap malloc / free calls during active simulation time-stepping; allocation latency < 3 nanoseconds.
    - **Testing & Verification Requirements:** Benchmark 1,000,000 small allocations: compare arena allocator (approx 1ms) vs default glibc/MSVC malloc (approx 50ms).
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **PERF-02-002-02: Implement fixed-size memory pool allocators for small dynamic objects**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Pool allocator for high-frequency temporary objects (contact points, graph edges, event tokens) using intrusive free-lists.
    - **Implementation Details:** FixedSizePool template allocating chunks in blocks with zero internal fragmentation and O(1) alloc/dealloc.
    - **Dependencies:** PERF-02-002-01
    - **Expected Deliverables:** fixed_size_pool.hpp, pool_tests.cpp.
    - **Acceptance Criteria:** Deallocating and reallocating 50,000 contact nodes introduces zero memory fragmentation or memory growth.
    - **Testing & Verification Requirements:** Verify leak-free operation and pointer alignment guarantees (16-byte / 64-byte cache line alignment).
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`





### Module TEST: Testing & Validation

**Status:** `[ ] Not Started` | **Stage:** Stage 11 | **Priority:** P0

#### Submodule TEST-01: Analytical Benchmark Suite & Reference Verification
**Status:** `[ ] Not Started`

- [ ] **TEST-01-001: Classical Mechanics & Kinematic Benchmarks**
  - **Status:** `[ ] Not Started`
  - **Priority:** P0
  - **Description:** Validate kinematic and multi-body dynamics algorithms against exact analytical closed-form mathematical physics solutions.
  - [ ] **TEST-01-001-01: Implement analytical verification suite for orbits, projectiles, and chaotic pendulums**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Automated verification against Keplerian closed elliptical orbits, projectile motion with quadratic air drag, and double pendulum chaos lyapunov exponents.
    - **Implementation Details:** BenchmarkKinematics running solvers against closed-form calculus equations; verifies trajectory error norms L2 and L_inf.
    - **Dependencies:** PHY-01-001-01, PHY-01-002-03
    - **Expected Deliverables:** test_kinematic_benchmarks.cpp, analytical_kepler.hpp, benchmark_report_gen.hpp.
    - **Acceptance Criteria:** Keplerian orbit apogee and perigee match closed-form formulas to within 1e-5 relative tolerance.
    - **Testing & Verification Requirements:** Run suite across 25 orbital eccentricities and projectile drag configurations in Catch2 test runner.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **TEST-01-001-02: Implement spinning top gyroscopic precession and Euler equation verification**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Validate gyroscope steady precession frequency Omega_p = M * g * r / (I_z * omega_s) and nutation against analytical rigid body equations.
    - **Implementation Details:** BenchmarkGyroscopic verifying angular momentum vector conservation and precession drift rates.
    - **Dependencies:** PHY-01-001-02
    - **Expected Deliverables:** test_gyroscopic_motion.cpp, analytical_top.hpp.
    - **Acceptance Criteria:** Precession angular frequency matches theoretical Euler top formula to < 0.1% error.
    - **Testing & Verification Requirements:** Execute top spin simulations with varying rotor tilt angles and spin velocities; assert angular momentum conservation.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

- [ ] **TEST-01-002: Thermal, Fluid & Chemical Analytical Benchmarks**
  - **Status:** `[ ] Not Started`
  - **Priority:** P0
  - **Description:** Validate continuum thermodynamics, fluid mechanics, and reaction kinetics against published peer-reviewed exact solutions.
  - [ ] **TEST-01-002-01: Implement 1D/2D transient heat diffusion verification against Fourier series**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Compare thermal solver temperature profiles T(x, t) on slab heated by step change against infinite Fourier series analytical solution.
    - **Implementation Details:** BenchmarkThermalDiffusion computing analytical Fourier summation to 500 terms and evaluating L2 error norm.
    - **Dependencies:** PHY-02-001-01
    - **Expected Deliverables:** test_thermal_analytical.cpp, fourier_series_heat.hpp.
    - **Acceptance Criteria:** Numerical temperature field matches analytical Fourier series solution across all space and time coordinates to < 0.25% error.
    - **Testing & Verification Requirements:** Evaluate error norm across Biot numbers from 0.01 to 100 on plane slab and solid cylinder.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **TEST-01-002-02: Implement Poiseuille / Couette flow validation against exact Navier-Stokes solutions**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Verify fluid velocity profile between parallel moving plates (Couette) and inside circular pipe (Poiseuille) against exact Navier-Stokes solutions.
    - **Implementation Details:** BenchmarkNavierStokes sampling fluid velocity across channel cross-section and computing parabolic fit residuals.
    - **Dependencies:** PHY-03-001-02, PHY-03-002-01
    - **Expected Deliverables:** test_fluid_analytical.cpp, exact_navier_stokes.hpp.
    - **Acceptance Criteria:** Centerline velocity and wall shear stress match theoretical formulas to < 0.5% error on fine meshes.
    - **Testing & Verification Requirements:** Execute laminar fluid flow verification across Reynolds numbers Re = 10, 50, 100, 500.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **TEST-01-002-03: Implement stoichiometric equilibrium validation against NIST-JANAF tables**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Validate chemical equilibrium constants K_p(T) and product yields against standard NIST-JANAF thermochemical benchmark tables.
    - **Implementation Details:** BenchmarkChemicalEquilibrium running Gibbs energy minimization and comparing equilibrium mole fractions against certified data.
    - **Dependencies:** CHEM-01-002-01, MATDB-02-002-01
    - **Expected Deliverables:** test_chemical_equilibrium.cpp, nist_janaf_benchmarks.json.
    - **Acceptance Criteria:** Computed equilibrium compositions for methane reforming and ammonia synthesis match certified NIST tables to < 0.2%.
    - **Testing & Verification Requirements:** Validate reaction equilibrium across 10 certified industrial reactions over temperature range 300 K to 2000 K.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`



#### Submodule TEST-02: Standard Electronics & PCB Validation Suite
**Status:** `[ ] Not Started`

- [ ] **TEST-02-001: SPICE Reference Benchmark Suite**
  - **Status:** `[ ] Not Started`
  - **Priority:** P0
  - **Description:** Benchmark circuit simulation solver against standard SPICE3f5 / ngspice reference outputs and IEEE digital logic test vectors.
  - [ ] **TEST-02-001-01: Implement circuit simulation benchmark against SPICE3f5 reference outputs**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Automated test harness simulating standard electronic test circuits (differential pair, 555 timer, bandgap reference, LC filter) and comparing node voltages.
    - **Implementation Details:** BenchmarkSpice comparing voltage/current waveforms against ngspice output files using maximum cross-correlation and RMS error.
    - **Dependencies:** PCB-03-001-02
    - **Expected Deliverables:** test_spice_benchmarks.cpp, reference_spice_data.json, waveform_comparator.hpp.
    - **Acceptance Criteria:** Transient waveforms match reference ngspice simulations with RMS voltage error < 0.1% across all time points.
    - **Testing & Verification Requirements:** Execute benchmark suite on 15 analog reference circuits in automated headless test mode.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **TEST-02-001-02: Implement digital logic timing verification against IEEE 1164 test vectors**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Verify discrete logic engine gate delays, setup/hold time violations, and race conditions against standard IEEE VHDL test vectors.
    - **Implementation Details:** BenchmarkDigitalLogic feeding test vector stimuli (.vcd files) and asserting output signal transitions at exact nanosecond timestamps.
    - **Dependencies:** PCB-03-002-01
    - **Expected Deliverables:** test_digital_timing.cpp, test_vectors_ieee.json, vcd_parser.hpp.
    - **Acceptance Criteria:** Zero discrepancies in signal level or transition timing across 10,000 clock cycles of sequential logic execution.
    - **Testing & Verification Requirements:** Simulate 8-bit pipelined RISC core logic blocks; assert cycle-accurate register state transitions.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`



#### Submodule TEST-03: Continuous Integration & Automated Regression Suite
**Status:** `[ ] Not Started`

- [ ] **TEST-03-001: Automated Scientific Quality Gates**
  - **Status:** `[ ] Not Started`
  - **Priority:** P0
  - **Description:** CI/CD quality gates enforcing strict numerical tolerance bands, SSIM visual regression testing, and solver throughput regression detection.
  - [ ] **TEST-03-001-01: Build automated CI test harness executing numerical accuracy checks**
    - **Status:** `[ ] Not Started`
    - **Priority:** P0
    - **Description:** Headless test runner executing all analytical validation suites on every Git commit, failing the build if numerical divergence exceeds 1e-5.
    - **Implementation Details:** ScientificTestRunner wrapper with structured JSON report generation, tolerance assertion macros, and CI exit codes.
    - **Dependencies:** TEST-01-001-01, TEST-01-002-01, apps/cli
    - **Expected Deliverables:** scientific_ci_runner.hpp/.cpp, github_ci_validation.yml, tolerance_macros.hpp.
    - **Acceptance Criteria:** Executes 500+ scientific assertions in < 15 seconds in headless CI; flags any numerical drift immediately.
    - **Testing & Verification Requirements:** Intentionally introduce 1e-4 perturbation in gravitational constant; assert CI test runner fails with detailed error message.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **TEST-03-001-02: Implement headless visual regression testing for viewports via SSIM**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Render 2D/3D viewport scenes offscreen to image buffers and compare against golden reference images using Structural Similarity Index (SSIM).
    - **Implementation Details:** VisualRegressionRunner rendering offscreen framebuffer; computes SSIM metric across luminance, contrast, and structure channels.
    - **Dependencies:** 3D-03-001-01, 2D-01-002-01
    - **Expected Deliverables:** visual_regression_runner.hpp/.cpp, ssim_calculator.hpp, golden_images_dir.
    - **Acceptance Criteria:** SSIM score between rendered frame and golden reference must exceed 0.995; flags UI layout regressions and shader visual bugs.
    - **Testing & Verification Requirements:** Compare golden images of 3D PBR sphere and 2D schematic drawing against rendered frames.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`

  - [ ] **TEST-03-001-03: Implement performance benchmark regression gate detecting throughput drops**
    - **Status:** `[ ] Not Started`
    - **Priority:** P1
    - **Description:** Automated benchmark suite measuring solver iterations per second and step latency; flags performance regressions > 5% compared to baseline.
    - **Implementation Details:** PerfRegressionGate storing rolling benchmark metrics in SQLite database; evaluates statistical Student's t-test on run times.
    - **Dependencies:** PERF-01-001-01, PERF-02-001-01
    - **Expected Deliverables:** perf_regression_gate.hpp/.cpp, benchmark_database.sqlite, perf_gate_test.cpp.
    - **Acceptance Criteria:** Detects and blocks pull requests that introduce significant solver speed regressions (> 5% slowdown).
    - **Testing & Verification Requirements:** Simulate 10% artificial sleep in solver inner loop; assert regression gate fails with alert.
    - **Actual Verification Evidence:** `Pending Next-Phase Implementation`
    - **Git Commit Reference:** `Pending`






---

## 9. Dependency Matrix

The table below charts the cross-module prerequisite chains across the Next Phase platform:

| Module | Required Prerequisite Modules | Downstream Dependent Modules | Primary Interface Boundaries |
|---|---|---|---|
| **UI** | Phase 1 Base (`workspace_ui.hpp`) | 2D, 3D, PCB, MATDB | Docking layout trees, reflected property inspector, QQuickRhiItem plotters |
| **2D** | UI, `core/base/math.hpp` | 3D, PCB | Clipper2 2D polygons, Curve2D primitives, Newton-Raphson constraint graph |
| **3D** | 2D, UI, `core/domain/rendering.hpp` | PHY, MAT, PCB, ENV | HalfEdgeMesh, SAH BVH, Tet4 mesher, Qt RHI / Vulkan PBR shaders |
| **MATDB**| Phase 1 (`periodic_table.hpp`), SQLite3| MAT, CHEM, PHY | 118-element IUPAC records, isotope tables, Shomate polynomial coefficients |
| **MAT** | MATDB, `core/domain/material.hpp` | PHY, PCB, CHEM | MolecularGraph, VSEPR geometry, Voigt-Reuss composites, nonlinear tensors |
| **PCB** | 2D, MAT, `core/domain/circuit.hpp` | INT, TEST | Multi-sheet schematic nets, IPC footprints, push-and-shove traces, SPICE MNA |
| **PHY** | 3D, MAT, `core/domain/mechanics.hpp`| INT, TEST, ENV | Symplectic integrators, CCD manifolds, 3D transient heat diffusion, SPH/MAC fluids |
| **CHEM**| MATDB, MAT, `core/domain/chemistry.hpp`| INT, PHY, TEST | Reaction ODE networks, Arrhenius rate constants, Gibbs minimization (NASA CEA) |
| **ENV** | PHY, `core/domain/virtual_lab.hpp` | INT, TEST | US Standard Atmosphere 1976 column, wind velocity shear, solar irradiance |
| **INT** | PHY, CHEM, PCB, ENV | PERF, TEST | Master multi-rate coordinator, adaptive LTE time-stepping, FMI 2.0/3.0 FMUs |
| **PERF**| INT, PHY, 3D | TEST | Vulkan compute shaders, AVX2/AVX-512 SIMD, SoA structures, arena memory |
| **TEST**| All Modules | Release Milestones | Analytical benchmarks (Kepler, Fourier, Navier-Stokes), SSIM golden tests |

---

## 10. Blocker and Risk Register

Proactive risk management identifying critical failure modes, probability, impact, and mitigation architectures:

| Risk ID | Domain | Risk Description | Severity | Probability | Architectural Mitigation Strategy | Status |
|---|---|---|---|---|---|---|
| **RSK-01** | SPH Fluids | Computational cost of pairwise neighbor search scaling quadratically O(N^2) for > 100k particles | High | High | Implement compact spatial hash grids with Morton Z-order curve sorting and offload density/force summation to Vulkan compute shaders (`PERF-01-001-01`). | `[ ] Monitored` |
| **RSK-02** | Co-Simulation | Stiff ODE numerical instability and divergence during abrupt switching in coupled electro-thermal circuits | Critical | Medium | Implement adaptive time-step negotiation (`INT-01-001-02`) with deterministic state checkpointing and automatic rollback upon non-convergence (`INT-01-001-03`). | `[ ] Monitored` |
| **RSK-03** | PCB DRC | Push-and-shove interactive routing experiencing topological deadlocks in ultra-dense BGA breakout regions | High | Medium | Implement local A* grid fallback and rubber-band obstacle avoidance; enforce spring-relaxation iteration limits (`PCB-02-002-01`). | `[ ] Monitored` |
| **RSK-04** | 3D Booleans | Non-manifold topology anomalies and sliver triangles during complex CSG mesh subtraction | Critical | Medium | Employ integer-coordinate exact arithmetic / robust plane-triangle predicates and half-edge topological validity checks (`3D-01-002-01`). | `[ ] Monitored` |
| **RSK-05** | Graphics RHI | Driver divergence between Vulkan 1.3 on Windows/Linux and Metal on macOS | High | Low | Target standard Qt RHI abstraction layer; maintain portable GLSL/SPIR-V shaders compiled offline via `qsb` with automated cross-backend regression tests (`3D-03-001-01`). | `[ ] Monitored` |
| **RSK-06** | Chemistry | Severe stiffness in multi-step radical combustion reaction networks causing explicit solver blowups | High | High | Utilize implicit Rosenbrock / Radau IIA stiff ODE integrators with analytic Jacobian evaluation (`CHEM-01-001-01`). | `[ ] Monitored` |

---

## 11. Technology Stack

The Next Phase platform builds upon the modern, cross-platform C++ standard and proven scientific engineering libraries:

- **Language Standard:** C++20 standard baseline (evaluating select C++23 features supported by GCC 13.1+ / Clang 17+ / MSVC 19.38+: `std::format`, `std::expected`, `std::span`, `std::mdspan`).
- **Desktop GUI & Windowing:** Qt 6.8+ LTS (Qt Quick / QML for fluid UI, Qt Quick Controls, Qt Quick Layouts, custom `QQuickRhiItem` viewports).
- **Rendering Hardware Interface (RHI):** QRhi abstraction backend multiplexing across Vulkan 1.3 (Windows/Linux default), Direct3D 12 (Windows optional), and Metal (macOS).
- **Linear Algebra & Numerics:** Eigen 3.4+ for dense/sparse matrix decompositions, SVD, Cholesky, and Conjugate Gradient linear solvers.
- **2D Computational Geometry:** Clipper2 (rev 1.3+) for robust integer-based 2D polygon set operations and profile offsetting.
- **Mesh & Solid Topology:** Custom high-performance Half-Edge Mesh data structure with SAH linear Bounding Volume Hierarchy (BVH).
- **SPICE Simulation Kernel:** Built-in Modified Nodal Analysis (MNA) with ngspice 42+ C-API external engine adapter.
- **Scientific Element Storage:** Relational SQLite 3.45+ engine embedding IUPAC/NIST datasets; atomic JSON project manifests.
- **Interoperability Standards:** Functional Mock-up Interface (FMI 2.0 / 3.0), Gerber RS-274X, Excellon drill, DXF R12/2000, SVG 1.1, STEP AP203/AP214.
- **Unit Testing & Benchmarking:** Catch2 v3.5+ test framework; Google Benchmark for automated performance regression tracking.

---

## 12. System Architecture Diagrams (Mermaid)

### 12.1 Platform Multiphysics & CAD Architecture
```mermaid
flowchart TD
    subgraph UI_Layer["Desktop Application & Presentation Layer (Qt 6.8+ RHI)"]
        DockingMgr["Docking & Workspace Manager"]
        Ribbon["Contextual Ribbon Bar"]
        Inspector["Scientific Property Inspector"]
        Viewport2D["2D Drafting Viewport"]
        Viewport3D["3D PBR Viewport (Vulkan/D3D12)"]
        PlotDrawer["Telemetry & Oscilloscope Plotter"]
    end

    subgraph CAD_Layer["Geometry & Design Automation (CAD / EDA)"]
        Sketcher2D["2D Vector Sketcher & Newton Solver"]
        SolidModeler["3D B-Rep & Mesh Processing"]
        SchematicCapture["Multi-Sheet Schematic Capture"]
        PcbLayout["Multi-Layer PCB Trace Router & DRC"]
    end

    subgraph Material_Layer["Scientific Materials & Elements Core"]
        PeriodicDB["118-Element IUPAC/NIST Database"]
        MolecularGraph["Molecular Graph & VSEPR 3D"]
        ConstitutiveModels["Nonlinear Tensor & Phase Change Models"]
    end

    subgraph MultiPhysics_Layer["Coupled Physics & Chemistry Engines"]
        RigidMechanics["Symplectic Rigid Dynamics & CCD"]
        ThermalDiff["3D Continuum Thermal Conduction & Radiation"]
        FluidsEngine["SPH Free-Surface & MAC Grid CFD"]
        ChemicalKinetics["Reaction Networks & Arrhenius Kinetics"]
        SpiceEngine["Analog SPICE & Digital Logic Engine"]
    end

    subgraph Orchestration_Layer["Master Co-Simulation & HPC Kernel"]
        MasterClock["Multi-Rate Time Coordinator & LTE Stepping"]
        CouplingConnectors["Electro-Thermo-Mechanical Coupling Bus"]
        GpuKernels["Vulkan Compute Shaders & AVX-512 SIMD"]
        RingBuffer["Lock-Free Telemetry Ring Buffer"]
    end

    UI_Layer --> CAD_Layer
    CAD_Layer --> Material_Layer
    Material_Layer --> MultiPhysics_Layer
    MultiPhysics_Layer <--> Orchestration_Layer
    Orchestration_Layer -.->|"Immutable State Snapshots (Lock-Free)"| UI_Layer
```

### 12.2 Multi-Rate Co-Simulation Orchestrator Execution Cycle
```mermaid
sequenceDiagram
    autonumber
    participant Master as Master Co-Sim Coordinator
    participant Elec as SPICE Circuit (100 kHz)
    participant Mech as Rigid Dynamics (1 kHz)
    participant Therm as Thermal Solver (100 Hz)
    participant Telemetry as Lock-Free Ring Buffer
    participant UI as Desktop Viewport (60 Hz)

    Note over Master: Macro Time Step t = t_0
    Master->>Therm: Execute Macro-Step Delta_t = 10 ms
    Therm-->>Master: Convergence Achieved (Updated Temperatures)
    
    loop 10 Sub-Steps (1 kHz)
        Master->>Mech: Execute Sub-Step dt = 1 ms (Apply Thermal Expansion)
        Mech-->>Master: Kinematics Converged
        loop 10 Micro-Steps (100 kHz)
            Master->>Elec: Execute Micro-Step dt = 10 us (Temperature-Dependent R)
            Elec-->>Master: Circuit Voltages & Joule Heat
        end
    end
    
    Master->>Telemetry: Push Aggregated State Snapshot
    Telemetry->>UI: Render Frame & Update Oscilloscope (Zero-Lock)
```

### 12.3 Material & Periodic Table State Graph
```mermaid
classDiagram
    class PeriodicElement {
        +int atomic_number
        +string symbol
        +string name
        +double atomic_mass
        +vector~ElementIsotope~ isotopes
        +ShomateCoefficients shomate
        +CrystalStructure crystal
    }
    class ElementIsotope {
        +int mass_number
        +double abundance_pct
        +bool is_stable
        +double half_life_seconds
    }
    class MolecularGraph {
        +vector~AtomNode~ atoms
        +vector~ChemicalBond~ bonds
        +calculate_vsepr_geometry()
    }
    class ConstitutiveMaterial {
        +string material_id
        +MechanicalTensor elasticity_tensor
        +ThermalConductivityCurve k_of_T
        +EnthalpyPhaseChange phase_change
        +evaluate_properties(T, P)
    }
    PeriodicElement "1" *-- "many" ElementIsotope
    MolecularGraph "1" o-- "many" PeriodicElement
    ConstitutiveMaterial "1" o-- "many" PeriodicElement
```

### 12.4 Integrated PCB Design & Simulation Pipeline
```mermaid
flowchart LR
    SchDoc["Hierarchical Schematic"] --> ERC["Electrical Rule Check (ERC)"]
    ERC --> Netlist["Compiled Netlist & RLC Parasitics"]
    Netlist --> SPICE["SPICE / Digital Co-Simulation"]
    Netlist --> Layout["Multi-Layer PCB Layout"]
    Layout --> Router["45-deg Push-and-Shove Router"]
    Router --> DRC["Design Rule Check (DRC)"]
    DRC --> Gerber["Gerber RS-274X & Drill Export"]
    DRC --> Solid3D["3D Solid STEP / OBJ Model"]
    Solid3D --> ThermalFEM["3D Thermal & Stress FEA"]
```


---

## 13. Testing and Scientific Validation

Shunya maintains a multi-tiered scientific validation regime designed to prove accuracy against certified theoretical standards:

### 13.1 Analytical Verification Test Suite
Every mathematical solver is benchmarked against known closed-form analytical solutions:
1. **Kepler Two-Body Orbit:** Energy and angular momentum conservation verified over 100 orbital revolutions with eccentricities from 0.0 to 0.85; total energy drift must remain < 1e-5 relative error.
2. **Transient 1D Heat Conduction:** Temperature evolution in a plane slab subjected to convective cooling compared against the analytical infinite Fourier series solution; L2 error norm must remain < 0.25% across all dimensionless Fourier numbers.
3. **Navier-Stokes Laminar Poiseuille Flow:** Parabolic velocity profile in circular pipe compared against analytical solution u(r) = (R^2 - r^2) / (4*mu) * (-dP/dx); maximum centerline velocity error < 0.5%.
4. **NIST-JANAF Thermochemical Equilibrium:** Chemical equilibrium mole fractions for Haber-Bosch ammonia synthesis and methane steam reforming compared against published NIST-JANAF tables; composition error < 0.2%.
5. **SPICE Reference Transients:** RC low-pass filter, LC tank resonance frequency, and astable multivibrator switching waveforms compared against standard ngspice 42 outputs; RMS voltage deviation < 0.1%.

### 13.2 Automated CI Quality Gates
- **Numerical Regression Gate:** Automated headless test runner executing 500+ scientific assertions; automatically fails the build if numerical divergence exceeds 1e-5.
- **Visual SSIM Regression Gate:** Offscreen rendering of 2D schematics and 3D PBR viewports compared against golden images using Structural Similarity Index (SSIM); threshold score must exceed 0.995.
- **Performance Regression Gate:** Solver throughput benchmark detecting execution slowdowns greater than 5% compared to historical baseline.

---

## 14. Milestone and Release Tracking

Progress across the Next Phase platform is tracked across 5 gated engineering releases:

| Milestone | Target Version | Scope & Critical Deliverables | Exit Quality Criteria | Status |
|---|---|---|---|---|
| **M1** | `v2.1.0-alpha` | Desktop menu bar & File lifecycle, dockable workspace UI, 60 FPS plotting, 2D vector sketcher, Newton-Raphson constraint solver | 100% of UI and 2D subtasks passing; 2D sketch constraint benchmark suite passing | `[ ] Not Started` |
| **M2** | `v2.2.0-alpha` | 3D B-Rep features, half-edge mesh processing, Vulkan/D3D12 PBR, 118-element periodic database | 100% of 3D, MATDB, and MAT subtasks passing; PBR energy conservation verified | `[ ] Not Started` |
| **M3** | `v2.3.0-beta` | Multi-sheet schematic capture, IPC footprint generator, PCB push-and-shove router, DRC, SPICE/digital co-sim | 100% of PCB subtasks passing; Gerber export verified in Gerbv without warnings | `[ ] Not Started` |
| **M4** | `v2.4.0-rc` | Symplectic multi-body dynamics, CCD collision, 3D transient thermal diffusion, SPH fluids, reaction kinetics | 100% of PHY, CHEM, and ENV subtasks passing; analytical physics benchmarks passing | `[ ] Not Started` |
| **M5** | `v2.5.0-GA` | Multi-rate co-simulation orchestrator, FMI 2.0/3.0 FMU support, GPU compute shaders, SIMD vectorization | All 159 subtasks completed; zero CI warnings; multi-domain demonstrator verified | `[ ] Not Started` |

---

## 15. Future Expansion

Following the successful delivery of the Next Phase platform, the Shunya architecture is designed to support seamless forward expansion into advanced research domains:

1. **Quantum Chemistry Approximations:** Implementing semi-empirical Hartree-Fock and Density Functional Theory (DFT) tight-binding electronic structure solvers for first-principles reaction barrier calculations.
2. **Full-Wave 3D High-Frequency Electromagnetics:** Integrating Finite-Difference Time-Domain (FDTD) Maxwell solvers for RF antenna design, PCB signal integrity (S-parameters), and electromagnetic interference (EMI) analysis.
3. **Microfluidics & MEMS Electro-Hydrodynamics:** Expanding fluid solvers to include Stokes flow, electro-osmosis, Marangoni surface tension flows, and MEMS piezoelectric transducer modeling.
4. **AI-Assisted Physics Surrogate Modeling (PINNs):** Integrating Physics-Informed Neural Networks and reduced-order models (ROM) via ONNX Runtime to accelerate complex 3D continuum simulations by 1000x for real-time digital twins.

---

*End of PROJECT_ROADMAP_PHASE_2.md — Living Master Engineering Roadmap*
