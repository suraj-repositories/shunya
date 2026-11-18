import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

Window {
    id: helpWin
    title: "Shunya Scientific Help Center"
    width: 860
    height: 600
    color: Theme.bgCanvas

    property string activeTopicId: "mixing-lab-overview"
    property string searchQuery: ""

    function openTopic(topicId) {
        activeTopicId = topicId;
        helpWin.show();
        helpWin.raise();
        helpWin.requestActivate();
    }

    readonly property var topics: [
        {
            id: "mixing-lab-overview",
            title: "Mixing Lab Architecture & Overview",
            category: "Mixing Lab",
            summary: "Core framework for synthesising novel multi-element alloys, chemical compounds, and composites.",
            content: "The Material Mixing Laboratory & Property Engine is an integrated thermodynamic and electronic property computation platform. It calculates effective macroscopic physical tensors from constituent elemental ground states loaded directly from elements.csv.\n\nThe lab operates on a three-card reactive architecture:\n• Left Card: Presets, Scenario selection, T/P environmental parameters, and Constituent element stoichiometry.\n• Middle Card: Live rule validation status, Base vs. Mixture comparison, and Property vs. Temperature sweep charts.\n• Right Card: Project Materials catalog, Virtual Test Bench, Atomic Bond analysis, Spider Radar comparisons, and Physical Explanations."
        },
        {
            id: "formation-scenarios",
            title: "6 Formation Scenarios & Governing Equations",
            category: "Physics & Chemistry",
            summary: "Mathematical models governing alloy lattices, solutions, chemical reactions, and composites.",
            content: "Shunya provides 6 discrete physical formation scenarios:\n\n1. PhysicalMixture: Linear Rule of Mixtures (Voigt/Reuss bounds):\n   rho_eff = sum(w_i * rho_i),  1/k_eff = sum(w_i / k_i)\n\n2. Alloy: Hume-Rothery metallic solid solution with lattice distortion hardening:\n   sigma_y = sigma_0 + sum(k_i * sqrt(c_i))\n\n3. Solution: Liquid or liquid-solid dissolution obeying Raoult and Henry's laws:\n   P_tot = sum(x_i * P_i*)\n\n4. ChemicalCompound: Stoichiometric valence balancing and enthalpy of formation:\n   Delta_H_f = sum(nu_prod * H_f,prod) - sum(nu_react * H_f,react)\n\n5. LayeredMaterial: Directional anisotropic laminate tensor formulation across in-plane and cross-plane axes.\n\n6. CompositeMaterial: Mori-Tanaka and Hashin-Shtrikman homogenization for matrix-inclusion composites."
        },
        {
            id: "constituent-normalization",
            title: "Constituent Validation, Units & Normalization",
            category: "Data Integrity",
            summary: "Single source of truth for element stoichiometry, autocomplete, and 1-click normalization.",
            content: "All constituents in the Mixing Lab must resolve to authoritative IUPAC elements from elements.csv. Duplicate entries are automatically coalesced, and non-positive quantities are rejected.\n\nConstituent fractions can be specified in Mass %, Mole %, or Volume %. The sum of all constituent fractions must strictly equal 100.0%. When deviations occur, the 1-click 'Normalize' button re-scales all fractions proportionally to guarantee thermodynamic consistency."
        },
        {
            id: "virtual-test-bench",
            title: "Virtual Test Bench Models & Environmental Qualification",
            category: "Simulation & Testing",
            summary: "7 Multiphysics qualification domains gating promotion into Project Materials.",
            content: "The Virtual Material Test Bench subjects candidate mixtures to 7 harsh environmental simulations:\n\n1. Thermal: Thermal shock parameter R = sigma_f*(1 - nu)/(E*alpha).\n2. Gravity & Mechanical: Self-weight deflection, yield and ultimate tensile limits.\n3. Vacuum & Space: Langmuir sublimation mass loss W = P_v * sqrt(M / (2*pi*R*T)).\n4. Electrical Current: Joule heating thermal runaway limits.\n5. Caloric Heat: Debye/Shomate enthalpy curves and thermal diffusivity.\n6. Rust & Oxidation: Pilling-Bedworth ratio (PBR) oxide passivation kinetics.\n7. Extreme Environments: Radiation displacement per atom (DPA) and hydrostatic pressure."
        },
        {
            id: "atomic-bonds",
            title: "Atomic Bond Checker & Hume-Rothery Miscibility Rules",
            category: "Physics & Chemistry",
            summary: "First-principles electronegativity and lattice isomorphism verification.",
            content: "The Atomic Bond Checker evaluates Pauling electronegativity differences (Delta_EN = |chi_A - chi_B|) across all constituent element pairs:\n\n• Ionic Bonding: Delta_EN > 1.7\n• Polar Covalent: 0.5 <= Delta_EN <= 1.7\n• Nonpolar Covalent: Delta_EN < 0.5 (between nonmetals)\n• Metallic Bonding: High metallic character with low electronegativities\n\nHume-Rothery Miscibility Rules:\n1. Atomic size factor: delta = (|r_A - r_B| / r_A) <= 15% for extensive solid solubility.\n2. Crystal structure: solvent and solute crystal structures must match (FCC, BCC, HCP).\n3. Valence rule: lower valences dissolve higher valences more readily.\n4. Electronegativity: Delta_EN <= 0.4 favors solid solution; higher differences trigger intermetallic precipitation."
        },
        {
            id: "comparison-radar",
            title: "Comparison Engine, Radar Charts & Ranking Logic",
            category: "Analytics",
            summary: "8-Axis normalized engineering performance benchmarks and multi-objective ranking.",
            content: "The Material Comparison Engine standardizes 8 core engineering criteria on a 0.0 to 1.0 radar polygon:\n\n1. Electrical Conductivity (sigma)\n2. Thermal Conductivity (k)\n3. Temperature Tolerance (T_m)\n4. Corrosion Resistance Index\n5. Lightweight Rating (inverted mass density)\n6. Mechanical Yield Strength\n7. Economic Abundance Index (crustal rarity log scale)\n8. EMI Shielding Suitability\n\nThe 'Which is better for...' ranker evaluates candidate materials using goal-weighted scoring algorithms for specialized applications."
        },
        {
            id: "experiment-persistence",
            title: "Experiment Persistence, Schema Versioning & Sharing",
            category: "Storage",
            summary: "Atomic JSON file serialization and safe cross-platform experiment exchange.",
            content: "Mixture experiments are serialized as standalone .shunya-mix.json files conforming to schemaVersion '5.0.0'. Writes utilize atomic temporary file buffering to guarantee zero data corruption.\n\nAll imported experiments undergo strict JSON schema sanitization, element existence verification, and duplicate-name resolution before ingestion into the active workspace."
        },
        {
            id: "keyboard-shortcuts",
            title: "Desktop Keyboard Shortcuts & Navigation Guide",
            category: "Navigation",
            summary: "Comprehensive hotkey reference for Shunya Engineering Studio.",
            content: "Essential Desktop Shortcuts:\n\n• Ctrl + B: Toggle Left Navigation Sidebar\n• Ctrl + J: Toggle Bottom Diagnostics Console\n• Ctrl + Alt + B: Toggle Right Properties Sidebar\n• F11 / Ctrl + Space: Maximize Viewport Mode\n• Ctrl + Shift + P: Open Command Palette\n• Ctrl + Z / Ctrl + Y: Global Undo / Redo\n• Space / Enter: Activate focused control / Expand info popover\n• Escape: Dismiss open popovers, dropdowns, and modals"
        }
    ]

    property var filteredTopics: {
        var q = searchQuery.toLowerCase().trim();
        if (q.length === 0) return topics;
        var res = [];
        for (var i = 0; i < topics.length; ++i) {
            var t = topics[i];
            if (t.title.toLowerCase().indexOf(q) !== -1 ||
                t.summary.toLowerCase().indexOf(q) !== -1 ||
                t.content.toLowerCase().indexOf(q) !== -1 ||
                t.category.toLowerCase().indexOf(q) !== -1) {
                res.push(t);
            }
        }
        return res;
    }

    property var currentTopic: {
        for (var i = 0; i < topics.length; ++i) {
            if (topics[i].id === activeTopicId) return topics[i];
        }
        return topics.length > 0 ? topics[0] : null;
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Left Navigation Column
        Rectangle {
            Layout.preferredWidth: 260
            Layout.fillHeight: true
            color: Theme.bgSurface
            border.color: Theme.borderDefault
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Text {
                    text: "Scientific Help Center"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontBase
                    font.weight: Font.Bold
                    color: Theme.textPrimary
                }

                MonoInput {
                    Layout.fillWidth: true
                    placeholder: "Search documentation..."
                    text: helpWin.searchQuery
                    onTextEdited: (val) => helpWin.searchQuery = val
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    ColumnLayout {
                        width: parent.width
                        spacing: 4

                        Repeater {
                            model: helpWin.filteredTopics
                            delegate: Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 48
                                radius: Theme.radiusSm
                                color: (helpWin.activeTopicId === modelData.id) ?
                                    Qt.rgba(Theme.accentPrimary.r, Theme.accentPrimary.g, Theme.accentPrimary.b, 0.15) :
                                    (topicMouse.containsMouse ? Theme.bgHover : "transparent")
                                border.color: (helpWin.activeTopicId === modelData.id) ? Theme.accentPrimary : "transparent"
                                border.width: 1

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    spacing: 2

                                    Text {
                                        text: modelData.title
                                        font.family: Theme.fontSans
                                        font.pixelSize: Theme.fontXs
                                        font.weight: (helpWin.activeTopicId === modelData.id) ? Font.DemiBold : Font.Normal
                                        color: (helpWin.activeTopicId === modelData.id) ? Theme.accentPrimary : Theme.textPrimary
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text: modelData.category
                                        font.family: Theme.fontSans
                                        font.pixelSize: 10
                                        color: Theme.textMuted
                                    }
                                }

                                MouseArea {
                                    id: topicMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: helpWin.activeTopicId = modelData.id
                                }
                            }
                        }
                    }
                }
            }
        }

        // Right Detail Reading Pane
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.bgCanvas

            ScrollView {
                anchors.fill: parent
                anchors.margins: 24
                clip: true

                ColumnLayout {
                    width: parent.width
                    spacing: 14

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        MonoBadge {
                            text: helpWin.currentTopic ? helpWin.currentTopic.category : "General"
                            tone: "neutral"
                        }
                    }

                    Text {
                        text: helpWin.currentTopic ? helpWin.currentTopic.title : ""
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontXl
                        font.weight: Font.Bold
                        color: Theme.textPrimary
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.borderSubtle
                    }

                    Text {
                        text: helpWin.currentTopic ? helpWin.currentTopic.content : ""
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSm
                        lineHeight: 1.4
                        color: Theme.textPrimary
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }
}
