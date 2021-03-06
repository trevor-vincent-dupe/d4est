#ifndef GRID_FUNCTIONS_H
#define GRID_FUNCTIONS_H 

#include "../pXest/pXest.h"
/** 
 * Function pointer for 
 * functions of the form
 * F(x,y,z) or F(x,y)
 * where x,y,z is a 
 * grid point
 * 
 * @param d4est_xyz_fcn_t 
 * 
 * @return 
 */
typedef double
(*d4est_xyz_fcn_t)
(
 double,
 double,
#if (P4EST_DIM)==3
 double,
#endif 
 void* user
);

typedef double
(*d4est_xyzu_fcn_t)
(
 double,
 double,
#if (P4EST_DIM)==3
 double,
#endif
 double,
 void* user
);


double zero_fcn
(
 double x,
 double y,
#if (P4EST_DIM)==3
 double z,
#endif
 void* user
);

double identity_fcn
(
 double x,
 double y,
#if (P4EST_DIM)==3
 double z,
#endif // (P4EST_DIM)==3
 double u,
 void* user
);

double
sinpix_fcn
(
 double x,
 double y,
#if (P4EST_DIM)==3
 double z,
#endif
 void* user
);

#endif
