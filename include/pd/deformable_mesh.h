#ifndef PD_PD_DEFORMABLE_MESH_H
#define PD_PD_DEFORMABLE_MESH_H

#include "constraint.h"

#include <Eigen/Core>
#include <numeric>

namespace pd {

class deformable_mesh_t
{
  public:
    using positions_type   = Eigen::MatrixXd;
    using masses_type      = Eigen::VectorXd;
    using velocities_type  = Eigen::MatrixX3d;
    using faces_type       = Eigen::MatrixXi;
    using elements_type    = Eigen::MatrixXi;
    using constraints_type = std::vector<std::unique_ptr<constraint_t>>;
    using scalar_type      = typename constraint_t::scalar_type;

  public:
    deformable_mesh_t() = default;

    deformable_mesh_t(
        positions_type const& positions,
        faces_type const& faces,
        elements_type const& elements,
        masses_type const& masses)
        : p0_(positions),
          p_(positions),
          F_(faces),
          E_(elements),
          m_(masses),
          v_(positions.rows(), positions.cols()),
          constraints_{},
          fixed_(positions.rows(), false)
    {
        v_.setZero();
    }

    deformable_mesh_t(
        positions_type const& positions,
        faces_type const& faces,
        elements_type const& elements = elements_type{})
        : p0_(positions),
          p_(positions),
          F_(faces),
          E_(elements),
          m_(positions.rows()),
          v_(positions.rows(), positions.cols()),
          constraints_{},
          fixed_(positions.rows(), false)
    {
        m_.setOnes();
        v_.setZero();
    }

    bool is_fixed(int i) const { return fixed_[i]; }
    void fix(int i)
    {
        fixed_[i] = true;
        m_(i)     = scalar_type{1e10};
    }
    void unfix(int i, scalar_type const mass)
    {
        fixed_[i] = false;
        m_(i)     = mass;
    }
    void toggle_fixed(int i, scalar_type const mass_when_unfixed = 1.0)
    {
        fixed_[i] = !fixed_[i];
        if (fixed_[i])
            m_(i) = scalar_type{1e10};
        else
            m_(i) = mass_when_unfixed;
    }

    positions_type const& positions() const { return p_; }
    faces_type const& faces() const { return F_; }
    elements_type const& elements() const { return E_; }
    constraints_type const& constraints() const { return constraints_; }
    velocities_type const& velocity() const { return v_; }
    masses_type const& mass() const { return m_; }
    std::vector<bool> const& fixed() const { return fixed_; }

    positions_type& positions() { return p_; }
    faces_type& faces() { return F_; }
    elements_type& elements() { return E_; }
    constraints_type& constraints() { return constraints_; }
    velocities_type& velocity() { return v_; }
    masses_type& mass() { return m_; }
    std::vector<bool>& fixed() { return fixed_; }

    void immobilize() { v_.setZero(); }
    void tetrahedralize(Eigen::MatrixXd const& V, Eigen::MatrixXi const& F);
    void set_target_shape();
    void constrain_edge_lengths(scalar_type wi = 1'000'000.);
    void add_positional_constraint(int vi, scalar_type wi = 1'000'000'000.);
    void constrain_deformation_gradient(scalar_type wi = 1'000'000.);
    void constrain_corotated_deformation_gradient(scalar_type wi = 1'000'000.);
    void constrain_shape_targeting(scalar_type wi = 1'000'000.);
    void constrain_strain(scalar_type min, scalar_type max, scalar_type wi = 1'000'000.);

  protected:
    positions_type const& p0() const { return p0_; }

  private:
    positions_type p0_;            ///< Rest positions
    positions_type p_;             ///< Positions
    faces_type F_;                 ///< Faces
    elements_type E_;              ///< Elements
    masses_type m_;                ///< Per-vertex mass_when_unfixed
    velocities_type v_;            ///< Per-vertex velocity
    constraints_type constraints_; ///< PBD constraints
    std::vector<bool> fixed_;      ///< Flags fixed positions
};

} // namespace pd

#endif // PD_PD_DEFORMABLE_MESH_H