pragma Singleton
import QtQuick

QtObject {
    id: theme

    // Persistent Light / Dark Monochrome Mode
    property bool isDarkMode: true

    // =========================================================================
    // Core Monochrome Black & White Chrome Tokens (Zero Blue UI Accents)
    // =========================================================================
    readonly property color bgPrimary:      isDarkMode ? "#0A0A0A" : "#FFFFFF"
    readonly property color bgCanvas:       isDarkMode ? "#111111" : "#FAFAFA"
    readonly property color bgSurface:      isDarkMode ? "#171717" : "#F5F5F5"
    readonly property color bgElevated:     isDarkMode ? "#202020" : "#EFEFEF"
    readonly property color bgHover:        isDarkMode ? "#262626" : "#E5E5E5"
    readonly property color bgActive:       isDarkMode ? "#2E2E2E" : "#DCDCDC"
    readonly property color bgInput:        isDarkMode ? "#121212" : "#FFFFFF"

    readonly property color borderSubtle:   isDarkMode ? "#262626" : "#E5E5E5"
    readonly property color borderDefault:  isDarkMode ? "#333333" : "#D4D4D4"
    readonly property color borderStrong:   isDarkMode ? "#525252" : "#A3A3A3"
    readonly property color borderFocus:    isDarkMode ? "#FAFAFA" : "#0A0A0A"

    readonly property color textPrimary:    isDarkMode ? "#FAFAFA" : "#0A0A0A"
    readonly property color textSecondary:  isDarkMode ? "#D4D4D4" : "#262626"
    readonly property color textMuted:      isDarkMode ? "#A3A3A3" : "#525252"
    readonly property color textSubtle:     isDarkMode ? "#737373" : "#737373"
    readonly property color textInverse:    isDarkMode ? "#0A0A0A" : "#FFFFFF"

    // Monochrome Contrast Action Buttons
    readonly property color btnPrimaryBg:   isDarkMode ? "#FAFAFA" : "#0A0A0A"
    readonly property color btnPrimaryHover:isDarkMode ? "#E5E5E5" : "#262626"
    readonly property color btnPrimaryFg:   isDarkMode ? "#0A0A0A" : "#FFFFFF"
    readonly property color btnSecBg:       "transparent"
    readonly property color btnSecHover:    isDarkMode ? "#262626" : "#EFEFEF"
    readonly property color btnSecBorder:   isDarkMode ? "#404040" : "#D4D4D4"

    // Icon hex string without '#' for ShunyaIcon image://icons/<name>/<hex>
    readonly property string iconHexPrimary: isDarkMode ? "FAFAFA" : "0A0A0A"
    readonly property string iconHexMuted:   isDarkMode ? "A3A3A3" : "525252"
    readonly property string iconHexInverse: isDarkMode ? "0A0A0A" : "FFFFFF"

    // =========================================================================
    // Typography Tokens (Unified Modern Geometric Sans & Tabular Mono)
    // =========================================================================
    readonly property string fontSans: Qt.platform.os === "windows" ? "Segoe UI" : "Inter"
    readonly property string fontMono: Qt.platform.os === "windows" ? "Consolas" : "JetBrains Mono"

    readonly property int fontXs:  11
    readonly property int fontSm:  12
    readonly property int fontMd:  13
    readonly property int fontLg:  15
    readonly property int fontXl:  18
    readonly property int font2Xl: 22

    // Spacing & Radius Scale
    readonly property int radiusSm: 4
    readonly property int radiusMd: 6
    readonly property int radiusLg: 8

    // =========================================================================
    // Preserved Scientific Data Visualization & Diagnostic Colors
    // (Reserved strictly for plots, thermal colormaps, periodic categories & alerts)
    // =========================================================================
    readonly property var chartColors: ({
        "voltage":     "#2563EB",
        "current":     "#EA580C",
        "power":       "#9333EA",
        "temperature": "#DC2626",
        "density":     "#0D9488",
        "pressure":    "#CA8A04",
        "soc":         "#16A34A",
        "resistance":  "#0284C7",
        "strain":      "#DB2777",
        "velocity":    "#059669"
    })
    readonly property var seriesPalette: [
        "#2563EB", "#EA580C", "#16A34A", "#DC2626", "#9333EA", "#0D9488", "#CA8A04", "#0284C7"
    ]

    // Scientific Alert & Validation Colors (for data diagnostics/safety banners only)
    readonly property color statusSuccess: isDarkMode ? "#22C55E" : "#15803D"
    readonly property color statusWarning: isDarkMode ? "#F59E0B" : "#B45309"
    readonly property color statusError:   isDarkMode ? "#EF4444" : "#B91C1C"
    readonly property color statusInfo:    isDarkMode ? "#D4D4D4" : "#404040"
    readonly property color gridLine:      isDarkMode ? "#242424" : "#E5E5E5"
    readonly property color axisLine:      isDarkMode ? "#525252" : "#737373"
}
