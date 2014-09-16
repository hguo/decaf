//---------------------------------------------------------------------------
//
// decaf mpi transport layer
//
// Tom Peterka
// Argonne National Laboratory
// 9700 S. Cass Ave.
// Argonne, IL 60439
// tpeterka@mcs.anl.gov
//
//--------------------------------------------------------------------------

#ifndef DECAF_TRANSPORT_MPI_COMM_HPP
#define DECAF_TRANSPORT_MPI_COMM_HPP

#include<mpi.h>

using namespace decaf;

Comm::Comm(DecafComm old_comm, int prod_size, int con_size, int dflow_size, int err)
{
  // split ranks into {ranks} = {producer}, {consumer}, {dataflow}, {world (remainder)}
  // wrt increasing rank order in old communicator
  int world_rank, world_size; // MPI usual
  MPI_Comm_rank(old_comm, &world_rank);
  MPI_Comm_size(old_comm, &world_size);
  // debug
//   fprintf(stderr, "world_size = %d prod_size = %d con_size = %d dflow_size = %d\n",
//           world_size, prod_size, con_size, dflow_size);
  if (prod_size + con_size + dflow_size > world_size)
  {
    err = DECAF_COMM_SIZES_ERR;
    return;
  }
  if (world_rank < prod_size)
    type_ = DECAF_PRODUCER_COMM;
  else if (world_rank < prod_size + con_size)
    type_ = DECAF_CONSUMER_COMM;
  else if (world_rank < prod_size + con_size + dflow_size)
    type_ = DECAF_DATAFLOW_COMM;
  else
    type_ = DECAF_WORLD_COMM;
  MPI_Comm_split(old_comm, type_, world_rank, &comm_);
  // debug
//   fprintf(stderr, "comm_type = %d\n", type_);

  err = DECAF_OK;
}

#endif
