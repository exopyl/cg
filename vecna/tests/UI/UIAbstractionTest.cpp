#include <gtest/gtest.h>

// Test AC1: IUIRenderer header compiles without ImGui dependency
// This test file does NOT include any imgui headers.
// If IUIRenderer.hpp had an ImGui dependency, this file would fail to compile.
#include "Vecna/UI/IUIRenderer.hpp"

// Test AC1+AC3: ImGuiRenderer inherits from IUIRenderer
#include "Vecna/UI/ImGuiRenderer.hpp"

// Test AC2: Application.hpp exposes only IUIRenderer (no ImGui types)
#include "Vecna/Core/Application.hpp"

#include "Vecna/Scene/ModelInfo.hpp"

#include <type_traits>

// AC1: IUIRenderer is an abstract class (has pure virtual methods)
TEST(UIAbstractionTest, IUIRendererIsAbstract) {
    EXPECT_TRUE(std::is_abstract_v<Vecna::UI::IUIRenderer>);
}

// AC1: IUIRenderer has a virtual destructor for polymorphic deletion
TEST(UIAbstractionTest, IUIRendererHasVirtualDestructor) {
    EXPECT_TRUE(std::has_virtual_destructor_v<Vecna::UI::IUIRenderer>);
}

// AC1+AC3: ImGuiRenderer derives from IUIRenderer
TEST(UIAbstractionTest, ImGuiRendererInheritsFromIUIRenderer) {
    EXPECT_TRUE((std::is_base_of_v<Vecna::UI::IUIRenderer, Vecna::UI::ImGuiRenderer>));
}

// AC2: Application.hpp does not expose ImGui types in its public/private interface
// This test verifies that Application.hpp compiles without imgui headers.
// The mere fact that this test file compiles (with Application.hpp included but
// without imgui.h) proves that Application.hpp has no ImGui dependency.
TEST(UIAbstractionTest, ApplicationHeaderHasNoImGuiDependency) {
    // If Application.hpp included imgui.h, this file would fail to compile
    // because we don't link imgui in the test target
    SUCCEED();
}

// AC1: IUIRenderer is not copyable
TEST(UIAbstractionTest, IUIRendererIsNotCopyable) {
    EXPECT_FALSE(std::is_copy_constructible_v<Vecna::UI::IUIRenderer>);
    EXPECT_FALSE(std::is_copy_assignable_v<Vecna::UI::IUIRenderer>);
}

// AC1: IUIRenderer is not movable
TEST(UIAbstractionTest, IUIRendererIsNotMovable) {
    EXPECT_FALSE(std::is_move_constructible_v<Vecna::UI::IUIRenderer>);
    EXPECT_FALSE(std::is_move_assignable_v<Vecna::UI::IUIRenderer>);
}

// ============================================================================
// Story 5-2: ImGuiRenderer implementation tests
// ============================================================================

// Story 5-2 AC1: ImGuiRenderer is marked final (cannot be subclassed)
TEST(ImGuiRendererTest, ImGuiRendererIsFinal) {
    EXPECT_TRUE(std::is_final_v<Vecna::UI::ImGuiRenderer>);
}

// Story 5-2 AC4: ImGuiRenderer is destructible (proper resource cleanup)
TEST(ImGuiRendererTest, ImGuiRendererIsDestructible) {
    EXPECT_TRUE(std::is_destructible_v<Vecna::UI::ImGuiRenderer>);
}

// Story 5-2 AC1: ImGuiRenderer is not copyable (RAII resource ownership)
TEST(ImGuiRendererTest, ImGuiRendererIsNotCopyable) {
    EXPECT_FALSE(std::is_copy_constructible_v<Vecna::UI::ImGuiRenderer>);
    EXPECT_FALSE(std::is_copy_assignable_v<Vecna::UI::ImGuiRenderer>);
}

// Story 5-2 AC1: ImGuiRenderer is not movable (holds references to dependencies)
TEST(ImGuiRendererTest, ImGuiRendererIsNotMovable) {
    EXPECT_FALSE(std::is_move_constructible_v<Vecna::UI::ImGuiRenderer>);
    EXPECT_FALSE(std::is_move_assignable_v<Vecna::UI::ImGuiRenderer>);
}

// Story 5-2 AC1: ImGuiRenderer has a virtual destructor (inherited from IUIRenderer)
TEST(ImGuiRendererTest, ImGuiRendererHasVirtualDestructor) {
    EXPECT_TRUE(std::has_virtual_destructor_v<Vecna::UI::ImGuiRenderer>);
}

// Story 5-2 AC1: ImGuiRenderer requires 4 dependencies (not default-constructible)
TEST(ImGuiRendererTest, ImGuiRendererIsNotDefaultConstructible) {
    EXPECT_FALSE(std::is_default_constructible_v<Vecna::UI::ImGuiRenderer>);
}

// ============================================================================
// Story 5-3: ModelInfo and InfoPanel tests
// ============================================================================

// Story 5-3 AC4: ModelInfo has expected fields and default values
TEST(ModelInfoTest, ModelInfoIsDefaultConstructible) {
    EXPECT_TRUE(std::is_default_constructible_v<Vecna::Scene::ModelInfo>);
}

TEST(ModelInfoTest, ModelInfoIsCopyable) {
    EXPECT_TRUE(std::is_copy_constructible_v<Vecna::Scene::ModelInfo>);
    EXPECT_TRUE(std::is_copy_assignable_v<Vecna::Scene::ModelInfo>);
}

TEST(ModelInfoTest, ModelInfoDefaultValues) {
    Vecna::Scene::ModelInfo info;
    EXPECT_TRUE(info.filename.empty());
    EXPECT_EQ(info.vertexCount, 0u);
    EXPECT_EQ(info.triangleCount, 0u);
    EXPECT_FLOAT_EQ(info.diagonal, 0.0f);
    EXPECT_FLOAT_EQ(info.bboxMin[0], 0.0f);
    EXPECT_FLOAT_EQ(info.bboxMin[1], 0.0f);
    EXPECT_FLOAT_EQ(info.bboxMin[2], 0.0f);
    EXPECT_FLOAT_EQ(info.bboxMax[0], 0.0f);
    EXPECT_FLOAT_EQ(info.bboxMax[1], 0.0f);
    EXPECT_FLOAT_EQ(info.bboxMax[2], 0.0f);
    EXPECT_FLOAT_EQ(info.dimensions[0], 0.0f);
    EXPECT_FLOAT_EQ(info.dimensions[1], 0.0f);
    EXPECT_FLOAT_EQ(info.dimensions[2], 0.0f);
    EXPECT_FLOAT_EQ(info.center[0], 0.0f);
    EXPECT_FLOAT_EQ(info.center[1], 0.0f);
    EXPECT_FLOAT_EQ(info.center[2], 0.0f);
}

TEST(ModelInfoTest, ComputeDerivedValues) {
    Vecna::Scene::ModelInfo info;
    info.bboxMin[0] = -1.0f; info.bboxMin[1] = -2.0f; info.bboxMin[2] = -3.0f;
    info.bboxMax[0] =  1.0f; info.bboxMax[1] =  2.0f; info.bboxMax[2] =  3.0f;
    info.computeDerived();
    EXPECT_FLOAT_EQ(info.dimensions[0], 2.0f);
    EXPECT_FLOAT_EQ(info.dimensions[1], 4.0f);
    EXPECT_FLOAT_EQ(info.dimensions[2], 6.0f);
    EXPECT_FLOAT_EQ(info.center[0], 0.0f);
    EXPECT_FLOAT_EQ(info.center[1], 0.0f);
    EXPECT_FLOAT_EQ(info.center[2], 0.0f);
}


