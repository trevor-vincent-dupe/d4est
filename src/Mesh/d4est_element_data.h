#ifndef D4EST_ELEMENT_DATA_H
#define D4EST_ELEMENT_DATA_H 

#include <pXest.h>
#include <d4est_xyz_functions.h>
#include <d4est_operators.h>
/* #include <d4est_quadrature.h> */
#include <d4est_field.h>

#define MAX_DEGREE 8
#if (P4EST_DIM) == 3
#define MAX_NODES (MAX_DEGREE + 1) * (MAX_DEGREE + 1) * (MAX_DEGREE + 1)
#else
#define MAX_NODES (MAX_DEGREE + 1) * (MAX_DEGREE + 1)
#endif

typedef struct {

  /* identification */
  int id;
  int mpi_rank;
  
  int sqr_nodal_stride;
  int nodal_stride;
  int quad_stride;

  int boundary_quad_vector_stride [P4EST_FACES];
  int mortar_quad_scalar_stride [P4EST_FACES];
  int mortar_quad_vector_stride [P4EST_FACES];
  int mortar_quad_matrix_stride [P4EST_FACES];
  
  int region;
  int tree;
  int tree_quadid;
  p4est_qcoord_t q [(P4EST_DIM)];
  p4est_qcoord_t dq;
  
  /* WILL BE DEPRECATED SOON */
  /* double u_elem[MAX_NODES];   /\* storage for MPI transfers *\/ */

#ifdef D4EST_TEST
  double test_vecs[3][MAX_NODES];
#endif

  /* double* dudr_elem[(P4EST_DIM)];   /\* alias for dudr *\/ */
  /* double* Au_elem;  /\* alias for Au *\/ */
  
  int deg; /* nodal degree */
  int deg_quad; /* deg for quadrature */

  /* different element size parameters */
  /* double j_div_sj_min [(P4EST_FACES)]; */
  /* double diam_face [(P4EST_FACES)]; */
  /* double diam_volume; */
  /* double area [(P4EST_FACES)]; */
  /* double volume; */
  
} d4est_element_data_t;

/* This file was automatically generated.  Do not edit! */
int d4est_element_data_get_stride_for_field(d4est_element_data_t *ed,d4est_field_type_t type);
int d4est_element_data_get_size_of_field(d4est_element_data_t *ed,d4est_field_type_t type);
void d4est_element_data_print_local_estimator(p4est_t *p4est);
void d4est_element_data_reorient_f_p_elements_to_f_m_order(d4est_element_data_t **e_p,int face_dim,int f_m,int f_p,int o,int faces_p,d4est_element_data_t *e_p_oriented[(P4EST_HALF)]);
void d4est_element_data_store_nodal_vec_in_vertex_array(p4est_t *p4est,double *nodal_vec,double *corner_vec);
void d4est_element_data_store_element_scalar_in_vertex_array(p4est_t *p4est,double *vertex_array,double(*get_local_scalar_fcn)(d4est_element_data_t *));
void d4est_element_data_copy_from_storage_to_vec(p4est_t *p4est,double *vec);
void d4est_element_data_copy_from_vec_to_storage(p4est_t *p4est,double *vec);


#endif
