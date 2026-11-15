import QtQuick

QtObject {
    id: root
    property bool dark: true

    // Surface hierarchy (smooth ColorAnimation on theme toggle)
    property color bgCanvas: dark ? "#101319" : "#f4f6fb"
    property color bgSidebar: dark ? "#161a23" : "#ebeff6"
    property color bgCard: dark ? "#1c212c" : "#ffffff"
    property color bgCardHover: dark ? "#222836" : "#f8faff"
    property color bgSubtle: dark ? "#141821" : "#f1f4f9"
    property color bgCode: dark ? "#0d1016" : "#eff3f9"
    property color bgInput: dark ? "#141820" : "#ffffff"

    // Borders & shadows
    property color border: dark ? "#2a3142" : "#dce3ee"
    property color borderStrong: dark ? "#3b455c" : "#bcc7d8"
    property color shadowColor: dark ? "#66000000" : "#220e172a"

    // Typography
    property color textPrimary: dark ? "#f1f4fa" : "#141924"
    property color textSecondary: dark ? "#a5b0c4" : "#505d72"
    property color textMuted: dark ? "#737e95" : "#7c889d"

    // Brand Accent (Windows 11 Fluent Azure)
    readonly property color accent: dark ? "#4da3ff" : "#0067c0"
    readonly property color accentHover: dark ? "#6bb3ff" : "#0058a6"
    readonly property color accentPressed: dark ? "#388ceb" : "#004a8c"
    readonly property color accentSoft: dark ? "#244da3ff" : "#160067c0"
    readonly property color accentBorder: dark ? "#664da3ff" : "#550067c0"
    readonly property color accentOn: "#ffffff"

    // Semantics
    readonly property color successFg: dark ? "#4ade80" : "#15803d"
    readonly property color successBg: dark ? "#2022c55e" : "#1616a34a"
    readonly property color successBd: dark ? "#5222c55e" : "#4416a34a"

    readonly property color warningFg: dark ? "#fbbf24" : "#b45309"
    readonly property color warningBg: dark ? "#20f59e0b" : "#16d97706"
    readonly property color warningBd: dark ? "#52f59e0b" : "#44d97706"

    readonly property color dangerFg: dark ? "#f87171" : "#b91c1c"
    readonly property color dangerBg: dark ? "#20ef4444" : "#16dc2626"
    readonly property color dangerBd: dark ? "#52ef4444" : "#44dc2626"

    readonly property color infoFg: dark ? "#60a5fa" : "#0369a1"
    readonly property color infoBg: dark ? "#203b82f6" : "#140284c7"
    readonly property color infoBd: dark ? "#523b82f6" : "#3d0284c7"

    // Clean 24x24 SVG Icon Paths
    readonly property string icAddLink: "M3.9 12c0-1.71 1.39-3.1 3.1-3.1h4V7H7c-2.76 0-5 2.24-5 5s2.24 5 5 5h4v-1.9H7c-1.71 0-3.1-1.39-3.1-3.1zM8 13h8v-2H8v2zm9-6h-4v1.9h4c1.71 0 3.1 1.39 3.1 3.1s-1.39 3.1-3.1 3.1h-4V17h4c2.76 0 5-2.24 5-5s-2.24-5-5-5z"
    readonly property string icSearch: "M15.5 14h-.79l-.28-.27A6.471 6.471 0 0 0 16 9.5 6.5 6.5 0 1 0 9.5 16c1.61 0 3.09-.59 4.23-1.57l.27.28v.79l5 4.99L20.49 19l-4.99-5zm-6 0C7.01 14 5 11.99 5 9.5S7.01 5 9.5 5 14 7.01 14 9.5 11.99 14 9.5 14z"
    readonly property string icHistory: "M13 3a9 9 0 0 0-9 9H1l3.89 3.89.07.14L9 12H6c0-3.87 3.13-7 7-7s7 3.13 7 7-3.13 7-7 7c-1.93 0-3.68-.79-4.94-2.06l-1.42 1.42A8.954 8.954 0 0 0 13 21a9 9 0 0 0 0-18zm-1 5v5l4.28 2.54.72-1.21-3.5-2.08V8H12z"
    readonly property string icShield: "M12 1L3 5v6c0 5.55 3.84 10.74 9 12 5.16-1.26 9-6.45 9-12V5l-9-4zm-1 14l-3.5-3.5 1.41-1.41L11 12.17l4.59-4.58L17 9l-6 6z"
    readonly property string icSun: "M12 7c-2.76 0-5 2.24-5 5s2.24 5 5 5 5-2.24 5-5-2.24-5-5-5zM2 13h2c.55 0 1-.45 1-1s-.45-1-1-1H2c-.55 0-1 .45-1 1s.45 1 1 1zm18 0h2c.55 0 1-.45 1-1s-.45-1-1-1h-2c-.55 0-1 .45-1 1s.45 1 1 1zM11 2v2c0 .55.45 1 1 1s1-.45 1-1V2c0-.55-.45-1-1-1s-1 .45-1 1zm0 18v2c0 .55.45 1 1 1s1-.45 1-1v-2c0-.55-.45-1-1-1s-1 .45-1 1zM5.99 4.58a.996.996 0 0 0-1.41 0 .996.996 0 0 0 0 1.41l1.06 1.06c.39.39 1.03.39 1.41 0s.39-1.03 0-1.41L5.99 4.58zm12.37 12.37a.996.996 0 0 0-1.41 0 .996.996 0 0 0 0 1.41l1.06 1.06c.39.39 1.03.39 1.41 0a.996.996 0 0 0 0-1.41l-1.06-1.06zm1.06-10.96a.996.996 0 0 0 0-1.41.996.996 0 0 0-1.41 0l-1.06 1.06c-.39.39-.39 1.03 0 1.41s1.03.39 1.41 0l1.06-1.06zM7.05 18.36a.996.996 0 0 0 0-1.41.996.996 0 0 0-1.41 0l-1.06 1.06c-.39.39-.39 1.03 0 1.41s1.03.39 1.41 0l1.06-1.06z"
    readonly property string icMoon: "M12 3a9 9 0 1 0 9 9c0-.46-.04-.92-.1-1.36a5.389 5.389 0 0 1-4.4 2.26 5.403 5.403 0 0 1-3.14-9.8c-.44-.06-.9-.1-1.36-.1z"
    readonly property string icSwap: "M6.99 11L3 15l3.99 4v-3H14v-2H6.99v-3zM21 9l-3.99-4v3H10v2h7.01v3L21 9z"
    readonly property string icCheck: "M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-2 15l-5-5 1.41-1.41L10 14.17l7.59-7.59L19 8l-9 9z"
    readonly property string icWarn: "M1 21h22L12 2 1 21zm12-3h-2v-2h2v2zm0-4h-2v-4h2v4z"
    readonly property string icCopy: "M16 1H4c-1.1 0-2 .9-2 2v14h2V3h12V1zm3 4H8c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h11c1.1 0 2-.9 2-2V7c0-1.1-.9-2-2-2zm0 16H8V7h11v14z"

    Behavior on bgCanvas { ColorAnimation { duration: 220; easing.type: Easing.OutQuad } }
    Behavior on bgSidebar { ColorAnimation { duration: 220; easing.type: Easing.OutQuad } }
    Behavior on bgCard { ColorAnimation { duration: 220; easing.type: Easing.OutQuad } }
    Behavior on bgCardHover { ColorAnimation { duration: 220; easing.type: Easing.OutQuad } }
    Behavior on bgSubtle { ColorAnimation { duration: 220; easing.type: Easing.OutQuad } }
    Behavior on bgCode { ColorAnimation { duration: 220; easing.type: Easing.OutQuad } }
    Behavior on bgInput { ColorAnimation { duration: 220; easing.type: Easing.OutQuad } }
    Behavior on border { ColorAnimation { duration: 220; easing.type: Easing.OutQuad } }
    Behavior on textPrimary { ColorAnimation { duration: 220; easing.type: Easing.OutQuad } }
    Behavior on textSecondary { ColorAnimation { duration: 220; easing.type: Easing.OutQuad } }
    Behavior on textMuted { ColorAnimation { duration: 220; easing.type: Easing.OutQuad } }
}
