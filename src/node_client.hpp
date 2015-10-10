/*
 * node_client.hpp
 *
 *  Created on: Jun 11, 2015
 *      Author: dmarce1
 */

#ifndef NODE_CLIENT_HPP_
#define NODE_CLIENT_HPP_

#include "defs.hpp"
#include "node_location.hpp"
#include "taylor.hpp"
#include "grid.hpp"

struct diagnostics_t;
//#include <boost/mpi/packed_iarchive.hpp>

class node_server;

class node_client {
private:
//	hpx::shared_future<hpx::id_type> id_fut;
	hpx::id_type id;
public:
	template<class Arc>
	void serialize(Arc& arc, unsigned) {
		arc & id;
	}
	bool empty() const;
	hpx::id_type get_gid() const;
	node_client& operator=(hpx::future<hpx::id_type>&& fut );
	node_client& operator=(const hpx::id_type& _id );
	node_client(hpx::future<hpx::id_type>&& fut );
	node_client(const hpx::id_type& _id );
	hpx::future<void> send_hydro_children( std::vector<real>&&,  integer ci) const;
	hpx::future<void> send_hydro_flux_correct( std::vector<real>&&,  integer face, integer ci) const;
	hpx::future<hpx::id_type> load_node(std::size_t fpos, const std::string& fname, const node_location&, const hpx::id_type& );
	hpx::future<diagnostics_t> diagnostics() const;
	node_client();
	hpx::future<std::vector<hpx::id_type>> get_nieces(const hpx::id_type&, integer) const;
	hpx::future<void> set_aunt(const hpx::id_type&, integer) const;
	hpx::future<node_server*> get_ptr() const;
	hpx::future<void> form_tree(const hpx::id_type&, const hpx::id_type&, const std::vector<hpx::id_type>& );
	hpx::future<hpx::id_type> get_child_client(integer ci);
	hpx::future<void> regrid_scatter(integer, integer) const;
	hpx::future<integer> regrid_gather() const;
	hpx::future<void> send_hydro_boundary(std::vector<real>&&, integer face) const;
//r	hpx::future<void> send_hydro_amr_to_parent(std::vector<real>&&, integer ci, integer rk, integer face) const;
	hpx::future<void> send_gravity_boundary(std::vector<real>&&, integer face, bool monopole) const;
	hpx::future<void> send_gravity_multipoles(multipole_pass_type&&, integer ci) const;
	hpx::future<void> send_gravity_expansions(expansion_pass_type&&) const;
	hpx::future<void> step() const;
	hpx::future<void> start_run() const;
	hpx::future<void> regrid(const hpx::id_type&) const;
	hpx::future<void> solve_gravity(bool ene) const;
	hpx::future<hpx::id_type> copy_to_locality(const hpx::id_type& ) const;
	hpx::future<void> set_grid(std::vector<real>&&,std::vector<real>&&) const;
	hpx::future<real> timestep_driver() const;
	hpx::future<void> timestep_driver_ascend(real) const;
	hpx::future<real> timestep_driver_descend() const;
	hpx::future<grid::output_list_type> output() const;
	hpx::future<std::pair<std::size_t,std::size_t>> save(integer loc_id, std::string) const;
	hpx::future<bool> check_for_refinement() const;
	hpx::future<void> force_nodes_to_exist(std::list<node_location>&& loc) const;

//	hpx::future<void> find_family() const;

		};
#endif /* NODE_CLIENT_HPP_ */
