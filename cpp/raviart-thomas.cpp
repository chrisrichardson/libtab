// Copyright (c) 2020 Chris Richardson
// FEniCS Project
// SPDX-License-Identifier:    MIT

#include "raviart-thomas.h"
#include "dof-permutations.h"
#include "lagrange.h"
#include "moments.h"
#include "polyset.h"
#include "quadrature.h"
#include <Eigen/Dense>
#include <numeric>
#include <vector>

using namespace libtab;

//----------------------------------------------------------------------------
FiniteElement libtab::create_rt(cell::type celltype, int degree,
                                const std::string& name)
{
  if (celltype != cell::type::triangle and celltype != cell::type::tetrahedron)
    throw std::runtime_error("Unsupported cell type");

  const int tdim = cell::topological_dimension(celltype);

  const cell::type facettype
      = (tdim == 2) ? cell::type::interval : cell::type::triangle;

  // The number of order (degree-1) scalar polynomials
  const int nv = polyset::dim(celltype, degree - 1);
  // The number of order (degree-2) scalar polynomials
  const int ns0 = polyset::dim(celltype, degree - 2);
  // The number of additional polnomials in the polynomial basis for
  // Raviart-Thomas
  const int ns = polyset::dim(facettype, degree - 1);

  // Evaluate the expansion polynomials at the quadrature points
  auto [Qpts, Qwts] = quadrature::make_quadrature(celltype, 2 * degree);
  Eigen::ArrayXXd Pkp1_at_Qpts
      = polyset::tabulate(celltype, degree, 0, Qpts)[0];

  // The number of order (degree) polynomials
  const int psize = Pkp1_at_Qpts.cols();

  // Create coefficients for order (degree-1) vector polynomials
  Eigen::MatrixXd wcoeffs = Eigen::MatrixXd::Zero(nv * tdim + ns, psize * tdim);
  for (int j = 0; j < tdim; ++j)
  {
    wcoeffs.block(nv * j, psize * j, nv, nv)
        = Eigen::MatrixXd::Identity(nv, nv);
  }

  // Create coefficients for additional polynomials in Raviart-Thomas
  // polynomial basis
  for (int i = 0; i < ns; ++i)
  {
    for (int k = 0; k < psize; ++k)
    {
      for (int j = 0; j < tdim; ++j)
      {
        const double w_sum = (Qwts * Pkp1_at_Qpts.col(ns0 + i) * Qpts.col(j)
                              * Pkp1_at_Qpts.col(k))
                                 .sum();
        wcoeffs(nv * tdim + i, k + psize * j) = w_sum;
      }
    }
  }

  // Dual space
  Eigen::MatrixXd dual = Eigen::MatrixXd::Zero(nv * tdim + ns, psize * tdim);

  // quadrature degree
  int quad_deg = 5 * degree;

  // Add rows to dualmat for integral moments on facets
  const int facet_count = tdim + 1;
  const int facet_dofs = ns;
  dual.block(0, 0, facet_count * facet_dofs, psize * tdim)
      = moments::make_normal_integral_moments(
          create_dlagrange(facettype, degree - 1), celltype, tdim, degree,
          quad_deg);

  // Add rows to dualmat for integral moments on interior
  if (degree > 1)
  {
    const int internal_dofs = tdim * ns0;
    // Interior integral moment
    dual.block(facet_count * facet_dofs, 0, internal_dofs, psize * tdim)
        = moments::make_integral_moments(create_dlagrange(celltype, degree - 2),
                                         celltype, tdim, degree, quad_deg);
  }

  const std::vector<std::vector<std::vector<int>>> topology
      = cell::topology(celltype);

  const int ndofs = dual.rows();
  int perm_count = 0;
  for (int i = 1; i < tdim; ++i)
    perm_count += topology[i].size() * i;

  std::vector<Eigen::MatrixXd> base_permutations(
      perm_count, Eigen::MatrixXd::Identity(ndofs, ndofs));
  if (tdim == 2)
  {
    Eigen::ArrayXi edge_ref = dofperms::interval_reflection(degree - 1);
    for (int edge = 0; edge < facet_count; ++edge)
    {
      const int start = edge_ref.size() * edge;
      for (int i = 0; i < edge_ref.size(); ++i)
      {
        base_permutations[edge](start + i, start + i) = 0;
        base_permutations[edge](start + i, start + edge_ref[i]) = -1;
      }
    }
  }
  else if (tdim == 3)
  {
    Eigen::ArrayXi face_ref = dofperms::triangle_reflection(degree - 1);
    Eigen::ArrayXi face_rot = dofperms::triangle_rotation(degree - 1);

    for (int face = 0; face < facet_count; ++face)
    {
      const int start = face_ref.size() * face;
      for (int i = 0; i < face_rot.size(); ++i)
      {
        base_permutations[2 * face](start + i, start + i) = 0;
        base_permutations[2 * face](start + i, start + face_rot[i]) = 1;
        base_permutations[2 * face + 1](start + i, start + i) = 0;
        base_permutations[2 * face + 1](start + i, start + face_ref[i]) = -1;
      }
    }
  }

  // Raviart-Thomas has ns dofs on each facet, and ns0*tdim in the interior
  std::vector<std::vector<int>> entity_dofs(topology.size());
  for (int i = 0; i < tdim - 1; ++i)
    entity_dofs[i].resize(topology[i].size(), 0);
  entity_dofs[tdim - 1].resize(topology[tdim - 1].size(), ns);
  entity_dofs[tdim] = {ns0 * tdim};

  Eigen::MatrixXd coeffs = compute_expansion_coefficients(wcoeffs, dual);
  return FiniteElement(name, celltype, degree, {tdim}, coeffs, entity_dofs,
                       base_permutations);
}
//-----------------------------------------------------------------------------
