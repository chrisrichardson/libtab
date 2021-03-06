// Copyright (c) 2020 Chris Richardson
// FEniCS Project
// SPDX-License-Identifier:    MIT

#include "quadrature.h"
#include <cmath>
#include <iostream>
#include <vector>

using namespace libtab;

namespace
{
//----------------------------------------------------------------------------
std::tuple<Eigen::ArrayXd, Eigen::ArrayXd> rec_jacobi(int N, double a, double b)
{
  // Generate the recursion coefficients alpha_k, beta_k

  // P_{k+1}(x) = (x-alpha_k)*P_{k}(x) - beta_k P_{k-1}(x)

  // for the Jacobi polynomials which are orthogonal on [-1,1]
  // with respect to the weight w(x)=[(1-x)^a]*[(1+x)^b]

  // Inputs:
  // N - polynomial order
  // a - weight parameter
  // b - weight parameter

  // Outputs:
  // alpha - recursion coefficients
  // beta - recursion coefficients

  // Adapted from the MATLAB code by Dirk Laurie and Walter Gautschi
  // http://www.cs.purdue.edu/archives/2002/wxg/codes/r_jacobi.m

  double nu = (b - a) / (a + b + 2.0);
  double mu = pow(2.0, (a + b + 1)) * tgamma(a + 1.0) * tgamma(b + 1.0)
              / tgamma(a + b + 2.0);

  Eigen::ArrayXd alpha(N), beta(N);

  alpha[0] = nu;
  beta[0] = mu;

  Eigen::ArrayXd n = Eigen::ArrayXd::LinSpaced(N - 1, 1.0, N - 1);
  Eigen::ArrayXd nab = 2.0 * n + a + b;
  alpha.tail(N - 1) = (b * b - a * a) / (nab * (nab + 2.0));
  beta.tail(N - 1) = 4 * (n + a) * (n + b) * n * (n + a + b)
                     / (nab * nab * (nab + 1.0) * (nab - 1.0));

  return {alpha, beta};
}
//----------------------------------------------------------------------------
std::tuple<Eigen::ArrayXd, Eigen::ArrayXd> gauss(const Eigen::ArrayXd& alpha,
                                                 const Eigen::ArrayXd& beta)
{
  // Compute the Gauss nodes and weights from the recursion
  // coefficients associated with a set of orthogonal polynomials
  //
  //  Inputs:
  //  alpha - recursion coefficients
  //  beta - recursion coefficients
  //
  // Outputs:
  // x - quadrature nodes
  // w - quadrature weights
  //
  // Adapted from the MATLAB code by Walter Gautschi
  // http://www.cs.purdue.edu/archives/2002/wxg/codes/gauss.m

  Eigen::MatrixXd A = alpha.matrix().asDiagonal();
  int nb = beta.rows();
  assert(nb == A.cols());
  A.bottomLeftCorner(nb - 1, nb - 1)
      += beta.cwiseSqrt().tail(nb - 1).matrix().asDiagonal();
  Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> solver(
      A, Eigen::DecompositionOptions::ComputeEigenvectors);
  return {solver.eigenvalues(),
          beta[0] * solver.eigenvectors().row(0).array().square()};
}
//----------------------------------------------------------------------------
std::tuple<Eigen::ArrayXd, Eigen::ArrayXd> lobatto(const Eigen::ArrayXd& alpha,
                                                   const Eigen::ArrayXd& beta,
                                                   double xl1, double xl2)
{
  // Compute the Lobatto nodes and weights with the preassigned
  // nodes xl1,xl2
  //
  // Inputs:
  //   alpha - recursion coefficients
  //   beta - recursion coefficients
  //   xl1 - assigned node location
  //   xl2 - assigned node location

  // Outputs:
  // x - quadrature nodes
  // w - quadrature weights

  // Based on the section 7 of the paper
  // "Some modified matrix eigenvalue problems"
  // by Gene Golub, SIAM Review Vol 15, No. 2, April 1973, pp.318--334

  assert(alpha.rows() == beta.rows());
  Eigen::VectorXd bsqrt = beta.cwiseSqrt();

  // Solve tridiagonal system using Thomas algorithm
  double g1 = 0.0;
  double g2 = 0.0;
  const int n = alpha.rows();
  for (int i = 1; i < n - 1; ++i)
  {
    g1 = bsqrt(i) / (alpha(i) - xl1 - bsqrt(i - 1) * g1);
    g2 = bsqrt(i) / (alpha(i) - xl2 - bsqrt(i - 1) * g2);
  }
  g1 = 1.0 / (alpha(n - 1) - xl1 - bsqrt(n - 2) * g1);
  g2 = 1.0 / (alpha(n - 1) - xl2 - bsqrt(n - 2) * g2);

  Eigen::ArrayXd alpha_l = alpha;
  alpha_l(n - 1) = (g1 * xl2 - g2 * xl1) / (g1 - g2);
  Eigen::ArrayXd beta_l = beta;
  beta_l(n - 1) = (xl2 - xl1) / (g1 - g2);

  return gauss(alpha_l, beta_l);
}
}; // namespace

//-----------------------------------------------------------------------------
Eigen::ArrayXXd quadrature::compute_jacobi_deriv(double a, int n, int nderiv,
                                                 const Eigen::ArrayXd& x)
{
  std::vector<Eigen::ArrayXXd> J;
  Eigen::ArrayXXd Jd(n + 1, x.rows());
  for (int i = 0; i < nderiv + 1; ++i)
  {
    if (i == 0)
      Jd.row(0).fill(1.0);
    else
      Jd.row(0).setZero();

    if (n > 0)
    {
      if (i == 0)
        Jd.row(1) = (x.transpose() * (a + 2.0) + a) * 0.5;
      else if (i == 1)
        Jd.row(1) = a * 0.5 + 1;
      else
        Jd.row(1).setZero();
    }

    for (int k = 2; k < n + 1; ++k)
    {
      const double a1 = 2 * k * (k + a) * (2 * k + a - 2);
      const double a2 = (2 * k + a - 1) * (a * a) / a1;
      const double a3 = (2 * k + a - 1) * (2 * k + a) / (2 * k * (k + a));
      const double a4 = 2 * (k + a - 1) * (k - 1) * (2 * k + a) / a1;
      Jd.row(k)
          = Jd.row(k - 1) * (x.transpose() * a3 + a2) - Jd.row(k - 2) * a4;
      if (i > 0)
        Jd.row(k) += i * a3 * J[i - 1].row(k - 1);
    }

    J.push_back(Jd);
  }

  Eigen::ArrayXXd result(nderiv + 1, x.rows());
  for (int i = 0; i < nderiv + 1; ++i)
    result.row(i) = J[i].row(n);

  return result;
}
//-----------------------------------------------------------------------------
Eigen::ArrayXd quadrature::compute_gauss_jacobi_points(double a, int m)
{
  /// Computes the m roots of \f$P_{m}^{a,0}\f$ on [-1,1] by Newton's method.
  ///    The initial guesses are the Chebyshev points.  Algorithm
  ///    implemented from the pseudocode given by Karniadakis and
  ///    Sherwin

  const double eps = 1.e-8;
  const int max_iter = 100;
  Eigen::ArrayXd x(m);
  for (int k = 0; k < m; ++k)
  {
    // Initial guess
    x[k] = -cos((2.0 * k + 1.0) * M_PI / (2.0 * m));
    if (k > 0)
      x[k] = 0.5 * (x[k] + x[k - 1]);

    int j = 0;
    while (j < max_iter)
    {
      double s = 0;
      for (int i = 0; i < k; ++i)
        s += 1.0 / (x[k] - x[i]);
      const Eigen::ArrayXd f
          = quadrature::compute_jacobi_deriv(a, m, 1, x.row(k));
      const double delta = f[0] / (f[1] - f[0] * s);
      x[k] -= delta;

      if (std::abs(delta) < eps)
        break;
      ++j;
    }
  }

  return x;
}
//-----------------------------------------------------------------------------
std::pair<Eigen::ArrayXd, Eigen::ArrayXd>
quadrature::compute_gauss_jacobi_rule(double a, int m)
{
  /// @note Computes on [-1, 1]
  const Eigen::ArrayXd pts = quadrature::compute_gauss_jacobi_points(a, m);
  const Eigen::ArrayXd Jd
      = quadrature::compute_jacobi_deriv(a, m, 1, pts).row(1);

  const double a1 = pow(2.0, a + 1.0);
  const double a3 = tgamma(m + 1.0);
  // factorial(m)
  double a5 = 1.0;
  for (int i = 0; i < m; ++i)
    a5 *= (i + 1);
  const double a6 = a1 * a3 / a5;

  Eigen::ArrayXd wts(m);
  for (int i = 0; i < m; ++i)
  {
    const double x = pts[i];
    const double f = Jd[i];
    wts[i] = a6 / (1.0 - x * x) / (f * f);
  }

  return {pts, wts};
}
//-----------------------------------------------------------------------------
std::pair<Eigen::ArrayXd, Eigen::ArrayXd>
quadrature::make_quadrature_line(int m)
{
  auto [ptx, wx] = quadrature::compute_gauss_jacobi_rule(0.0, m);
  return {0.5 * (ptx + 1.0), wx * 0.5};
}
//-----------------------------------------------------------------------------
std::pair<Eigen::ArrayX2d, Eigen::ArrayXd>
quadrature::make_quadrature_triangle_collapsed(int m)
{
  auto [ptx, wx] = quadrature::compute_gauss_jacobi_rule(0.0, m);
  auto [pty, wy] = quadrature::compute_gauss_jacobi_rule(1.0, m);

  Eigen::ArrayX2d pts(m * m, 2);
  Eigen::ArrayXd wts(m * m);
  int c = 0;
  for (int i = 0; i < m; ++i)
  {
    for (int j = 0; j < m; ++j)
    {
      pts(c, 0) = 0.25 * (1.0 + ptx[i]) * (1.0 - pty[j]);
      pts(c, 1) = 0.5 * (1.0 + pty[j]);
      wts[c] = wx[i] * wy[j] * 0.125;
      ++c;
    }
  }

  return {pts, wts};
}
//-----------------------------------------------------------------------------
std::pair<Eigen::ArrayX3d, Eigen::ArrayXd>
quadrature::make_quadrature_tetrahedron_collapsed(int m)
{
  auto [ptx, wx] = quadrature::compute_gauss_jacobi_rule(0.0, m);
  auto [pty, wy] = quadrature::compute_gauss_jacobi_rule(1.0, m);
  auto [ptz, wz] = quadrature::compute_gauss_jacobi_rule(2.0, m);

  Eigen::ArrayX3d pts(m * m * m, 3);
  Eigen::ArrayXd wts(m * m * m);
  int c = 0;
  for (int i = 0; i < m; ++i)
  {
    for (int j = 0; j < m; ++j)
    {
      for (int k = 0; k < m; ++k)
      {
        pts(c, 0) = 0.125 * (1.0 + ptx[i]) * (1.0 - pty[j]) * (1.0 - ptz[k]);
        pts(c, 1) = 0.25 * (1. + pty[j]) * (1. - ptz[k]);
        pts(c, 2) = 0.5 * (1.0 + ptz[k]);
        wts[c] = wx[i] * wy[j] * wz[k] * 0.125 * 0.125;
        ++c;
      }
    }
  }

  return {pts, wts};
}
//-----------------------------------------------------------------------------
std::pair<Eigen::ArrayXXd, Eigen::ArrayXd>
quadrature::make_quadrature(cell::type celltype, int m)
{
  switch (celltype)
  {
  case cell::type::interval:
    return quadrature::make_quadrature_line(m);
  case cell::type::quadrilateral:
  {
    auto [QptsL, QwtsL] = quadrature::make_quadrature_line(m);
    Eigen::ArrayX2d Qpts(m * m, 2);
    Eigen::ArrayXd Qwts(m * m);
    int c = 0;
    for (int j = 0; j < m; ++j)
    {
      for (int i = 0; i < m; ++i)
      {
        Qpts.row(c) << QptsL(i, 0), QptsL(j, 0);
        Qwts[c] = QwtsL[i] * QwtsL[j];
        ++c;
      }
    }
    return {Qpts, Qwts};
  }
  case cell::type::hexahedron:
  {
    auto [QptsL, QwtsL] = quadrature::make_quadrature_line(m);
    Eigen::ArrayX3d Qpts(m * m * m, 3);
    Eigen::ArrayXd Qwts(m * m * m);
    int c = 0;
    for (int k = 0; k < m; ++k)
    {
      for (int j = 0; j < m; ++j)
      {
        for (int i = 0; i < m; ++i)
        {
          Qpts.row(c) << QptsL(i, 0), QptsL(j, 0), QptsL(k, 0);
          Qwts[c] = QwtsL[i] * QwtsL[j] * QwtsL[k];
          ++c;
        }
      }
    }
    return {Qpts, Qwts};
  }
  case cell::type::prism:
  {
    auto [QptsL, QwtsL] = quadrature::make_quadrature_line(m);
    auto [QptsT, QwtsT] = quadrature::make_quadrature_triangle_collapsed(m);
    Eigen::ArrayX3d Qpts(m * QptsT.rows(), 3);
    Eigen::ArrayXd Qwts(m * QptsT.rows());
    int c = 0;
    for (int k = 0; k < m; ++k)
    {
      for (int i = 0; i < QptsT.rows(); ++i)
      {
        Qpts.row(c) << QptsT(i, 0), QptsT(i, 1), QptsL(k, 0);
        Qwts[c] = QwtsT[i] * QwtsL[k];
        ++c;
      }
    }
    return {Qpts, Qwts};
  }
  case cell::type::pyramid:
    throw std::runtime_error("Pyramid not yet supported");
  case cell::type::triangle:
    return quadrature::make_quadrature_triangle_collapsed(m);
  case cell::type::tetrahedron:
    return quadrature::make_quadrature_tetrahedron_collapsed(m);
  default:
    throw std::runtime_error("Unsupported celltype for make_quadrature");
  }
}
//----------------------------------------------------------------------------
std::pair<Eigen::ArrayXXd, Eigen::ArrayXd>
quadrature::make_quadrature(const Eigen::ArrayXXd& simplex, int m)
{
  const int dim = simplex.rows() - 1;
  if (dim < 1 or dim > 3)
    throw std::runtime_error("Unsupported dim");
  if (simplex.cols() < dim)
    throw std::runtime_error("Invalid simplex");

  // Compute edge vectors of simplex
  Eigen::MatrixXd bvec(dim, simplex.cols());
  for (int i = 0; i < dim; ++i)
    bvec.row(i) = simplex.row(i + 1) - simplex.row(0);

  Eigen::ArrayXXd Qpts;
  Eigen::ArrayXd Qwts;
  double scale = 1.0;
  if (dim == 1)
  {
    std::tie(Qpts, Qwts) = quadrature::make_quadrature_line(m);
    scale = bvec.norm();
  }
  else if (dim == 2)
  {
    std::tie(Qpts, Qwts) = quadrature::make_quadrature_triangle_collapsed(m);
    if (bvec.cols() == 2)
      scale = bvec.determinant();
    else
    {
      Eigen::Vector3d a = bvec.row(0);
      Eigen::Vector3d b = bvec.row(1);
      scale = a.cross(b).norm();
    }
  }
  else
  {
    std::tie(Qpts, Qwts) = quadrature::make_quadrature_tetrahedron_collapsed(m);
    assert(bvec.cols() == 3);
    scale = bvec.determinant();
  }

#ifndef NDEBUG
  std::cout << "vecs = \n[" << bvec << "]\n";
  std::cout << "scale = " << scale << "\n";
#endif

  Eigen::ArrayXXd Qpts_scaled(Qpts.rows(), bvec.cols());
  Eigen::ArrayXd Qwts_scaled = Qwts * scale;
  for (int i = 0; i < Qpts.rows(); ++i)
    Qpts_scaled.row(i) = simplex.row(0) + (Qpts.row(i).matrix() * bvec).array();

  return {Qpts_scaled, Qwts_scaled};
}
//-----------------------------------------------------------------------------
std::tuple<Eigen::ArrayXd, Eigen::ArrayXd>
quadrature::gauss_lobatto_legendre_line_rule(int m)
{
  // Implement the Gauss-Lobatto-Legendre quadrature rules on the interval
  // using Greg von Winckel's implementation. This facilitates implementing
  // spectral elements.
  // The quadrature rule uses m points for a degree of precision of 2m-3.

  if (m < 2)
  {
    throw std::runtime_error(
        "Gauss-Labotto-Legendre quadrature invalid for fewer than 2 points");
  }

  // Calculate the recursion coefficients
  auto [alpha, beta] = rec_jacobi(m, 0, 0);

  // Compute Lobatto nodes and weights
  auto [xs_ref, ws_ref] = lobatto(alpha, beta, -1.0, 1.0);

  // TODO: scaling

  return {xs_ref, ws_ref};
}
//-----------------------------------------------------------------------------
