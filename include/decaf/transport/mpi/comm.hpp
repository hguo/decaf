//---------------------------------------------------------------------------
//
// decaf communicator implementation in mpi transport layer
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

#include "../../comm.hpp"
#include <mpi.h>
#include <stdio.h>

// forms a communicator from contiguous world ranks
// only collective over the ranks in the range [min_rank, max_rank]
decaf::
Comm::Comm(CommHandle world_comm, int min_rank, int max_rank) : min_rank_(min_rank)
{
  MPI_Group group, newgroup;
  int range[3];
  range[0] = min_rank;
  range[1] = max_rank;
  range[2] = 1;
  MPI_Comm_group(world_comm, &group);
  MPI_Group_range_incl(group, 1, &range, &newgroup);
  MPI_Comm_create_group(world_comm, newgroup, 0, &handle_);
  MPI_Group_free(&group);
  MPI_Group_free(&newgroup);

  MPI_Comm_rank(handle_, &rank_);
  MPI_Comm_size(handle_, &size_);
}

decaf::
Comm::~Comm()
{
  MPI_Comm_free(&handle_);
}

// puts data to a destination
// it forwarding, sends data->get_items()
// else, sends data->put_items()
void
decaf::
Comm::put(Data *data, int dest, bool forward)
{
  // TODO: prepend typemap?

  // short-circuit put to self
  if (world_rank(dest) == world_rank(rank()))
  {
    data->put_self(true);
    if (!forward) // forwarding would only copy get_items to get_items; no need
    {
      MPI_Aint extent; // datatype size in bytes
      MPI_Type_extent(data->complete_datatype_, &extent);
      // TODO: I don't think there is a good way to avoid the following deep copy
      memcpy(data->resize_get_items(data->put_nitems() * extent), data->put_items(), extent);
    }
  }
  if (world_rank(dest) != world_rank(rank()))
  {
    MPI_Request req;
    if (forward)
      MPI_Isend(data->get_items(), data->get_nitems(), data->complete_datatype_, dest, 0,
                handle_, &req);
    else
      MPI_Isend(data->put_items(), data->put_nitems(), data->complete_datatype_, dest, 0,
                handle_, &req);
    reqs.push_back(req);
  }
}

// gets dsta from any source
void
decaf::
Comm::get(Data* data)
{
  if (data->put_self())
    data->put_self(false);
  else
  {
    // TODO: read type from typemap instead of argument?
    MPI_Status status;
    MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, handle_, &status);
    int nitems; // number of items (of type dtype) in the message
    MPI_Get_count(&status, data->complete_datatype_, &nitems);
    MPI_Aint extent; // datatype size in bytes
    MPI_Type_extent(data->complete_datatype_, &extent);
    MPI_Recv(data->resize_get_items(nitems * extent), nitems, data->complete_datatype_,
             status.MPI_SOURCE, 0, handle_, &status);
  }
}

// completes nonblocking sends
void
decaf::
Comm::flush()
{
  if (reqs.size())
  {
    std::vector<MPI_Status> stats(reqs.size());
    MPI_Waitall(reqs.size(), &reqs[0], &stats[0]);
  }
  reqs.clear();
}

#endif
