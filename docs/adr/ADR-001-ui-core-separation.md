# ADR-001: Separation of Desktop UI and Simulation Core

## Status
Accepted

## Context
Shunya is a virtual engineering and multiphysics platform requiring high-performance, deterministic numerical solvers, rigorous unit checking, material modeling, and extensible scientific components. A common architectural anti-pattern in desktop simulation tools is tightly coupling UI framework primitives (such as Qt QObjects, signals/slots, or GUI events) into physical domain calculations and data structures.

## Decision
1. **Strict Core Independence:** The core simulation runtime (`core/base`, `core/domain`, `core/simulation`, `engines/*`) shall have zero compile-time or runtime dependencies on Qt, VTK, OCCT, or any presentation layer.
2. **Read-Only Simulation State Snapshots:** The desktop application shell (`apps/desktop` or `shunya`) interacts with the core through stable C++ interfaces, controllers, and read-only immutable simulation state snapshots.
3. **Headless Execution:** All scientific calculations, unit conversions, and domain solves must be capable of running in headless CI/CLI test environments without an active windowing system or GUI event loop.

## Consequences
- **Positive:** Maximum execution speed, zero GUI thread contention in numerical loops, seamless automated testing in headless CI, clean architecture.
- **Negative:** Requires explicit snapshot/data-binding abstractions between Qt Quick UI models and core domain objects.
