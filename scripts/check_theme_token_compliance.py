#!/usr/bin/env python3
"""
Theme Token Compliance Checker
Scans QML files to enforce zero hard-coded hex colors in core workspace and authoring files.
All colors in UI chrome and authoring panels must reference Theme.* tokens.
"""

import os
import re
import sys

# Files that legitimately define tokens or color palettes
EXEMPT_FILES = {
    "Theme.qml",                # Theme singleton definition
    "ThemePalette.qml",         # Theme definition
    "shunya_gui_lib_Theme_qml.cpp"
}

# Core workspaces and controls that must strictly adhere to Theme.* contract (0 tolerance)
STRICT_CORE_FILES = {
    "ElementCell.qml",
    "PeriodicTableWindow.qml",
    "ElementsWorkspace.qml",
    "MixingLabWorkspace.qml",
    "ObjectsWorkspace.qml",
    "ObjectRegionsView.qml",
    "ObjectGeometryTab.qml",
    "ObjectCavityTab.qml",
    "ObjectTestBenchTab.qml",
    "ObjectSimulatorTab.qml",
    "MonoInfoButton.qml",
    "MaterialPickerCombo.qml",
    "ObjectImportPreviewDialog.qml",
    "ImportPreviewDialog.qml",
    "MainMenuBar.qml",
    "Editor3DWorkspace.qml",
}

HEX_REGEX = re.compile(r'#[0-9a-fA-F]{3,8}\b')

def scan_files(directory="shunya"):
    violations = []
    checked_files = 0

    if not os.path.exists(directory):
        print(f"Directory '{directory}' does not exist.")
        return 1

    for root, dirs, files in os.walk(directory):
        # Ignore build directories
        dirs[:] = [d for d in dirs if not d.startswith("build") and not d.startswith(".")]
        for fname in files:
            if not fname.endswith(".qml"):
                continue
            if fname in EXEMPT_FILES:
                continue

            rel_path = os.path.relpath(os.path.join(root, fname), directory)
            base_name = os.path.basename(fname)

            # Check strict core files
            if base_name in STRICT_CORE_FILES:
                checked_files += 1
                filepath = os.path.join(root, fname)
                with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
                    for line_num, line in enumerate(f, 1):
                        # Skip single-line comments
                        stripped = line.strip()
                        if stripped.startswith("//") or stripped.startswith("*"):
                            continue
                        matches = HEX_REGEX.findall(line)
                        if matches:
                            violations.append((rel_path, line_num, matches, line.strip()))

    print(f"Scanned {checked_files} strict core UI QML files for Theme token compliance.")
    if violations:
        print(f"FAIL: Found {len(violations)} hard-coded hex color violations:")
        for path, line_no, matches, line_text in violations:
            print(f"  {path}:{line_no} -> {matches} in: {line_text}")
        return 1
    else:
        print("PASS: 100% Theme token compliance across all core UI and authoring files!")
        return 0

if __name__ == "__main__":
    sys.exit(scan_files())
