#include <sc_reduce.h>
#include <pXest.h>
#include <d4est_util.h>
#include <d4est_linalg.h>
#include <problem.h>
#include <d4est_elliptic_data.h>
#include <d4est_elliptic_eqns.h>
#include <d4est_estimator_bi.h>
#include <d4est_solver_cg.h>
#include <d4est_amr.h>
#include <d4est_amr_smooth_pred.h>
#include <d4est_geometry.h>
#include <d4est_geometry_cubed_sphere.h>
#include <d4est_vtk.h>
#include <d4est_norms.h>
#include <d4est_mesh.h>
#include <ini.h>
#include <d4est_element_data.h>
#include <d4est_estimator_stats.h>
#include <d4est_poisson.h>
#include <d4est_poisson_flux_sipg.h>
#include <d4est_solver_newton.h>
#include <multigrid.h>
#include <krylov_pc_multigrid.h>
#include <multigrid_logger_residual.h>
#include <multigrid_element_data_updater.h>
#include <multigrid_matrix_operator.h>
#include <krylov_petsc.h>
#include <newton_petsc.h>
#include <d4est_util.h>
#include <d4est_checkpoint.h>
#include <d4est_h5.h>
#include <time.h>
#include <zlog.h>
#include "constant_density_star_fcns.h"


typedef struct {
  
  int do_not_solve;
  int amr_level_for_uniform_p;
  
} constant_density_star_init_params_t;

static
int two_punctures_init_params_handler
(
 void* user,
 const char* section,
 const char* name,
 const char* value
)
{
  constant_density_star_init_params_t* pconfig = (constant_density_star_init_params_t*)user;
  if (d4est_util_match_couple(section,"problem",name,"do_not_solve")) {
    D4EST_ASSERT(pconfig->do_not_solve == -1);
    pconfig->do_not_solve = atoi(value);
  }
  else if (d4est_util_match_couple(section,"amr",name,"amr_level_for_uniform_p")) {
    D4EST_ASSERT(pconfig->amr_level_for_uniform_p == -1);
    pconfig->amr_level_for_uniform_p = atoi(value);
  }
  else {
    return 0;  /* unknown section/name, error */
  }
  return 1;
}


static
constant_density_star_init_params_t
constant_density_star_init_params_input
(
 const char* input_file
)
{
  constant_density_star_init_params_t input;
  input.do_not_solve = -1;
  input.amr_level_for_uniform_p = -1;

  if (ini_parse(input_file, two_punctures_init_params_handler, &input) < 0) {
    D4EST_ABORT("Can't load input file");
  }

  D4EST_CHECK_INPUT("problem", input.do_not_solve, -1);
  D4EST_CHECK_INPUT("amr", input.amr_level_for_uniform_p, -1);
  
  return input;
}

static
int
amr_mark_element
(
 p4est_t* p4est,
 double eta2,
 d4est_estimator_stats_t* stats,
 d4est_element_data_t* elem_data,
 d4est_amr_smooth_pred_params_t* params,
 void* user
)
{
  problem_ctx_t* ctx = user;
  if (p4est->local_num_quadrants*p4est->mpisize < params->inflation_size){
    double eta2_percentile
      = d4est_estimator_stats_get_percentile(stats,25);
    return ((eta2 >= eta2_percentile) || fabs(eta2 - eta2_percentile) < eta2*1e-4);
  }
  else{
    double eta2_percentile
      = d4est_estimator_stats_get_percentile(stats,params->percentile);
    return ((eta2 >= eta2_percentile) || fabs(eta2 - eta2_percentile) < eta2*1e-4);
  }
}

static
gamma_params_t
amr_set_element_gamma
(
 p4est_t* p4est,
 d4est_estimator_stats_t* stats,
 d4est_element_data_t* elem_data,
 d4est_amr_smooth_pred_params_t* params,
 void* user
)
{
  problem_ctx_t* ctx = user;
  
  gamma_params_t gamma_hpn;
  gamma_hpn.gamma_h = params->gamma_h;
  gamma_hpn.gamma_p = params->gamma_p;
  gamma_hpn.gamma_n = params->gamma_n;

  return gamma_hpn;
}


void
problem_init
(
 p4est_t* p4est,
 d4est_ghost_t** d4est_ghost,
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 d4est_quadrature_t* d4est_quad,
 d4est_mesh_data_t* d4est_factors,
 d4est_mesh_initial_extents_t* initial_extents,
 const char* input_file,
 sc_MPI_Comm mpicomm
)
{
  zlog_category_t *c_default = zlog_get_category("problem");

  int initial_nodes = initial_extents->initial_nodes;
  constant_density_star_init_params_t init_params = constant_density_star_init_params_input(input_file);

  constant_density_star_params_t constant_density_star_params = constant_density_star_input(input_file);
  
  d4est_poisson_dirichlet_bc_t bc_data_for_jac;
  bc_data_for_jac.dirichlet_fcn = zero_fcn;
  bc_data_for_jac.user = &constant_density_star_params;
  bc_data_for_jac.eval_method = EVAL_BNDRY_FCN_ON_LOBATTO;
  
  d4est_poisson_dirichlet_bc_t bc_data_for_res;
  bc_data_for_res.dirichlet_fcn = constant_density_star_boundary_fcn;
  bc_data_for_res.user = &constant_density_star_params;
  bc_data_for_res.eval_method = EVAL_BNDRY_FCN_ON_LOBATTO;
                                          
  d4est_poisson_flux_data_t* flux_data_for_jac =
    d4est_poisson_flux_new(p4est, input_file, BC_DIRICHLET, &bc_data_for_jac);
  d4est_poisson_flux_data_t* flux_data_for_res = d4est_poisson_flux_new(p4est, input_file, BC_DIRICHLET, &bc_data_for_res);

  problem_ctx_t ctx;
  ctx.constant_density_star_params = &constant_density_star_params;
  ctx.flux_data_for_jac = flux_data_for_jac;
  ctx.flux_data_for_res = flux_data_for_res;
                           
  d4est_elliptic_eqns_t prob_fcns;
  prob_fcns.build_residual = constant_density_star_build_residual;
  prob_fcns.apply_lhs = constant_density_star_apply_jac;
  prob_fcns.user = &ctx;
  
  
  d4est_elliptic_data_t prob_vecs;
  prob_vecs.Au = P4EST_ALLOC(double, initial_nodes);
  prob_vecs.u = P4EST_ALLOC(double, initial_nodes);
  prob_vecs.local_nodes = initial_nodes;

  d4est_poisson_flux_sipg_params_t* sipg_params = flux_data_for_jac->flux_data;

 
  d4est_estimator_bi_penalty_data_t penalty_data;
  penalty_data.u_penalty_fcn = houston_u_prefactor_maxp_minh;
  penalty_data.size_params = NULL;
  penalty_data.u_dirichlet_penalty_fcn = houston_u_dirichlet_prefactor_maxp_minh;
  penalty_data.gradu_penalty_fcn = houston_gradu_prefactor_maxp_minh;
  penalty_data.penalty_prefactor = sipg_params->sipg_penalty_prefactor;
  penalty_data.sipg_flux_h = sipg_params->sipg_flux_h;
  penalty_data.user = &constant_density_star_params;
  
  d4est_amr_smooth_pred_marker_t amr_marker;
  amr_marker.user = (void*)&ctx;
  amr_marker.mark_element_fcn = amr_mark_element;
  amr_marker.set_element_gamma_fcn = amr_set_element_gamma;

  d4est_amr_t* d4est_amr =
    d4est_amr_init
    (
     p4est,
     input_file,
     &amr_marker
    );

  d4est_amr_t* d4est_amr_uniform_p = d4est_amr_init_uniform_p(p4est,d4est_amr->max_degree,d4est_amr->num_of_amr_steps);
  
  if (initial_extents->checkpoint_prefix == NULL){
    d4est_mesh_init_field
      (
       p4est,
       prob_vecs.u,
       constant_density_star_initial_guess,
       d4est_ops,
       d4est_geom,
       d4est_factors,
       INIT_FIELD_ON_LOBATTO,
       NULL
      );
  }
  else {
    d4est_h5_read_dataset(p4est->mpirank,initial_extents->checkpoint_prefix,"u",H5T_NATIVE_DOUBLE, prob_vecs.u);
  }

  
  /* d4est_norms_fcn_energy_fit_t* fit = d4est_norms_new_energy_norm_fit(d4est_amr->num_of_amr_steps + 1); */
  
  /*   d4est_ip_energy_norm_data_t ip_norm_data; */
  /*   ip_norm_data.u_penalty_fcn = sipg_params->sipg_penalty_fcn; */
  /*   ip_norm_data.sipg_flux_h = sipg_params->sipg_flux_h; */
  /*   ip_norm_data.penalty_prefactor = sipg_params->sipg_penalty_prefactor; */

  // Norm function contexts
  
  d4est_norms_fcn_L2_ctx_t L2_norm_ctx;
  L2_norm_ctx.p4est = p4est;
  L2_norm_ctx.d4est_ops = d4est_ops;
  L2_norm_ctx.d4est_geom = d4est_geom;
  L2_norm_ctx.d4est_quad = d4est_quad;
  L2_norm_ctx.d4est_factors = d4est_factors;
  
  d4est_norms_fcn_energy_ctx_t energy_norm_ctx;
  energy_norm_ctx.p4est = p4est;
  energy_norm_ctx.d4est_ops = d4est_ops;
  energy_norm_ctx.d4est_geom = d4est_geom;
  energy_norm_ctx.d4est_quad = d4est_quad;
  energy_norm_ctx.d4est_factors = d4est_factors;
  energy_norm_ctx.which_field = 0;
  /* energy_norm_ctx.fit = NULL; */
  // These are updated later
 

  if (p4est->mpirank == 0)
    d4est_norms_write_headers(
      (const char * []){"u", NULL},
      (const char * []){"L_2", "L_infty", "energy_norm", "energy_estimator", NULL}
    );


  if (p4est->mpirank == 0) {
    zlog_info(c_default, "Initialization complete. Starting procedure with %d AMR steps.", d4est_amr->num_of_amr_steps);
  }

  for (int level = 0; level < d4est_amr->num_of_amr_steps + 2; level++) {

    d4est_field_type_t field_type = VOLUME_NODAL;
    d4est_ghost_data_t* d4est_ghost_data = d4est_ghost_data_init(p4est,
                                                                 *d4est_ghost,
                                                                 &field_type,
                                                                 1);
    
    // Extract mesh data

    double* estimator = d4est_estimator_bi_compute(
      p4est,
      &prob_vecs,
      &prob_fcns,
      penalty_data,
      constant_density_star_boundary_fcn,
      *d4est_ghost,
      d4est_ghost_data,
      d4est_ops,
      d4est_geom,
      d4est_factors,
      d4est_geom,
      d4est_factors,
      d4est_quad,
      DIAM_APPROX_CUBE,
      0
    );
    
    d4est_estimator_stats_t* stats = P4EST_ALLOC(d4est_estimator_stats_t,1);
    d4est_estimator_stats_compute(p4est, estimator, stats);
    d4est_estimator_stats_print(stats);


    
    // Compute analytical field values on mesh
    double* u_analytic = P4EST_ALLOC(double, prob_vecs.local_nodes);
    d4est_mesh_init_field(
      p4est,
      u_analytic,
      constant_density_star_analytic_solution,
      d4est_ops,
      d4est_geom,
      d4est_factors,
      INIT_FIELD_ON_LOBATTO,
      &ctx
    );

    // Compute errors between numerical and analytical field values
    double* error = P4EST_ALLOC(double, prob_vecs.local_nodes);
    for (int i = 0; i < prob_vecs.local_nodes; i++){
      error[i] = fabs(prob_vecs.u[i] - u_analytic[i]);
    }
    
    // Save mesh data to VTK file
    d4est_vtk_save(
      p4est,
      d4est_ops,
      input_file,
      "d4est_vtk",
      (const char * []){"u","u_analytic","error", NULL},
      (double* []){prob_vecs.u, u_analytic, error},
      (const char * []){NULL},
      (double* []){NULL},
      level
    );

    P4EST_FREE(u_analytic);
    P4EST_FREE(error);

    // Compute and save norms

    // TODO: aren't these quantities constant throughout the loop, so move this outside?
    d4est_ip_energy_norm_data_t ip_norm_data;
    ip_norm_data.u_penalty_fcn = sipg_params->sipg_penalty_fcn;
    ip_norm_data.sipg_flux_h = sipg_params->sipg_flux_h;
    ip_norm_data.penalty_prefactor = sipg_params->sipg_penalty_prefactor;
    ip_norm_data.size_params = NULL;

    energy_norm_ctx.energy_norm_data = &ip_norm_data;
    energy_norm_ctx.energy_estimator_sq_local = stats->total;
    energy_norm_ctx.ghost = *d4est_ghost;
    energy_norm_ctx.ghost_data = d4est_ghost_data;

    d4est_norms_save(
      p4est,
      d4est_factors,
      (const char * []){ "u", NULL },
      (double * []){ prob_vecs.u },
      (double * []){ NULL },
      (d4est_xyz_fcn_t []){ constant_density_star_analytic_solution },
      (void * []){ &ctx },
      (const char * []){"L_2", "L_infty", "energy_norm", "energy_estimator", NULL},
      (d4est_norm_fcn_t[]){ &d4est_norms_fcn_L2, &d4est_norms_fcn_Linfty, &d4est_norms_fcn_energy, &d4est_norms_fcn_energy_estimator },
      (void * []){ &L2_norm_ctx, NULL, &energy_norm_ctx, &energy_norm_ctx },
      NULL
    );


    if (level >= d4est_amr->num_of_amr_steps + 1) {
      // Procedure is complete, we only saved the output one last time.
      break;
    }

    // Perform the next AMR step
    if (level > 0) {

      if (p4est->mpirank == 0)
        zlog_info(c_default, "Performing AMR level %d of %d...", level, d4est_amr->num_of_amr_steps);

      d4est_amr_step
        (
         p4est,
         NULL,
         NULL,
         d4est_ops,
         (level >= init_params.amr_level_for_uniform_p) ? d4est_amr_uniform_p : d4est_amr,
         &prob_vecs.u,
         estimator,
         stats
        );
      
      if (p4est->mpirank == 0)
        zlog_info(c_default, "AMR level %d of %d complete.", level, d4est_amr->num_of_amr_steps);

    }

    P4EST_FREE(stats);

    if (p4est->mpirank == 0)
      zlog_info(c_default, "Performing d4est mesh update...");

    prob_vecs.local_nodes = d4est_mesh_update(
      p4est,
      d4est_ghost,
      d4est_ops,
      d4est_geom,
      d4est_quad,
      d4est_factors,
      INITIALIZE_GHOST,
      INITIALIZE_QUADRATURE_DATA,
      INITIALIZE_GEOMETRY_DATA,
      INITIALIZE_GEOMETRY_ALIASES,
      d4est_mesh_set_quadratures_after_amr,
      initial_extents
    );


   if (d4est_ghost_data != NULL){
      d4est_ghost_data_destroy(d4est_ghost_data);
      d4est_ghost_data = NULL;
    } 
    

   d4est_ghost_data = d4est_ghost_data_init(p4est,
                                            *d4est_ghost,
                                            &field_type,
                                            1);

    
    if (p4est->mpirank == 0)
      zlog_info(c_default, "d4est mesh update complete.");

    prob_vecs.Au = P4EST_REALLOC(prob_vecs.Au, double, prob_vecs.local_nodes);

    /* DEBUG_PRINT_ARR_DBL(prob_vecs.u, prob_vecs.local_nodes); */
    /* DEBUG_PRINT_ARR_DBL_SUM(prob_vecs.u, prob_vecs.local_nodes); */

    /* double u_sum = 0.; */
    /* for (int i = 0; i < prob_vecs.local_nodes; i++) */
      /* u_sum += prob_vecs.u[i]; */
    /* printf("u sum = %.25f\n", u_sum); */


    // Setup multigrid
    
    int min_level, max_level;

    multigrid_get_level_range(p4est, &min_level, &max_level);
    zlog_debug(c_default, "Multigrid (min_level, max_level) = (%d, %d)", min_level, max_level);

    /* need to do a reduce on min,max_level before supporting multiple proc */
    /* mpi_assert(proc_size == 1); */
    int num_of_levels = max_level + 1;

    multigrid_logger_t* logger = multigrid_logger_residual_init();
    
    multigrid_element_data_updater_t* updater = multigrid_element_data_updater_init(
      num_of_levels,
      d4est_ghost,
      &d4est_ghost_data,
      d4est_factors,
      d4est_mesh_set_quadratures_after_amr,
      initial_extents
    );

    multigrid_user_callbacks_t* user_callbacks = multigrid_matrix_operator_init(p4est, num_of_levels);

    multigrid_data_t* mg_data = multigrid_data_init(
      p4est,
      d4est_ops,
      d4est_geom,
      d4est_quad,
      num_of_levels,
      logger,
      user_callbacks,
      updater,
      input_file
    );

    krylov_pc_t* pc = krylov_pc_multigrid_create(mg_data, constant_density_star_krylov_pc_setup_fcn);
    ctx.use_matrix_operator = 1;
    ctx.mg_data = mg_data;

    
    // Newton PETSc solve

    if (!init_params.do_not_solve){

      newton_petsc_params_t newton_params;
      newton_petsc_input(p4est, input_file, "[NEWTON_PETSC]", &newton_params);

      krylov_petsc_params_t krylov_params;
      krylov_petsc_input(p4est, input_file, "krylov_petsc", &krylov_params);
      
      if (p4est->mpirank == 0)
        zlog_info(c_default, "Performing Newton PETSc solve...");

      prob_vecs.field_types = &field_type;
      prob_vecs.num_of_fields = 1;
      

      
      newton_petsc_solve(
        p4est,
        &prob_vecs,
        &prob_fcns,
        d4est_ghost,
        &d4est_ghost_data,
        d4est_ops,
        d4est_geom,
        d4est_quad,
        d4est_factors,
        &krylov_params,
        &newton_params,
        pc//NULL//pc
      );

      if (p4est->mpirank == 0)
        zlog_info(c_default, "Newton PETSc solve complete.");
    }
    
    // Save checkpoint
    d4est_checkpoint_save(
      level,
      "checkpoint",
      p4est,
      d4est_factors,
      (const char * []){"u", NULL},
      (double* []){prob_vecs.u}
    );

    zlog_info(c_default, "Saved checkpoint %d for process %d.", level, p4est->mpirank);

    krylov_pc_multigrid_destroy(pc);
    multigrid_logger_residual_destroy(logger);
    multigrid_element_data_updater_destroy(updater, num_of_levels);
    multigrid_data_destroy(mg_data);
    multigrid_matrix_operator_destroy(user_callbacks);

    P4EST_FREE(estimator);



    if (d4est_ghost_data != NULL){
      d4est_ghost_data_destroy(d4est_ghost_data);
      d4est_ghost_data = NULL;
    } 
  
    
  }


  printf("[D4EST_INFO]: Starting garbage collection...\n");
  d4est_amr_destroy(d4est_amr);
  d4est_amr_destroy(d4est_amr_uniform_p);
  d4est_poisson_flux_destroy(flux_data_for_jac);
  d4est_poisson_flux_destroy(flux_data_for_res);
  /* d4est_norms_destroy_energy_norm_fit(fit); */
  P4EST_FREE(prob_vecs.u);
  P4EST_FREE(prob_vecs.Au);
}
