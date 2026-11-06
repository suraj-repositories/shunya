# Shunya Virtual Engineering & Multiphysics Platform - Master Development Roadmap
**Version:** 2.0.0-dev  
**Status:** Active Development  
**Last Updated:** 2026-11-06  
**Central Source of Truth:** PROJECT_ROADMAP.md  
---
## 1. Executive Summary & Vision
Shunya is a desktop-first, extensible virtual engineering laboratory and multiphysics platform designed to provide rigorous scientific simulation, composable component models, and deterministic multi-domain engineering. Rather than serving as an approximate 3D animation tool or video game engine, Shunya enforces scientific honesty across five explicit fidelity tiers (L0 to L4), typed physical connectivity, observable simulation states, and sandboxed programmability.
### Core Architectural Principles
| Principle | Implementation Rule |
|---|---|
| **Scientific Honesty** | Every result identifies model assumptions, SI units, fidelity tier, and validation benchmarks. |
| **Domain Modularity** | Each physics domain owns its equations, state variables, solvers, and validation suite. |
| **Typed Connectivity** | Ports declare domain, direction, units, constraints, and physical compatibility rules. |
| **Deterministic Core** | Identical project manifest, solver settings, inputs, and seeds reproduce exact numerical results. |
| **Realtime vs Physics** | Interactive previews are approximate; authoritative scientific runs enforce explicit time-step settings. |
| **Composable Assets** | Reusable components encapsulate interfaces, parameters, implementations, tests, and documentation. |
| **Safe Programmability** | User scripts run inside a capability-restricted sandbox; native extensions require explicit signing. |
| **Observable Execution** | Every run exposes telemetry, solver convergence diagnostics, residuals, and energy/mass balances. |

### Fidelity Levels (L0 - L4)
| Level | Meaning | Examples | Expected Use |
|---|---|---|---|
| **L0 - Visual** | Appearance & layout only; no physical claim | 3D component model placed visually | Concept layout, education |
| **L1 - Functional** | Idealized / simplified behavioral equations | Ideal resistor, Boolean gate, rigid body contact | Early prototyping, logic design |
| **L2 - Eng. Approximation** | Validated numerical model in operating range | RC transient, diode exponential, heat conduction | Design exploration, comparative tests |
| **L3 - High Fidelity** | Calibrated non-linear models & fine mesh | Non-linear transistor, coupled electro-thermal, CFD | Specialist engineering workflows |
| **L4 - Research/Certified** | Traceable validation against reference standards | Calibrated material state, certified multiphysics | Research and regulatory certification |

## 2. Technology Stack & Architectural Boundaries
- **Language Baseline:** C++20 standard baseline (evaluating C++23 features supported by GCC 13.1+ / MinGW-w64: std::format, std::expected, std::span).
- **Desktop Application Shell:** Qt 6 (Qt Quick/QML + select QtWidgets for docking, command palette, and property inspection).
- **Build System:** CMake (>= 3.20) + CMake Presets + Ninja + MSYS2 MinGW-w64.
- **Math & Linear Algebra:** Eigen 3.4+ for vector/matrix mathematics and sparse linear systems.
- **Circuit Simulation:** Native Modified Nodal Analysis (MNA) baseline solver + ngspice external engine adapter.
- **Persistence:** Atomic JSON manifests (project metadata), SQLite relational storage (material database), HDF5 (numerical time-series).
- **Scripting & Rules:** Lua 5.4 with restricted capability API sandbox.
- **Visualization:** VTK (scientific fields and slices) + Vulkan/OpenGL viewport abstractions.
- **Testing:** Catch2 / GoogleTest for unit and integration testing + Google Benchmark for performance regression.

### Strict Dependency Layering Rules (Non-Negotiable)
1. core/base and core/domain must NEVER depend on Qt, VTK, OCCT, ngspice, or any UI framework.
2. Material models depend only on core math and unit abstractions; zero UI or renderer coupling.
3. Physics engines depend solely on domain models, units, material properties, and simulation scheduling.
4. Third-party engine types (e.g. ngspice internal structs) must never leak across public platform APIs.
5. Rendering consumes read-only immutable simulation state snapshots; rendering never mutates solver state.
6. Scripting runs inside a capability-isolated sandbox; scripts receive handle tokens, never raw C++ pointers.

## 3. Overall Progress & Task Metrics

| Status Indicator | Meaning | Task Count |
|---|---|---|
| [ ] | Not started | 285 |
| [~] | In progress | 0 |
| [x] | Completed | 24 |
| [!] | Blocked | 0 |
| **Total** | **All catalogued tasks** | **309** |

### Module Breakdown Matrix
| Module ID | Module Name | Phase / Stage | Priority | Total Tasks | Completed | Progress |
|---|---|---|---|---|---|---|
| **ADR** | Architecture Decision Records | Phase 0 / T0 | P0 | 10 | 0 | 0.0% |
| **FND** | Cross-Cutting Foundations | Phase 1 / T0 | P0 | 10 | 10 | 100.0% |
| **UNIT** | Units, Dimensions and Quantities | Phase 1 / T2 | P0 | 7 | 7 | 100.0% |
| **PRJ** | Project and Scene Management | Phase 1 / T1 | P0 | 8 | 7 | 87.5% |
| **ELM** | Elements and Object Model | Phase 1 / T2 | P0 | 8 | 0 | 0.0% |
| **MAT** | Materials Foundation | Phase 1 / T2 | P0 | 8 | 0 | 0.0% |
| **MATDB** | Periodic Elements & Material Database | Phase 1 / T2 | P0 | 10 | 0 | 0.0% |
| **MATRT** | Element/Material Runtime & Properties | Phase 1 / T2 | P0 | 12 | 0 | 0.0% |
| **MATUI** | Material Editor & Periodic Table UI | Phase 1 / T2 | P1 | 10 | 0 | 0.0% |
| **CMP** | Generic Component & Device Framework | Phase 1 / T1 | P0 | 8 | 0 | 0.0% |
| **SIM** | Simulation Runtime and Scheduling | Phase 2 / T3 | P0 | 10 | 0 | 0.0% |
| **SOL** | Numerical Solver Framework | Phase 2 / T3 | P0 | 10 | 0 | 0.0% |
| **OBS** | Observability, Probes & Dashboards | Phase 2 / T4 | P0 | 10 | 0 | 0.0% |
| **RUN** | Simulation Execution & Safe Modes | Phase 2 / T3 | P0 | 10 | 0 | 0.0% |
| **IO** | Data Exchange, Formats & Persistence | Phase 1 / T2 | P0 | 10 | 0 | 0.0% |
| **UI** | User Interface & Workspaces | Phase 1 / T1 | P0 | 12 | 0 | 0.0% |
| **REN** | Rendering & Graphics Engine | Phase 1 / T4 | P1 | 10 | 0 | 0.0% |
| **ELEC** | Electronics & Circuit Simulation | Phase 5 / T3 | P0 | 14 | 0 | 0.0% |
| **LOGIC** | Digital Logic & State Machines | Phase 6 / T3 | P1 | 9 | 0 | 0.0% |
| **MEC** | Classical Mechanics & Rigid Bodies | Phase 3 / T5 | P1 | 10 | 0 | 0.0% |
| **THM** | Thermal & Heat Transfer | Phase 3 / T5 | P1 | 9 | 0 | 0.0% |
| **DEV** | Physical Device Construction | Phase 7 / T5 | P1 | 12 | 0 | 0.0% |
| **ENV** | Real-time Virtual Lab Environment | Phase 1 / T1 | P1 | 8 | 0 | 0.0% |
| **CODE** | Programmability, Formulas & Scripting | Phase 4 / T6 | P1 | 12 | 0 | 0.0% |
| **SEC** | Security, Sandboxing & Reliability | Phase 4 / T6 | P1 | 10 | 0 | 0.0% |
| **LIB** | Component Library, Templates & Sharing | Phase 7 / T6 | P1 | 14 | 0 | 0.0% |
| **EVT** | Event & Co-Simulation Orchestration | Phase 9 / T6 | P1 | 6 | 0 | 0.0% |
| **FLD** | Fluid Dynamics & Hydraulics | Phase 8 / T7 | P2 | 10 | 0 | 0.0% |
| **CHEM** | Chemical Reactions & Stoichiometry | Phase 8 / T7 | P2 | 9 | 0 | 0.0% |
| **OPT** | Optics & Wave Phenomena | Phase 8 / T7 | P2 | 6 | 0 | 0.0% |
| **BIO** | Biological & Cellular Models | Phase 8 / T7 | P2 | 7 | 0 | 0.0% |
| **PERF** | Performance, Benchmarks & Profiling | Phase 10 / T8 | P1 | 10 | 0 | 0.0% |

---

## 4. Comprehensive Work Breakdown Structure (WBS)

### Module ADR: Architecture Decision Records
**Scope & Objective:** Governing architectural decisions and decision records across the system lifecycle.  
**Delivery Stage:** Phase 0 / T0 | **Default Priority:** P0

#### [ADR-001] 001
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 0 / T0
- **Dependencies:** Module Baseline
- **Description:** Desktop UI and application framework? Select a mature cross-platform desktop framework after a prototype; keep UI independent of simulation core.
- **Implementation Details:** Implement ADR-001 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ADR-002] 002
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 0 / T0
- **Dependencies:** ADR-001
- **Description:** Graphics backend? Use a renderer abstraction; choose backend based on platform and feature requirements.
- **Implementation Details:** Implement ADR-002 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ADR-003] 003
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 0 / T0
- **Dependencies:** ADR-002
- **Description:** Numerical libraries? Evaluate maintained linear algebra/sparse solver libraries against licensing, performance and portability.
- **Implementation Details:** Implement ADR-003 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ADR-004] 004
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 0 / T0
- **Dependencies:** ADR-003
- **Description:** Scripting runtime? Choose based on sandboxing, embedding, debugging, ecosystem and deterministic requirements.
- **Implementation Details:** Implement ADR-004 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ADR-005] 005
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 0 / T0
- **Dependencies:** ADR-004
- **Description:** Project serialization? Versioned manifest plus binary payloads for large data; guarantee atomic save and migration.
- **Implementation Details:** Implement ADR-005 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ADR-006] 006
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 0 / T0
- **Dependencies:** ADR-005
- **Description:** Simulation threading? Define ownership and thread-safety before parallel domain solvers.
- **Implementation Details:** Implement ADR-006 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ADR-007] 007
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 0 / T0
- **Dependencies:** ADR-006
- **Description:** Electronics model compatibility? Decide whether to implement native models, support a SPICE-compatible subset, or integrate a proven solver.
- **Implementation Details:** Implement ADR-007 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ADR-008] 008
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 0 / T0
- **Dependencies:** ADR-007
- **Description:** CAD interoperability? Select formats based on target workflows and licensing; do not promise full CAD feature parity.
- **Implementation Details:** Implement ADR-008 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ADR-009] 009
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 0 / T0
- **Dependencies:** ADR-008
- **Description:** Public package trust? No automatic execution of untrusted code; signed releases and validation required.
- **Implementation Details:** Implement ADR-009 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ADR-010] 010
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 0 / T0
- **Dependencies:** ADR-009
- **Description:** Scientific validation governance? Assign domain owners/reviewers and require benchmark evidence before raising fidelity claims. 20. Project management and ticket decomposition
- **Implementation Details:** Implement ADR-010 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module FND: Cross-Cutting Foundations
**Scope & Objective:** Core platform primitives, build system, error model, UUID, logging, threading, configuration.  
**Delivery Stage:** Phase 1 / T0 | **Default Priority:** P0

#### [FND-01] Build and Dependency Management
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T0
- **Dependencies:** None (Platform Root Foundation)
- **Description:** Choose C++ standard (C++20 baseline; evaluate C++23 features against compiler support), CMake presets, package manager, compiler matrix, reproducible build flags, and modular CMake target hierarchy.
- **Implementation Details:** Created root CMakeLists.txt establishing C++20 baseline, strict compiler warnings (-Wall -Wextra -Wpedantic / W4), modular subdirectories (core/base, tests, shunya), CMakePresets.json, ADR-001 (UI/Core separation), and ADR-002 (C++ Toolchain and Standard).
- **Acceptance Criteria:** Full CMake configuration and Ninja build of core static library (shunya_core_base), desktop shell (appshunya), and automated test runner (shunya_unit_tests); CTest test execution passing with zero errors.
- **Verification & Git Commit:** Verified via CTest and shunya_unit_tests.exe (2/2 test cases, 6/6 assertions passed, C++20 concepts and version validation); Git Commit: c8fce74

#### [FND-02] Application Bootstrap
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T0
- **Dependencies:** FND-01, FND-03, FND-04, FND-05, FND-06, FND-08, FND-09
- **Description:** Create app lifecycle, configuration loading, service registration, startup/shutdown and clean failure reporting.
- **Implementation Details:** Implemented unified AppContext in core/base/include/shunya/core/base/app_context.hpp and app_context.cpp managing full platform lifecycle (Uninitialized, Initializing, Running, ShuttingDown, Stopped), structured AppInitOptions, central Configuration binding, ThreadPool initialization, ServiceRegistry for type-safe dependency injection and service locator pattern, and clean shutdown draining.
- **Acceptance Criteria:** Unit tests verifying state machine transitions, configuration text loading during bootstrap, double initialization rejection with error code 1001, service registration and type retrieval, and graceful shutdown.
- **Verification & Git Commit:** Verified via unit tests (3/3 test cases, 23/23 assertions passed in test_app_context.cpp); Git Commit: d1994bf

#### [FND-03] Identity System
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T0
- **Dependencies:** FND-01
- **Description:** Define stable UUIDs for projects, entities, ports, models, components, runs and packages; never use display names as identifiers.
- **Implementation Details:** Implemented 128-bit RFC 4122 v4 compliant Uuid class in core/base/include/shunya/core/base/uuid.hpp and core/base/src/uuid.cpp with random generation, canonical string serialization/parsing, nil checking, and std::hash specialization for hash containers.
- **Acceptance Criteria:** Comprehensive unit tests validating RFC 4122 v4 version and variant bits, nil identity, string round-trips, rejection of invalid strings, and std::unordered_set hashing.
- **Verification & Git Commit:** Verified via unit tests (4/4 test cases, 17/17 assertions passed in test_uuid.cpp); Git Commit: bf28b9

#### [FND-04] Error and Result Model
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T0
- **Dependencies:** FND-01
- **Description:** Create typed errors, severity levels, recoverable errors, validation reports and user-facing diagnostic mapping.
- **Implementation Details:** Implemented Severity (Info, Warning, Error, Fatal), ErrorDomain (System, Validation, Simulation, IO, Scripting, Materials), Error struct with diagnostic formatting, ValidationReport aggregator, and modern monadic Result<T> / Result<void> templates in core/base/include/shunya/core/base/error.hpp and result.hpp.
- **Acceptance Criteria:** Unit tests verifying value unwrapping, error propagation, fallback value_or semantics, void specialization, and multi-issue validation reporting.
- **Verification & Git Commit:** Verified via unit tests (3/3 test cases, 18/18 assertions passed in test_error_result.cpp); Git Commit: bf28b9

#### [FND-05] Logging and Diagnostics
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T0
- **Dependencies:** FND-01, FND-03
- **Description:** Structured logs, categories, run IDs, timestamps, log rotation, crash reports and exportable diagnostic bundles.
- **Implementation Details:** Implemented LogLevel (Debug, Info, Warning, Error, Critical), LogRecord with sub-millisecond timestamps, run ID tracking, file/line metadata, thread-safe MemorySink with capacity limiting, ConsoleSink with stderr routing for errors, Logger singleton, diagnostic bundle exporter, and SHUNYA_LOG_* macros in core/base/include/shunya/core/base/logging.hpp and logging.cpp.
- **Acceptance Criteria:** Unit tests verifying level filtering, memory sink buffer constraints, active simulation run ID association, log record formatting, and diagnostic bundle generation.
- **Verification & Git Commit:** Verified via unit tests (4/4 test cases, 19/19 assertions passed in test_logging.cpp); Git Commit: 8239a05

#### [FND-06] Event and Message System
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T0
- **Dependencies:** FND-01
- **Description:** Define typed events, subscriptions, ownership, thread-safety and event ordering.
- **Implementation Details:** Implemented type-safe EventBus in core/base/include/shunya/core/base/event_bus.hpp supporting synchronous immediate dispatch, FIFO queued dispatch with batch flushing, RAII subscription tokens, unsubscription by ID, re-entrant event publishing, and mutex-protected concurrency safety.
- **Acceptance Criteria:** Comprehensive unit tests verifying type isolation across distinct event structs, unsubscription semantics, FIFO delivery ordering of queued events, and re-entrant handler execution.
- **Verification & Git Commit:** Verified via unit tests (4/4 test cases, 15/15 assertions passed in test_event_bus.cpp); Git Commit: 9bfbf72

#### [FND-07] Math Foundation
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T0
- **Dependencies:** FND-01
- **Description:** Vectors, matrices, quaternions, transforms, interpolation, tolerances and numerical-safe operations.
- **Implementation Details:** Implemented C++20 standard math primitives in core/base/include/shunya/core/base/math.hpp including Vector2, Vector3, Quaternion, Matrix4x4, and Transform with support for vector cross/dot products, normalization with zero-guard, quaternion axis-angle construction, vector rotation, Slerp spherical linear interpolation, matrix-vector transformation, lerp, clamp, and approx_equal relative/absolute tolerance utilities.
- **Acceptance Criteria:** Unit tests verifying vector arithmetic, cross product orthogonality, quaternion 3D rotation, Slerp interpolation accuracy, and composite spatial transformation equivalence with 4x4 matrix multiplication.
- **Verification & Git Commit:** Verified via unit tests (4/4 test cases, 24/24 assertions passed in test_math.cpp); Git Commit: 5b0aa5a

#### [FND-08] Threading and Jobs
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T0
- **Dependencies:** FND-01
- **Description:** Worker pool, task graph, cancellation, priorities, progress reporting and main-thread dispatch.
- **Implementation Details:** Implemented ThreadPool in core/base/include/shunya/core/base/thread_pool.hpp and thread_pool.cpp featuring configurable worker thread count (defaults to hardware concurrency), template submit returning std::future<T>, priority-based task queue (Low, Normal, High, Critical) with FIFO sequence tie-breaking, CancellationToken with thread-safe atomic status, wait_idle synchronization, and graceful worker thread shutdown.
- **Acceptance Criteria:** Unit tests verifying parallel batch job execution, future return value resolution, priority queue ordering execution, cancellation token flag propagation, and worker pool idle synchronization.
- **Verification & Git Commit:** Verified via unit tests (4/4 test cases, 13/13 assertions passed in test_thread_pool.cpp); Git Commit: 3b2c6a5

#### [FND-09] Configuration
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T0
- **Dependencies:** FND-01, FND-04
- **Description:** User/project/application settings, schema validation, defaults and migration.
- **Implementation Details:** Implemented dynamic typed Configuration store in core/base/include/shunya/core/base/config.hpp and config.cpp supporting bool, int64_t, double, and string types, safe type conversion with Result<T>, default fallback getters, key-value line-oriented text parser with comment and whitespace stripping, serialization to text format, and Error/ValidationReport diagnostic integration.
- **Acceptance Criteria:** Unit tests verifying in-memory typed lookups, default fallbacks, comment-preserving text parsing, round-trip serialization, and syntax error diagnostics.
- **Verification & Git Commit:** Verified via unit tests (3/3 test cases, 19/19 assertions passed in test_config.cpp); Git Commit: 039f43

#### [FND-10] CI Build Pipeline
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T0
- **Dependencies:** FND-01
- **Description:** Automated formatting, static analysis, build, unit tests, packaging and artifact retention.
- **Implementation Details:** Created multi-platform GitHub Actions CI workflow in .github/workflows/ci.yml targeting Windows and Linux matrices with automated GCC/Clang compilers, Ninja generator, CMake build, and CTest automated verification. Added standardized code formatting specifications in .clang-format.
- **Acceptance Criteria:** Validated GitHub Actions workflow schema, Ninja/CMake multi-platform build steps, CTest automated test execution, and .clang-format style configuration.
- **Verification & Git Commit:** Verified via local CTest run (100% passed, 31 test cases); Git Commit: 6f02d79

---

### Module UNIT: Units, Dimensions and Quantities
**Scope & Objective:** Rigorous dimensional analysis, SI base units, derived units, conversion, and validation.  
**Delivery Stage:** Phase 1 / T2 | **Default Priority:** P0

#### [UNIT-01] SI Base Units
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** FND-01, FND-04
- **Description:** Represent metre, kilogram, second, ampere, kelvin, mole and candela.
- **Implementation Details:** Implemented 7-dimensional Dimension vector in core/base/include/shunya/core/base/units.hpp representing integer/rational exponents for metre (length), kilogram (mass), second (time), ampere (current), kelvin (temperature), mole (amount of substance), and candela (luminous intensity), with support for dimension multiplication (exponent addition), division (exponent subtraction), power scaling, and standard symbol formatting.
- **Acceptance Criteria:** Unit tests verifying individual base unit exponents, standard SI symbols (m, kg, s, A, K, mol, cd), and dimensionless verification.
- **Verification & Git Commit:** Verified via unit tests (1/1 test case, 15/15 assertions passed in test_units.cpp); Git Commit: fd7cfe

#### [UNIT-02] Derived Units
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** UNIT-01
- **Description:** Support newton, joule, watt, pascal, volt, ohm, farad, tesla, coulomb and derived dimensions.
- **Implementation Details:** Defined standard derived SI dimensions in core/base/include/shunya/core/base/units.hpp including Force (N), Energy (J), Power (W), Pressure (Pa), ElectricCharge (C), Voltage (V), ElectricalResistance (Ohm), ElectricalCapacitance (F), ElectricalConductance (S), MagneticFlux (Wb), MagneticField (T), Inductance (H), Frequency (Hz), Area, Volume, Velocity, and Acceleration with standard dimensional algebra.
- **Acceptance Criteria:** Unit tests validating dimensional algebraic derivation (e.g. Length/Time -> Velocity, Mass*Acceleration -> Force, Force*Length -> Energy, Power/Current -> Voltage, Voltage/Current -> Resistance, Charge/Voltage -> Capacitance).
- **Verification & Git Commit:** Verified via unit tests (1/1 test case, 16/16 assertions passed in test_units.cpp); Git Commit: fd7cfe

#### [UNIT-03] Dimensional Analysis
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** UNIT-01, UNIT-02
- **Description:** Reject invalid equation combinations and incompatible port connections.
- **Implementation Details:** Implemented Quantity class and check_compatible functions in core/base/include/shunya/core/base/units.hpp and units.cpp enforcing strict dimensional matching on additions and subtractions via Result<Quantity>, automatic dimensional multiplication/division, and typed error diagnostic reporting (error codes 3001, 3002, 3003) on incompatible dimensions.
- **Acceptance Criteria:** Unit tests verifying valid arithmetic on matching dimensions, automatic derived quantity creation, rejection of incompatible additions/subtractions, and port compatibility checking.
- **Verification & Git Commit:** Verified via unit tests (1/1 test case, 15/15 assertions passed in test_units.cpp); Git Commit: fd7cfe

#### [UNIT-04] Unit Conversion
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** UNIT-01, UNIT-02
- **Description:** Convert display units without changing canonical stored values.
- **Implementation Details:** Implemented UnitRegistry in core/base/include/shunya/core/base/units.hpp and units.cpp with pre-registered standard units across metric prefixes (nm, um, mm, cm, m, km, ms, us, ns, g, mg, tonne, mA, uA, mV, kV, kOhm, MOhm, uF, nF, pF, mH, uH, kHz, MHz, GHz), imperial conversions (in, ft, yd, mi, lb, oz, psi, lbf, hp), and affine temperature conversions (degC, degF with scale and absolute offset).
- **Acceptance Criteria:** Unit tests verifying conversion from display to canonical storage, canonical storage to display, and exact offset conversions for Celsius and Fahrenheit.
- **Verification & Git Commit:** Verified via unit tests (1/1 test case, 12/12 assertions passed in test_units.cpp); Git Commit: 992c9b2

#### [UNIT-05] Quantity Types
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** UNIT-01, UNIT-02, FND-07
- **Description:** Store value plus dimension; define scalar/vector/tensor quantity support.
- **Implementation Details:** Implemented Scalar Quantity, VectorQuantity (3D vector with physical dimension, dot product, cross product, magnitude, and addition/subtraction), and TensorQuantity (3x3 second-order tensor with physical dimension, trace calculation, tensor-vector transformation, and tensor addition) in core/base/include/shunya/core/base/units.hpp.
- **Acceptance Criteria:** Unit tests verifying vector quantity addition, vector dot product yielding energy from force and displacement, tensor trace calculation, and spatial vector transformation.
- **Verification & Git Commit:** Verified via unit tests (1/1 test case, 11/11 assertions passed in test_units.cpp); Git Commit: 992c9b2

#### [UNIT-06] Unit-Aware UI
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** UNIT-04
- **Description:** Show unit labels, precision, significant figures and conversion controls.
- **Implementation Details:** Implemented suggest_best_unit and format utilities in UnitRegistry automatically choosing optimal engineering prefix scale (e.g. converting 0.005 V to 5.0 mV, 10.5e6 Hz to 10.50 MHz) and formatting numbers with configurable decimal precision.
- **Acceptance Criteria:** Unit tests validating automatic engineering unit suggestions across voltage and frequency ranges with custom decimal precision.
- **Verification & Git Commit:** Verified via unit tests (1/1 test case, 5/5 assertions passed in test_units.cpp); Git Commit: 992c9b2

#### [UNIT-07] Validation Suite
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** UNIT-01, UNIT-02, UNIT-03, UNIT-04, UNIT-05, UNIT-06
- **Description:** Test conversion round trips, dimension algebra and invalid expression rejection.
- **Implementation Details:** Comprehensive validation test suite testing round-trip conversions (display -> canonical -> display) within 1e-9 tolerance, unknown unit symbol rejection (error code 3010), and incompatible unit conversion rejection (error code 3012).
- **Acceptance Criteria:** Unit test suite passing 100% of round-trip precision checks and negative error handling tests.
- **Verification & Git Commit:** Verified via unit tests (1/1 test case, 5/5 assertions passed in test_units.cpp); Git Commit: 992c9b2

---

### Module PRJ: Project and Scene Management
**Scope & Objective:** Project file lifecycle, scene graph, undo/redo command stack, and project package serialization.  
**Delivery Stage:** Phase 1 / T1 | **Default Priority:** P0

#### [PRJ-01] Project Format
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** Module Baseline
- **Description:** Define project manifest, schema version, project identity, authoring metadata and dependency list.
- **Implementation Details:** Implemented ProjectManifest struct with RFC 4122 v4 UUID, semantic Version, FidelityTier (L0 to L4), dependencies array, and flexible metadata map. Provided robust JSON serialization and deserialization in core/domain/src/project.cpp.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** Verified via unit tests (test_project.cpp: serialization roundtrip, invalid JSON error handling); Git Commit: 0966a6c
#### [PRJ-02] Create/Open/Save
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** PRJ-01
- **Description:** Implement project creation, save-as, autosave, recovery and read-only mode.
- **Implementation Details:** Implemented Project class with atomic saving (.tmp file + rename/replace), dirty state tracking (mark_dirty, clear_dirty), read-only guard enforcement (error code 2013), and file open with validation.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** Verified via unit tests (test_project.cpp: create_new, save and reopen lifecycle, read-only protection, open nonexistent file); Git Commit: 0966a6c
#### [PRJ-03] 03 – Scene graph
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** PRJ-02
- **Description:** Entities, parent-child hierarchy, transforms, visibility, tags and domain attachments.
- **Implementation Details:** Implemented Entity and SceneGraph in core/domain/include/shunya/core/domain/scene_graph.hpp and src/scene_graph.cpp with cycle-detected reparenting (prevent self & descendant loops), hierarchical world matrix computation, global visibility/active evaluation, multi-tag filtering, domain attachments, and depth-first traversal.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** Verified via unit tests (test_scene_graph.cpp: 8 test cases, 65 assertions passed); Git Commit: 980e680
#### [PRJ-04] 04 – Undo/redo commands
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** PRJ-03
- **Description:** Use command pattern or transaction log; support grouped operations and undo-safe editing.
- **Implementation Details:** Implemented ICommand interface, CompositeCommand for atomic macro actions with forward rollback on failure, and CommandStack with undo/redo history, branching history management, bounded history limits, and atomic transaction grouping (begin, commit, rollback) in core/domain/include/shunya/core/domain/command_stack.hpp and src/command_stack.cpp.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** Verified via unit tests (test_command_stack.cpp: 6 test cases, 75 assertions passed); Git Commit: a83cf75
#### [PRJ-05] 05 – Selection and editing
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** PRJ-04
- **Description:** Selection sets, multi-select, gizmos, snapping, duplication and deletion.
- **Implementation Details:** Implemented SelectionSet and SelectionManager in core/domain/include/shunya/core/domain/selection.hpp and src/selection.cpp with multi-selection modes (Replace, Add, Remove, Toggle), primary vs secondary tracking, predicate filtering, change notifications, named selection sets, grid/rotation snapping, and atomic duplication/deletion.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** Verified via unit tests (test_selection.cpp: 5 test cases, 50 assertions passed); Git Commit: 210537a
#### [PRJ-06] 06 – Version migration
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** PRJ-05
- **Description:** Migrate old project schemas with preview, backup and rollback.
- **Implementation Details:** Implemented IMigrationStep interface, ProjectMigrator with chained migration planning, in-memory preview (preview_migration), automated backup creation (.bak), atomic file migration with replacement, rollback_from_backup restoration, and standard legacy v0.0.1 to v0.1.0-dev upgrade step in core/domain/include/shunya/core/domain/migration.hpp and src/migration.cpp.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** Verified via unit tests (test_migration.cpp: 5 test cases, 33 assertions passed); Git Commit: 0d5e0c6
#### [PRJ-07] 07 – Project validation
- **Status:** [x] Completed
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** PRJ-06
- **Description:** Detect missing assets, broken references, invalid IDs and unsupported versions.
- **Implementation Details:** Implemented ProjectValidator in core/domain/include/shunya/core/domain/validator.hpp and src/validator.cpp to inspect ProjectManifest and SceneGraph for schema compatibility, nil UUIDs, empty names, broken parent-child relations, asymmetric links, duplicate names, and missing on-disk asset files (CAD meshes, SPICE subcircuits, scripts, textures) with structured ValidationReport aggregation.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** Verified via unit tests (test_validator.cpp: 3 test cases, 14 assertions passed); Git Commit: cd687bd
#### [PRJ-08] 08 – Project packaging
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** PRJ-07
- **Description:** Collect assets, scripts, model dependencies and result references into a portable package. ELM – Elements and object model
- **Implementation Details:** Implement PRJ-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module ELM: Elements and Object Model
**Scope & Objective:** Element definitions, scene instances, typed parameter schemas, and multi-domain ports.  
**Delivery Stage:** Phase 1 / T2 | **Default Priority:** P0

#### [ELM-01] 01 – Element definition
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** Module Baseline
- **Description:** Separate reusable definition from scene instance; include type, metadata, schema and version.
- **Implementation Details:** Implement ELM-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ELM-02] 02 – Element instance
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** ELM-01
- **Description:** Instance ID, transform, parameter values, state binding and component reference.
- **Implementation Details:** Implement ELM-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ELM-03] 03 – Parameter schema
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** ELM-02
- **Description:** Typed parameters, defaults, min/max, units, enums, visibility and validation constraints.
- **Implementation Details:** Implement ELM-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ELM-04] 04 – Port schema
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** ELM-03
- **Description:** Typed ports with domain, direction, units, cardinality and connection constraints.
- **Implementation Details:** Implement ELM-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ELM-05] 05 – Element lifecycle
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** ELM-04
- **Description:** Create, initialise, update, reset, duplicate, serialize and destroy.
- **Implementation Details:** Implement ELM-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ELM-06] 06 – Element categories
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** ELM-05
- **Description:** Physical object, material, field, source, sensor, actuator, device, logic block and abstract node.
- **Implementation Details:** Implement ELM-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ELM-07] 07 – Element inspector
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** ELM-06
- **Description:** Property panels generated from schemas, validation and contextual help.
- **Implementation Details:** Implement ELM-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ELM-08] 08 – Element test harness
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** ELM-07
- **Description:** Test default values, lifecycle, serialization and port contracts. MAT – Materials and substances
- **Implementation Details:** Implement ELM-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module MAT: Materials Foundation
**Scope & Objective:** Material database schemas, physical property groups, temperature/pressure curves, and assignments.  
**Delivery Stage:** Phase 1 / T2 | **Default Priority:** P0

#### [MAT-01] 01 – Material database schema
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** Module Baseline
- **Description:** Material ID, name, composition, density, state, provenance, source and version.
- **Implementation Details:** Implement MAT-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MAT-02] 02 – Mechanical properties
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MAT-01
- **Description:** Elastic modulus, Poisson ratio, yield strength, plasticity and fracture parameters where supported.
- **Implementation Details:** Implement MAT-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MAT-03] 03 – Thermal properties
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MAT-02
- **Description:** Conductivity, heat capacity, emissivity, expansion and phase-change parameters.
- **Implementation Details:** Implement MAT-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MAT-04] 04 – Electrical properties
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MAT-03
- **Description:** Conductivity, resistivity, permittivity, permeability and temperature dependence.
- **Implementation Details:** Implement MAT-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MAT-05] 05 – Fluid/chemical properties
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MAT-04
- **Description:** Viscosity, density, diffusivity, heat capacity and supported constitutive data.
- **Implementation Details:** Implement MAT-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MAT-06] 06 – Material assignment
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MAT-05
- **Description:** Assign material to bodies, regions, layers or component subdomains.
- **Implementation Details:** Implement MAT-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MAT-07] 07 – Material curves
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MAT-06
- **Description:** Temperature/frequency/strain-dependent tables with interpolation and extrapolation policy.
- **Implementation Details:** Implement MAT-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MAT-08] 08 – Material validation
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MAT-07
- **Description:** Units, physically plausible ranges, source metadata and regression fixtures. CMP – Generic component and device framework
- **Implementation Details:** Implement MAT-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module MATDB: Periodic Elements & Material Database
**Scope & Objective:** Periodic table element catalog, data dictionary, dataset ingestion, provenance, and validation.  
**Delivery Stage:** Phase 1 / T2 | **Default Priority:** P0

#### [MATDB-01] 01
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** Module Baseline
- **Description:** Create a versioned schema and data dictionary for every supplied field, including canonical name, aliases, type, unit, dimensions, applicability and definition.
- **Implementation Details:** Implement MATDB-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATDB-02] 02
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATDB-01
- **Description:** Build import adapters for trusted scientific datasets; preserve provider identifiers and source metadata.
- **Implementation Details:** Implement MATDB-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATDB-03] 03
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATDB-02
- **Description:** Add field-level provenance and licence tracking; verify that dataset redistribution is permitted.
- **Implementation Details:** Implement MATDB-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATDB-04] 04
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATDB-03
- **Description:** Normalise units and identifiers; preserve original raw value/unit for audit.
- **Implementation Details:** Implement MATDB-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATDB-05] 05
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATDB-04
- **Description:** Validate ranges, duplicate element identities, periodic-table consistency and isotope relationships.
- **Implementation Details:** Implement MATDB-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATDB-06] 06
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATDB-05
- **Description:** Create a review queue for conflicting sources and anomalous values.
- **Implementation Details:** Implement MATDB-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATDB-07] 07
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATDB-06
- **Description:** Support dataset releases and immutable version snapshots so old projects reproduce old results.
- **Implementation Details:** Implement MATDB-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATDB-08] 08
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATDB-07
- **Description:** Add update diff reports showing changed values, sources and downstream components affected.
- **Implementation Details:** Implement MATDB-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATDB-09] 09
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATDB-08
- **Description:** Create missing-data reports by property and domain model requirement.
- **Implementation Details:** Implement MATDB-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATDB-10] 10
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATDB-09
- **Description:** Add regression fixtures for representative elements and extreme-condition curves.
- **Implementation Details:** Implement MATDB-10 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module MATRT: Element/Material Runtime & Properties
**Scope & Objective:** Immutable element definitions, isotope definitions, material states, phase transitions, and domain adapters.  
**Delivery Stage:** Phase 1 / T2 | **Default Priority:** P0

#### [MATRT-01] 01
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** Module Baseline
- **Description:** Implement ElementDefinition as immutable periodic-table identity and reference metadata.
- **Implementation Details:** Implement MATRT-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATRT-02] 02
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATRT-01
- **Description:** Implement IsotopeDefinition and isotope-aware atomic/nuclear data.
- **Implementation Details:** Implement MATRT-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATRT-03] 03
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATRT-02
- **Description:** Implement MaterialDefinition for pure elements, compounds, alloys and mixtures.
- **Implementation Details:** Implement MATRT-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATRT-04] 04
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATRT-03
- **Description:** Implement MaterialRegion attached to geometry with material ID and local state.
- **Implementation Details:** Implement MATRT-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATRT-05] 05
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATRT-04
- **Description:** Implement MaterialState containing temperature, pressure, phase fraction, composition, strain and other domain state.
- **Implementation Details:** Implement MATRT-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATRT-06] 06
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATRT-05
- **Description:** Implement PropertyProvider interface with validity ranges and diagnostic output.
- **Implementation Details:** Implement MATRT-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATRT-07] 07
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATRT-06
- **Description:** Implement property tables, interpolation, derivatives and controlled extrapolation.
- **Implementation Details:** Implement MATRT-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATRT-08] 08
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATRT-07
- **Description:** Implement phase transition and latent-heat interfaces.
- **Implementation Details:** Implement MATRT-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATRT-09] 09
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATRT-08
- **Description:** Implement material-to-domain adapters for thermal, mechanical, electrical, fluid and optical solvers.
- **Implementation Details:** Implement MATRT-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATRT-10] 10
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATRT-09
- **Description:** Implement cache invalidation when state, composition, temperature or model version changes.
- **Implementation Details:** Implement MATRT-10 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATRT-11] 11
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATRT-10
- **Description:** Implement state serialisation and restart without losing phase or history variables.
- **Implementation Details:** Implement MATRT-11 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATRT-12] 12
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATRT-11
- **Description:** Implement per-region diagnostics for unsupported conditions and missing properties.
- **Implementation Details:** Implement MATRT-12 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module MATUI: Material Editor & Periodic Table UI
**Scope & Objective:** Interactive periodic table explorer, property inspector, condition-dependent curves, and compare views.  
**Delivery Stage:** Phase 1 / T2 | **Default Priority:** P1

#### [MATUI-01] 01
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** Module Baseline
- **Description:** Create periodic-table browser with search, group/period filters and element detail panel.
- **Implementation Details:** Implement MATUI-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATUI-02] 02
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATUI-01
- **Description:** Show the supplied property catalogue grouped by category, with unit, source, condition and missing-value status.
- **Implementation Details:** Implement MATUI-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATUI-03] 03
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATUI-02
- **Description:** Allow users to create a material from an element, isotope, compound, alloy or mixture.
- **Implementation Details:** Implement MATUI-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATUI-04] 04
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATUI-03
- **Description:** Allow users to assign material to a whole object, selected faces, volumes or layered regions.
- **Implementation Details:** Implement MATUI-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATUI-05] 05
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATUI-04
- **Description:** Expose reference conditions and current simulated local conditions separately.
- **Implementation Details:** Implement MATUI-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATUI-06] 06
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATUI-05
- **Description:** Show current phase, phase fraction and transition warnings in the viewport/inspector.
- **Implementation Details:** Implement MATUI-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATUI-07] 07
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATUI-06
- **Description:** Provide property-vs-temperature and property-vs-pressure plots where source/model data supports them.
- **Implementation Details:** Implement MATUI-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATUI-08] 08
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATUI-07
- **Description:** Provide a material validation report before a simulation starts.
- **Implementation Details:** Implement MATUI-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATUI-09] 09
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATUI-08
- **Description:** Show where a property is used by active solver models and which missing values block a run.
- **Implementation Details:** Implement MATUI-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MATUI-10] 10
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** MATUI-09
- **Description:** Provide a compare view for two materials or two model-fidelity levels.
- **Implementation Details:** Implement MATUI-10 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module CMP: Generic Component & Device Framework
**Scope & Objective:** Encapsulated hierarchical components, typed public ports, internal topologies, and variant models.  
**Delivery Stage:** Phase 1 / T1 | **Default Priority:** P0

#### [CMP-01] 01 – Component contract
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** Module Baseline
- **Description:** Define ports, parameters, internal state, implementation model, metadata and compatibility.
- **Implementation Details:** Implement CMP-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CMP-02] 02 – Hierarchical composition
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** CMP-01
- **Description:** Allow a component to contain child components and internal connections.
- **Implementation Details:** Implement CMP-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CMP-03] 03 – Encapsulation
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** CMP-02
- **Description:** Expose public ports while keeping internal topology hidden or inspectable by permission.
- **Implementation Details:** Implement CMP-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CMP-04] 04 – Component variants
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** CMP-03
- **Description:** Support model variants and fidelity variants without changing external interface.
- **Implementation Details:** Implement CMP-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CMP-05] 05 – Component validation
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** CMP-04
- **Description:** Run schema, port, dependency, solver compatibility and behaviour tests.
- **Implementation Details:** Implement CMP-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CMP-06] 06 – Component editor
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** CMP-05
- **Description:** Create/edit symbol, geometry, ports, parameters, documentation and implementation.
- **Implementation Details:** Implement CMP-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CMP-07] 07 – Component runtime
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** CMP-06
- **Description:** Instantiate, initialise, step, reset and serialize component state.
- **Implementation Details:** Implement CMP-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CMP-08] 08 – Component packaging
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** CMP-07
- **Description:** Package definitions, models, scripts, assets, tests and dependency lockfile. 5. Simulation runtime and numerical infrastructure SIM – Simulation coordinator
- **Implementation Details:** Implement CMP-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module SIM: Simulation Runtime and Scheduling
**Scope & Objective:** State machines, physical simulation clock, deterministic scheduler, checkpoints, and run isolation.  
**Delivery Stage:** Phase 2 / T3 | **Default Priority:** P0

#### [SIM-01] 01
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** Module Baseline
- **Description:** Define simulation states: stopped, configuring, running, paused, stepping, completed, failed and cancelled.
- **Implementation Details:** Implement SIM-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SIM-02] 02
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** SIM-01
- **Description:** Create simulation configuration: start/end time, time-step policy, solver selection, tolerances, seed and output sampling.
- **Implementation Details:** Implement SIM-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SIM-03] 03
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** SIM-02
- **Description:** Implement simulation clock with physical time distinct from wall-clock time.
- **Implementation Details:** Implement SIM-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SIM-04] 04
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** SIM-03
- **Description:** Coordinate domain engines through a deterministic step protocol.
- **Implementation Details:** Implement SIM-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SIM-05] 05
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** SIM-04
- **Description:** Support fixed and adaptive time steps; expose minimum/maximum step and rejection diagnostics.
- **Implementation Details:** Implement SIM-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SIM-06] 06
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** SIM-05
- **Description:** Implement pause, resume, single-step, stop, reset and checkpoint/restart.
- **Implementation Details:** Implement SIM-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SIM-07] 07
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** SIM-06
- **Description:** Define event scheduling for discrete events, switching and user-triggered events.
- **Implementation Details:** Implement SIM-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SIM-08] 08
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** SIM-07
- **Description:** Implement run isolation so editing a project does not silently mutate an active run.
- **Implementation Details:** Implement SIM-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SIM-09] 09
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** SIM-08
- **Description:** Record run manifest with project hash, model versions, solver settings and environment.
- **Implementation Details:** Implement SIM-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SIM-10] 10
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** SIM-09
- **Description:** Add progress estimates, cancellation and graceful failure recovery. SOL – Numerical solver framework
- **Implementation Details:** Implement SIM-10 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module SOL: Numerical Solver Framework
**Scope & Objective:** Abstract solver interfaces, sparse matrix/vector linear algebra, ODE/DAE integrators, and PDE meshes.  
**Delivery Stage:** Phase 2 / T3 | **Default Priority:** P0

#### [SOL-01] 01
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** Module Baseline
- **Description:** Define solver interfaces for linear, nonlinear, ODE, DAE, PDE and discrete-event problems.
- **Implementation Details:** Implement SOL-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SOL-02] 02
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** SOL-01
- **Description:** Create sparse matrix/vector abstractions and select tested numerical backend.
- **Implementation Details:** Implement SOL-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SOL-03] 03
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** SOL-02
- **Description:** Implement linear solve adapters, preconditioner interface and convergence reporting.
- **Implementation Details:** Implement SOL-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SOL-04] 04
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** SOL-03
- **Description:** Implement nonlinear iteration interface with damping/line search and residual monitoring.
- **Implementation Details:** Implement SOL-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SOL-05] 05
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** SOL-04
- **Description:** Implement ODE integration methods and adaptive error control.
- **Implementation Details:** Implement SOL-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SOL-06] 06
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** SOL-05
- **Description:** Define mesh and discretization abstractions for PDE domains.
- **Implementation Details:** Implement SOL-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SOL-07] 07
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** SOL-06
- **Description:** Create solver capability registry and compatibility checks.
- **Implementation Details:** Implement SOL-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SOL-08] 08
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** SOL-07
- **Description:** Create numerical benchmark suite with reference solutions and tolerances.
- **Implementation Details:** Implement SOL-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SOL-09] 09
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** SOL-08
- **Description:** Record solver diagnostics: iterations, residual, condition estimates, rejected steps and runtime.
- **Implementation Details:** Implement SOL-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SOL-10] 10
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** SOL-09
- **Description:** Add numerical regression tests across supported compilers and platforms. EVT – Events, signals and co-simulation
- **Implementation Details:** Implement SOL-10 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module OBS: Observability, Probes & Dashboards
**Scope & Objective:** Probes, time-series ring buffers, live charts, vector field visualizations, and exportable data.  
**Delivery Stage:** Phase 2 / T4 | **Default Priority:** P0

#### [OBS-01] 01
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T4
- **Dependencies:** Module Baseline
- **Description:** Probe model for scalar, vector, tensor, field and event measurements.
- **Implementation Details:** Implement OBS-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [OBS-02] 02
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T4
- **Dependencies:** OBS-01
- **Description:** Live dashboards with charts, gauges, tables and status indicators.
- **Implementation Details:** Implement OBS-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [OBS-03] 03
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T4
- **Dependencies:** OBS-02
- **Description:** Time-series storage with sampling policies and compression.
- **Implementation Details:** Implement OBS-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [OBS-04] 04
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T4
- **Dependencies:** OBS-03
- **Description:** Spatial visualisation: vector fields, heatmaps, contours, streamlines and particle traces as applicable.
- **Implementation Details:** Implement OBS-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [OBS-05] 05
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T4
- **Dependencies:** OBS-04
- **Description:** Instrument models: oscilloscope, multimeter, thermometer, force sensor and flow meter.
- **Implementation Details:** Implement OBS-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [OBS-06] 06
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T4
- **Dependencies:** OBS-05
- **Description:** Units, significant digits, axis scaling and export controls.
- **Implementation Details:** Implement OBS-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [OBS-07] 07
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T4
- **Dependencies:** OBS-06
- **Description:** Result annotations, bookmarks and experiment notes.
- **Implementation Details:** Implement OBS-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [OBS-08] 08
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T4
- **Dependencies:** OBS-07
- **Description:** Export data to CSV/JSON and plots to image/PDF; include metadata sidecar.
- **Implementation Details:** Implement OBS-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [OBS-09] 09
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T4
- **Dependencies:** OBS-08
- **Description:** Result provenance: run ID, project hash, solver, model versions and environment.
- **Implementation Details:** Implement OBS-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [OBS-10] 10
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T4
- **Dependencies:** OBS-09
- **Description:** Large-result retention, cache policy and cleanup. 10. Component library, templates and sharing LIB – Reusable component library
- **Implementation Details:** Implement OBS-10 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module RUN: Simulation Execution & Safe Modes
**Scope & Objective:** Interactive visual preview, realtime wall-clock synced mode, scientific authoritative mode, and step control.  
**Delivery Stage:** Phase 2 / T3 | **Default Priority:** P0

#### [RUN-01] 01
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** Module Baseline
- **Description:** Interactive preview mode with clear approximate/visual-only label when not using authoritative solvers.
- **Implementation Details:** Implement RUN-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [RUN-02] 02
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** RUN-01
- **Description:** Realtime mode with target wall-clock rate, pause, step and performance indicator.
- **Implementation Details:** Implement RUN-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [RUN-03] 03
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** RUN-02
- **Description:** Scientific mode with explicit physical time, solver tolerances and reproducible output.
- **Implementation Details:** Implement RUN-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [RUN-04] 04
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** RUN-03
- **Description:** Accelerated mode where compute permits simulation faster than wall-clock.
- **Implementation Details:** Implement RUN-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [RUN-05] 05
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** RUN-04
- **Description:** Slow-motion and time dilation display controls that do not alter physical model semantics.
- **Implementation Details:** Implement RUN-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [RUN-06] 06
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** RUN-05
- **Description:** Headless batch mode for sweeps, CI and remote compute.
- **Implementation Details:** Implement RUN-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [RUN-07] 07
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** RUN-06
- **Description:** Checkpoint/restart and deterministic replay.
- **Implementation Details:** Implement RUN-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [RUN-08] 08
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** RUN-07
- **Description:** Run comparison: overlay traces, compare metrics and diff configurations.
- **Implementation Details:** Implement RUN-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [RUN-09] 09
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** RUN-08
- **Description:** Run failure report with failing element, domain, solver and suggested diagnostic actions.
- **Implementation Details:** Implement RUN-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [RUN-10] 10
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 2 / T3
- **Dependencies:** RUN-09
- **Description:** Run safety controls: stop conditions, numerical divergence limits and user-defined thresholds. OBS – Measurement, instrumentation and results
- **Implementation Details:** Implement RUN-10 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module IO: Data Exchange, Formats & Persistence
**Scope & Objective:** Atomic JSON manifests, SQLite relational storage, HDF5 numerical datasets, and schema migration.  
**Delivery Stage:** Phase 1 / T2 | **Default Priority:** P0

#### [IO-01] 01
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** Module Baseline
- **Description:** Define versioned project and component schemas.
- **Implementation Details:** Implement IO-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [IO-02] 02
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** IO-01
- **Description:** Choose human-readable metadata format plus efficient binary storage for large arrays/meshes/results.
- **Implementation Details:** Implement IO-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [IO-03] 03
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** IO-02
- **Description:** Define canonical units and numeric precision policy for persistence.
- **Implementation Details:** Implement IO-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [IO-04] 04
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** IO-03
- **Description:** Implement atomic save, backup, autosave and crash recovery.
- **Implementation Details:** Implement IO-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [IO-05] 05
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** IO-04
- **Description:** Implement import/export for selected mesh/CAD formats based on actual demand.
- **Implementation Details:** Implement IO-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [IO-06] 06
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** IO-05
- **Description:** Implement tabular data import for material curves, waveforms and experimental measurements.
- **Implementation Details:** Implement IO-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [IO-07] 07
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** IO-06
- **Description:** Implement CSV/JSON result export and metadata sidecars.
- **Implementation Details:** Implement IO-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [IO-08] 08
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** IO-07
- **Description:** Add project/package integrity checks and content hashes.
- **Implementation Details:** Implement IO-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [IO-09] 09
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** IO-08
- **Description:** Support schema migration and backwards compatibility tests.
- **Implementation Details:** Implement IO-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [IO-10] 10
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T2
- **Dependencies:** IO-09
- **Description:** Document supported formats, limitations and round-trip guarantees. 14. Security, reliability and operational quality
- **Implementation Details:** Implement IO-10 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module UI: User Interface & Workspaces
**Scope & Objective:** Qt desktop shell, dockable panels, project browser, 2D schematic/3D viewport, and property inspectors.  
**Delivery Stage:** Phase 1 / T1 | **Default Priority:** P0

#### [UI-01] 01 – Workspace shell
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** Module Baseline
- **Description:** Dockable panels, tabs, command palette, layout persistence, multi-monitor support and workspace presets.
- **Implementation Details:** Implement UI-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [UI-02] 02 – Project browser
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** UI-01
- **Description:** Project tree, assets, components, experiments, runs and search.
- **Implementation Details:** Implement UI-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [UI-03] 03 – 2D/3D viewport
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** UI-02
- **Description:** Navigation, selection, gizmos, grid, camera, clipping and render diagnostics.
- **Implementation Details:** Implement UI-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [UI-04] 04 – Schematic editor
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** UI-03
- **Description:** Wires, nodes, symbols, net labels, hierarchical sheets and electrical diagnostics.
- **Implementation Details:** Implement UI-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [UI-05] 05 – Graph/flow editor
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** UI-04
- **Description:** Typed nodes, wires, data flow, rule blocks and execution tracing.
- **Implementation Details:** Implement UI-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [UI-06] 06 – Component editor
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** UI-05
- **Description:** Ports, parameters, internal model, geometry/symbol, documentation and tests.
- **Implementation Details:** Implement UI-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [UI-07] 07 – Property inspector
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** UI-06
- **Description:** Schema-driven properties, units, validation, reset-to-default and advanced sections.
- **Implementation Details:** Implement UI-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [UI-08] 08 – Simulation toolbar
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** UI-07
- **Description:** Configure, run, pause, step, reset, stop, mode selector and solver status.
- **Implementation Details:** Implement UI-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [UI-09] 09 – Results workspace
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** UI-08
- **Description:** Plots, probes, dashboards, spatial fields and run comparisons.
- **Implementation Details:** Implement UI-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [UI-10] 10 – Code editor
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** UI-09
- **Description:** Script editing, API reference, diagnostics and controlled debugging.
- **Implementation Details:** Implement UI-10 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [UI-11] 11 – Library browser
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** UI-10
- **Description:** Private/public/template tabs, compatibility labels, dependency preview and install flow.
- **Implementation Details:** Implement UI-11 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [UI-12] 12 – Accessibility and usability
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** UI-11
- **Description:** Keyboard navigation, scalable UI, contrast, localisation readiness and discoverable errors. 12. Rendering, graphics and visual fidelity
- **Implementation Details:** Implement UI-12 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module REN: Rendering & Graphics Engine
**Scope & Objective:** Rendering abstraction layer, viewport cameras, lighting, materials, and scientific result overlays.  
**Delivery Stage:** Phase 1 / T4 | **Default Priority:** P1

#### [REN-01] 01
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T4
- **Dependencies:** Module Baseline
- **Description:** Choose renderer abstraction and backend strategy; keep graphics API details out of domain engines.
- **Implementation Details:** Implement REN-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [REN-02] 02
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T4
- **Dependencies:** REN-01
- **Description:** Create viewport camera, scene submission, lighting and material pipeline.
- **Implementation Details:** Implement REN-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [REN-03] 03
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T4
- **Dependencies:** REN-02
- **Description:** Support 2D schematic rendering and 3D scene rendering as separate presentation modes.
- **Implementation Details:** Implement REN-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [REN-04] 04
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T4
- **Dependencies:** REN-03
- **Description:** Build picking/selection, gizmos, overlays and debug draw.
- **Implementation Details:** Implement REN-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [REN-05] 05
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T4
- **Dependencies:** REN-04
- **Description:** Implement GPU resource lifetime, asset cache and asynchronous loading.
- **Implementation Details:** Implement REN-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [REN-06] 06
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T4
- **Dependencies:** REN-05
- **Description:** Add visualisation layers for solver output without changing simulation state.
- **Implementation Details:** Implement REN-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [REN-07] 07
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T4
- **Dependencies:** REN-06
- **Description:** Profile draw calls, memory, shader compilation and large scenes.
- **Implementation Details:** Implement REN-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [REN-08] 08
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T4
- **Dependencies:** REN-07
- **Description:** Support headless mode without requiring a graphics context.
- **Implementation Details:** Implement REN-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [REN-09] 09
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T4
- **Dependencies:** REN-08
- **Description:** Add renderer capability detection and fallback path.
- **Implementation Details:** Implement REN-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [REN-10] 10
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T4
- **Dependencies:** REN-09
- **Description:** Test rendering independently from numerical correctness. 13. Data, file formats, import/export and interoperability
- **Implementation Details:** Implement REN-10 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module ELEC: Electronics & Circuit Simulation
**Scope & Objective:** Schematic netlists, MNA assembly, DC/transient solvers, non-linear diodes/transistors, and probes.  
**Delivery Stage:** Phase 5 / T3 | **Default Priority:** P0

#### [ELEC-01] 01
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 5 / T3
- **Dependencies:** Module Baseline
- **Description:** Schematic graph: symbols, wires, nodes, junctions and netlist generation.
- **Implementation Details:** Implement ELEC-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ELEC-02] 02
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 5 / T3
- **Dependencies:** ELEC-01
- **Description:** Terminal/pin model with polarity, electrical domain and connection rules.
- **Implementation Details:** Implement ELEC-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ELEC-03] 03
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 5 / T3
- **Dependencies:** ELEC-02
- **Description:** Passive models: resistor, capacitor, inductor and transformer baseline.
- **Implementation Details:** Implement ELEC-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ELEC-04] 04
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 5 / T3
- **Dependencies:** ELEC-03
- **Description:** Sources: independent/dependent voltage and current sources; waveform definitions.
- **Implementation Details:** Implement ELEC-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ELEC-05] 05
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 5 / T3
- **Dependencies:** ELEC-04
- **Description:** Ground/reference nodes and floating-node detection.
- **Implementation Details:** Implement ELEC-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ELEC-06] 06
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 5 / T3
- **Dependencies:** ELEC-05
- **Description:** Modified nodal analysis (MNA) assembly and sparse solve integration.
- **Implementation Details:** Implement ELEC-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ELEC-07] 07
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 5 / T3
- **Dependencies:** ELEC-06
- **Description:** DC operating point analysis.
- **Implementation Details:** Implement ELEC-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ELEC-08] 08
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 5 / T3
- **Dependencies:** ELEC-07
- **Description:** Transient analysis with capacitor voltage and inductor current state handling.
- **Implementation Details:** Implement ELEC-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ELEC-09] 09
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 5 / T3
- **Dependencies:** ELEC-08
- **Description:** Diode and nonlinear device model integration; Newton iteration and convergence handling.
- **Implementation Details:** Implement ELEC-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ELEC-10] 10
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 5 / T3
- **Dependencies:** ELEC-09
- **Description:** Transistor models: start with documented simplified models, then add validated SPICE-compatible models.
- **Implementation Details:** Implement ELEC-10 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ELEC-11] 11
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 5 / T3
- **Dependencies:** ELEC-10
- **Description:** AC small-signal analysis and frequency response.
- **Implementation Details:** Implement ELEC-11 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ELEC-12] 12
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 5 / T3
- **Dependencies:** ELEC-11
- **Description:** Waveform viewer, probes, current direction and power calculations.
- **Implementation Details:** Implement ELEC-12 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ELEC-13] 13
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 5 / T3
- **Dependencies:** ELEC-12
- **Description:** Diagnostics: singular matrix, conflicting ideal sources, floating node and convergence failure.
- **Implementation Details:** Implement ELEC-13 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ELEC-14] 14
- **Status:** [ ] Not started
- **Priority:** P0
- **Stage / Phase:** Phase 5 / T3
- **Dependencies:** ELEC-13
- **Description:** Validation: divider, RC/RL, diode rectifier, transistor switch and amplifier benchmarks. LOGIC – Digital logic and computing devices
- **Implementation Details:** Implement ELEC-14 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module LOGIC: Digital Logic & State Machines
**Scope & Objective:** Discrete-event logic solver, Boolean gates, propagation delays, timing diagrams, and mixed-signal bridges.  
**Delivery Stage:** Phase 6 / T3 | **Default Priority:** P1

#### [LOGIC-01] 01
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 6 / T3
- **Dependencies:** Module Baseline
- **Description:** Boolean signal type, logic levels, propagation delay and unknown/high-impedance states.
- **Implementation Details:** Implement LOGIC-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LOGIC-02] 02
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 6 / T3
- **Dependencies:** LOGIC-01
- **Description:** Functional gates: NOT, AND, OR, NAND, NOR, XOR and XNOR.
- **Implementation Details:** Implement LOGIC-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LOGIC-03] 03
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 6 / T3
- **Dependencies:** LOGIC-02
- **Description:** Combinational circuit evaluation and truth-table test harness.
- **Implementation Details:** Implement LOGIC-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LOGIC-04] 04
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 6 / T3
- **Dependencies:** LOGIC-03
- **Description:** Sequential elements: latch, flip-flop, counter and register.
- **Implementation Details:** Implement LOGIC-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LOGIC-05] 05
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 6 / T3
- **Dependencies:** LOGIC-04
- **Description:** Clock generation, event scheduling and setup/hold abstractions where supported.
- **Implementation Details:** Implement LOGIC-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LOGIC-06] 06
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 6 / T3
- **Dependencies:** LOGIC-05
- **Description:** Bus, pin grouping and hierarchical digital modules.
- **Implementation Details:** Implement LOGIC-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LOGIC-07] 07
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 6 / T3
- **Dependencies:** LOGIC-06
- **Description:** Functional CPU/computing abstractions as later modules: ALU, registers, memory and bus.
- **Implementation Details:** Implement LOGIC-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LOGIC-08] 08
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 6 / T3
- **Dependencies:** LOGIC-07
- **Description:** Mixed-signal bridge between analog voltages and digital states.
- **Implementation Details:** Implement LOGIC-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LOGIC-09] 09
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 6 / T3
- **Dependencies:** LOGIC-08
- **Description:** Validation: truth tables, timing tests and sequential state tests. OPT – Optics and electromagnetic extensions
- **Implementation Details:** Implement LOGIC-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module MEC: Classical Mechanics & Rigid Bodies
**Scope & Objective:** Rigid body state integration, contact mechanics, joints, constraints, collision geometry, and kinematics.  
**Delivery Stage:** Phase 3 / T5 | **Default Priority:** P1

#### [MEC-01] 01
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 3 / T5
- **Dependencies:** Module Baseline
- **Description:** Rigid body state: position, orientation, linear/angular velocity and mass properties.
- **Implementation Details:** Implement MEC-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MEC-02] 02
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 3 / T5
- **Dependencies:** MEC-01
- **Description:** Forces and torques: gravity, applied force, impulses and constraints.
- **Implementation Details:** Implement MEC-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MEC-03] 03
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 3 / T5
- **Dependencies:** MEC-02
- **Description:** Collision geometry: primitive shapes, convex meshes and broad/narrow phase.
- **Implementation Details:** Implement MEC-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MEC-04] 04
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 3 / T5
- **Dependencies:** MEC-03
- **Description:** Contact and friction models with documented assumptions.
- **Implementation Details:** Implement MEC-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MEC-05] 05
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 3 / T5
- **Dependencies:** MEC-04
- **Description:** Joints: fixed, hinge, slider, ball and spring-damper.
- **Implementation Details:** Implement MEC-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MEC-06] 06
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 3 / T5
- **Dependencies:** MEC-05
- **Description:** Kinematics and motion constraints.
- **Implementation Details:** Implement MEC-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MEC-07] 07
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 3 / T5
- **Dependencies:** MEC-06
- **Description:** Energy and momentum diagnostics.
- **Implementation Details:** Implement MEC-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MEC-08] 08
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 3 / T5
- **Dependencies:** MEC-07
- **Description:** Mechanics visualisation: vectors, trails, contact points and telemetry.
- **Implementation Details:** Implement MEC-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MEC-09] 09
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 3 / T5
- **Dependencies:** MEC-08
- **Description:** Validation: free fall, pendulum, collisions, conservation and joint tests.
- **Implementation Details:** Implement MEC-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [MEC-10] 10
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 3 / T5
- **Dependencies:** MEC-09
- **Description:** Performance: broad-phase scaling, sleeping bodies and deterministic replay. THM – Heat transfer and thermal physics
- **Implementation Details:** Implement MEC-10 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module THM: Thermal & Heat Transfer
**Scope & Objective:** Conduction, convection, radiation boundary conditions, transient heat equations, and phase change.  
**Delivery Stage:** Phase 3 / T5 | **Default Priority:** P1

#### [THM-01] 01
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 3 / T5
- **Dependencies:** Module Baseline
- **Description:** Thermal state: temperature, internal energy and heat capacity.
- **Implementation Details:** Implement THM-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [THM-02] 02
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 3 / T5
- **Dependencies:** THM-01
- **Description:** Conduction models and boundary conditions.
- **Implementation Details:** Implement THM-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [THM-03] 03
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 3 / T5
- **Dependencies:** THM-02
- **Description:** Convection boundary models with declared coefficients or coupled fluid solution.
- **Implementation Details:** Implement THM-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [THM-04] 04
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 3 / T5
- **Dependencies:** THM-03
- **Description:** Radiation model and emissivity support.
- **Implementation Details:** Implement THM-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [THM-05] 05
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 3 / T5
- **Dependencies:** THM-04
- **Description:** Thermal contacts and interface resistance.
- **Implementation Details:** Implement THM-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [THM-06] 06
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 3 / T5
- **Dependencies:** THM-05
- **Description:** Transient and steady-state analysis.
- **Implementation Details:** Implement THM-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [THM-07] 07
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 3 / T5
- **Dependencies:** THM-06
- **Description:** Temperature-dependent material properties.
- **Implementation Details:** Implement THM-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [THM-08] 08
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 3 / T5
- **Dependencies:** THM-07
- **Description:** Thermal visualisation: temperature maps, heat flux and probes.
- **Implementation Details:** Implement THM-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [THM-09] 09
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 3 / T5
- **Dependencies:** THM-08
- **Description:** Validation against analytic slabs, rods and benchmark cases. FLD – Fluid mechanics and CFD
- **Implementation Details:** Implement THM-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module DEV: Physical Device Construction
**Scope & Objective:** Multi-part physical devices, geometry primitives, CAD/STEP import, contacts, and domain tagging.  
**Delivery Stage:** Phase 7 / T5 | **Default Priority:** P1

#### [DEV-01] 01
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T5
- **Dependencies:** Module Baseline
- **Description:** Define a device as a hierarchical assembly of elements, materials, interfaces, ports and internal constraints.
- **Implementation Details:** Implement DEV-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [DEV-02] 02
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T5
- **Dependencies:** DEV-01
- **Description:** Create geometry primitives and import pipeline for supported CAD/mesh formats.
- **Implementation Details:** Implement DEV-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [DEV-03] 03
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T5
- **Dependencies:** DEV-02
- **Description:** Define geometry-to-physics mapping: body regions, surfaces, volumes, contacts and material assignments.
- **Implementation Details:** Implement DEV-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [DEV-04] 04
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T5
- **Dependencies:** DEV-03
- **Description:** Support layered and composite construction: substrate, conductor, dielectric, semiconductor, coating and enclosure.
- **Implementation Details:** Implement DEV-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [DEV-05] 05
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T5
- **Dependencies:** DEV-04
- **Description:** Define conductor paths and electrical connectivity separately from visual overlap.
- **Implementation Details:** Implement DEV-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [DEV-06] 06
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T5
- **Dependencies:** DEV-05
- **Description:** Add contact/interface models: electrical contact resistance, thermal contact and mechanical contact as separate model types.
- **Implementation Details:** Implement DEV-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [DEV-07] 07
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T5
- **Dependencies:** DEV-06
- **Description:** Create a material/element editor with property validation and source references.
- **Implementation Details:** Implement DEV-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [DEV-08] 08
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T5
- **Dependencies:** DEV-07
- **Description:** Create assembly constraints and attachment points for mechanical and electrical integration.
- **Implementation Details:** Implement DEV-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [DEV-09] 09
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T5
- **Dependencies:** DEV-08
- **Description:** Add sensors and probes that read named physical quantities without altering the simulated system.
- **Implementation Details:** Implement DEV-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [DEV-10] 10
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T5
- **Dependencies:** DEV-09
- **Description:** Create device-level tests: continuity, connectivity, mass properties, thermal paths and model completeness.
- **Implementation Details:** Implement DEV-10 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [DEV-11] 11
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T5
- **Dependencies:** DEV-10
- **Description:** Support component variants and parameter sweeps.
- **Implementation Details:** Implement DEV-11 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [DEV-12] 12
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T5
- **Dependencies:** DEV-11
- **Description:** Document limitations when a geometric feature is not represented in the solver mesh/model. 8. Programmability: user-defined rules, formulas and code CODE – Code and rule execution system
- **Implementation Details:** Implement DEV-12 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module ENV: Real-time Virtual Lab Environment
**Scope & Objective:** World coordinate frames, gravity, atmospheric profiles, thermal chambers, and external excitation fields.  
**Delivery Stage:** Phase 1 / T1 | **Default Priority:** P1

#### [ENV-01] 01
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** Module Baseline
- **Description:** Define world coordinate system, scale, origin, gravity vector and simulation units.
- **Implementation Details:** Implement ENV-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ENV-02] 02
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** ENV-01
- **Description:** Create environment profiles: vacuum, air-like atmosphere, water-like fluid, thermal chamber and custom conditions.
- **Implementation Details:** Implement ENV-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ENV-03] 03
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** ENV-02
- **Description:** Define environmental fields: gravity, pressure, temperature, ambient medium and external sources.
- **Implementation Details:** Implement ENV-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ENV-04] 04
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** ENV-03
- **Description:** Create boundary-condition editor for domain-specific surfaces and regions.
- **Implementation Details:** Implement ENV-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ENV-05] 05
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** ENV-04
- **Description:** Support initial conditions and spatially varying fields.
- **Implementation Details:** Implement ENV-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ENV-06] 06
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** ENV-05
- **Description:** Add environment presets with declared assumptions and editable parameters.
- **Implementation Details:** Implement ENV-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ENV-07] 07
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** ENV-06
- **Description:** Support experiment fixtures: clamps, mounts, test benches, containers and circuit instruments.
- **Implementation Details:** Implement ENV-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [ENV-08] 08
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 1 / T1
- **Dependencies:** ENV-07
- **Description:** Add environment validation for missing/contradictory conditions. RUN – Interactive and scientific run modes
- **Implementation Details:** Implement ENV-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module CODE: Programmability, Formulas & Scripting
**Scope & Objective:** Embedded sandboxed scripting, condition-action rules, controller scripts, and deterministic execution.  
**Delivery Stage:** Phase 4 / T6 | **Default Priority:** P1

#### [CODE-01] 01
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** Module Baseline
- **Description:** Define rule types: event-driven, time-driven, condition/action, equation, controller and custom behaviour.
- **Implementation Details:** Implement CODE-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CODE-02] 02
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** CODE-01
- **Description:** Design a typed host API for reading state, writing permitted controls, emitting events and querying units.
- **Implementation Details:** Implement CODE-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CODE-03] 03
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** CODE-02
- **Description:** Choose an initial scripting language/runtime and define version compatibility.
- **Implementation Details:** Implement CODE-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CODE-04] 04
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** CODE-03
- **Description:** Implement sandboxing: memory/time limits, filesystem/network denial by default and capability permissions.
- **Implementation Details:** Implement CODE-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CODE-05] 05
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** CODE-04
- **Description:** Provide deterministic mode and controlled random-number generation.
- **Implementation Details:** Implement CODE-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CODE-06] 06
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** CODE-05
- **Description:** Implement script lifecycle: compile/validate, attach, initialise, execute, pause, reset and detach.
- **Implementation Details:** Implement CODE-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CODE-07] 07
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** CODE-06
- **Description:** Add code editor: syntax highlighting, diagnostics, autocomplete, API docs and examples.
- **Implementation Details:** Implement CODE-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CODE-08] 08
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** CODE-07
- **Description:** Add visual rule builder that compiles to the same rule representation as code where feasible.
- **Implementation Details:** Implement CODE-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CODE-09] 09
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** CODE-08
- **Description:** Create formula parser with dimensional analysis and safe expression evaluation.
- **Implementation Details:** Implement CODE-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CODE-10] 10
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** CODE-09
- **Description:** Add debugging: breakpoints for supported script runtime, watch values, call traces and event logs.
- **Implementation Details:** Implement CODE-10 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CODE-11] 11
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** CODE-10
- **Description:** Version scripts with component/project packages; record runtime version and permissions.
- **Implementation Details:** Implement CODE-11 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CODE-12] 12
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** CODE-11
- **Description:** Test infinite loops, memory exhaustion, invalid units, race conditions and malicious inputs.
- **Implementation Details:** Implement CODE-12 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module SEC: Security, Sandboxing & Reliability
**Scope & Objective:** Host protection, Lua sandboxing, resource budgets, extension signing, and safe deserialization.  
**Delivery Stage:** Phase 4 / T6 | **Default Priority:** P1

#### [SEC-01] 01
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** Module Baseline
- **Description:** Threat-model scripts, plugins, public packages, file imports and collaboration.
- **Implementation Details:** Implement SEC-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SEC-02] 02
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** SEC-01
- **Description:** Sandbox user scripts with strict capabilities and resource budgets.
- **Implementation Details:** Implement SEC-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SEC-03] 03
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** SEC-02
- **Description:** Require explicit trust and signing for native extensions.
- **Implementation Details:** Implement SEC-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SEC-04] 04
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** SEC-03
- **Description:** Validate imported files; defend against path traversal, oversized assets and malformed schemas.
- **Implementation Details:** Implement SEC-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SEC-05] 05
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** SEC-04
- **Description:** Separate project data from executable code.
- **Implementation Details:** Implement SEC-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SEC-06] 06
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** SEC-05
- **Description:** Add permission model for private, team and public assets.
- **Implementation Details:** Implement SEC-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SEC-07] 07
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** SEC-06
- **Description:** Create crash recovery, autosave and corrupted-project recovery tests.
- **Implementation Details:** Implement SEC-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SEC-08] 08
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** SEC-07
- **Description:** Define privacy-safe telemetry as opt-in with clear data inventory.
- **Implementation Details:** Implement SEC-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SEC-09] 09
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** SEC-08
- **Description:** Add dependency vulnerability scanning and software bill of materials.
- **Implementation Details:** Implement SEC-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [SEC-10] 10
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 4 / T6
- **Dependencies:** SEC-09
- **Description:** Define release signing, update channel and rollback strategy. 15. Testing and scientific validation framework
- **Implementation Details:** Implement SEC-10 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module LIB: Component Library, Templates & Sharing
**Scope & Objective:** Package manifests, private component repositories, templates, dependency resolution, and versioning.  
**Delivery Stage:** Phase 7 / T6 | **Default Priority:** P1

#### [LIB-01] 01
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T6
- **Dependencies:** Module Baseline
- **Description:** Define package manifest: identity, version, creator, license, description, tags and domain compatibility.
- **Implementation Details:** Implement LIB-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LIB-02] 02
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T6
- **Dependencies:** LIB-01
- **Description:** Support private project-local components.
- **Implementation Details:** Implement LIB-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LIB-03] 03
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T6
- **Dependencies:** LIB-02
- **Description:** Support personal reusable library across projects.
- **Implementation Details:** Implement LIB-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LIB-04] 04
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T6
- **Dependencies:** LIB-03
- **Description:** Support templates: starter project, device template, experiment template and subsystem template.
- **Implementation Details:** Implement LIB-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LIB-05] 05
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T6
- **Dependencies:** LIB-04
- **Description:** Support organization/team library with access controls.
- **Implementation Details:** Implement LIB-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LIB-06] 06
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T6
- **Dependencies:** LIB-05
- **Description:** Support public library only after moderation, trust and dependency policies exist.
- **Implementation Details:** Implement LIB-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LIB-07] 07
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T6
- **Dependencies:** LIB-06
- **Description:** Implement semantic versioning and immutable published releases.
- **Implementation Details:** Implement LIB-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LIB-08] 08
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T6
- **Dependencies:** LIB-07
- **Description:** Implement dependency lockfiles, version constraints and dependency graph.
- **Implementation Details:** Implement LIB-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LIB-09] 09
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T6
- **Dependencies:** LIB-08
- **Description:** Detect missing, incompatible and circular dependencies.
- **Implementation Details:** Implement LIB-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LIB-10] 10
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T6
- **Dependencies:** LIB-09
- **Description:** Provide update preview, migration notes and rollback.
- **Implementation Details:** Implement LIB-10 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LIB-11] 11
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T6
- **Dependencies:** LIB-10
- **Description:** Search, filter, preview, install, duplicate, fork and rate components.
- **Implementation Details:** Implement LIB-11 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LIB-12] 12
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T6
- **Dependencies:** LIB-11
- **Description:** Record provenance, validation status, license and known limitations.
- **Implementation Details:** Implement LIB-12 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LIB-13] 13
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T6
- **Dependencies:** LIB-12
- **Description:** Never execute untrusted native code or scripts automatically on install.
- **Implementation Details:** Implement LIB-13 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [LIB-14] 14
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 7 / T6
- **Dependencies:** LIB-13
- **Description:** Create package validator and automated compatibility test runner.
- **Implementation Details:** Implement LIB-14 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module EVT: Event & Co-Simulation Orchestration
**Scope & Objective:** Event channels, discrete signals, domain coupling intervals, co-simulation bridges, and replay.  
**Delivery Stage:** Phase 9 / T6 | **Default Priority:** P1

#### [EVT-01] 01
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 9 / T6
- **Dependencies:** Module Baseline
- **Description:** Define typed event channels and timestamp semantics.
- **Implementation Details:** Implement EVT-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [EVT-02] 02
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 9 / T6
- **Dependencies:** EVT-01
- **Description:** Implement signal sampling, interpolation, buffering and rate conversion.
- **Implementation Details:** Implement EVT-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [EVT-03] 03
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 9 / T6
- **Dependencies:** EVT-02
- **Description:** Define domain coupling schedule and communication points.
- **Implementation Details:** Implement EVT-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [EVT-04] 04
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 9 / T6
- **Dependencies:** EVT-03
- **Description:** Support discrete/continuous event interaction without hidden time drift.
- **Implementation Details:** Implement EVT-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [EVT-05] 05
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 9 / T6
- **Dependencies:** EVT-04
- **Description:** Implement co-simulation adapters for external tools only through explicit APIs.
- **Implementation Details:** Implement EVT-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [EVT-06] 06
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 9 / T6
- **Dependencies:** EVT-05
- **Description:** Test event ordering, simultaneity, feedback loops and deterministic replay. 6. Physics and engineering domain modules Each domain must implement the same lifecycle: model catalogue → parameters/materials → topology/geometry → governing equations → discretization → solver adapter → diagnostics → visualisation → validation. Domain-specific work must not leak into the generic scene or UI layers. MEC – Classical mechanics and rigid bodies
- **Implementation Details:** Implement EVT-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module FLD: Fluid Dynamics & Hydraulics
**Scope & Objective:** Incompressible/compressible Navier-Stokes, pipe networks, boundary conditions, and mesh interfaces.  
**Delivery Stage:** Phase 8 / T7 | **Default Priority:** P2

#### [FLD-01] 01
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** Module Baseline
- **Description:** Fluid properties and incompressible/compressible model selection.
- **Implementation Details:** Implement FLD-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [FLD-02] 02
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** FLD-01
- **Description:** Flow domain geometry and mesh pipeline.
- **Implementation Details:** Implement FLD-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [FLD-03] 03
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** FLD-02
- **Description:** Boundary conditions: inlet, outlet, wall, symmetry and initial state.
- **Implementation Details:** Implement FLD-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [FLD-04] 04
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** FLD-03
- **Description:** Choose and document initial flow formulation; begin with a bounded, testable subset.
- **Implementation Details:** Implement FLD-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [FLD-05] 05
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** FLD-04
- **Description:** Pressure/velocity coupling and solver convergence diagnostics.
- **Implementation Details:** Implement FLD-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [FLD-06] 06
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** FLD-05
- **Description:** Laminar flow baseline; turbulence models only after validation.
- **Implementation Details:** Implement FLD-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [FLD-07] 07
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** FLD-06
- **Description:** Free-surface and multiphase support as a later capability.
- **Implementation Details:** Implement FLD-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [FLD-08] 08
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** FLD-07
- **Description:** Particle and tracer visualisation.
- **Implementation Details:** Implement FLD-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [FLD-09] 09
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** FLD-08
- **Description:** Validation: Poiseuille flow, cavity flow and canonical benchmark cases.
- **Implementation Details:** Implement FLD-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [FLD-10] 10
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** FLD-09
- **Description:** Mesh sensitivity and performance profiling. CHEM – Chemistry and reaction systems
- **Implementation Details:** Implement FLD-10 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module CHEM: Chemical Reactions & Stoichiometry
**Scope & Objective:** Species concentrations, balanced reactions, kinetic rate laws, equilibrium solvers, and thermal coupling.  
**Delivery Stage:** Phase 8 / T7 | **Default Priority:** P2

#### [CHEM-01] 01
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** Module Baseline
- **Description:** Species, concentration, phase and reaction schema.
- **Implementation Details:** Implement CHEM-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CHEM-02] 02
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** CHEM-01
- **Description:** Reaction definitions and stoichiometric balance validation.
- **Implementation Details:** Implement CHEM-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CHEM-03] 03
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** CHEM-02
- **Description:** Kinetic rate laws with unit checking.
- **Implementation Details:** Implement CHEM-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CHEM-04] 04
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** CHEM-03
- **Description:** Batch reactor and well-mixed baseline.
- **Implementation Details:** Implement CHEM-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CHEM-05] 05
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** CHEM-04
- **Description:** Transport/diffusion model and boundary conditions.
- **Implementation Details:** Implement CHEM-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CHEM-06] 06
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** CHEM-05
- **Description:** Temperature-dependent reaction rates and thermal coupling.
- **Implementation Details:** Implement CHEM-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CHEM-07] 07
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** CHEM-06
- **Description:** Mass conservation and positivity constraints.
- **Implementation Details:** Implement CHEM-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CHEM-08] 08
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** CHEM-07
- **Description:** Reaction progress and concentration visualisation.
- **Implementation Details:** Implement CHEM-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [CHEM-09] 09
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** CHEM-08
- **Description:** Validation against analytic reaction kinetics. ELEC – Analog electronics and circuit simulation
- **Implementation Details:** Implement CHEM-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module OPT: Optics & Wave Phenomena
**Scope & Objective:** Ray tracing, Snell refraction, optical materials, lens/mirror geometry, and wave interference.  
**Delivery Stage:** Phase 8 / T7 | **Default Priority:** P2

#### [OPT-01] 01
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** Module Baseline
- **Description:** Define whether initial scope is geometric optics, wave optics or electromagnetic field solving.
- **Implementation Details:** Implement OPT-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [OPT-02] 02
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** OPT-01
- **Description:** Optical material properties and interfaces.
- **Implementation Details:** Implement OPT-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [OPT-03] 03
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** OPT-02
- **Description:** Ray sources, lenses, mirrors and detectors for a geometric-optics baseline.
- **Implementation Details:** Implement OPT-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [OPT-04] 04
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** OPT-03
- **Description:** Ray tracing, reflection, refraction and absorption.
- **Implementation Details:** Implement OPT-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [OPT-05] 05
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** OPT-04
- **Description:** Field-based electromagnetic solver architecture as a later major project.
- **Implementation Details:** Implement OPT-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [OPT-06] 06
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** OPT-05
- **Description:** Validation against Snell's law and canonical optical systems. BIO – Biological and cellular systems
- **Implementation Details:** Implement OPT-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module BIO: Biological & Cellular Models
**Scope & Objective:** Agent-based biology, cell dynamics, compartment models, population kinetics, and environmental coupling.  
**Delivery Stage:** Phase 8 / T7 | **Default Priority:** P2

#### [BIO-01] 01
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** Module Baseline
- **Description:** Define supported biological abstraction: agent, population, tissue or cellular model.
- **Implementation Details:** Implement BIO-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [BIO-02] 02
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** BIO-01
- **Description:** Cell/agent state, parameters, compartments and interaction rules.
- **Implementation Details:** Implement BIO-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [BIO-03] 03
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** BIO-02
- **Description:** Growth, division, movement and death event models where scientifically justified.
- **Implementation Details:** Implement BIO-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [BIO-04] 04
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** BIO-03
- **Description:** Diffusion/transport coupling for nutrients or signals.
- **Implementation Details:** Implement BIO-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [BIO-05] 05
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** BIO-04
- **Description:** Rule-based and stochastic behaviour with reproducible random seeds.
- **Implementation Details:** Implement BIO-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [BIO-06] 06
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** BIO-05
- **Description:** Visualisation and population-level metrics.
- **Implementation Details:** Implement BIO-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [BIO-07] 07
- **Status:** [ ] Not started
- **Priority:** P2
- **Stage / Phase:** Phase 8 / T7
- **Dependencies:** BIO-06
- **Description:** Validation against declared reference behaviours and datasets. 7. Elements, metals, materials and physical device construction This is the bridge between a visual world and an engineering model. A metal object, semiconductor region or composite assembly must carry material and model data; its appearance alone must not determine its physics.
- **Implementation Details:** Implement BIO-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

### Module PERF: Performance, Benchmarks & Profiling
**Scope & Objective:** Performance budgets, memory tracking, multi-threading scaling benchmarks, and lockless queues.  
**Delivery Stage:** Phase 10 / T8 | **Default Priority:** P1

#### [PERF-01] 01
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 10 / T8
- **Dependencies:** Module Baseline
- **Description:** Set measurable budgets for startup, editing latency, simulation throughput, memory and result storage.
- **Implementation Details:** Implement PERF-01 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [PERF-02] 02
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 10 / T8
- **Dependencies:** PERF-01
- **Description:** Create representative workloads for scene, circuit, mesh, particle and script-heavy projects.
- **Implementation Details:** Implement PERF-02 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [PERF-03] 03
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 10 / T8
- **Dependencies:** PERF-02
- **Description:** Profile CPU, GPU, memory, allocations, locks and I/O before optimising.
- **Implementation Details:** Implement PERF-03 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [PERF-04] 04
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 10 / T8
- **Dependencies:** PERF-03
- **Description:** Use data-oriented layouts in hot numerical loops where benchmarks justify them.
- **Implementation Details:** Implement PERF-04 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [PERF-05] 05
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 10 / T8
- **Dependencies:** PERF-04
- **Description:** Parallelise independent work and domain assembly while preserving deterministic reductions where required.
- **Implementation Details:** Implement PERF-05 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [PERF-06] 06
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 10 / T8
- **Dependencies:** PERF-05
- **Description:** Use sparse data structures for sparse physical systems.
- **Implementation Details:** Implement PERF-06 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [PERF-07] 07
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 10 / T8
- **Dependencies:** PERF-06
- **Description:** Add adaptive output sampling and result compression.
- **Implementation Details:** Implement PERF-07 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [PERF-08] 08
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 10 / T8
- **Dependencies:** PERF-07
- **Description:** Support background solve and responsive UI with safe state snapshots.
- **Implementation Details:** Implement PERF-08 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [PERF-09] 09
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 10 / T8
- **Dependencies:** PERF-08
- **Description:** Add memory budgets, cancellation and back-pressure for large jobs.
- **Implementation Details:** Implement PERF-09 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

#### [PERF-10] 10
- **Status:** [ ] Not started
- **Priority:** P1
- **Stage / Phase:** Phase 10 / T8
- **Dependencies:** PERF-09
- **Description:** Publish benchmark baselines and detect performance regressions in CI. 17. Build and delivery roadmap The roadmap below is a dependency-aware sequence. Estimates should be assigned only after a technical spike and team-capacity assessment; the phases are gates, not calendar promises. Phase Work focus Exit deliverable Phase 0 – Product/science specification Define first users, use cases, fidelity policy, supported domains, model review process and initial acceptance cases. Approved product brief, architecture decisions, model catalogue and risk register. Phase 1 – Desktop and project foundation Build app shell, project lifecycle, scene graph, element schema, units, logging, CI and basic viewport. Create/open/save project; add/select/move an element; unit-aware properties. Phase 2 – First executable simulation Implement simulation lifecycle, clock, solver interfaces, results/probes and one simple validated domain. A reproducible free-fall or spring-mass experiment with plotted output. Phase 3 – Mechanical and thermal baseline Add rigid mechanics and basic heat conduction with independent validation. Build a mechanical test rig and inspect motion/temperature results. Phase 4 – Programmable rules Add safe scripting, event rules, sensors, actuators and deterministic execution. A coded controller reads a sensor and changes an actuator during a run. Phase 5 – Circuit/electronics MVP Add schematic editor, netlist, MNA, passive components, DC and RC transient analysis. Build and simulate a resistor network and RC circuit; inspect waveforms. Phase 6 – Semiconductor and digital logic Add diode/transistor models, functional gates, sequential logic and mixed-signal bridge. Build a transistor switch and functional gate; compare model fidelity. Phase 7 – Generic component library Add hierarchical components, private library, templates, validation and versioning. Save/reuse an RC block, transistor circuit and mechanical assembly. Phase 8 – Additional physics domains Add fluids, chemistry, optics and biological models in independently validated slices. Each domain has its own accepted reference cases and result visualisation. Phase 9 – Multiphysics coupling Add typed cross-domain coupling and co-simulation with energy/mass accounting. Demonstrate electro-thermal or mechanical-thermal coupling with validated tests. Phase 10 – Public ecosystem and advanced scale Add public registry, moderation, package signing, collaboration and batch/remote compute. Install a verified public component and reproduce a packaged experiment. 18. First implementable release: strict MVP boundary The MVP should prove the complete product loop rather than attempt many incomplete physics domains. MVP modules Desktop application shell, project create/open/save and a basic scene or schematic workspace. Stable element/component schema with typed parameters, units and ports. Basic material records and property inspector. Simulation lifecycle, physical clock, one numerical solver path and result storage. One validated mechanical example OR one validated electrical example; then add the second only after the first is stable. Programmable rule prototype with restricted API, resource limits and event triggers. Probe, chart, logs, run metadata and reproducible rerun. Private component save/reuse with schema version and dependency capture. Recommended first demonstrator Build a small virtual test bench that proves the architecture: a DC source, resistor, capacitor, ground, switch, voltage probe and a script-controlled switch. Run DC operating point and RC transient analysis, show voltage/current plots, record the run settings, save the RC network as a reusable private component, then instantiate it in a second project. Why this demonstrator: it exercises elements, typed ports, units, materials/parameters, topology, numerical solving, code-defined rules, realtime controls, measurement, persistence and component reuse without pretending to solve all physics. 19. Architecture decisions that must be recorded Decision ID Question Recommended default / decision gate
- **Implementation Details:** Implement PERF-10 according to the modular C++ architecture in core/ or engines/ or pps/, adhering to strict memory safety, exception/error handling, and zero-leakage interfaces.
- **Acceptance Criteria:** Unit and integration test coverage verifying functionality under nominal and edge conditions; complete test execution log; documentation updated.
- **Verification & Git Commit:** None (Pending)

---

## 5. Proposed Improvements (Architectural Enhancements Beyond Original Scope)
These improvements represent proactive technical optimizations identified during architectural review. They are strictly segregated from original requirements and do not alter baseline contracts without formal authorization.

### [IMP-01] Compile-Time Dimensional Unit Analysis via C++20 Non-Type Template Parameters (NTTP)
- **Purpose:** Provide zero-runtime-overhead unit checking for physical equations in core numerical kernels while retaining dynamic runtime unit checking for user-defined schematic ports.
- **Benefits:** Eliminates runtime performance penalties in solver tight loops while preventing dimensional errors at compile-time.
- **Technical Impact:** Header-only type traits in core/base/units.hpp.
- **Dependencies:** FND-01, UNIT-01
- **Implementation Status:** [ ] Proposed

### [IMP-02] Headless Simulation CLI (shunya-cli)
- **Purpose:** Allow running projects, batch sweeps, and scientific validation benchmarks in headless CI/CD environments without initializing Qt GUI.
- **Benefits:** Rapid automated regression testing, cluster job execution, parametric sweeps, and seamless integration with automated test harnesses.
- **Technical Impact:** Standalone executable target in CMake (pps/cli).
- **Dependencies:** FND-02, SIM-01, IO-01
- **Implementation Status:** [ ] Proposed

### [IMP-03] Lock-Free Ring Buffer for Simulation Probe Observability
- **Purpose:** Decouple high-frequency simulation steps (e.g. 100 kHz) from 60 Hz UI viewport rendering without mutex contention.
- **Benefits:** Prevents simulation thread stuttering and UI frame drops during intense data logging.
- **Technical Impact:** Single-producer single-consumer lock-free ring buffer in core/base/ring_buffer.hpp.
- **Dependencies:** FND-08, OBS-03
- **Implementation Status:** [ ] Proposed

### [IMP-04] WebAssembly / Emscripten Headless Solver Target Readiness
- **Purpose:** Architect core solvers and mathematical engines so they compile cleanly to WebAssembly for future browser-based demonstration or cloud sandboxing.
- **Benefits:** Expands platform portability and enables lightweight online simulation viewers without rewriting core simulation logic.
- **Technical Impact:** Strict POSIX/ISO C++ compliance without platform-specific system calls in core/base and core/simulation.
- **Dependencies:** FND-01, SOL-01
- **Implementation Status:** [ ] Proposed

## 6. Development Session State & Immediate Next Steps
- **Current Session Goal:** Initial analysis, Git setup, master roadmap creation, and kickoff of Foundational Task FND-01.
- **Completed Work:** Planning document analysis; Git repository initialized; baseline commit created; PROJECT_ROADMAP.md created with all 309 catalogued tasks.
- **Last Completed Task:** [x] FND-01: Build and dependency management (Commit: c8fce74)
- **Last Completed Tasks:** [x] FND-01, [x] FND-03, [x] FND-04 (Commit: bf28b9)
- **Last Completed Tasks:** [x] FND-01, [x] FND-03, [x] FND-04, [x] FND-05
- **Last Completed Tasks:** [x] FND-01, [x] FND-03, [x] FND-04, [x] FND-05, [x] FND-06
- **Last Completed Tasks:** [x] FND-01, [x] FND-03, [x] FND-04, [x] FND-05, [x] FND-06, [x] FND-07
- **Last Completed Tasks:** [x] FND-01, [x] FND-03, [x] FND-04, [x] FND-05, [x] FND-06, [x] FND-07, [x] FND-09
- **Last Completed Tasks:** [x] FND-01, [x] FND-03, [x] FND-04, [x] FND-05, [x] FND-06, [x] FND-07, [x] FND-09, [x] FND-08
- **Last Completed Tasks:** [x] FND-01, [x] FND-03, [x] FND-04, [x] FND-05, [x] FND-06, [x] FND-07, [x] FND-09, [x] FND-08, [x] FND-02
- **Last Completed Tasks:** All Module FND Tasks [x] FND-01 through [x] FND-10 (**100% Completed**)
- **Last Completed Tasks:** [x] UNIT-01, [x] UNIT-02, [x] UNIT-03
- **Last Completed Tasks:** All Module UNIT Tasks [x] UNIT-01 through [x] UNIT-07 (**100% Completed**)
- **Pending Immediate Task:** [ ] PRJ-08: Project packaging (Collect assets, scripts, model dependencies and result references into a portable package).
- **Blockers:** None.
- **Active Git Commit:** Baseline 92b20f.
