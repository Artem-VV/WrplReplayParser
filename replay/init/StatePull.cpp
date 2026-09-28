#include "Pull.h"
extern volatile size_t ecs_primary_pulls;
extern volatile size_t mpi_primary_pulls;

size_t get_state_pulls() { return ecs_primary_pulls + mpi_primary_pulls; }
