#include "../pXest/pXest.h"
#include "../Solver/cg_eigs.h"
#include "../LinearAlgebra/d4est_linalg.h"
#include "../Utilities/d4est_util.h"
#include "sc_reduce.h"
#include <signal.h>


static void
tridiag_gershgorin
(
 int i,
 int local_nodes,
 double a0,
 double b0,
 double a1,
 double b1,
 double* max,
 double* min
)
{
  double diag, offdiag_sum;
  if (i != 0 && i < local_nodes) {
    diag = (1./a1 + b0/a0);
    offdiag_sum = fabs(sqrt(b1)/a1) + fabs(sqrt(b0)/a0);
  }
  else if (i == 0){
    diag = 1./a1;
    offdiag_sum = sqrt(b1)/a1;
  }
  else {
    diag = 1./a1 + b0/a0;
    offdiag_sum = fabs(sqrt(b0)/a0);
  }
  *max = diag + offdiag_sum;
  *min = diag - offdiag_sum;
}


void
cg_eigs
(
 p4est_t* p4est,
 d4est_elliptic_data_t* vecs,
 d4est_elliptic_eqns_t* fcns,
 d4est_ghost_t* ghost,
 d4est_ghost_data_t* ghost_data,
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 d4est_quadrature_t* d4est_quad,
 d4est_mesh_data_t* d4est_factors,
 int imax,
 double* eig_max
)
{
  
  /* debug("Conjugate Gradient Solver Begins"); */

  /* p4est_ghost_t* ghost; */
  /* element_data_t* ghost_data; */
  /* create the ghost quadrants */
  /* ghost = p4est_ghost_new (p4est, P4EST_CONNECT_FACE); */
  /* create space for storing the ghost data */
  /* ghost_data = P4EST_ALLOC (element_data_t, */
                            /* ghost->ghosts.elem_count); */

  /* int max_nodes = d4est_lgl_get_nodes((P4EST_DIM), d4est_operators_fetch_max_degree_used()); */
  /* ghost_data = malloc( (sizeof(element_data_t) + */
                        /* sizeof(double)*(max_nodes-1))*ghost->ghosts.elem_count ); */
  int local_nodes;
  double delta_new, delta_old;

  /* get rid of warnings */
  double alpha = -1.;
  double beta = -1.;
  
  double* Au; 

  double* rhs;

  double* d;
  double* r;
  
  local_nodes = vecs->local_nodes;
  Au = vecs->Au;
  double* tmp = vecs->u;
  double* u;
  /* if (use_zero_vec_as_initial){ */
  /*   u = P4EST_ALLOC_ZERO(double, vecs->local_nodes); */
  /*   vecs->u = u; */
  /* } */
  /* else{ */
    u = vecs->u;
  /* } */
  rhs = vecs->rhs; 

  double d_dot_Au;
  
  d = P4EST_ALLOC(double, local_nodes);
  r = P4EST_ALLOC(double, local_nodes);

  /* Build rhs of the weak dG equations */
  /* debug("Build RHS in CG solve starts"); */
  /* fcns->build_rhs(p4est, ghost, ghost_data, vecs); */
  /* debug("Build RHS in CG solve ends"); */
  
  /* first iteration data, store Au in r */  
  d4est_elliptic_eqns_apply_lhs
    (
     p4est,
     ghost,
     ghost_data,
     fcns,
     vecs,
     d4est_ops,
     d4est_geom,
     d4est_quad,
     d4est_factors
    );

  d4est_util_copy_1st_to_2nd(Au, r, local_nodes);
  /* r = f - Au ; Au is stored in r so r = rhs - r */
  d4est_linalg_vec_xpby(rhs, -1., r, local_nodes);
  d4est_util_copy_1st_to_2nd(r, d, local_nodes);
  delta_new = d4est_linalg_vec_dot(r,r,local_nodes);
  /* delta_new = (element_data_compute_l2_norm(p4est, r)); */

  double delta_new_global = -1;
  double d_dot_Au_global = -1;
  
  sc_allreduce
    (
     &delta_new,
     &delta_new_global,
     1,
     sc_MPI_DOUBLE,
     sc_MPI_SUM,
     sc_MPI_COMM_WORLD
    );

  delta_new = delta_new_global;
  
  /* start working on d */
  vecs->u = d;

  int i;
  double alpha_old, beta_old;
  double temp_max, temp_min;

  /* d4est_util_print_matrix(u, vecs->local_nodes, 1, "u in cg_eigs",0); */
  
  for (i = 0; i < imax; i++){
    
  /* while ( iterate == 1) { */

    /* d4est_util_print_matrix( u, vecs->local_nodes, 1, "u = ", 0); */
    
    /* Au = A*d; */
    
  d4est_elliptic_eqns_apply_lhs
    (
     p4est,
     ghost,
     ghost_data,
     fcns,
     vecs,
     d4est_ops,
     d4est_geom,
     d4est_quad,
     d4est_factors
    );

    d_dot_Au = d4est_linalg_vec_dot(d,Au,local_nodes);

    sc_allreduce
      (
       &d_dot_Au,
       &d_dot_Au_global,
       1,
       sc_MPI_DOUBLE,
       sc_MPI_SUM,
       sc_MPI_COMM_WORLD
    );

    d_dot_Au = d_dot_Au_global;
    alpha_old = alpha;
    alpha = delta_new/d_dot_Au;
    
    d4est_linalg_vec_axpy(alpha, d, u, local_nodes);

    /* r = r - Au*alpha */
    d4est_linalg_vec_axpy(-alpha, Au, r, local_nodes);

    delta_old = delta_new;
    delta_new = d4est_linalg_vec_dot(r, r, local_nodes);

    sc_allreduce
      (
       &delta_new,
       &delta_new_global,
       1,
       sc_MPI_DOUBLE,
       sc_MPI_SUM,
       sc_MPI_COMM_WORLD
    );
    delta_new = delta_new_global;

    beta_old = beta;
    beta = delta_new/delta_old;
    d4est_linalg_vec_xpby(r, beta, d, local_nodes);

  tridiag_gershgorin(i, local_nodes, alpha_old, beta_old, alpha, beta, &temp_max, &temp_min);

  /* printf("alpha_old, alpha, beta_old, beta = %f,%f,%f,%f\n", alpha_old, alpha, beta_old, beta); */
  /* printf("i = %d, eig_min, eig_max, temp_min, temp_max = %f,%f,%f,%f\n", i , *eig_min, *eig_max, temp_min, temp_max); */

  if (i > 0){
      /* *eig_min = d4est_util_min( *eig_min, temp_min ); */
      *eig_max = d4est_util_max( *eig_max, temp_max );  
    }
  else{
    /* *eig_min = temp_min; */
    *eig_max = temp_max;
  }
  /* printf("current eigmax = %.25f\n", *eig_max); */
  }

  
    

  /* set back from d */
  vecs->u = u;

  P4EST_FREE(d);
  /* if (use_zero_vec_as_initial){ */
  /*   vecs->u = tmp; */
  /*   P4EST_FREE(u); */
  /* } */
  P4EST_FREE(r);

  /* p4est_ghost_destroy(ghost); */
  /* P4EST_FREE(ghost_data); */

  /* debug("Conjugate Gradient Solver Ends"); */
  
}
