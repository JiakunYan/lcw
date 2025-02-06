#ifndef LCW_LCWI_HPP
#define LCW_LCWI_HPP

#include <cstdio>
#include <cstdlib>
#include <string>
#include <memory>
#include <deque>
#include <vector>
#include <pthread.h>
#include <atomic>
#include "lct.h"

#include "api/lcw.hpp"
#include "env.hpp"
#include "util/pcounter.hpp"
#include "util/spinlock.hpp"
#include "logger/logger.hpp"
#include "backend/backend.hpp"
#ifdef LCW_ENABLE_BACKEND_LCI
#include "backend/lci/backend_lci.hpp"
#endif
#ifdef LCW_ENABLE_BACKEND_MPI
#include "backend/mpi/comp_manager/manager_base.hpp"
#include "backend/mpi/backend_mpi.hpp"
#include "backend/mpi/comp_manager/manager_req.hpp"
#include "backend/mpi/comp_manager/manager_cont.hpp"
#endif
#ifdef LCW_ENABLE_BACKEND_LCI2
#include "backend/lci2/backend_lci2.hpp"
#endif

#endif  // LCW_LCWI_HPP
