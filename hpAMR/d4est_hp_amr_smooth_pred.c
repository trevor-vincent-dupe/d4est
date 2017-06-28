#include <pXest.h>
#include <d4est_element_data.h>
#include <problem_data.h>
#include <problem_weakeqn_ptrs.h>
#include <d4est_linalg.h>
#include <util.h>
#include <d4est_hp_amr.h>
#include <d4est_hp_amr_smooth_pred.h>

#if (P4EST_DIM)==3
#define ONE_OVER_CHILDREN 0.125
#else
#define ONE_OVER_CHILDREN 0.25
#endif

void
d4est_hp_amr_smooth_pred_print
(
 p4est_t* p4est
)
{
  for (p4est_topidx_t tt = p4est->first_local_tree;
       tt <= p4est->last_local_tree;
       ++tt)
    {
      p4est_tree_t* tree = p4est_tree_array_index (p4est->trees, tt);
      sc_array_t* tquadrants = &tree->quadrants;
      int Q = (p4est_locidx_t) tquadrants->elem_count;
      for (int q = 0; q < Q; ++q) {
        p4est_quadrant_t* quad = p4est_quadrant_array_index (tquadrants, q);
        d4est_element_data_t* ed = quad->p.user_data;
        printf("Element %d predictor = %.25f\n", ed->id, ed->local_predictor);
      }
    }
}



static void
d4est_hp_amr_smooth_pred_pre_refine_callback
(
 p4est_t* p4est,
 void* user
)
{
  d4est_hp_amr_smooth_pred_data_t* smooth_pred_data =
    (d4est_hp_amr_smooth_pred_data_t*)user;

  if (smooth_pred_data->predictors == NULL){
    smooth_pred_data->predictors = P4EST_REALLOC
                                   (
                                    smooth_pred_data->predictors,
                                    double,
                                    p4est->local_num_quadrants
                                   );
    d4est_linalg_fill_vec(smooth_pred_data->predictors, 0., p4est->local_num_quadrants);
  }
  
  int pred_stride = 0;
  for (p4est_topidx_t tt = p4est->first_local_tree;
       tt <= p4est->last_local_tree;
       ++tt)
    {
      p4est_tree_t* tree = p4est_tree_array_index (p4est->trees, tt);
      sc_array_t* tquadrants = &tree->quadrants;
      int Q = (p4est_locidx_t) tquadrants->elem_count;
      for (int q = 0; q < Q; ++q) {
        p4est_quadrant_t* quad = p4est_quadrant_array_index (tquadrants, q);
        d4est_element_data_t* ed = quad->p.user_data;
        ed->local_predictor = smooth_pred_data->predictors[pred_stride];
        pred_stride++;
      }
    }
}

static void
d4est_hp_amr_smooth_pred_post_balance_callback
(
 p4est_t* p4est,
 void* user
)
{
  d4est_hp_amr_smooth_pred_data_t* smooth_pred_data =
    (d4est_hp_amr_smooth_pred_data_t*)user;
  smooth_pred_data->predictors = P4EST_REALLOC
                                 (
                                  smooth_pred_data->predictors,
                                  double,
                                  p4est->local_num_quadrants
                                 );  
  int pred_stride = 0;
  for (p4est_topidx_t tt = p4est->first_local_tree;
       tt <= p4est->last_local_tree;
       ++tt)
    {
      p4est_tree_t* tree = p4est_tree_array_index (p4est->trees, tt);
      sc_array_t* tquadrants = &tree->quadrants;
      int Q = (p4est_locidx_t) tquadrants->elem_count;
      for (int q = 0; q < Q; ++q) {
        p4est_quadrant_t* quad = p4est_quadrant_array_index (tquadrants, q);
        d4est_element_data_t* ed = quad->p.user_data;
        smooth_pred_data->predictors[pred_stride] = ed->local_predictor;
        pred_stride++;
      }
    }
 
}




static void
d4est_hp_amr_smooth_pred_set_refinement
(
 p4est_iter_volume_info_t* info,
 void* user_data
)
{
  d4est_hp_amr_data_t* d4est_hp_amr_data = (d4est_hp_amr_data_t*) info->p4est->user_pointer;
  d4est_hp_amr_smooth_pred_data_t* smooth_pred_data = (d4est_hp_amr_smooth_pred_data_t*) (d4est_hp_amr_data->hp_amr_scheme_data);
  d4est_element_data_t* elem_data = (d4est_element_data_t*) info->quad->p.user_data;
  estimator_stats_t** stats = d4est_hp_amr_data->estimator_stats;
  
  double eta2 = elem_data->local_estimator;
  double eta2_pred = elem_data->local_predictor;

  gamma_params_t gamma_hpn =
    smooth_pred_data->marker.set_element_gamma_fcn
    (
     info->p4est,
     eta2,
     stats,
     elem_data,
     smooth_pred_data->marker.user
    );
  
  int is_marked = 
    smooth_pred_data->marker.mark_element_fcn
    (
     info->p4est,
     eta2,
     stats,
     elem_data,
     smooth_pred_data->marker.user
    );

  
  if (is_marked){
    if (eta2 <= elem_data->local_predictor && elem_data->deg < smooth_pred_data->max_degree){
      d4est_hp_amr_data->refinement_log[elem_data->id] = util_min_int(elem_data->deg + 1, smooth_pred_data->max_degree);
      eta2_pred = gamma_hpn.gamma_p*eta2;
    }
    else {
      d4est_hp_amr_data->refinement_log[elem_data->id] = -elem_data->deg;
      eta2_pred = gamma_hpn.gamma_h*eta2*util_dbl_pow_int(.5, 2*(elem_data->deg))*(ONE_OVER_CHILDREN);
    }
  }
  else {
    eta2_pred = gamma_hpn.gamma_n*eta2_pred;
    d4est_hp_amr_data->refinement_log[elem_data->id] = elem_data->deg;
  }
  
  elem_data->local_predictor = eta2_pred;
}

static void
d4est_hp_amr_smooth_pred_balance_replace_callback (
			     p4est_t * p4est,
			     p4est_topidx_t which_tree,
			     int num_outgoing,
			     p4est_quadrant_t * outgoing[],
			     int num_incoming,
			     p4est_quadrant_t * incoming[]
			     )
{
#ifdef SAFETY  
  mpi_assert(num_outgoing == 1);
#endif
  d4est_hp_amr_data_t* d4est_hp_amr_data = (d4est_hp_amr_data_t*) p4est->user_pointer;
  d4est_operators_t* d4est_ops = d4est_hp_amr_data->d4est_ops;
  d4est_hp_amr_smooth_pred_data_t* smooth_pred_data = (d4est_hp_amr_smooth_pred_data_t*) (d4est_hp_amr_data->hp_amr_scheme_data);
  estimator_stats_t** stats = d4est_hp_amr_data->estimator_stats;
  d4est_element_data_t* parent_data = (d4est_element_data_t*) outgoing[0]->p.user_data;
  d4est_element_data_t* child_data;
  int i;

  int degh [(P4EST_CHILDREN)];
  int degH = parent_data->deg;

  gamma_params_t gamma_hpn
    = smooth_pred_data->marker.set_element_gamma_fcn(p4est,parent_data->local_estimator, stats, parent_data, smooth_pred_data->marker.user);
  
  for (i = 0; i < (P4EST_CHILDREN); i++)
    degh[i] = degH;

  int volume_nodes = d4est_lgl_get_nodes((P4EST_DIM), degH);  
  int h_pow = parent_data->deg;
    
  for (i = 0; i < (P4EST_CHILDREN); i++){
    child_data = (d4est_element_data_t*) incoming[i]->p.user_data;   
    child_data->local_predictor = (ONE_OVER_CHILDREN)*gamma_hpn.gamma_h*util_dbl_pow_int(.5, 2*(h_pow))*parent_data->local_predictor;
  }

}


static void
d4est_hp_amr_smooth_pred_refine_replace_callback (
			     p4est_t * p4est,
			     p4est_topidx_t which_tree,
			     int num_outgoing,
			     p4est_quadrant_t * outgoing[],
			     int num_incoming,
			     p4est_quadrant_t * incoming[]
			     )
{
#ifdef SAFETY  
  mpi_assert(num_outgoing == 1);
#endif
  d4est_hp_amr_data_t* d4est_hp_amr_data = (d4est_hp_amr_data_t*) p4est->user_pointer;
  d4est_operators_t* d4est_ops = d4est_hp_amr_data->d4est_ops;

  d4est_element_data_t* parent_data = (d4est_element_data_t*) outgoing[0]->p.user_data;
  d4est_element_data_t* child_data;
  int i;

  int degh [(P4EST_CHILDREN)];
  int degH = parent_data->deg;
    
  for (i = 0; i < (P4EST_CHILDREN); i++)
    degh[i] = degH;

  for (i = 0; i < (P4EST_CHILDREN); i++){
    child_data = (d4est_element_data_t*) incoming[i]->p.user_data;
    child_data->local_predictor = parent_data->local_predictor;
  }
}


void
d4est_hp_amr_smooth_pred_destroy(d4est_hp_amr_scheme_t* scheme){

  d4est_hp_amr_smooth_pred_data_t* smooth_pred_data =
    (d4est_hp_amr_smooth_pred_data_t*)scheme->hp_amr_scheme_data;  
  P4EST_FREE(smooth_pred_data->predictors);
  P4EST_FREE(smooth_pred_data);
  P4EST_FREE(scheme);
}



d4est_hp_amr_scheme_t*
d4est_hp_amr_smooth_pred_init
(
 p4est_t* p4est,
 int max_degree,
 smooth_pred_marker_t marker
)
{  
  d4est_hp_amr_scheme_t* scheme = P4EST_ALLOC(d4est_hp_amr_scheme_t, 1);
  d4est_hp_amr_smooth_pred_data_t* smooth_pred_data;
  smooth_pred_data = P4EST_ALLOC(d4est_hp_amr_smooth_pred_data_t, 1);
  smooth_pred_data->max_degree = max_degree;
  smooth_pred_data->marker = marker;
  smooth_pred_data->predictors = NULL; 

  scheme->pre_refine_callback
    = d4est_hp_amr_smooth_pred_pre_refine_callback;
  
  scheme->balance_replace_callback_fcn_ptr
    = d4est_hp_amr_smooth_pred_balance_replace_callback;

  scheme->refine_replace_callback_fcn_ptr
    = d4est_hp_amr_smooth_pred_refine_replace_callback;

  scheme->iter_volume
    = d4est_hp_amr_smooth_pred_set_refinement;

  scheme->hp_amr_scheme_data
    = smooth_pred_data;

  scheme->post_balance_callback
    = d4est_hp_amr_smooth_pred_post_balance_callback;
  
  return scheme;
}