#include "geometry/get_simple_bar_model.h"
#include "geometry/get_simple_cloth_model.h"
#include "pd/deformable_mesh.h"
#include "pd/solver.h"
#include "ui/mouse_down_handler.h"
#include "ui/mouse_move_handler.h"
#include "ui/physics_params.h"
#include "ui/picking_state.h"
#include "ui/pre_draw_handler.h"

#include <array>
#include <filesystem>
#include <igl/decimate.h>
#include <igl/file_dialog_open.h>
#include <igl/file_dialog_save.h>
#include <igl/opengl/glfw/Viewer.h>
#include <igl/opengl/glfw/imgui/ImGuiHelpers.h>
#include <igl/opengl/glfw/imgui/ImGuiMenu.h>
#include <igl/readMESH.h>
#include <igl/read_triangle_mesh.h>
#include <igl/writeMESH.h>
#include <igl/write_triangle_mesh.h>

int main(int argc, char** argv)
{
    pd::deformable_mesh_t model{};
    Eigen::MatrixX3d fext;
    ui::picking_state_t picking_state{};
    ui::physics_params_t physics_params{};
    pd::solver_t solver;

    auto const is_model_ready = [&]() {
        return model.positions().rows() > 0;
    };

    igl::opengl::glfw::Viewer viewer;
    viewer.data().point_size   = 10.f;
    viewer.core().is_animating = false;
    viewer.core().rotation_type == igl::opengl::ViewerCore::RotationType::ROTATION_TYPE_TRACKBALL;

    igl::opengl::glfw::imgui::ImGuiMenu menu;
    viewer.plugins.push_back(&menu);

    viewer.callback_mouse_down =
        ui::mouse_down_handler_t{is_model_ready, &picking_state, &solver, &physics_params};

    viewer.callback_mouse_move =
        ui::mouse_move_handler_t{is_model_ready, &picking_state, &model, &fext};

    viewer.callback_mouse_up =
        [&](igl::opengl::glfw::Viewer& viewer, int button, int modifier) -> bool {
        if (picking_state.is_picking)
            picking_state.is_picking = false;

        return false;
    };

    auto const rescale = [](Eigen::MatrixXd& V) {
        Eigen::RowVector3d v_mean = V.colwise().mean();
        V.rowwise() -= v_mean;
        V.array() /= V.maxCoeff() - V.minCoeff();
    };

    auto const reset_simulation_model = [&](Eigen::MatrixXd& V,
                                            Eigen::MatrixXi& F,
                                            Eigen::MatrixXi& T,
                                            bool should_rescale = false) {
        if (should_rescale)
            rescale(V);

        model = pd::deformable_mesh_t{V, F, T};
        solver.set_model(&model);

        fext.resizeLike(model.positions());
        fext.setZero();

        viewer.data().clear();
        viewer.data().set_mesh(model.positions(), model.faces());
        viewer.core().align_camera_center(model.positions());
    };

    menu.callback_draw_viewer_window = [&]() {
        ImGui::SetNextWindowSize(ImVec2(300.0f, 480.0f), ImGuiSetCond_FirstUseEver);
        ImGui::Begin("Projective Dynamics");

        float const w = ImGui::GetContentRegionAvailWidth();
        float const p = ImGui::GetStyle().FramePadding.x;

        if (ImGui::CollapsingHeader("File I/O", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::Button("Load triangle mesh", ImVec2((w - p) / 2.f, 0)))
            {
                std::string const filename = igl::file_dialog_open();
                std::filesystem::path const mesh{filename};
                if (std::filesystem::exists(mesh) && std::filesystem::is_regular_file(mesh))
                {
                    Eigen::MatrixXd V;
                    Eigen::MatrixXi F;
                    if (igl::read_triangle_mesh(mesh.string(), V, F))
                    {
                        reset_simulation_model(V, F, F, true);
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Save triangle mesh", ImVec2((w - p) / 2.f, 0)))
            {
                std::string const filename = igl::file_dialog_save();
                std::filesystem::path const mesh{filename};
                igl::write_triangle_mesh(mesh.string(), model.positions(), model.faces());
            }
            if (ImGui::Button("Load tet mesh", ImVec2((w - p) / 2.f, 0)))
            {
                std::string const filename = igl::file_dialog_open();
                std::filesystem::path const mesh{filename};
                if (std::filesystem::exists(mesh) && std::filesystem::is_regular_file(mesh))
                {
                    Eigen::MatrixXd V;
                    Eigen::MatrixXi T, F;
                    if (igl::readMESH(mesh.string(), V, T, F))
                    {
                        reset_simulation_model(V, F, T, true);
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Save tet mesh", ImVec2((w - p) / 2.f, 0)))
            {
                std::string const filename = igl::file_dialog_save();
                std::filesystem::path const mesh{filename};
                igl::writeMESH(mesh.string(), model.positions(), model.elements(), model.faces());
            }
        }
        if (ImGui::CollapsingHeader("Geometry", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::TreeNode("Triangle##Geometry"))
            {
                if (ImGui::Button("Compute##Triangle", ImVec2((w - p) / 2.f, 0)))
                {
                    Eigen::MatrixXd V;
                    Eigen::MatrixXi F;
                    V.resize(3, 3);
                    V.row(0) = Eigen::RowVector3d{0., 0., 0.};
                    V.row(1) = Eigen::RowVector3d{1., 0., 0.};
                    V.row(2) = Eigen::RowVector3d{0., 1., 0.};

                    F.resize(1, 3);
                    F.row(0) = Eigen::RowVector3i{0, 1, 2};

                    reset_simulation_model(V, F, F, false);
                }
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Bar"))
            {
                static int bar_width  = 12;
                static int bar_height = 4;
                static int bar_depth  = 4;

                ImGui::InputInt("width##Bar", &bar_width);
                ImGui::InputInt("height##Bar", &bar_height);
                ImGui::InputInt("depth##Bar", &bar_depth);

                if (ImGui::Button("Compute##Bar", ImVec2((w - p) / 2.f, 0)))
                {
                    auto [V, T, F] = geometry::get_simple_bar_model(
                        static_cast<std::size_t>(bar_width),
                        static_cast<std::size_t>(bar_height),
                        static_cast<std::size_t>(bar_depth));

                    reset_simulation_model(V, F, T, true);
                }

                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Cloth"))
            {
                static int cloth_width  = 20;
                static int cloth_height = 20;

                ImGui::InputInt("width##Cloth", &cloth_width);
                ImGui::InputInt("height##Cloth", &cloth_height);

                if (ImGui::Button("Compute##Bar", ImVec2((w - p) / 2.f, 0)))
                {
                    auto [V, F] = geometry::get_simple_cloth_model(cloth_width, cloth_height);

                    reset_simulation_model(V, F, F, true);
                }

                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Decimation"))
            {
                static int max_facet_count = 30'000;
                ImGui::InputInt("Max facet count", &max_facet_count);
                if (ImGui::Button("Simplify", ImVec2((w - p) / 2.f, 0)))
                {
                    Eigen::MatrixXd V;
                    Eigen::MatrixXi F;
                    Eigen::VectorXi J;
                    igl::decimate(model.positions(), model.faces(), max_facet_count, V, F, J);
                    reset_simulation_model(V, F, F);
                }
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Tetrahedralization"))
            {
                if (ImGui::Button("Tetrahedralize", ImVec2((w - p) / 2.f, 0)))
                {
                    model.tetrahedralize(model.positions(), model.faces());
                    viewer.data().clear();
                    viewer.data().set_mesh(model.positions(), model.faces());
                    viewer.core().align_camera_center(model.positions());
                }
                ImGui::TreePop();
            }
            std::string const vertex_count  = std::to_string(model.positions().rows());
            std::string const element_count = std::to_string(model.elements().rows());
            std::string const facet_count   = std::to_string(model.faces().rows());
            ImGui::BulletText(std::string("Vertices: " + vertex_count).c_str());
            ImGui::BulletText(std::string("Elements: " + element_count).c_str());
            ImGui::BulletText(std::string("Faces: " + facet_count).c_str());
        }
        if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::TreeNode("Constraints"))
            {
                static std::array<bool, 5u> is_constraint_type_active;
                if (ImGui::TreeNode("Edge length##Constraints"))
                {
                    ImGui::InputFloat(
                        "wi##EdgeLength",
                        &physics_params.edge_constraint_wi,
                        1.f,
                        10.f,
                        "%.1f");
                    ImGui::Checkbox("Active##EdgeLength", &is_constraint_type_active[0]);
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Deformation Gradient##Constraints"))
                {
                    ImGui::BulletText("Valid for tetrahedral models only");
                    ImGui::InputFloat(
                        "wi##DeformationGradient",
                        &physics_params.deformation_gradient_constraint_wi,
                        10.f,
                        100.f,
                        "%.1f");
                    ImGui::Checkbox("Active##DeformationGradient", &is_constraint_type_active[1]);
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Corotated Deformation Gradient##Constraints"))
                {
                    ImGui::BulletText("Valid for tetrahedral models only");
                    ImGui::InputFloat(
                        "wi##CorotatedDeformationGradient",
                        &physics_params.corotated_deformation_gradient_constraint_wi,
                        10.f,
                        100.f,
                        "%.1f");
                    ImGui::Checkbox(
                        "Active##CorotatedDeformationGradient", &is_constraint_type_active[2]);
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Shape Targeting##Constraints"))
                {
                    ImGui::BulletText("Valid for tetrahedral models only");
                    ImGui::InputFloat(
                        "wi##ShapeTargeting", &physics_params.shape_targeting_constraint_wi, 10.f,
                        100.f, "%.1f");
                    ImGui::Checkbox("Active##ShapeTargeting", &is_constraint_type_active[3]);
                    if (ImGui::Button("Set Shape Target", ImVec2((w - p) / 2.f, 0))) {
                        if (is_constraint_type_active[3])
                            model.set_target_shape();
                    }
                    ImGui::TreePop();
                }
                static float sigma_min = 0.99f;
                static float sigma_max = 1.01f;
                if (ImGui::TreeNode("Strain Limit##Constraints"))
                {
                    ImGui::BulletText("Valid for tetrahedral models only");
                    ImGui::InputFloat(
                        "wi##StrainLimit",
                        &physics_params.strain_limit_constraint_wi,
                        10.f,
                        100.f,
                        "%.1f");
                    ImGui::InputFloat(
                        "Minimum singular value##StrainLimit",
                        &sigma_min,
                        0.01f,
                        0.1f);
                    ImGui::InputFloat(
                        "Maximum singular value##StrainLimit",
                        &sigma_max,
                        0.01f,
                        0.1f);
                    ImGui::Checkbox("Active##StrainLimit", &is_constraint_type_active[3]);
                    ImGui::TreePop();
                }

                ImGui::BulletText(
                    "Hold SHIFT and left click points\non the model to fix / unfix them");
                ImGui::BulletText(
                    "Positional constraints are only added\nafter clicking on Apply (constraints)");
                ImGui::InputFloat(
                    "Positional constraint wi",
                    &physics_params.positional_constraint_wi,
                    10.f,
                    100.f,
                    "%.1f");

                if (ImGui::Button("Apply##Constraints", ImVec2((w - p) / 2.f, 0)))
                {
                    model.immobilize();
                    model.constraints().clear();
                    solver.set_dirty();
                    if (is_constraint_type_active[0])
                    {
                        model.constrain_edge_lengths(physics_params.edge_constraint_wi);
                    }
                    if (is_constraint_type_active[1])
                    {
                        model.constrain_deformation_gradient(
                            physics_params.deformation_gradient_constraint_wi);
                    }
                    if (is_constraint_type_active[2])
                    {
                        model.constrain_corotated_deformation_gradient(
                            physics_params.corotated_deformation_gradient_constraint_wi);
                    }
                    if (is_constraint_type_active[3])
                    {
                        model.constrain_shape_targeting(physics_params.shape_targeting_constraint_wi);
                    }
                    if (is_constraint_type_active[4])
                    {
                        model.constrain_strain(
                            sigma_min,
                            sigma_max,
                            physics_params.strain_limit_constraint_wi);
                    }
                }
                std::string const constraint_count = std::to_string(model.constraints().size());
                ImGui::BulletText(std::string("Constraints: " + constraint_count).c_str());
                ImGui::TreePop();
            }
            ImGui::InputFloat("Timestep", &physics_params.dt, 0.01f, 0.1f, "%.4f");
            ImGui::InputInt("Solver iterations", &physics_params.solver_iterations);
            ImGui::InputFloat("mass per particle", &physics_params.mass_per_particle, 1, 10, 1);
            ImGui::Checkbox("Gravity", &physics_params.is_gravity_active);
            ImGui::Checkbox("Simulate", &viewer.core().is_animating);
        }

        if (ImGui::CollapsingHeader("Picking", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::BulletText("Hold SHIFT and left click points\non the model to fix/unfix them");
            ImGui::BulletText(
                "Hold CTRL and hold left mouse\n"
                "button while dragging your\n"
                "mouse to apply external\n"
                "forces to the model");
            ImGui::InputFloat("Dragging force", &picking_state.force, 1.f, 10.f, "%.3f");
        }

        if (ImGui::CollapsingHeader("Visualization", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox(
                "Wireframe",
                [&]() { return viewer.data().show_lines != 0u; },
                [&](bool value) { viewer.data().show_lines = value; });
            ImGui::InputFloat("Point size", &viewer.data().point_size, 1.f, 10.f);
        }
        ImGui::End();
    };

    viewer.callback_pre_draw =
        ui::pre_draw_handler_t{is_model_ready, &physics_params, &solver, &fext};

    viewer.launch();

    return 0;
}