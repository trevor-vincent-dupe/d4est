#ifndef D4EST_FIELD_H
#define D4EST_FIELD_H 

#include <pXest.h>

#define D4EST_FIELD_TYPES 3

/* more types will be added in the future */
typedef enum {
  VOLUME,
  VOLUME_NODAL,
  FACE,
  NO_TYPE
} d4est_field_type_t;

typedef struct {

  double* field_data;
  d4est_field_type_t field_type;
  
} d4est_field_t;

#define D4EST_FIELD_CHECK_TYPE(a) do {                  \
    D4EST_ASSERT(a >= 0 && a < D4EST_FIELD_TYPES);      \
  } while(0)


#endif
