

#include "Pull.h"

#define REG_SYS RS(MpiSerializers)


#define RS(x) DECL_PULL_VAR(x);
REG_SYS
#undef RS

volatile size_t mpi_primary_pulls = 0
#define RS(x) +PULL_VAR(x)
  REG_SYS
#undef RS

  ;
