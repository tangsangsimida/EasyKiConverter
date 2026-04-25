import QtQuick
import QtTest
import "../../src/ui/qml/styles"

TestCase {
    name: "ThemeBinding"
    width: 400
    height: 400
    visible: true
    when: windowShown

    function test_darkModeToggle() {
        // Store original state
        var originalMode = AppStyle.isDarkMode

        // Verify known dark mode values
        AppStyle.isDarkMode = true
        compare(AppStyle.colors.background.toString().toUpperCase(), "#0F172A", "Dark mode background should be correct")
        compare(AppStyle.colors.surface.toString().toUpperCase(), "#1E293B", "Dark mode surface should be correct")

        // Verify known light mode values
        AppStyle.isDarkMode = false
        compare(AppStyle.colors.background.toString().toUpperCase(), "#F8FAFC", "Light mode background should be correct")
        compare(AppStyle.colors.surface.toString().toUpperCase(), "#FFFFFF", "Light mode surface should be correct")

        // Restore original state
        AppStyle.isDarkMode = originalMode
    }

    function test_colorConsistency() {
        // Ensure all required color tokens exist
        verify(AppStyle.colors.primary !== undefined, "Primary color should exist")
        verify(AppStyle.colors.success !== undefined, "Success color should exist")
        verify(AppStyle.colors.danger !== undefined, "Danger color should exist")
        verify(AppStyle.colors.warning !== undefined, "Warning color should exist")
        verify(AppStyle.colors.textPrimary !== undefined, "Text primary color should exist")
        verify(AppStyle.colors.textSecondary !== undefined, "Text secondary color should exist")
        verify(AppStyle.colors.border !== undefined, "Border color should exist")
    }

    function test_durationTokens() {
        verify(AppStyle.durations.fast > 0, "Fast duration should be positive")
        verify(AppStyle.durations.normal > AppStyle.durations.fast, "Normal duration should be longer than fast")
        verify(AppStyle.durations.slow > AppStyle.durations.normal, "Slow duration should be longer than normal")
    }
}
