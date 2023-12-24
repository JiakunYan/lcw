#ifndef LCW_LCWI_HPP
#define LCW_LCWI_HPP

#include <cstdio>
#include <cstdlib>
#include <string>
#include <memory>
#include <deque>
#include <pthread.h>
#include "lct.h"

#include "api/lcw.hpp"
#include "util/spinlock.hpp"
#include "logger/logger.hpp"
#include "backend/backend.hpp"
#ifdef LCW_ENABLE_BACKEND_LCI
#include "backend/lci/backend_lci.hpp"
#endif
#ifdef LCW_ENABLE_BACKEND_MPI
#include "backend/mpi/backend_mpi.hpp"
#endif

#endif  // LCW_LCWI_HPP
