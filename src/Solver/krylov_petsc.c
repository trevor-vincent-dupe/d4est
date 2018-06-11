#include <d4est_util.h>
#include <pXest.h>
#include <d4est_linalg.h>
#include <d4est_mesh.h>
#include <d4est_elliptic_data.h>
#include <krylov_petsc.h>
#include <petscsnes.h>
#include <krylov_pc.h>
#include <ini.h>
#include <time.h>


static
int krylov_petsc_input_handler
(
 void* user,
 const char* section,
 const char* name,
 const char* value
)
{
  krylov_petsc_params_t* pconfig = (krylov_petsc_params_t*)user;
  if (d4est_util_match_couple(section,pconfig->input_section,name,"ksp_atol")) {
    D4EST_ASSERT(pconfig->ksp_atol[0] == '*');
    snprintf (pconfig->ksp_atol, sizeof(pconfig->ksp_atol), "%s", value);
  }
  else if (d4est_util_match_couple(section,pconfig->input_section,name,"ksp_rtol")) {
    D4EST_ASSERT(pconfig->ksp_rtol[0] == '*');
    snprintf (pconfig->ksp_rtol, sizeof(pconfig->ksp_rtol), "%s", value);
  }
  else if (d4est_util_match_couple(section,pconfig->input_section,name,"ksp_max_it")) {
    D4EST_ASSERT(pconfig->ksp_max_it[0] == '*');
    snprintf (pconfig->ksp_max_it, sizeof(pconfig->ksp_max_it), "%s", value);
    D4EST_ASSERT(atoi(value) > -1);
  }
  else if (d4est_util_match_couple(section,pconfig->input_section,name,"ksp_view")) {
    D4EST_ASSERT(pconfig->ksp_view == -1);
    pconfig->ksp_view = atoi(value);
    D4EST_ASSERT(atoi(value) == 0 || atoi(value) == 1);
  }
  else if (d4est_util_match_couple(section,pconfig->input_section,name,"ksp_monitor")) {
    D4EST_ASSERT(pconfig->ksp_monitor == -1);
    pconfig->ksp_monitor = atoi(value);
    D4EST_ASSERT(atoi(value) == 0 || atoi(value) == 1);
  }
  else if (d4est_util_match_couple(section,pconfig->input_section,name,"ksp_converged_reason")) {
    D4EST_ASSERT(pconfig->ksp_converged_reason == -1);
    pconfig->ksp_converged_reason = atoi(value);
    D4EST_ASSERT(atoi(value) == 0 || atoi(value) == 1);
  }
  else if (d4est_util_match_couple(section,pconfig->input_section,name,"ksp_monitor_singular_value")) {
    pconfig->ksp_monitor_singular_value = atoi(value);
    D4EST_ASSERT(atoi(value) == 0 || atoi(value) == 1);
  }
  else if (d4est_util_match_couple(section,pconfig->input_section,name,"ksp_initial_guess_nonzero")) {
    D4EST_ASSERT(pconfig->ksp_initial_guess_nonzero == -1);
    pconfig->ksp_initial_guess_nonzero = atoi(value);
    D4EST_ASSERT(atoi(value) == 0 || atoi(value) == 1);
  }
  else if (d4est_util_match_couple(section,pconfig->input_section,name,"ksp_type")) {
    D4EST_ASSERT(pconfig->ksp_type[0] == '*');
    snprintf (pconfig->ksp_type, sizeof(pconfig->ksp_type), "%s", value);
  }
  else if (d4est_util_match_couple(section,pconfig->input_section,name,"ksp_chebyshev_esteig_steps")) {
    D4EST_ASSERT(pconfig->ksp_chebyshev_esteig_steps[0] == '*');
    snprintf (pconfig->ksp_chebyshev_esteig_steps, sizeof(pconfig->ksp_chebyshev_esteig_steps), "%s", value);
  }
  else if (d4est_util_match_couple(section,pconfig->input_section,name,"ksp_chebyshev_esteig")) {
    D4EST_ASSERT(pconfig->ksp_chebyshev_esteig[0] == '*');
    snprintf (pconfig->ksp_chebyshev_esteig, sizeof(pconfig->ksp_chebyshev_esteig), "%s", value);
  }
  else if (d4est_util_match_couple(section,pconfig->input_section,name,"ksp_chebyshev_esteig_random")) {
    D4EST_ASSERT(pconfig->ksp_chebyshev_esteig_random == -1);
    pconfig->ksp_chebyshev_esteig_random = atoi(value);
    D4EST_ASSERT(atoi(value) == 0 || atoi(value) == 1);
  }
  else if (d4est_util_match_couple(section,pconfig->input_section,name,"ksp_do_not_use_preconditioner")) {
    D4EST_ASSERT(pconfig->ksp_do_not_use_preconditioner == 0);
    pconfig->ksp_do_not_use_preconditioner = atoi(value);
    D4EST_ASSERT(atoi(value) == 0 || atoi(value) == 1);
  }
  
  else {
    return 0;  /* unknown section/name, error */
  }
  return 1;
}

void
krylov_petsc_set_options_database_from_params
(
 krylov_petsc_params_t* input
)
{
  if(input->ksp_monitor)
    PetscOptionsSetValue(NULL,"-ksp_monitor","");
  else
    PetscOptionsClearValue(NULL,"-ksp_monitor");

  if(input->ksp_view)
    PetscOptionsSetValue(NULL,"-ksp_view","");
  else
    PetscOptionsClearValue(NULL,"-ksp_view");

  if(input->ksp_converged_reason)
     PetscOptionsSetValue(NULL,"-ksp_converged_reason","");
  else
    PetscOptionsClearValue(NULL,"-ksp_converged_reason");

  if(input->ksp_initial_guess_nonzero)
    PetscOptionsSetValue(NULL,"-ksp_initial_guess_nonzero","");
  else
    PetscOptionsClearValue(NULL,"-ksp_initial_guess_nonzero");

  if(input->ksp_monitor_singular_value == 1)
    PetscOptionsSetValue(NULL,"-ksp_monitor_singular_value","");
  else
    PetscOptionsClearValue(NULL,"-ksp_monitor_singular_value");
  
  PetscOptionsClearValue(NULL,"-ksp_type");
  PetscOptionsSetValue(NULL,"-ksp_type",input->ksp_type);
  
  PetscOptionsClearValue(NULL,"-ksp_atol");
  PetscOptionsSetValue(NULL,"-ksp_atol",input->ksp_atol);
  
  PetscOptionsClearValue(NULL,"-ksp_rtol");
  PetscOptionsSetValue(NULL,"-ksp_rtol",input->ksp_rtol);
  
  PetscOptionsClearValue(NULL,"-ksp_max_it");
  PetscOptionsSetValue(NULL,"-ksp_max_it", input->ksp_max_it);

  if(d4est_util_match(input->ksp_type,"chebyshev")){
    PetscOptionsClearValue(NULL,"-ksp_chebyshev_esteig_steps");
    PetscOptionsSetValue(NULL,"-ksp_chebyshev_esteig_steps",input->ksp_chebyshev_esteig_steps);
    PetscOptionsClearValue(NULL,"-ksp_chebyshev_esteig");
    PetscOptionsSetValue(NULL,"-ksp_chebyshev_esteig",input->ksp_chebyshev_esteig);
    if(input->ksp_chebyshev_esteig_random)
      PetscOptionsSetValue(NULL,"-ksp_chebyshev_esteig_random","");
    else
      PetscOptionsClearValue(NULL,"-ksp_chebyshev_esteig_random");
  }
}

void
krylov_petsc_input
(
 p4est_t* p4est,
 const char* input_file,
 const char* input_section,
 krylov_petsc_params_t* input
)
{
  input->ksp_view = -1;
  input->ksp_monitor = -1;
  input->ksp_converged_reason = -1;
  input->ksp_initial_guess_nonzero = -1;
  input->ksp_type[0] = '*';
  input->ksp_atol[0] = '*';
  input->ksp_rtol[0] = '*';
  input->ksp_max_it[0] = '*';
  input->ksp_chebyshev_esteig_steps[0] = '*';
  input->ksp_chebyshev_esteig[0] = '*';
  input->ksp_chebyshev_esteig_random = -1;
  input->ksp_monitor_singular_value = 0;
  input->ksp_do_not_use_preconditioner = 0;
  
  D4EST_ASSERT(sizeof(input->input_section) <= 50);
  snprintf (input->input_section, sizeof(input->input_section), "%s", input_section);
  if (ini_parse(input_file, krylov_petsc_input_handler, input) < 0) {
    D4EST_ABORT("Can't load input file");
  }

  D4EST_CHECK_INPUT(input_section, input->ksp_view, -1);
  D4EST_CHECK_INPUT(input_section, input->ksp_monitor, -1);
  D4EST_CHECK_INPUT(input_section, input->ksp_converged_reason, -1);
  D4EST_CHECK_INPUT(input_section, input->ksp_initial_guess_nonzero, -1);
  D4EST_CHECK_INPUT(input_section, input->ksp_type[0], '*');
  D4EST_CHECK_INPUT(input_section, input->ksp_atol[0], '*');
  D4EST_CHECK_INPUT(input_section, input->ksp_rtol[0], '*');
  D4EST_CHECK_INPUT(input_section, input->ksp_max_it[0], '*');

  if(d4est_util_match(input->ksp_type,"chebyshev")){
    D4EST_CHECK_INPUT(input_section, input->ksp_chebyshev_esteig_steps[0], '*');
    D4EST_CHECK_INPUT(input_section, input->ksp_chebyshev_esteig[0], '*');
    D4EST_CHECK_INPUT(input_section, input->ksp_chebyshev_esteig_random, -1);
  }
    
  if(p4est->mpirank == 0){
    zlog_category_t *c_default = zlog_get_category("krylov_petsc");
    zlog_debug(c_default, "ksp_type = %s", input->ksp_type);
    zlog_debug(c_default, "ksp_view = %d", input->ksp_view);
    zlog_debug(c_default, "ksp_monitor = %d", input->ksp_monitor);
    zlog_debug(c_default, "ksp_atol = %s", input->ksp_atol);
    zlog_debug(c_default, "ksp_rtol = %s", input->ksp_rtol);
    zlog_debug(c_default, "ksp_maxit = %s", input->ksp_max_it);
    zlog_debug(c_default, "ksp_converged_reason = %d", input->ksp_converged_reason);
    zlog_debug(c_default, "ksp_initial_guess_nonzero = %d", input->ksp_initial_guess_nonzero);
    zlog_debug(c_default, "ksp_do_not_use_preconditioner = %d", input->ksp_do_not_use_preconditioner);
    if(d4est_util_match(input->ksp_type,"chebyshev")){
      zlog_debug(c_default, "ksp_chebyshev_esteig_steps = %s", input->ksp_chebyshev_esteig_steps);
      zlog_debug(c_default, "ksp_chebyshev_esteig = %s", input->ksp_chebyshev_esteig);
      zlog_debug(c_default, "ksp_chebyshev_esteig_random = %d", input->ksp_chebyshev_esteig_random);
    }
  }

  
}

static
PetscErrorCode krylov_petsc_apply_aij( Mat A, Vec x, Vec y )
{
  void           *ctx;
  PetscErrorCode ierr;

  krylov_ctx_t* petsc_ctx;
  const double* px;
  double* py;

  /* PetscFunctionBegin; */
  ierr = MatShellGetContext( A, &ctx ); CHKERRQ(ierr);
  petsc_ctx = (krylov_ctx_t *)ctx;
  ierr = VecGetArrayRead( x, &px ); CHKERRQ(ierr);
  ierr = VecGetArray( y, &py ); CHKERRQ(ierr);

  /* d4est_elliptic_eqns_t* fcns = petsc_ctx->fcns; */
  p4est_t* p4est = petsc_ctx->p4est;
  d4est_ghost_t* ghost = *petsc_ctx->ghost;

  d4est_elliptic_data_t vecs_for_aij;
  d4est_elliptic_data_copy_ptrs(petsc_ctx->vecs, &vecs_for_aij);

  vecs_for_aij.u = (double*)px;
  vecs_for_aij.Au = py;


  /* double residual = 0.; */
  /* for (int i = 0; i < vecs_for_aij.local_nodes; i++){ */
    /* double r_temp = vecs_for_aij.Au[i] - vecs_for_aij.rhs[i]; */
    /* residual += r_temp*r_temp; */
  /* } */
  /* printf("residual = %.15f\n", sqrt(residual)); */

  
  d4est_elliptic_eqns_apply_lhs
    (
     petsc_ctx->p4est,
     *petsc_ctx->ghost,
     *petsc_ctx->ghost_data,
     petsc_ctx->fcns,
     &vecs_for_aij,
     petsc_ctx->d4est_ops,
     petsc_ctx->d4est_geom,
     petsc_ctx->d4est_quad,
     petsc_ctx->d4est_factors
    );


  /* DEBUG_PRINT_ARR_DBL_SUM(vecs_for_aij.u, vecs_for_aij.local_nodes); */
  /* DEBUG_PRINT_ARR_DBL_SUM(vecs_for_aij.Au, vecs_for_aij.local_nodes); */
  /* DEBUG_PRINT_ARR_DBL_SUM(vecs_for_aij.rhs, vecs_for_aij.local_nodes); */
  /* d4est_mesh_data_printout(petsc_ctx->d4est_factors); */


  /* residual = 0.; */
  /* for (int i = 0; i < vecs_for_aij.local_nodes; i++){ */
    /* double r_temp = vecs_for_aij.Au[i] - vecs_for_aij.rhs[i]; */
    /* residual += r_temp*r_temp; */
  /* } */
  /* printf("residual = %.15f\n", sqrt(residual)); */

  
  
  ierr = VecRestoreArrayRead( x, &px ); CHKERRQ(ierr);
  ierr = VecRestoreArray( y, &py ); CHKERRQ(ierr);
  return ierr;
}

krylov_petsc_info_t
krylov_petsc_solve
(
 p4est_t* p4est,
 d4est_elliptic_data_t* vecs,
 d4est_elliptic_eqns_t* fcns,
 d4est_ghost_t** ghost,
 d4est_ghost_data_t** ghost_data,
 d4est_operators_t* d4est_ops,
 d4est_geometry_t* d4est_geom,
 d4est_quadrature_t* d4est_quad,
 d4est_mesh_data_t* d4est_factors,
 krylov_petsc_params_t* krylov_petsc_params,
 krylov_pc_t* krylov_pc
)
{
  zlog_category_t *c_default = zlog_get_category("krylov_petsc");
  clock_t start;
  if (p4est->mpirank == 0) {
    zlog_info(c_default, "Performing Krylov PETSc solve...");
    start = clock();
  }

  krylov_petsc_set_options_database_from_params(krylov_petsc_params);

  krylov_petsc_info_t info;
  KSP ksp;
  Vec x,b;
  PC             pc;
  /* double* u_temp; */
  /* double* rhs_temp; */

  krylov_ctx_t petsc_ctx;
  petsc_ctx.p4est = p4est;
  petsc_ctx.vecs = vecs;
  petsc_ctx.fcns = fcns;
  petsc_ctx.ghost = ghost;
  petsc_ctx.ghost_data = ghost_data;
  petsc_ctx.d4est_ops = d4est_ops;
  petsc_ctx.d4est_geom = d4est_geom;
  petsc_ctx.d4est_quad = d4est_quad;
  petsc_ctx.d4est_factors = d4est_factors;

  int local_nodes = vecs->local_nodes;
  double* u = vecs->u;
  double* rhs = vecs->rhs;

  /* printf("PRE KRYLOV SOLVE REDUCTIONS\n"); */
  /* DEBUG_PRINT_ARR_DBL_SUM(u, local_nodes); */
  /* DEBUG_PRINT_ARR_DBL_SUM(rhs, local_nodes); */
  
  KSPCreate(PETSC_COMM_WORLD,&ksp);
  VecCreate(PETSC_COMM_WORLD,&x);//CHKERRQ(ierr);
  VecSetSizes(x, local_nodes, PETSC_DECIDE);//CHKERRQ(ierr);
  VecSetFromOptions(x);//CHKERRQ(ierr);
  VecDuplicate(x,&b);//CHKERRQ(ierr);

  
  KSPGetPC(ksp,&pc);
  if (krylov_pc != NULL && krylov_petsc_params->ksp_do_not_use_preconditioner == 0) {
    PCSetType(pc,PCSHELL);//CHKERRQ(ierr);
    krylov_pc->pc_ctx = &petsc_ctx;
    PCShellSetApply(pc, krylov_petsc_pc_apply);//CHKERRQ(ierr);
    if (krylov_pc->pc_setup != NULL){
      PCShellSetSetUp(pc, krylov_petsc_pc_setup);
    }
    PCShellSetContext(pc, krylov_pc);//CHKERRQ(ierr);
  }
  else {
    PCSetType(pc,PCNONE);//CHKERRQ(ierr);
  }


  KSPSetFromOptions(ksp);
  /* KSPSetUp(ksp); */
  /* Create matrix-free shell for Aij */
  Mat A;
  MatCreateShell
    (
     PETSC_COMM_WORLD,
     local_nodes,
     local_nodes,
     PETSC_DETERMINE,
     PETSC_DETERMINE,
     (void*)&petsc_ctx,
     &A
    );
  MatShellSetOperation(A,MATOP_MULT,(void(*)())krylov_petsc_apply_aij);

  /* Set Amat and Pmat, where Pmat is the matrix the Preconditioner needs */
  KSPSetOperators(ksp,A,A);
  VecPlaceArray(b, rhs);
  VecPlaceArray(x, u);

  KSPSolve(ksp,b,x);
  
  KSPGetIterationNumber(ksp, &(info.total_krylov_iterations));
  KSPGetResidualNorm(ksp, &(info.residual_norm));

  MatDestroy(&A);
  VecResetArray(b);
  VecResetArray(x);
  VecDestroy(&x);
  VecDestroy(&b);
  KSPDestroy(&ksp);
  
  if (p4est->mpirank == 0) {
    double duration_seconds = ((double)(clock() - start)) / CLOCKS_PER_SEC;
    zlog_info(c_default, "Krylov PETSc solve complete in %.2f seconds (%d iterations). Residual norm: %.2e", duration_seconds, info.total_krylov_iterations, info.residual_norm);
  }

  return info;
}
