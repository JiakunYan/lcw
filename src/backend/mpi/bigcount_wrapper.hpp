#ifndef LCW_BACKEND_MPI_BIGCOUNT_WRAPPER_HPP
#define LCW_BACKEND_MPI_BIGCOUNT_WRAPPER_HPP
#include <mpi.h>
#include <limits>

namespace lcw
{
namespace mpi
{
// Acknowledgement: code adapted from github.com/jeffhammond/BigMPI
MPI_Datatype type_contiguous(size_t nbytes)
{
  size_t int_max = (std::numeric_limits<int>::max)();

  size_t c = nbytes / int_max;
  size_t r = nbytes % int_max;

  LCW_Assert(c < int_max, "c is too large");
  LCW_Assert(r < int_max, "r is too large");

  MPI_Datatype chunks;
  MPI_Type_vector(c, int_max, int_max, MPI_BYTE, &chunks);

  MPI_Datatype remainder;
  MPI_Type_contiguous(r, MPI_BYTE, &remainder);

  MPI_Aint remdisp = (MPI_Aint)c * int_max;
  int blocklengths[2] = {1, 1};
  MPI_Aint displacements[2] = {0, remdisp};
  MPI_Datatype types[2] = {chunks, remainder};
  MPI_Datatype newtype;
  MPI_Type_create_struct(2, blocklengths, displacements, types, &newtype);

  MPI_Type_free(&chunks);
  MPI_Type_free(&remainder);

  return newtype;
}

MPI_Request isend(void* address, size_t size, int rank, int tag, MPI_Comm comm)
{
  MPI_Request request;
  MPI_Datatype datatype;
  int length;
  if (size > static_cast<size_t>((std::numeric_limits<int>::max)())) {
    datatype = type_contiguous(size);
    MPI_Type_commit(&datatype);
    length = 1;
  } else {
    datatype = MPI_BYTE;
    length = static_cast<int>(size);
  }

  MPI_SAFECALL(MPI_Isend(address, length, datatype, rank, tag, comm, &request));

  if (datatype != MPI_BYTE) MPI_Type_free(&datatype);
  return request;
}

MPI_Request irecv(void* address, size_t size, int rank, int tag, MPI_Comm comm)
{
  MPI_Request request;
  MPI_Datatype datatype;
  int length;
  if (size > static_cast<size_t>((std::numeric_limits<int>::max)())) {
    datatype = type_contiguous(size);
    MPI_Type_commit(&datatype);
    length = 1;
  } else {
    datatype = MPI_BYTE;
    length = static_cast<int>(size);
  }

  MPI_SAFECALL(MPI_Irecv(address, length, datatype, rank, tag, comm, &request));

  if (datatype != MPI_BYTE) MPI_Type_free(&datatype);

  return request;
}

}  // namespace mpi
}  // namespace lcw

#endif  // LCW_BACKEND_MPI_BIGCOUNT_WRAPPER_HPP