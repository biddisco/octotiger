/*
 * grid.cpp
 *
 *  Created on: May 26, 2015
 *      Author: dmarce1
 */

#include "grid.hpp"
#include "problem.hpp"
#include "options.hpp"
#include <cmath>
#include <cassert>

extern options opts;

real grid::omega = ZERO;
real grid::omega_dot = ZERO;
space_vector grid::pivot(ZERO);
real grid::scaling_factor = 1.0;

integer grid::max_level = 0;

line_of_centers_t grid::line_of_centers(const std::pair<space_vector, space_vector>& line) {
	line_of_centers_t loc;
	for (integer i = H_BW; i != H_NX - H_BW; ++i) {
		for (integer j = H_BW; j != H_NX - H_BW; ++j) {
			for (integer k = H_BW; k != H_NX - H_BW; ++k) {
				const integer iii = hindex(i, j, k);
				const integer iiig = gindex(i+G_BW-H_BW, j+G_BW-H_BW, k+G_BW-H_BW);
				space_vector a = line.first;
				const space_vector& o = line.second;
				space_vector b;
				real aa = 0.0;
				real bb = 0.0;
				real ab = 0.0;
				for (integer d = 0; d != NDIM; ++d) {
					a[d] -= o[d];
					b[d] = X[d][iii] - o[d];
				}
				for (integer d = 0; d != NDIM; ++d) {
					aa += a[d] * a[d];
					bb += b[d] * b[d];
					ab += a[d] * b[d];
				}
				const real d = std::sqrt((aa * bb - ab * ab) / aa);
				real p = ab / std::sqrt(aa);
				std::vector<real> data(NF+NGF);
				if (d < std::sqrt(3.0) * dx / 2.0) {
					for (integer ui = 0; ui != NF; ++ui) {
						data[ui] = U[ui][iii];
					}
					for (integer gi = 0; gi != NGF; ++gi) {
						data[NF+gi] = G[gi][iiig];
					}
					loc.resize(loc.size() + 1);
					loc[loc.size() - 1].first = p;
					loc[loc.size() - 1].second = std::move(data);
				}
			}
		}
	}
	return loc;
}

std::pair<std::vector<real>, std::vector<real>> grid::diagnostic_error() const {
	std::pair<std::vector<real>, std::vector<real>> e;
	const real dV = dx * dx * dx;
	if (opts.problem == SOLID_SPHERE) {
		e.first.resize(8, ZERO);
		e.second.resize(8, ZERO);
	}
	for (integer i = G_BW; i != G_NX - G_BW; ++i) {
		for (integer j = G_BW; j != G_NX - G_BW; ++j) {
			for (integer k = G_BW; k != G_NX - G_BW; ++k) {
				const integer iii = gindex(i, j, k);
				const integer bdif = H_BW - G_BW;
				const integer iiih = hindex(i + bdif, j + bdif, k + bdif);
				const real x = X[XDIM][iiih];
				const real y = X[YDIM][iiih];
				const real z = X[ZDIM][iiih];
				if (opts.problem == SOLID_SPHERE) {
					const auto a = solid_sphere_analytic_phi(x, y, z, 0.0);
					std::vector<real> n(4);
					n[phi_i] = G[phi_i][iii];
					n[gx_i] = G[gx_i][iii];
					n[gy_i] = G[gy_i][iii];
					n[gz_i] = G[gz_i][iii];
					const real rho = U[rho_i][iiih];
					for (integer l = 0; l != 4; ++l) {
						e.first[l] += std::abs(a[l] - n[l]) * dV * rho;
						e.first[4 + l] += std::abs(a[l]) * dV * rho;
						e.second[l] += std::pow((a[l] - n[l]) * rho, 2) * dV;
						e.second[4 + l] += std::pow(a[l] * rho, 2) * dV;
					}
				}
			}
		}
	}
//	printf("%e\n", e[0]);
	return e;
}

void grid::set_omega(real o) {
	omega = o;
}


void grid::set_omega_dot(real o) {
	omega_dot = o;
}

real grid::get_omega()  {
	return omega;
}

real grid::get_omega_dot() {
	return omega_dot;
}

void grid::velocity_inc(const space_vector& dv) {

	for (integer iii = 0; iii != H_N3; ++iii) {
		const real rho = U[rho_i][iii];
		if (rho != ZERO) {
			const real rhoinv = ONE / rho;
			real& sx = U[sx_i][iii];
			real& sy = U[sy_i][iii];
			real& sz = U[sz_i][iii];
			real& egas = U[egas_i][iii];
			egas -= HALF * (sx * sx + sy * sy + sz * sz) * rhoinv;
			sx += dv[XDIM] * rho;
			sy += dv[YDIM] * rho;
			sz += dv[ZDIM] * rho;
			egas += HALF * (sx * sx + sy * sy + sz * sz) * rhoinv;
		}
	}

}

void grid::set_pivot(const space_vector& p) {
	pivot = p;
}

inline real minmod(real a, real b) {
	return (std::copysign(HALF, a) + std::copysign(HALF, b)) * std::min(std::abs(a), std::abs(b));
}

inline real minmod_theta(real a, real b, real c, real theta) {
	return minmod(theta * minmod(a, b), c);
}

inline real minmod_theta(real a, real b, real theta = 1.0) {
	return minmod(theta * minmod(a, b), HALF * (a + b));
}

std::pair<real, real> grid::omega_part(const space_vector& pivot) const {
	real I = ZERO;
	real L = ZERO;
	const real dV = dx * dx * dx;
	for (integer i = H_BW; i != H_NX - H_BW; ++i) {
		for (integer j = H_BW; j != H_NX - H_BW; ++j) {
			for (integer k = H_BW; k != H_NX - H_BW; ++k) {
				const integer iii = hindex(i, j, k);
				const real x = X[XDIM][iii] - pivot[XDIM];
				const real y = X[YDIM][iii] - pivot[YDIM];
				const real sx = U[sx_i][iii];
				const real sy = U[sy_i][iii];
				const real zz = U[zz_i][iii];
				const real rho = U[rho_i][iii];
				L += (x * sy - y * sx + zz) * dV;
				I += (x * x + y * y) * rho * dV;
			}
		}
	}
//	printf( "%e\n", L);
	return std::make_pair(L, I);
}

std::vector<real> grid::get_flux_restrict(const std::array<integer, NDIM>& lb, const std::array<integer, NDIM>& ub,
		const geo::dimension& dim) const {
	std::vector<real> data;
	integer size = 1;
	for (auto& dim : geo::dimension::full_set()) {
		size *= (ub[dim] - lb[dim]);
	}
	size /= (NCHILD / 2);
	size *= NF;
	data.reserve(size);
	const integer stride1 = (dim == XDIM) ? H_DNY : H_DNX;
	const integer stride2 = (dim == ZDIM) ? H_DNY : H_DNZ;
	for (integer field = 0; field != NF; ++field) {
		for (integer i = lb[XDIM]; i < ub[XDIM]; i += 2) {
			for (integer j = lb[YDIM]; j < ub[YDIM]; j += 2) {
				for (integer k = lb[ZDIM]; k < ub[ZDIM]; k += 2) {
					const integer i00 = hindex(i, j, k);
					const integer i10 = i00 + stride1;
					const integer i01 = i00 + stride2;
					const integer i11 = i00 + stride1 + stride2;
					real value = ZERO;
					value += F[dim][field][i00];
					value += F[dim][field][i10];
					value += F[dim][field][i01];
					value += F[dim][field][i11];
					const real f = dx / TWO;
					if (field == zx_i) {
						if (dim == YDIM) {
							value += F[dim][sy_i][i00] * f;
							value += F[dim][sy_i][i10] * f;
							value -= F[dim][sy_i][i01] * f;
							value -= F[dim][sy_i][i11] * f;
						} else if (dim == ZDIM) {
							value -= F[dim][sz_i][i00] * f;
							value -= F[dim][sz_i][i10] * f;
							value += F[dim][sz_i][i01] * f;
							value += F[dim][sz_i][i11] * f;
						} else if (dim == XDIM) {
							value += F[dim][sy_i][i00] * f;
							value += F[dim][sy_i][i10] * f;
							value -= F[dim][sy_i][i01] * f;
							value -= F[dim][sy_i][i11] * f;
							value -= F[dim][sz_i][i00] * f;
							value += F[dim][sz_i][i10] * f;
							value -= F[dim][sz_i][i01] * f;
							value += F[dim][sz_i][i11] * f;
						}
					} else if (field == zy_i) {
						if (dim == XDIM) {
							value -= F[dim][sx_i][i00] * f;
							value -= F[dim][sx_i][i10] * f;
							value += F[dim][sx_i][i01] * f;
							value += F[dim][sx_i][i11] * f;
						} else if (dim == ZDIM) {
							value += F[dim][sz_i][i00] * f;
							value -= F[dim][sz_i][i10] * f;
							value += F[dim][sz_i][i01] * f;
							value -= F[dim][sz_i][i11] * f;
						} else if (dim == YDIM) {
							value -= F[dim][sx_i][i00] * f;
							value -= F[dim][sx_i][i10] * f;
							value += F[dim][sx_i][i01] * f;
							value += F[dim][sx_i][i11] * f;
							value += F[dim][sz_i][i00] * f;
							value -= F[dim][sz_i][i10] * f;
							value += F[dim][sz_i][i01] * f;
							value -= F[dim][sz_i][i11] * f;
						}
					} else if (field == zz_i) {
						if (dim == XDIM) {
							value += F[dim][sx_i][i00] * f;
							value -= F[dim][sx_i][i10] * f;
							value += F[dim][sx_i][i01] * f;
							value -= F[dim][sx_i][i11] * f;
						} else if (dim == YDIM) {
							value -= F[dim][sy_i][i00] * f;
							value += F[dim][sy_i][i10] * f;
							value -= F[dim][sy_i][i01] * f;
							value += F[dim][sy_i][i11] * f;
						} else if (dim == ZDIM) {
							value -= F[dim][sy_i][i00] * f;
							value += F[dim][sy_i][i10] * f;
							value -= F[dim][sy_i][i01] * f;
							value += F[dim][sy_i][i11] * f;
							value += F[dim][sx_i][i00] * f;
							value += F[dim][sx_i][i10] * f;
							value -= F[dim][sx_i][i01] * f;
							value -= F[dim][sx_i][i11] * f;
						}
					}
					value /= real(4);
					data.push_back(value);
				}
			}
		}
	}
//				src[zz_i][iii] = (-(F[XDIM][sy_i][iii + DNX] + F[XDIM][sy_i][iii])
//						+ (F[YDIM][sx_i][iii + DNY] + F[YDIM][sx_i][iii])) * HALF;

	return data;
}

void grid::set_flux_restrict(const std::vector<real>& data, const std::array<integer, NDIM>& lb,
		const std::array<integer, NDIM>& ub, const geo::dimension& dim) {
	integer index = 0;
	for (integer field = 0; field != NF; ++field) {
		for (integer i = lb[XDIM]; i < ub[XDIM]; ++i) {
			for (integer j = lb[YDIM]; j < ub[YDIM]; ++j) {
				for (integer k = lb[ZDIM]; k < ub[ZDIM]; ++k) {
					const integer iii = hindex(i, j, k);
					F[dim][field][iii] = data[index];
					++index;
				}
			}
		}
	}
}

void grid::set_outflows(std::vector<real>&& u) {
	U_out = std::move(u);
}

void grid::set_prolong(const std::vector<real>& data, std::vector<real>&& outflows) {
	integer index = 0;
	U_out = std::move(outflows);
	for (integer field = 0; field != NF; ++field) {
		for (integer i = H_BW; i != H_NX - H_BW; ++i) {
			for (integer j = H_BW; j != H_NX - H_BW; ++j) {
				for (integer k = H_BW; k != H_NX - H_BW; ++k) {
					const integer iii = hindex(i, j, k);
					auto& value = U[field][iii];
					value = data[index];
					++index;
				}
			}
		}
	}
}

std::vector<real> grid::get_prolong(const std::array<integer, NDIM>& lb, const std::array<integer, NDIM>& ub) {
	std::vector<real> data;
	const auto U0 = U;
	integer size = NF;
	for (integer dim = 0; dim != NDIM; ++dim) {
		size *= (ub[dim] - lb[dim]);
	}
	data.reserve(size);
	auto lb0 = lb;
	auto ub0 = ub;
	for (integer d = 0; d != NDIM; ++d) {
		lb0[d] /= 2;
		ub0[d] /= 2;
	}
	const auto V = compute_primitives(lb0, ub0);
	const auto dVdx = compute_primitive_slopes(V, 1.0, lb0, ub0);
	auto dUdx = compute_conserved_slopes(V, dVdx, lb0, ub0);

	for (integer i = lb[XDIM]; i < ub[XDIM]; i += 2) {
		for (integer j = lb[YDIM]; j < ub[YDIM]; j += 2) {
			for (integer k = lb[ZDIM]; k < ub[ZDIM]; k += 2) {
				const integer iii = hindex(i / 2, j / 2, k / 2);
				for (integer d0 = 0; d0 != NDIM; ++d0) {
					for (integer d1 = 0; d1 != NDIM; ++d1) {
						dUdx[d1][zx_i + d0][iii] -= V[rho_i][iii] * dVdx[d1][zx_i + d0][iii];
					}
				}
			}
		}
	}
	for (integer field = 0; field != NF; ++field) {
		for (integer i = lb[XDIM]; i != ub[XDIM]; ++i) {
			const real xsgn = (i % 2) ? +1 : -1;
			for (integer j = lb[YDIM]; j != ub[YDIM]; ++j) {
				const real ysgn = (j % 2) ? +1 : -1;
				for (integer k = lb[ZDIM]; k != ub[ZDIM]; ++k) {
					const integer iii = hindex(i / 2, j / 2, k / 2);
					const real zsgn = (k % 2) ? +1 : -1;
					real value = U[field][iii];
					value += xsgn * dUdx[XDIM][field][iii] * 0.25;
					value += ysgn * dUdx[YDIM][field][iii] * 0.25;
					value += zsgn * dUdx[ZDIM][field][iii] * 0.25;
					if (field == sx_i) {
						U[zy_i][iii] -= 0.25 * zsgn * value * dx / 8.0;
						U[zz_i][iii] += 0.25 * ysgn * value * dx / 8.0;
					} else if (field == sy_i) {
						U[zx_i][iii] += 0.25 * zsgn * value * dx / 8.0;
						U[zz_i][iii] -= 0.25 * xsgn * value * dx / 8.0;
					} else if (field == sz_i) {
						U[zx_i][iii] -= 0.25 * ysgn * value * dx / 8.0;
						U[zy_i][iii] += 0.25 * xsgn * value * dx / 8.0;
					}
					data.push_back(value);
				}
			}
		}
	}
	U = U0;
	return data;
}

std::vector<real> grid::get_restrict() const {
	constexpr
	integer Size = NF * INX * INX * INX / NCHILD + NF;
	std::vector<real> data;
	data.reserve(Size);
	for (integer field = 0; field != NF; ++field) {
		for (integer i = H_BW; i < H_NX - H_BW; i += 2) {
			for (integer j = H_BW; j < H_NX - H_BW; j += 2) {
				for (integer k = H_BW; k < H_NX - H_BW; k += 2) {
					const integer iii = hindex(i, j, k);
					real pt = ZERO;
					for (integer x = 0; x != 2; ++x) {
						for (integer y = 0; y != 2; ++y) {
							for (integer z = 0; z != 2; ++z) {
								const integer jjj = iii + x * H_DNX + y * H_DNY + z * H_DNZ;
								pt += U[field][jjj];
								if (field == zx_i) {
									pt += X[YDIM][jjj] * U[sz_i][jjj];
									pt -= X[ZDIM][jjj] * U[sy_i][jjj];
								} else if (field == zy_i) {
									pt -= X[XDIM][jjj] * U[sz_i][jjj];
									pt += X[ZDIM][jjj] * U[sx_i][jjj];
								} else if (field == zz_i) {
									pt += X[XDIM][jjj] * U[sy_i][jjj];
									pt -= X[YDIM][jjj] * U[sx_i][jjj];
								}
							}
						}
					}
					pt /= real(NCHILD);
					data.push_back(pt);
				}
			}
		}
	}
	for (integer field = 0; field != NF; ++field) {
		data.push_back(U_out[field]);
	}

	return data;
}

void grid::set_restrict(const std::vector<real>& data, const geo::octant& octant) {
	integer index = 0;
	const integer i0 = octant.get_side(XDIM) * (INX / 2);
	const integer j0 = octant.get_side(YDIM) * (INX / 2);
	const integer k0 = octant.get_side(ZDIM) * (INX / 2);
	for (integer field = 0; field != NF; ++field) {
		for (integer i = H_BW; i != H_NX / 2; ++i) {
			for (integer j = H_BW; j != H_NX / 2; ++j) {
				for (integer k = H_BW; k != H_NX / 2; ++k) {
					const integer iii = (i + i0) * H_DNX + (j + j0) * H_DNY + (k + k0) * H_DNZ;
					auto& v = U[field][iii];
					v = data[index];
					if (field == zx_i) {
						v -= X[YDIM][iii] * U[sz_i][iii];
						v += X[ZDIM][iii] * U[sy_i][iii];
					} else if (field == zy_i) {
						v += X[XDIM][iii] * U[sz_i][iii];
						v -= X[ZDIM][iii] * U[sx_i][iii];
					} else if (field == zz_i) {
						v -= X[XDIM][iii] * U[sy_i][iii];
						v += X[YDIM][iii] * U[sx_i][iii];
					}
					++index;
				}
			}
		}
	}
}

std::pair<std::vector<real>, std::vector<real> > grid::field_range() const {
	std::pair<std::vector<real>, std::vector<real> > minmax;
	minmax.first.resize(NF);
	minmax.second.resize(NF);
	for (integer field = 0; field != NF; ++field) {
		minmax.first[field] = +std::numeric_limits < real > ::max();
		minmax.second[field] = -std::numeric_limits < real > ::max();
	}
	for (integer i = H_BW; i != H_NX - H_BW; ++i) {
		for (integer j = H_BW; j != H_NX - H_BW; ++j) {
			for (integer k = H_BW; k != H_NX - H_BW; ++k) {
				const integer iii = hindex(i, j, k);
				for (integer field = 0; field != NF; ++field) {
					minmax.first[field] = std::min(minmax.first[field], U[field][iii]);
					minmax.second[field] = std::max(minmax.second[field], U[field][iii]);
				}
			}
		}
	}
	return minmax;
}

std::vector<real> grid::frac_moments(const std::vector<space_vector>& com) const {
	std::vector<real> I(NSPECIES);
	for( integer si = 0; si != NSPECIES; ++si) {
		I[0] = 0.0;
	}
	const real dV = dx * dx * dx;
	for (integer i = H_BW; i != H_NX - H_BW; ++i) {
		for (integer j = H_BW; j != H_NX - H_BW; ++j) {
			for (integer k = H_BW; k != H_NX - H_BW; ++k) {
				const integer iii = hindex(i, j, k);
				for( integer si = 0; si != NSPECIES; ++si) {
					const real x = X[XDIM][iii] - com[si][XDIM];
					const real y = X[YDIM][iii] - com[si][YDIM];
					const real R2 = x*x+y*y;
					I[si] += U[spc_i + si][iii] * (R2+dx*dx/6.0) * dV;
				}
			}
		}
	}
	return I;
}


std::vector<real> grid::frac_volumes() const {
	std::vector<real> V(NSPECIES, 0.0);
	const real dV = dx * dx * dx;
	for (integer i = H_BW; i != H_NX - H_BW; ++i) {
		for (integer j = H_BW; j != H_NX - H_BW; ++j) {
			for (integer k = H_BW; k != H_NX - H_BW; ++k) {
				const integer iii = hindex(i, j, k);
				for( integer si = 0; si != NSPECIES; ++si) {
					if (U[spc_i + si][iii] > 1.0e-5 ){
						V[si] += (U[spc_i + si][iii]/U[rho_i][iii]) * dV;
					}
				}
			}
		}
	}
//	printf( "%e", V[0]);
	return V;
}

real grid::z_moments(const std::pair<space_vector, space_vector>& axis,
		const std::pair<real, real>& l1, integer frac) const {
	real mom = 0.0;
	const real dV = dx * dx * dx;
	for (integer i = H_BW; i != H_NX - H_BW; ++i) {
		for (integer j = H_BW; j != H_NX - H_BW; ++j) {
			for (integer k = H_BW; k != H_NX - H_BW; ++k) {
				const integer iii = hindex(i, j, k);
				bool use = false;
				if (frac == 0) {
					use = true;
				} else {
					space_vector a = axis.first;
					const space_vector& o = axis.second;
					space_vector b;
					real aa = 0.0;
					real ab = 0.0;
					for (integer d = 0; d != NDIM; ++d) {
						a[d] -= o[d];
						b[d] = X[d][iii] - o[d];
					}
					for (integer d = 0; d != NDIM; ++d) {
						aa += a[d] * a[d];
						ab += a[d] * b[d];
					}
					real p = ab / std::sqrt(aa);
			//		printf( "%e\n", l1.first);
					if (p < l1.first && frac == +1) {
						use = true;
					} else if (p >= l1.first && frac == -1) {
						use = true;
					}
				}
				if (use) {
					mom += (std::pow(X[XDIM][iii],2)+dx*dx/6.0) * U[rho_i][iii] * dV;
					mom += (std::pow(X[YDIM][iii],2)+dx*dx/6.0) * U[rho_i][iii] * dV;
				}
			}
		}
	}
	return mom;
}


std::vector<real> grid::conserved_sums(space_vector& com,space_vector& com_dot, const std::pair<space_vector, space_vector>& axis,
		const std::pair<real, real>& l1, integer frac) const {
	std::vector<real> sum(NF, ZERO);
	com[0] = com[1] = com[2] = 0.0;
	com_dot[0] = com_dot[1] = com_dot[2] = 0.0;
	const real dV = dx * dx * dx;
	for (integer i = H_BW; i != H_NX - H_BW; ++i) {
		for (integer j = H_BW; j != H_NX - H_BW; ++j) {
			for (integer k = H_BW; k != H_NX - H_BW; ++k) {
				const integer iii = hindex(i, j, k);
				bool use = false;
				if (frac == 0) {
					use = true;
				} else {
					space_vector a = axis.first;
					const space_vector& o = axis.second;
					space_vector b;
					real aa = 0.0;
					real ab = 0.0;
					for (integer d = 0; d != NDIM; ++d) {
						a[d] -= o[d];
						b[d] = X[d][iii] - o[d];
					}
					for (integer d = 0; d != NDIM; ++d) {
						aa += a[d] * a[d];
						ab += a[d] * b[d];
					}
					real p = ab / std::sqrt(aa);
			//		printf( "%e\n", l1.first);
					if (p < l1.first && frac == +1) {
						use = true;
					} else if (p >= l1.first && frac == -1) {
						use = true;
					}
				}
				if (use) {
					com[0] += X[XDIM][iii] * U[rho_i][iii] * dV;
					com[1] += X[YDIM][iii] * U[rho_i][iii] * dV;
					com[2] += X[ZDIM][iii] * U[rho_i][iii] * dV;
					com_dot[0] += U[sx_i][iii] * dV;
					com_dot[1] += U[sy_i][iii] * dV;
					com_dot[2] += U[sz_i][iii] * dV;
					for (integer field = 0; field != NF; ++field) {
						sum[field] += U[field][iii] * dV;
					}
					sum[egas_i] += U[pot_i][iii] * HALF * dV;
					sum[zx_i] += X[YDIM][iii] * U[sz_i][iii] * dV;
					sum[zx_i] -= X[ZDIM][iii] * U[sy_i][iii] * dV;
					sum[zy_i] -= X[XDIM][iii] * U[sz_i][iii] * dV;
					sum[zy_i] += X[ZDIM][iii] * U[sx_i][iii] * dV;
					sum[zz_i] += X[XDIM][iii] * U[sy_i][iii] * dV;
					sum[zz_i] -= X[YDIM][iii] * U[sx_i][iii] * dV;
				}
			}
		}
	}
	if (sum[rho_i] > 0.0) {
		for (integer d = 0; d != NDIM; ++d) {
			com[d] /= sum[rho_i];
			com_dot[d] /= sum[rho_i];
		}
	}
	return sum;
}

std::vector<real> grid::gforce_sum(bool torque) const {
	std::vector<real> sum(NDIM, ZERO);
	const real dV = dx * dx * dx;
	for (integer i = H_BW; i != H_NX - H_BW; ++i) {
		for (integer j = H_BW; j != H_NX - H_BW; ++j) {
			for (integer k = H_BW; k != H_NX - H_BW; ++k) {
				const auto D = G_BW - H_BW;
				const integer iii = hindex(i, j, k);
				const integer iiig = gindex(i + D, j + D, k + D);
				const real& rho = U[rho_i][iii];
				const real x = X[XDIM][iii];
				const real y = X[YDIM][iii];
				const real z = X[ZDIM][iii];
				const real fx = rho * G[gx_i][iiig] * dV;
				const real fy = rho * G[gy_i][iiig] * dV;
				const real fz = rho * G[gz_i][iiig] * dV;
				if (!torque) {
					sum[XDIM] += fx;
					sum[YDIM] += fy;
					sum[ZDIM] += fz;
				} else {
					sum[XDIM] -= z * fy - y * fz;
					sum[YDIM] += z * fx - x * fz;
					sum[ZDIM] -= y * fx - x * fy;
				}
			}
		}
	}
	return sum;
}

std::vector<real> grid::l_sums() const {
	std::vector<real> sum(NDIM);
	const real dV = dx * dx * dx;
	std::fill(sum.begin(), sum.end(), ZERO);
	for (integer i = H_BW; i != H_NX - H_BW; ++i) {
		for (integer j = H_BW; j != H_NX - H_BW; ++j) {
			for (integer k = H_BW; k != H_NX - H_BW; ++k) {
				const integer iii = hindex(i, j, k);
				sum[XDIM] += X[YDIM][iii] * U[sz_i][iii] * dV;
				sum[XDIM] -= X[ZDIM][iii] * U[sy_i][iii] * dV;

				sum[YDIM] -= X[XDIM][iii] * U[sz_i][iii] * dV;
				sum[YDIM] += X[ZDIM][iii] * U[sx_i][iii] * dV;

				sum[ZDIM] += X[XDIM][iii] * U[sy_i][iii] * dV;
				sum[ZDIM] -= X[YDIM][iii] * U[sx_i][iii] * dV;

			}
		}
	}
	return sum;
}

bool grid::refine_me(integer lev) const {
	auto test = get_refine_test();
	if (lev < 2) {
		return true;
	}
	bool rc = false;
	for (integer i = H_BW - R_BW; i != H_NX - H_BW + R_BW; ++i) {
		for (integer j = H_BW - R_BW; j != H_NX - H_BW + R_BW; ++j) {
			for (integer k = H_BW - R_BW; k != H_NX - H_BW + R_BW; ++k) {
				const integer iii = hindex(i, j, k);
				std::vector<real> state(NF);
				for (integer i = 0; i != NF; ++i) {
					state[i] = U[i][iii];
				}
				if (test(lev, max_level, X[XDIM][iii], X[YDIM][iii], X[ZDIM][iii], state)) {
					rc = true;
					break;
				}
			}
			if (rc) {
				break;
			}
		}
		if (rc) {
			break;
		}
	}
	return rc;
}

integer grid::level_count() const {
	return nlevel;
}

grid::~grid() {

}

real& grid::hydro_value(integer f, integer i, integer j, integer k) {
	return U[f][hindex(i, j, k)];
}

space_vector& grid::center_of_mass_value(integer i, integer j, integer k) {
	return com[0][gindex(i, j, k)];
}

const space_vector& grid::center_of_mass_value(integer i, integer j, integer k) const {
	return com[0][gindex(i, j, k)];
}

space_vector grid::center_of_mass() const {
	space_vector this_com;
	this_com[0] = this_com[1] = this_com[2] = ZERO;
	real m = ZERO;
	for (integer i = G_BW; i != INX + G_BW; ++i) {
		for (integer j = G_BW; j != INX + G_BW; ++j) {
			for (integer k = G_BW; k != INX + G_BW; ++k) {
				const integer iii = gindex(i, j, k);
				const real& this_m = M[0][iii]();
				for (auto& dim : geo::dimension::full_set()) {
					this_com[dim] += this_m * com[0][iii][dim];
				}
				m += this_m;
			}
		}
	}
	if (m != ZERO) {
		for (auto& dim : geo::dimension::full_set()) {
			this_com[dim] /= m;
		}
	}
	return this_com;
}

multipole& grid::multipole_value(integer lev, integer i, integer j, integer k) {
	const integer bw = G_BW;
	const integer inx = INX >> lev;
	const integer nx = 2 * bw + inx;
	return M[lev][i * nx * nx + j * nx + k];
}

const multipole& grid::multipole_value(integer lev, integer i, integer j, integer k) const {
	const integer bw = G_BW;
	const integer inx = INX >> lev;
	const integer nx = 2 * bw + inx;
	return M[lev][i * nx * nx + j * nx + k];
}

real grid::hydro_value(integer f, integer i, integer j, integer k) const {
	return U[f][hindex(i, j, k)];
}

grid::grid(real _dx, std::array<real, NDIM> _xmin) :
		U(NF), U0(NF), dUdt(NF), Uf(NFACE), F(NDIM), X(NDIM), G(NGF), G0(NGF), src(NF), ilist_d_bnd(
				geo::direction::count()), ilist_n_bnd(geo::direction::count()), is_root(false), is_leaf(true) {
	dx = _dx;
	xmin = _xmin;
	allocate();
}

std::vector<std::vector<real>> grid::compute_primitives(const std::array<integer, NDIM> lb,
		const std::array<integer, NDIM> ub) {
	auto V = U;
	for (integer i = lb[XDIM] - 1; i != ub[XDIM] + 1; ++i) {
		for (integer j = lb[YDIM] - 1; j != ub[YDIM] + 1; ++j) {
#pragma GCC ivdep
			for (integer k = lb[ZDIM] - 1; k != ub[ZDIM] + 1; ++k) {
				const integer iii = hindex(i, j, k);
				const real rho = V[rho_i][iii];
				V[tau_i][iii] = std::pow(V[tau_i][iii], fgamma);
				V[egas_i][iii] /= rho;
				for( integer si = 0; si != NSPECIES; ++si) {
					V[spc_i+si][iii] /= rho;
				}
				V[pot_i][iii] /= rho;
				V[tau_i][iii] /= rho;
				for (integer d = 0; d != NDIM; ++d) {
					V[sx_i + d][iii] /= rho;
					V[egas_i][iii] -= 0.5 * std::pow(V[sx_i + d][iii], 2);
#ifdef ZCORRECT
					V[zx_i + d][iii] /= rho;
#endif
				}
				V[sx_i][iii] += X[YDIM][iii] * omega;
				V[sy_i][iii] -= X[XDIM][iii] * omega;
				V[zz_i][iii] -= dx * dx * omega / 6.0;
			}
		}
	}
	return V;
}

std::vector<std::vector<std::vector<real>>>grid::compute_primitive_slopes(const std::vector<std::vector<real>>& V, real theta, const std::array<integer, NDIM> lb, const std::array<integer, NDIM> ub ) {
	std::vector < std::vector<std::vector<real>>> dVdx(NDIM,
			std::vector < std::vector < real >> (NF, std::vector < real > (H_N3)));
	for (integer f = 0; f != NF; ++f) {
		const auto& v = V[f];
		for( integer i = lb[XDIM]; i != ub[XDIM]; ++i) {
			for( integer j = lb[YDIM]; j != ub[YDIM]; ++j) {
#pragma GCC ivdep
				for( integer k = lb[ZDIM]; k != ub[ZDIM]; ++k) {
					const integer iii = hindex(i,j,k);
					const auto v0 = v[iii];
					dVdx[XDIM][f][iii] = minmod_theta(v[iii + H_DNX] - v0, v0 - v[iii - H_DNX], theta);
					dVdx[YDIM][f][iii] = minmod_theta(v[iii + H_DNY] - v0, v0 - v[iii - H_DNY], theta);
					dVdx[ZDIM][f][iii] = minmod_theta(v[iii + H_DNZ] - v0, v0 - v[iii - H_DNZ], theta);
				}
			}
		}
	}
#ifdef ZCORRECT
	for( integer i = lb[XDIM]; i != ub[XDIM]; ++i) {
		for( integer j = lb[YDIM]; j != ub[YDIM]; ++j) {
#pragma GCC ivdep
			for( integer k = lb[ZDIM]; k != ub[ZDIM]; ++k) {
				const integer iii = hindex(i,j,k);
				real dV_sym[3][3];
				real dV_ant[3][3];
				for (integer d0 = 0; d0 != NDIM; ++d0) {
					for (integer d1 = 0; d1 != NDIM; ++d1) {
						dV_sym[d1][d0] = (dVdx[d0][sx_i + d1][iii] + dVdx[d1][sx_i + d0][iii]) / 2.0;
						dV_ant[d1][d0] = 0.0;
					}
				}
				dV_ant[XDIM][YDIM] = +12.0 * V[zz_i][iii] / dx;
				dV_ant[XDIM][ZDIM] = -12.0 * V[zy_i][iii] / dx;
				dV_ant[YDIM][ZDIM] = +12.0 * V[zx_i][iii] / dx;
				dV_ant[YDIM][XDIM] = -dV_ant[XDIM][YDIM];
				dV_ant[ZDIM][XDIM] = -dV_ant[XDIM][ZDIM];
				dV_ant[ZDIM][YDIM] = -dV_ant[YDIM][ZDIM];
				for (integer d0 = 0; d0 != NDIM; ++d0) {
					for (integer d1 = 0; d1 != NDIM; ++d1) {
						const real tmp = dV_sym[d0][d1] + dV_ant[d0][d1];
						dVdx[d0][sx_i + d1][iii] = minmod(tmp, 2.0/theta * dVdx[d0][sx_i + d1][iii]);
					}
				}
			}
		}
	}
#endif
	for( integer i = lb[XDIM]; i != ub[XDIM]; ++i) {
		for( integer j = lb[YDIM]; j != ub[YDIM]; ++j) {
#pragma GCC ivdep
			for( integer k = lb[ZDIM]; k != ub[ZDIM]; ++k) {
				const integer iii = hindex(i,j,k);
				for (integer d = 0; d != NDIM; ++d) {
					dVdx[d][sx_i][iii] -= X[YDIM][iii] * omega * dVdx[d][rho_i][iii];
					dVdx[d][sy_i][iii] += X[XDIM][iii] * omega * dVdx[d][rho_i][iii];
					dVdx[d][zz_i][iii] += dx * dx * omega * dVdx[d][rho_i][iii] / 6.0;
				}
				dVdx[YDIM][sx_i][iii] -= dx * omega * V[rho_i][iii];
				dVdx[XDIM][sy_i][iii] += dx * omega * V[rho_i][iii];
			}
		}
	}
	return dVdx;
}

std::vector<std::vector<std::vector<real>>>grid::compute_conserved_slopes(const std::vector<std::vector<real>>& V, const std::vector<std::vector<std::vector<real>>>& dVdx, const std::array<integer, NDIM> lb, const std::array<integer, NDIM> ub ) {
	std::vector < std::vector<std::vector<real>>> dUdx(NDIM,
			std::vector < std::vector < real >> (NF, std::vector < real > (H_N3)));
	const real theta = 1.0;
	for (integer d0 = 0; d0 != NDIM; ++d0) {
		auto& dV = dVdx[d0];
		auto& dU = dUdx[d0];
		for( integer i = lb[XDIM]; i != ub[XDIM]; ++i) {
			for( integer j = lb[YDIM]; j != ub[YDIM]; ++j) {
#pragma GCC ivdep
				for( integer k = lb[ZDIM]; k != ub[ZDIM]; ++k) {
					const integer iii = hindex(i,j,k);
					dU[rho_i][iii] = dV[rho_i][iii];

					for( integer si = 0; si != NSPECIES; ++si) {
						dU[spc_i+si][iii] = V[spc_i+si][iii] * dV[rho_i][iii] + dV[spc_i+si][iii] * V[rho_i][iii];
					}

					dU[pot_i][iii] = V[pot_i][iii] * dV[rho_i][iii] + dV[pot_i][iii] * V[rho_i][iii];

					dU[egas_i][iii] = V[egas_i][iii] * dV[rho_i][iii] + dV[egas_i][iii] * V[rho_i][iii];

					const real t0 = (std::pow(V[rho_i][iii] * V[tau_i][iii], 1.0 / fgamma - 1.0) / fgamma);
					dU[tau_i][iii] = t0 * (V[tau_i][iii] * dV[rho_i][iii] + dV[tau_i][iii] * V[rho_i][iii]);
					dU[tau_i][iii] = minmod(dU[tau_i][iii], theta * std::copysign(U[tau_i][iii], dU[tau_i][iii]));
					for (integer d1 = 0; d1 != NDIM; ++d1) {
						dU[sx_i + d1][iii] = V[sx_i + d1][iii] * dV[rho_i][iii] + dV[sx_i + d1][iii] * V[rho_i][iii];
						dU[egas_i][iii] += V[rho_i][iii] * (V[sx_i + d1][iii] * dV[sx_i + d1][iii]);
						dU[egas_i][iii] += dV[rho_i][iii] * 0.5 * std::pow(V[sx_i + d1][iii], 2);
#ifdef ZCORRECT
						dU[zx_i + d1][iii] = V[zx_i + d1][iii] * dV[rho_i][iii] + dV[zx_i + d1][iii] * V[rho_i][iii];
#endif
					}
				}
			}
		}
	}
	return dUdx;
}

void grid::set_root(bool flag) {
	if (is_root != flag) {
		is_root = flag;
		integer this_nlevel = 0;
		for (integer inx = INX; inx > 1; inx /= 2) {
			const integer this_nx = inx + 2 * G_BW;
			const integer sz = this_nx * this_nx * this_nx;
			com[this_nlevel].resize(sz);
			if (is_root || (inx >= INX / 2)) {
				M[this_nlevel].resize(sz);
			}
			if (is_root || (inx >= INX)) {
				L[this_nlevel].resize(sz);
				L_c[this_nlevel].resize(sz);
			}
			++this_nlevel;
		}
		compute_ilist();
	}
}

void grid::set_leaf(bool flag) {
	if (is_leaf != flag) {
		is_leaf = flag;
		compute_ilist();
	}
}

void grid::set_coordinates() {
	for (integer i = 0; i != H_NX; ++i) {
		for (integer j = 0; j != H_NX; ++j) {
			for (integer k = 0; k != H_NX; ++k) {
				const integer iii = hindex(i, j, k);
				X[XDIM][iii] = (real(i - H_BW) + HALF) * dx + xmin[XDIM] - pivot[XDIM];
				X[YDIM][iii] = (real(j - H_BW) + HALF) * dx + xmin[YDIM] - pivot[YDIM];
				X[ZDIM][iii] = (real(k - H_BW) + HALF) * dx + xmin[ZDIM] - pivot[ZDIM];
			}
		}
	}
}

void grid::allocate() {
	U_out0 = std::vector<real>(NF, ZERO);
	U_out = std::vector<real>(NF, ZERO);
	dphi_dt = std::vector<real>(H_N3);
	for (integer field = 0; field != NGF; ++field) {
		G[field].resize(G_N3);
		G0[field].resize(G_N3);
	}
	for (integer dim = 0; dim != NDIM; ++dim) {
		X[dim].resize(H_N3);
	}
	for (integer field = 0; field != NF; ++field) {
		src[field].resize(H_N3);
		U0[field].resize(H_N3);
		U[field].resize(H_N3);
		dUdt[field].resize(H_N3);
		for (integer dim = 0; dim != NDIM; ++dim) {
			F[dim][field].resize(H_N3);
		}
		for (integer face = 0; face != NFACE; ++face) {
			Uf[face][field].resize(H_N3);
		}
	}
	nlevel = 0;
	for (integer inx = INX; inx > 1; inx /= 2) {
		++nlevel;
	}
	com.resize(nlevel);
	M.resize(nlevel);
	L.resize(nlevel);
	L_c.resize(nlevel);
	nlevel = 0;
	for (integer inx = INX; inx > 1; inx /= 2) {
		const integer this_nx = inx + 2 * G_BW;
		const integer sz = this_nx * this_nx * this_nx;
		com[nlevel].resize(sz);
		if (is_root || (inx >= INX / 2)) {
			M[nlevel].resize(sz);
		}
		if (is_root || (inx >= INX)) {
			L[nlevel].resize(sz);
			L_c[nlevel].resize(sz);
		}
		++nlevel;
	}
	set_coordinates();
	compute_ilist();

}

grid::grid() :
		U(NF), U0(NF), dUdt(NF), Uf(NFACE), F(NDIM), X(NDIM), G(NGF), G0(NGF), src(NF), ilist_d_bnd(
				geo::direction::count()), ilist_n_bnd(geo::direction::count()), is_root(false), is_leaf(true), U_out(NF,
				ZERO), U_out0(NF, ZERO), dphi_dt(H_N3) {
//	allocate();
}

grid::grid(const init_func_type& init_func, real _dx, std::array<real, NDIM> _xmin) :
		U(NF), U0(NF), dUdt(NF), Uf(NFACE), F(NDIM), X(NDIM), G(NGF), G0(NGF), src(NF), ilist_d_bnd(
				geo::direction::count()), ilist_n_bnd(geo::direction::count()), is_root(false), is_leaf(true), U_out(NF,
				ZERO), U_out0(NF, ZERO), dphi_dt(H_N3) {
	dx = _dx;
	xmin = _xmin;
	allocate();
	for (integer i = 0; i != H_NX; ++i) {
		for (integer j = 0; j != H_NX; ++j) {
			for (integer k = 0; k != H_NX; ++k) {
				const integer iii = hindex(i, j, k);
				std::vector<real> this_u = init_func(X[XDIM][iii], X[YDIM][iii], X[ZDIM][iii], dx);
				for (integer field = 0; field != NF; ++field) {
					U[field][iii] = this_u[field];
				}
				U[zx_i][iii] = ZERO;
				U[zy_i][iii] = ZERO;
				U[zz_i][iii] = ZERO;
			}
		}
	}
}

void grid::reconstruct() {
	const auto limit_slope = [](real& ql, real q0, real& qr) {
		const real tmp1 = qr - ql;
		const real tmp2 = qr + ql;
		if ((qr - q0) * (q0 - ql) <= 0.0) {
			qr = ql = q0;
		} else if (tmp1 * (q0 - 0.5 * tmp2) > (1.0 / 6.0) * tmp1 * tmp1) {
			ql = 3.0 * q0 - 2.0 * qr;
		} else if (-(1.0 / 6.0) * tmp1 * tmp1 > tmp1 * (q0 - 0.5 * tmp2)) {
			qr = 3.0 * q0 - 2.0 * ql;
		}
	};
	/*
	 #ifdef USE_PPM
	 const real theta = 2.0;
	 #else
	 const real theta = 4.0/3.0;
	 #endif
	 const auto V = compute_primitives();
	 #ifdef USE_MINMOD
	 auto dUdx = compute_conserved_slopes(V, compute_primitive_slopes(V, theta));
	 for (integer face = 0; face != NFACE; ++face) {
	 real c = face % 2 == 0 ? -0.5 : +0.5;
	 for (integer f = 0; f != NF; ++f) {
	 for (integer iii = 0; iii != H_N3; ++iii) {
	 Uf[face][f][iii] = U[f][iii] + c * dUdx[face / 2][f][iii];
	 }
	 }
	 }
	 #endif

	 #ifdef USE_PPM
	 auto dVdx = compute_primitive_slopes(V, theta);
	 for (integer field = 0; field != NF; ++field) {
	 #pragma GCC ivdep
	 for (integer iii = H_NX * H_NX; iii != H_N3 - H_NX * H_NX; ++iii) {
	 const real v0 = V[field][iii];
	 for (integer d = 0; d != NDIM; ++d) {
	 const auto& dv0 = dVdx[d][field][iii];
	 Uf[2 * d + 1][field][iii] = (V[field][iii + H_DN[d]] + v0 - (dVdx[d][field][iii + H_DN[d]] - dv0) / 3.0)
	 * HALF;
	 Uf[2 * d + 0][field][iii] = (V[field][iii - H_DN[d]] + v0 - (dv0 - dVdx[d][field][iii - H_DN[d]]) / 3.0)
	 * HALF;
	 limit_slope(Uf[2 * d + 0][field][iii], v0, Uf[2 * d + 1][field][iii]);
	 }
	 }
	 }
	 for (integer field = 0; field != NF; ++field) {
	 if (field == pot_i) {
	 #pragma GCC ivdep
	 for (integer iii = H_NX * H_NX; iii != H_N3 - H_NX * H_NX; ++iii) {
	 const real phi_x = HALF * (Uf[FXM][field][iii] + Uf[FXP][field][iii - H_DNX]);
	 const real phi_y = HALF * (Uf[FYM][field][iii] + Uf[FYP][field][iii - H_DNY]);
	 const real phi_z = HALF * (Uf[FZM][field][iii] + Uf[FZP][field][iii - H_DNZ]);
	 Uf[FXM][field][iii] = phi_x;
	 Uf[FYM][field][iii] = phi_y;
	 Uf[FZM][field][iii] = phi_z;
	 Uf[FXP][field][iii - H_DNX] = phi_x;
	 Uf[FYP][field][iii - H_DNY] = phi_y;
	 Uf[FZP][field][iii - H_DNZ] = phi_z;
	 }
	 }
	 if (field != rho_i) {
	 #pragma GCC ivdep
	 for (integer iii = 0; iii != H_N3; ++iii) {
	 for (integer face = 0; face != NFACE; ++face) {
	 Uf[face][field][iii] *= Uf[face][rho_i][iii];
	 }
	 }
	 }
	 }
	 for (integer i = H_BW - 1; i != H_NX - H_BW + 1; ++i) {
	 for (integer j = H_BW - 1; j != H_NX - H_BW + 1; ++j) {
	 for (integer k = H_BW - 1; k != H_NX - H_BW + 1; ++k) {
	 const integer iii = hindex(i, j, k);
	 for (integer face = 0; face != NFACE; ++face) {
	 real x0 = ZERO;
	 real y0 = ZERO;
	 if (face == FXP) {
	 x0 = +HALF * dx;
	 } else if (face == FXM) {
	 x0 = -HALF * dx;
	 } else if (face == FYP) {
	 y0 = +HALF * dx;
	 } else if (face == FYM) {
	 y0 = -HALF * dx;
	 }
	 Uf[face][sx_i][iii] -= omega * (X[YDIM][iii] + y0) * Uf[face][rho_i][iii];
	 Uf[face][sy_i][iii] += omega * (X[XDIM][iii] + x0) * Uf[face][rho_i][iii];
	 Uf[face][zz_i][iii] += dx * dx * omega * Uf[face][rho_i][iii] / 6.0;
	 Uf[face][egas_i][iii] += HALF * Uf[face][sx_i][iii] * Uf[face][sx_i][iii] / Uf[face][rho_i][iii];
	 Uf[face][egas_i][iii] += HALF * Uf[face][sy_i][iii] * Uf[face][sy_i][iii] / Uf[face][rho_i][iii];
	 Uf[face][egas_i][iii] += HALF * Uf[face][sz_i][iii] * Uf[face][sz_i][iii] / Uf[face][rho_i][iii];
	 Uf[face][tau_i][iii] = std::pow(Uf[face][tau_i][iii], 1.0 / fgamma);
	 }
	 }
	 }
	 }
	 #endif

	 */
#pragma GCC ivdep
	for (integer iii = 0; iii != H_N3; ++iii) {
		const real& rho = U[rho_i][iii];
		const real x = X[XDIM][iii];
		const real y = X[YDIM][iii];
		U[egas_i][iii] -= HALF * U[sx_i][iii] * U[sx_i][iii] / rho;
		U[egas_i][iii] -= HALF * U[sy_i][iii] * U[sy_i][iii] / rho;
		U[egas_i][iii] -= HALF * U[sz_i][iii] * U[sz_i][iii] / rho;
		U[sx_i][iii] += omega * y * rho;
		U[sy_i][iii] -= omega * x * rho;
		U[zz_i][iii] -= dx * dx * omega * rho / 6.0;
		U[tau_i][iii] = std::pow(U[tau_i][iii], fgamma);
	}

	for (integer field = 0; field != NF; ++field) {
		if (field != rho_i) {
			for (integer iii = 0; iii != H_N3; ++iii) {
				U[field][iii] /= U[rho_i][iii];
			}
		}
	}

	std::array<std::vector<real>, NF> slpx, slpy, slpz;
	for (integer field = 0; field != NF; ++field) {
		slpx[field].resize(H_N3);
		slpy[field].resize(H_N3);
		slpz[field].resize(H_N3);
	}

	const auto limit_range = [](real a, real b, real& c) {
		const real max = std::max(a,b);
		const real min = std::min(a,b);
		c = std::min(max,std::max(min,c));
	};

	const auto limit_range_all = [=]( real am, real ap, real& bl, real& br ) {
		real avg = (br + bl)/2.0;
		limit_range(am, avg, bl );
		limit_range(ap, avg, br );
	};

	for (integer field = 0; field != NF; ++field) {
#pragma GCC ivdep

		for (integer iii = H_NX * H_NX; iii != H_N3 - H_NX * H_NX; ++iii) {
			const real u0 = U[field][iii];
			slpx[field][iii] = slpy[field][iii] = slpz[field][iii] = ZERO;
			if (field < zx_i || field > zz_i) {
				if (field != sy_i && field != sz_i) {
					slpx[field][iii] = minmod_theta(U[field][iii + H_DNX] - u0, u0 - U[field][iii - H_DNX], 2.0);
				}
				if (field != sx_i && field != sz_i) {
					slpy[field][iii] = minmod_theta(U[field][iii + H_DNY] - u0, u0 - U[field][iii - H_DNY], 2.0);
				}
				if (field != sx_i && field != sy_i) {
					slpz[field][iii] = minmod_theta(U[field][iii + H_DNZ] - u0, u0 - U[field][iii - H_DNZ], 2.0);
				}
			}
		}
#pragma GCC ivdep
		for (integer iii = 0; iii != H_N3 - H_NX * H_NX; ++iii) {
			const real& u0 = U[field][iii];
			if (field < zx_i || field > zz_i) {
				Uf[FXP][field][iii] = Uf[FXM][field][iii + H_DNX] = (U[field][iii + H_DNX] + u0) * HALF;
				Uf[FYP][field][iii] = Uf[FYM][field][iii + H_DNY] = (U[field][iii + H_DNY] + u0) * HALF;
				Uf[FZP][field][iii] = Uf[FZM][field][iii + H_DNZ] = (U[field][iii + H_DNZ] + u0) * HALF;
			} else {
				for (integer face = 0; face != NFACE; ++face) {
					Uf[face][field][iii] = U[field][iii];
				}
			}
		}
#pragma GCC ivdep
		for (integer iii = H_NX * H_NX; iii != H_N3 - H_NX * H_NX; ++iii) {
			const real& sx = slpx[field][iii];
			const real& sy = slpy[field][iii];
			const real& sz = slpz[field][iii];
			if (field < zx_i || field > zz_i) {
				Uf[FXP][field][iii] += (-(slpx[field][iii + H_DNX] - sx) / 3.0) * HALF;
				Uf[FXM][field][iii] += (+(slpx[field][iii - H_DNX] - sx) / 3.0) * HALF;
				Uf[FYP][field][iii] += (-(slpy[field][iii + H_DNY] - sy) / 3.0) * HALF;
				Uf[FYM][field][iii] += (+(slpy[field][iii - H_DNY] - sy) / 3.0) * HALF;
				Uf[FZP][field][iii] += (-(slpz[field][iii + H_DNZ] - sz) / 3.0) * HALF;
				Uf[FZM][field][iii] += (+(slpz[field][iii - H_DNZ] - sz) / 3.0) * HALF;
			}
		}
	}

	for (integer field = 0; field != NF; ++field) {
#pragma GCC ivdep
		for (integer iii = H_NX * H_NX; iii != H_N3 - H_NX * H_NX; ++iii) {
			const real u0 = U[field][iii];
			limit_slope(Uf[FXM][field][iii], u0, Uf[FXP][field][iii]);
			limit_slope(Uf[FYM][field][iii], u0, Uf[FYP][field][iii]);
			limit_slope(Uf[FZM][field][iii], u0, Uf[FZP][field][iii]);
		}
	}

#pragma GCC ivdep
	for (integer iii = H_NX * H_NX; iii != H_N3 - H_NX * H_NX; ++iii) {
		real xy_ant = +U[zz_i][iii] * 6.0 / dx;
		real xz_ant = -U[zy_i][iii] * 6.0 / dx;
		real yz_ant = +U[zx_i][iii] * 6.0 / dx;

		xy_ant -= HALF * (Uf[FXP][sy_i][iii] - Uf[FXM][sy_i][iii]);
		xy_ant += HALF * (Uf[FYP][sx_i][iii] - Uf[FYM][sx_i][iii]);

		xz_ant -= HALF * (Uf[FXP][sz_i][iii] - Uf[FXM][sz_i][iii]);
		xz_ant += HALF * (Uf[FZP][sx_i][iii] - Uf[FZM][sx_i][iii]);

		yz_ant -= HALF * (Uf[FYP][sz_i][iii] - Uf[FYM][sz_i][iii]);
		yz_ant += HALF * (Uf[FZP][sy_i][iii] - Uf[FZM][sy_i][iii]);

		Uf[FXP][sy_i][iii] += xy_ant;
		Uf[FXM][sy_i][iii] -= xy_ant;
		Uf[FYP][sx_i][iii] -= xy_ant;
		Uf[FYM][sx_i][iii] += xy_ant;

		Uf[FXP][sz_i][iii] += xz_ant;
		Uf[FXM][sz_i][iii] -= xz_ant;
		Uf[FZP][sx_i][iii] -= xz_ant;
		Uf[FZM][sx_i][iii] += xz_ant;

		Uf[FYP][sz_i][iii] += yz_ant;
		Uf[FYM][sz_i][iii] -= yz_ant;
		Uf[FZP][sy_i][iii] -= yz_ant;
		Uf[FZM][sy_i][iii] += yz_ant;

		limit_range(U[sy_i][iii + H_DNX], U[sy_i][iii], Uf[FXP][sy_i][iii]);
		limit_range(U[sy_i][iii - H_DNX], U[sy_i][iii], Uf[FXM][sy_i][iii]);
		limit_range(U[sx_i][iii + H_DNY], U[sx_i][iii], Uf[FYP][sx_i][iii]);
		limit_range(U[sx_i][iii - H_DNY], U[sx_i][iii], Uf[FYM][sx_i][iii]);

		limit_range(U[sz_i][iii + H_DNX], U[sz_i][iii], Uf[FXP][sz_i][iii]);
		limit_range(U[sz_i][iii - H_DNX], U[sz_i][iii], Uf[FXM][sz_i][iii]);
		limit_range(U[sx_i][iii + H_DNZ], U[sx_i][iii], Uf[FZP][sx_i][iii]);
		limit_range(U[sx_i][iii - H_DNZ], U[sx_i][iii], Uf[FZM][sx_i][iii]);

		limit_range(U[sz_i][iii + H_DNY], U[sz_i][iii], Uf[FYP][sz_i][iii]);
		limit_range(U[sz_i][iii - H_DNY], U[sz_i][iii], Uf[FYM][sz_i][iii]);
		limit_range(U[sy_i][iii + H_DNZ], U[sy_i][iii], Uf[FZP][sy_i][iii]);
		limit_range(U[sy_i][iii - H_DNZ], U[sy_i][iii], Uf[FZM][sy_i][iii]);

	}
#pragma GCC ivdep
	for (integer iii = H_NX * H_NX; iii != H_N3 - H_NX * H_NX; ++iii) {
		limit_range_all(U[sy_i][iii], U[sy_i][iii + H_DNX], Uf[FXP][sy_i][iii], Uf[FXM][sy_i][iii + H_DNX]);
		limit_range_all(U[sx_i][iii], U[sx_i][iii + H_DNY], Uf[FYP][sx_i][iii], Uf[FYM][sx_i][iii + H_DNY]);
		limit_range_all(U[sz_i][iii], U[sz_i][iii + H_DNX], Uf[FXP][sz_i][iii], Uf[FXM][sz_i][iii + H_DNX]);
		limit_range_all(U[sx_i][iii], U[sx_i][iii + H_DNZ], Uf[FZP][sx_i][iii], Uf[FZM][sx_i][iii + H_DNZ]);
		limit_range_all(U[sz_i][iii], U[sz_i][iii + H_DNY], Uf[FYP][sz_i][iii], Uf[FYM][sz_i][iii + H_DNY]);
		limit_range_all(U[sy_i][iii], U[sy_i][iii + H_DNZ], Uf[FZP][sy_i][iii], Uf[FZM][sy_i][iii + H_DNZ]);
	}
#pragma GCC ivdep
	for (integer iii = H_NX * H_NX; iii != H_N3 - H_NX * H_NX; ++iii) {
		limit_slope(Uf[FYM][sx_i][iii], U[sx_i][iii], Uf[FYP][sx_i][iii]);
		limit_slope(Uf[FZM][sx_i][iii], U[sx_i][iii], Uf[FZP][sx_i][iii]);
		limit_slope(Uf[FXM][sy_i][iii], U[sy_i][iii], Uf[FXP][sy_i][iii]);
		limit_slope(Uf[FZM][sy_i][iii], U[sy_i][iii], Uf[FZP][sy_i][iii]);
		limit_slope(Uf[FXM][sz_i][iii], U[sz_i][iii], Uf[FXP][sz_i][iii]);
		limit_slope(Uf[FYM][sz_i][iii], U[sz_i][iii], Uf[FYP][sz_i][iii]);

		real xy_ant = HALF * (Uf[FXP][sy_i][iii] - Uf[FXM][sy_i][iii]);
		xy_ant -= HALF * (Uf[FYP][sx_i][iii] - Uf[FYM][sx_i][iii]);

		real xz_ant = HALF * (Uf[FXP][sz_i][iii] - Uf[FXM][sz_i][iii]);
		xz_ant -= HALF * (Uf[FZP][sx_i][iii] - Uf[FZM][sx_i][iii]);

		real yz_ant = HALF * (Uf[FYP][sz_i][iii] - Uf[FYM][sz_i][iii]);
		yz_ant -= HALF * (Uf[FZP][sy_i][iii] - Uf[FZM][sy_i][iii]);

		for (integer face = 0; face != NFACE; ++face) {
			Uf[face][zz_i][iii] = U[zz_i][iii] - xy_ant * dx / 6.0;
			Uf[face][zy_i][iii] = U[zy_i][iii] + xz_ant * dx / 6.0;
			Uf[face][zx_i][iii] = U[zx_i][iii] - yz_ant * dx / 6.0;
		}
	}

	for (integer iii = 0; iii != H_N3; ++iii) {
		for (integer face = 0; face != NFACE; ++face) {
			real w = 0.0;
			for( integer si = 0; si != NSPECIES; ++si) {
				w += Uf[face][spc_i+si][iii];
			}
			if (w > ZERO) {
				for( integer si = 0; si != NSPECIES; ++si) {
					Uf[face][spc_i+si][iii] /= w;
				}
			}
		}
	}

	for (integer field = 0; field != NF; ++field) {
		if (field == pot_i) {
#pragma GCC ivdep
			for (integer iii = H_NX * H_NX; iii != H_N3 - H_NX * H_NX; ++iii) {
				const real phi_x = HALF * (Uf[FXM][field][iii] + Uf[FXP][field][iii - H_DNX]);
				const real phi_y = HALF * (Uf[FYM][field][iii] + Uf[FYP][field][iii - H_DNY]);
				const real phi_z = HALF * (Uf[FZM][field][iii] + Uf[FZP][field][iii - H_DNZ]);
				Uf[FXM][field][iii] = phi_x;
				Uf[FYM][field][iii] = phi_y;
				Uf[FZM][field][iii] = phi_z;
				Uf[FXP][field][iii - H_DNX] = phi_x;
				Uf[FYP][field][iii - H_DNY] = phi_y;
				Uf[FZP][field][iii - H_DNZ] = phi_z;
			}
		}
		if (field != rho_i) {
#pragma GCC ivdep
			for (integer iii = 0; iii != H_N3; ++iii) {
				U[field][iii] *= U[rho_i][iii];
				for (integer face = 0; face != NFACE; ++face) {
					Uf[face][field][iii] *= Uf[face][rho_i][iii];
				}
			}
		}
	}
	for (integer iii = 0; iii != H_N3; ++iii) {
		const real& rho = U[rho_i][iii];
		U[sx_i][iii] -= omega * X[YDIM][iii] * rho;
		U[sy_i][iii] += omega * X[XDIM][iii] * rho;
		U[zz_i][iii] += dx * dx * omega * rho / 6.0;
		U[egas_i][iii] += HALF * U[sx_i][iii] * U[sx_i][iii] / rho;
		U[egas_i][iii] += HALF * U[sy_i][iii] * U[sy_i][iii] / rho;
		U[egas_i][iii] += HALF * U[sz_i][iii] * U[sz_i][iii] / rho;
		U[tau_i][iii] = std::pow(U[tau_i][iii], 1.0 / fgamma);
	}
	for (integer i = H_BW - 1; i != H_NX - H_BW + 1; ++i) {
		for (integer j = H_BW - 1; j != H_NX - H_BW + 1; ++j) {
			for (integer k = H_BW - 1; k != H_NX - H_BW + 1; ++k) {
				const integer iii = hindex(i, j, k);
				for (integer face = 0; face != NFACE; ++face) {
					real x0 = ZERO;
					real y0 = ZERO;
					if (face == FXP) {
						x0 = +HALF * dx;
					} else if (face == FXM) {
						x0 = -HALF * dx;
					} else if (face == FYP) {
						y0 = +HALF * dx;
					} else if (face == FYM) {
						y0 = -HALF * dx;
					}
					Uf[face][sx_i][iii] -= omega * (X[YDIM][iii] + y0) * Uf[face][rho_i][iii];
					Uf[face][sy_i][iii] += omega * (X[XDIM][iii] + x0) * Uf[face][rho_i][iii];
					Uf[face][zz_i][iii] += dx * dx * omega * Uf[face][rho_i][iii] / 6.0;
					Uf[face][egas_i][iii] += HALF * Uf[face][sx_i][iii] * Uf[face][sx_i][iii] / Uf[face][rho_i][iii];
					Uf[face][egas_i][iii] += HALF * Uf[face][sy_i][iii] * Uf[face][sy_i][iii] / Uf[face][rho_i][iii];
					Uf[face][egas_i][iii] += HALF * Uf[face][sz_i][iii] * Uf[face][sz_i][iii] / Uf[face][rho_i][iii];
					Uf[face][tau_i][iii] = std::pow(Uf[face][tau_i][iii], 1.0 / fgamma);
				}
			}
		}
	}
}

real grid::compute_fluxes() {
	real max_lambda = ZERO;
	std::array<std::vector<real>, NF> ur, ul, f;
	std::vector<space_vector> x;

	const integer line_sz = H_NX - 2 * H_BW + 1;
	for (integer field = 0; field != NF; ++field) {
		ur[field].resize(line_sz);
		ul[field].resize(line_sz);
		f[field].resize(line_sz);
	}
	x.resize(line_sz);

	for (integer dim = 0; dim != NDIM; ++dim) {

		const integer dx_i = dim;
		const integer dy_i = (dim == XDIM ? YDIM : XDIM);
		const integer dz_i = (dim == ZDIM ? YDIM : ZDIM);
		const integer face_p = 2 * dim + 1;
		const integer face_m = 2 * dim;

		for (integer k = H_BW; k != H_NX - H_BW; ++k) {
			for (integer j = H_BW; j != H_NX - H_BW; ++j) {
				for (integer i = H_BW; i != H_NX - H_BW + 1; ++i) {
					const integer i0 = H_DN[dx_i] * i + H_DN[dy_i] * j + H_DN[dz_i] * k;
					const integer im = i0 - H_DN[dx_i];
					for (integer field = 0; field != NF; ++field) {
						ur[field][i - H_BW] = Uf[face_m][field][i0];
						ul[field][i - H_BW] = Uf[face_p][field][im];
					}
					for (integer d = 0; d != NDIM; ++d) {
						x[i - H_BW][d] = (X[d][i0] + X[d][im]) * HALF;
					}
				}
				const real this_max_lambda = roe_fluxes(f, ul, ur, x, omega, dim);
				max_lambda = std::max(max_lambda, this_max_lambda);
				for (integer field = 0; field != NF; ++field) {
					for (integer i = H_BW; i != H_NX - H_BW + 1; ++i) {
						const integer i0 = H_DN[dx_i] * i + H_DN[dy_i] * j + H_DN[dz_i] * k;
						F[dim][field][i0] = f[field][i - H_BW];
					}
				}
			}
		}
	}

	return max_lambda;
}

void grid::set_max_level(integer l) {
	max_level = l;
}

void grid::store() {
	for (integer field = 0; field != NF; ++field) {
#pragma GCC ivdep
		for (integer iii = 0; iii != H_N3; ++iii) {
			U0[field][iii] = U[field][iii];
		}
	}
	for (integer field = 0; field != NGF; ++field) {
#pragma GCC ivdep
		for (integer iii = 0; iii != G_N3; ++iii) {
			G0[field][iii] = G[field][iii];
		}
	}
	U_out0 = U_out;
}

void grid::restore() {
	for (integer field = 0; field != NF; ++field) {
#pragma GCC ivdep
		for (integer iii = 0; iii != H_N3; ++iii) {
			U[field][iii] = U0[field][iii];
		}
	}
	for (integer field = 0; field != NGF; ++field) {
#pragma GCC ivdep
		for (integer iii = 0; iii != G_N3; ++iii) {
			G[field][iii] = G0[field][iii];
		}
	}
	U_out = U_out0;
}

void grid::boundaries() {
	for (integer face = 0; face != NFACE; ++face) {
		set_physical_boundaries(face);
	}
}

void grid::set_physical_boundaries(const geo::face& face) {
	const auto dim = face.get_dimension();
	const auto side = face.get_side();
	const integer dni = dim == XDIM ? H_DNY : H_DNX;
	const integer dnj = dim == ZDIM ? H_DNY : H_DNZ;
	const integer dnk = H_DN[dim];
	const integer klb = side == geo::MINUS ? 0 : H_NX - H_BW;
	const integer kub = side == geo::MINUS ? H_BW : H_NX;
	const integer ilb = 0;
	const integer iub = H_NX;
	const integer jlb = 0;
	const integer jub = H_NX;

	for (integer field = 0; field != NF; ++field) {
		for (integer k = klb; k != kub; ++k) {
			for (integer j = jlb; j != jub; ++j) {
				for (integer i = ilb; i != iub; ++i) {
					integer k0;
					switch (boundary_types[face]) {
					case REFLECT:
						k0 = side == geo::MINUS ? (2 * H_BW - k - 1) : (2 * (H_NX - H_BW) - k - 1);
						break;
					case OUTFLOW:
						k0 = side == geo::MINUS ? H_BW : H_NX - H_BW - 1;
						break;
					default:
						k0 = -1;
						assert(false);
						abort();
					}
					const real value = U[field][i * dni + j * dnj + k0 * dnk];
					const integer iii = i * dni + j * dnj + k * dnk;
					real& ref = U[field][iii];
					if (field == sx_i + dim) {
						real s0;
						if (field == sx_i) {
							s0 = -omega * X[YDIM][iii] * U[rho_i][iii];
						} else if (field == sy_i) {
							s0 = +omega * X[XDIM][iii] * U[rho_i][iii];
						} else {
							s0 = ZERO;
						}
						switch (boundary_types[face]) {
						case REFLECT:
							ref = -value;
							break;
						case OUTFLOW:
							const real before = value;
							if (side == geo::MINUS) {
								ref = s0 + std::min(value - s0, ZERO);
							} else {
								ref = s0 + std::max(value - s0, ZERO);
							}
							const real after = ref;
							assert(rho_i < field);
							assert(egas_i < field);
							real this_rho = U[rho_i][iii];
							if (this_rho != ZERO) {
								U[egas_i][iii] += HALF * (after * after - before * before) / this_rho;
							}
							break;
						}
					} else {
						ref = +value;
					}
				}
			}
		}
	}

}

void grid::compute_sources(real t) {
	for (integer i = H_BW; i != H_NX - H_BW; ++i) {
		for (integer j = H_BW; j != H_NX - H_BW; ++j) {
#pragma GCC ivdep
			for (integer k = H_BW; k != H_NX - H_BW; ++k) {
				const integer iii = hindex(i, j, k);
				const integer iiig = gindex(i + G_BW - H_BW, j + G_BW - H_BW, k + G_BW - H_BW);
				for (integer field = 0; field != NF; ++field) {
					src[field][iii] = ZERO;
				}
				const real rho = U[rho_i][iii];
				src[zx_i][iii] = (-(F[YDIM][sz_i][iii + H_DNY] + F[YDIM][sz_i][iii])
						+ (F[ZDIM][sy_i][iii + H_DNZ] + F[ZDIM][sy_i][iii])) * HALF;
				src[zy_i][iii] = (+(F[XDIM][sz_i][iii + H_DNX] + F[XDIM][sz_i][iii])
						- (F[ZDIM][sx_i][iii + H_DNZ] + F[ZDIM][sx_i][iii])) * HALF;
				src[zz_i][iii] = (-(F[XDIM][sy_i][iii + H_DNX] + F[XDIM][sy_i][iii])
						+ (F[YDIM][sx_i][iii + H_DNY] + F[YDIM][sx_i][iii])) * HALF;
				src[sx_i][iii] += rho * G[gx_i][iiig];
				src[sy_i][iii] += rho * G[gy_i][iiig];
				src[sz_i][iii] += rho * G[gz_i][iiig];
				src[sx_i][iii] += omega * U[sy_i][iii];
				src[sy_i][iii] -= omega * U[sx_i][iii];
				src[egas_i][iii] -= omega * X[YDIM][iii] * rho * G[gx_i][iiig];
				src[egas_i][iii] += omega * X[XDIM][iii] * rho * G[gy_i][iiig];
#ifdef USE_DRIVING
				const real period = (2.0 * M_PI / grid::omega);
				if (t < DRIVING_TIME * period) {
					const real ff = -DRIVING_RATE / period;
					const real rho = U[rho_i][iii];
					const real sx = U[sx_i][iii];
					const real sy = U[sy_i][iii];
					const real zz = U[zz_i][iii];
					const real x = X[XDIM][iii];
					const real y = X[YDIM][iii];
					const real R = std::sqrt(x * x + y * y);
					const real lz = (x * sy - y * sx);
					const real dsx = -y / R / R * lz * ff;
					const real dsy = +x / R / R * lz * ff;
					src[sx_i][iii] += dsx;
					src[sy_i][iii] += dsy;
					src[egas_i][iii] += (sx * dsx + sy * dsy) / rho;
					src[zz_i][iii] += ff * zz;

				}
#endif

			}
		}
	}
}

void grid::compute_dudt() {
	for (integer i = H_BW; i != H_NX - H_BW; ++i) {
		for (integer j = H_BW; j != H_NX - H_BW; ++j) {
			for (integer field = 0; field != NF; ++field) {
#pragma GCC ivdep
				for (integer k = H_BW; k != H_NX - H_BW; ++k) {
					const integer iii = hindex(i, j, k);
					dUdt[field][iii] = ZERO;
					dUdt[field][iii] -= (F[XDIM][field][iii + H_DNX] - F[XDIM][field][iii]) / dx;
					dUdt[field][iii] -= (F[YDIM][field][iii + H_DNY] - F[YDIM][field][iii]) / dx;
					dUdt[field][iii] -= (F[ZDIM][field][iii + H_DNZ] - F[ZDIM][field][iii]) / dx;
					dUdt[field][iii] += src[field][iii];
				}
			}
#pragma GCC ivdep
			for (integer k = H_BW; k != H_NX - H_BW; ++k) {
				const integer iii = hindex(i, j, k);
				dUdt[egas_i][iii] += dUdt[pot_i][iii];
				dUdt[pot_i][iii] = ZERO;
			}
#pragma GCC ivdep
			for (integer k = H_BW; k != H_NX - H_BW; ++k) {
				const integer iii = hindex(i, j, k);
				const integer iiig = gindex(i + G_BW - H_BW, j + G_BW - H_BW, k + G_BW - H_BW);
				dUdt[egas_i][iii] -= (dUdt[rho_i][iii] * G[phi_i][iiig]) * HALF;
			}
		}
	}
//	solve_gravity(DRHODT);
}

void grid::egas_to_etot() {
	for (integer i = H_BW; i != H_NX - H_BW; ++i) {
		for (integer j = H_BW; j != H_NX - H_BW; ++j) {
#pragma GCC ivdep
			for (integer k = H_BW; k != H_NX - H_BW; ++k) {
				const integer iii = hindex(i, j, k);
				U[egas_i][iii] += U[pot_i][iii] * HALF;
			}
		}
	}
}

void grid::etot_to_egas() {
	for (integer i = H_BW; i != H_NX - H_BW; ++i) {
		for (integer j = H_BW; j != H_NX - H_BW; ++j) {
#pragma GCC ivdep
			for (integer k = H_BW; k != H_NX - H_BW; ++k) {
				const integer iii = hindex(i, j, k);
				U[egas_i][iii] -= U[pot_i][iii] * HALF;
			}
		}
	}
}

void grid::next_u(integer rk, real t, real dt) {

	for (integer i = H_BW; i != H_NX - H_BW; ++i) {
		for (integer j = H_BW; j != H_NX - H_BW; ++j) {
#pragma GCC ivdep
			for (integer k = H_BW; k != H_NX - H_BW; ++k) {
				const integer iii = hindex(i, j, k);
				dUdt[egas_i][iii] += (dphi_dt[iii] * U[rho_i][iii]) * HALF;
				dUdt[zx_i][iii] -= omega * X[ZDIM][iii] * U[sx_i][iii];
				dUdt[zy_i][iii] -= omega * X[ZDIM][iii] * U[sy_i][iii];
				dUdt[zz_i][iii] += omega * (X[XDIM][iii] * U[sx_i][iii] + X[YDIM][iii] * U[sy_i][iii]);
			}
		}
	}

	std::vector<real> du_out(NF, ZERO);

	std::vector<real> ds(NDIM, ZERO);
	for (integer i = H_BW; i != H_NX - H_BW; ++i) {
		for (integer j = H_BW; j != H_NX - H_BW; ++j) {
			for (integer k = H_BW; k != H_NX - H_BW; ++k) {
				const integer iii = hindex(i, j, k);
				for (integer field = 0; field != NF; ++field) {
					const real u1 = U[field][iii] + dUdt[field][iii] * dt;
					const real u0 = U0[field][iii];
					U[field][iii] = (ONE - rk_beta[rk]) * u0 + rk_beta[rk] * u1;
				}
			}
		}
	}

	du_out[sx_i] += omega * U_out[sy_i] * dt;
	du_out[sy_i] -= omega * U_out[sx_i] * dt;

	for (integer i = H_BW; i != H_NX - H_BW; ++i) {
		for (integer j = H_BW; j != H_NX - H_BW; ++j) {
			const real dx2 = dx * dx;
			const integer iii_p = H_DNX * (H_NX - H_BW) + H_DNY * i + H_DNZ * j;
			const integer jjj_p = H_DNY * (H_NX - H_BW) + H_DNZ * i + H_DNX * j;
			const integer kkk_p = H_DNZ * (H_NX - H_BW) + H_DNX * i + H_DNY * j;
			const integer iii_m = H_DNX * (H_BW) + H_DNY * i + H_DNZ * j;
			const integer jjj_m = H_DNY * (H_BW) + H_DNZ * i + H_DNX * j;
			const integer kkk_m = H_DNZ * (H_BW) + H_DNX * i + H_DNY * j;
			std::vector<real> du(NF);
			for (integer field = 0; field != NF; ++field) {
				//	if (field < zx_i || field > zz_i) {
				du[field] = ZERO;
				if (X[XDIM][iii_p] + pivot[XDIM] > scaling_factor) {
					du[field] += (F[XDIM][field][iii_p]) * dx2;
				}
				if (X[YDIM][jjj_p] + pivot[YDIM] > scaling_factor) {
					du[field] += (F[YDIM][field][jjj_p]) * dx2;
				}
				if (X[ZDIM][kkk_p] + pivot[ZDIM] > scaling_factor) {
					du[field] += (F[ZDIM][field][kkk_p]) * dx2;
				}
				if (X[XDIM][iii_m] + pivot[XDIM] < -scaling_factor + dx) {
					du[field] += (-F[XDIM][field][iii_m]) * dx2;
				}
				if (X[YDIM][jjj_m] + pivot[YDIM] < -scaling_factor + dx) {
					du[field] += (-F[YDIM][field][jjj_m]) * dx2;
				}
				if (X[ZDIM][kkk_m] + pivot[ZDIM] < -scaling_factor + dx) {
					du[field] += (-F[ZDIM][field][kkk_m]) * dx2;
				}
				//			}
			}

			if (X[XDIM][iii_p] + pivot[XDIM] > scaling_factor) {
				const real xp = X[XDIM][iii_p] - HALF * dx;
				du[zx_i] += (X[YDIM][iii_p] * F[XDIM][sz_i][iii_p]) * dx2;
				du[zx_i] -= (X[ZDIM][iii_p] * F[XDIM][sy_i][iii_p]) * dx2;
				du[zy_i] -= (xp * F[XDIM][sz_i][iii_p]) * dx2;
				du[zy_i] += (X[ZDIM][iii_p] * F[XDIM][sx_i][iii_p]) * dx2;
				du[zz_i] += (xp * F[XDIM][sy_i][iii_p]) * dx2;
				du[zz_i] -= (X[YDIM][iii_p] * F[XDIM][sx_i][iii_p]) * dx2;
			}
			if (X[YDIM][jjj_p] + pivot[YDIM] > scaling_factor) {
				const real yp = X[YDIM][jjj_p] - HALF * dx;
				du[zx_i] += (yp * F[YDIM][sz_i][jjj_p]) * dx2;
				du[zx_i] -= (X[ZDIM][jjj_p] * F[YDIM][sy_i][jjj_p]) * dx2;
				du[zy_i] -= (X[XDIM][jjj_p] * F[YDIM][sz_i][jjj_p]) * dx2;
				du[zy_i] += (X[ZDIM][jjj_p] * F[YDIM][sx_i][jjj_p]) * dx2;
				du[zz_i] += (X[XDIM][jjj_p] * F[YDIM][sy_i][jjj_p]) * dx2;
				du[zz_i] -= (yp * F[YDIM][sx_i][jjj_p]) * dx2;
			}
			if (X[ZDIM][kkk_p] + pivot[ZDIM] > scaling_factor) {
				const real zp = X[ZDIM][kkk_p] - HALF * dx;
				du[zx_i] -= (zp * F[ZDIM][sy_i][kkk_p]) * dx2;
				du[zx_i] += (X[YDIM][kkk_p] * F[ZDIM][sz_i][kkk_p]) * dx2;
				du[zy_i] += (zp * F[ZDIM][sx_i][kkk_p]) * dx2;
				du[zy_i] -= (X[XDIM][kkk_p] * F[ZDIM][sz_i][kkk_p]) * dx2;
				du[zz_i] += (X[XDIM][kkk_p] * F[ZDIM][sy_i][kkk_p]) * dx2;
				du[zz_i] -= (X[YDIM][kkk_p] * F[ZDIM][sx_i][kkk_p]) * dx2;
			}

			if (X[XDIM][iii_m] + pivot[XDIM] < -scaling_factor + dx) {
				const real xm = X[XDIM][iii_m] - HALF * dx;
				du[zx_i] += (-X[YDIM][iii_m] * F[XDIM][sz_i][iii_m]) * dx2;
				du[zx_i] -= (-X[ZDIM][iii_m] * F[XDIM][sy_i][iii_m]) * dx2;
				du[zy_i] -= (-xm * F[XDIM][sz_i][iii_m]) * dx2;
				du[zy_i] += (-X[ZDIM][iii_m] * F[XDIM][sx_i][iii_m]) * dx2;
				du[zz_i] += (-xm * F[XDIM][sy_i][iii_m]) * dx2;
				du[zz_i] -= (-X[YDIM][iii_m] * F[XDIM][sx_i][iii_m]) * dx2;
			}
			if (X[YDIM][jjj_m] + pivot[YDIM] < -scaling_factor + dx) {
				const real ym = X[YDIM][jjj_m] - HALF * dx;
				du[zx_i] -= (-X[ZDIM][jjj_m] * F[YDIM][sy_i][jjj_m]) * dx2;
				du[zx_i] += (-ym * F[YDIM][sz_i][jjj_m]) * dx2;
				du[zy_i] -= (-X[XDIM][jjj_m] * F[YDIM][sz_i][jjj_m]) * dx2;
				du[zy_i] += (-X[ZDIM][jjj_m] * F[YDIM][sx_i][jjj_m]) * dx2;
				du[zz_i] += (-X[XDIM][jjj_m] * F[YDIM][sy_i][jjj_m]) * dx2;
				du[zz_i] -= (-ym * F[YDIM][sx_i][jjj_m]) * dx2;
			}
			if (X[ZDIM][kkk_m] + pivot[ZDIM] < -scaling_factor + dx) {
				const real zm = X[ZDIM][kkk_m] - HALF * dx;
				du[zx_i] -= (-zm * F[ZDIM][sy_i][kkk_m]) * dx2;
				du[zx_i] += (-X[YDIM][kkk_m] * F[ZDIM][sz_i][kkk_m]) * dx2;
				du[zy_i] += (-zm * F[ZDIM][sx_i][kkk_m]) * dx2;
				du[zy_i] -= (-X[XDIM][kkk_m] * F[ZDIM][sz_i][kkk_m]) * dx2;
				du[zz_i] += (-X[XDIM][kkk_m] * F[ZDIM][sy_i][kkk_m]) * dx2;
				du[zz_i] -= (-X[YDIM][kkk_m] * F[ZDIM][sx_i][kkk_m]) * dx2;
			}
			for (integer field = 0; field != NF; ++field) {
				du_out[field] += du[field] * dt;
			}
		}
	}
#pragma GCC ivdep
	for (integer field = 0; field != NF; ++field) {
		const real out1 = U_out[field] + du_out[field];
		const real out0 = U_out0[field];
		U_out[field] = (ONE - rk_beta[rk]) * out0 + rk_beta[rk] * out1;
	}
	for (integer i = H_BW; i != H_NX - H_BW; ++i) {
		for (integer j = H_BW; j != H_NX - H_BW; ++j) {
			for (integer k = H_BW; k != H_NX - H_BW; ++k) {
				const integer iii = hindex(i, j, k);
				U[rho_i][iii] = ZERO;
				for( integer si = 0; si != NSPECIES; ++si) {
					U[rho_i][iii] += U[spc_i+si][iii];
				}
				if (U[tau_i][iii] < ZERO) {
					printf("Tau is negative- %e\n", double(U[tau_i][iii]));
					abort();
				} else if (U[rho_i][iii] <= ZERO) {
					printf("Rho is non-positive - %e\n", double(U[rho_i][iii]));
					abort();
				}

			}
		}
	}
}

void grid::dual_energy_update() {
	for (integer i = H_BW; i != H_NX - H_BW; ++i) {
		for (integer j = H_BW; j != H_NX - H_BW; ++j) {
			for (integer k = H_BW; k != H_NX - H_BW; ++k) {
				const integer iii = hindex(i, j, k);
				real ek = ZERO;
				ek += HALF * pow(U[sx_i][iii], 2) / U[rho_i][iii];
				ek += HALF * pow(U[sy_i][iii], 2) / U[rho_i][iii];
				ek += HALF * pow(U[sz_i][iii], 2) / U[rho_i][iii];
				//	ek += HALF * pow(U[zx_i][iii], 2) / U[rho_i][iii] / (dx*dx);
				//	ek += HALF * pow(U[zy_i][iii], 2) / U[rho_i][iii] / (dx*dx);
				//	ek += HALF * pow(U[zz_i][iii], 2) / U[rho_i][iii] / (dx*dx);
				real ei = U[egas_i][iii] - ek;
				real et = U[egas_i][iii];
				et = std::max(et, U[egas_i][iii + H_DNX]);
				et = std::max(et, U[egas_i][iii - H_DNX]);
				et = std::max(et, U[egas_i][iii + H_DNY]);
				et = std::max(et, U[egas_i][iii - H_DNY]);
				et = std::max(et, U[egas_i][iii + H_DNZ]);
				et = std::max(et, U[egas_i][iii - H_DNZ]);
				if (ei > de_switch1 * et) {
					U[tau_i][iii] = pow(ei, ONE / fgamma);
				}
			}
		}
	}

}

std::vector<real> grid::conserved_outflows() const {
	auto Uret = U_out;
	Uret[egas_i] += Uret[pot_i];
	return Uret;
}
