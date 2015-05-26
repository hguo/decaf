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
#include <assert.h>
#include <stdlib.h>
#include <string.h>

// forms a communicator from contiguous world ranks
// only collective over the ranks in the range [min_rank, max_rank]
decaf::
Comm::Comm(CommHandle world_comm, int min_rank, int max_rank, int num_srcs,
           int num_dests, int start_dest, CommType comm_type):
  min_rank(min_rank), num_srcs(num_srcs), num_dests(num_dests), start_dest(start_dest),
  type_(comm_type)
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
void
decaf::
Comm::put(Data *data,             // data or NULL for empty put
          int dest,               // destination rank
          TaskType task_type)     // type of task
{
  char null_byte = 0;             // payload for an empty put
  // TODO: prepend typemap?

  // NB: putting to self is handled during the get

  if (world_rank(dest) != world_rank(rank())) // put to other (not self)
  {
    MPI_Request req;
    reqs.push_back(req);
    if (task_type == DECAF_DFLOW) // forwarding through the dataflow
    {
      if (data)
      {
        // debug
//         fprintf(stderr, "putting %d items\n", data->put_nitems());
        MPI_Isend(data->get_items(DECAF_DFLOW), data->get_nitems(DECAF_DFLOW),
                  data->complete_datatype_, dest, 0, handle_, &reqs.back());
      }
      else                        // tag = 1 indicates an empty put
      {
        // debug
//         fprintf(stderr, "putting null byte\n");
        MPI_Isend(&null_byte, 1, MPI_BYTE, dest, 1, handle_, &reqs.back());
      }
    }
    else
    {
      if (data)
      {
        // debug
//         fprintf(stderr, "putting %d items\n", data->put_nitems());
        MPI_Isend(data->put_items(), data->put_nitems(), data->complete_datatype_, dest, 0,
                  handle_, &reqs.back());
      }
      else                        // tag = 1 indicates an empty put
      {
        // debug
//         fprintf(stderr, "putting null byte\n");
        MPI_Isend(&null_byte, 1, MPI_BYTE, dest, 1, handle_, &reqs.back());
      }
    }
  }
}

// gets data from one or more sources
void
decaf::
Comm::get(Data* data,
          TaskType task_type)
{
  for (int i = start_input(); i < start_input() + num_inputs(); i++)
  {
    if (world_rank(i) == world_rank(rank())) // receive from self
    {
      MPI_Aint extent; // datatype size in bytes
      MPI_Type_extent(data->complete_datatype_, &extent);
      // NB: no way to avoid the following deep copy; some of the get items may have come remotely,
      // others from self, and we want them all in one place.
      // There is a separate no_copy option in decaf::get() that simply returns the put items
      memcpy(data->resize_get_items(data->put_nitems() * extent, task_type),
             data->put_items(), extent);
    }
    else // receive from other
    {
      // TODO: read type from typemap instead of argument?
      MPI_Status status;
      MPI_Probe(i, MPI_ANY_TAG, handle_, &status);
      if (status.MPI_TAG == 0)  // normal, non-null get
      {
        int nitems; // number of items (of type dtype) in the message
        MPI_Get_count(&status, data->complete_datatype_, &nitems);
        MPI_Aint extent; // datatype size in bytes
        MPI_Type_extent(data->complete_datatype_, &extent);
        // debug
//         fprintf(stderr, "getting %d items from input %d\n", nitems, i);
        MPI_Recv(data->resize_get_items(nitems * extent, task_type), nitems,
                 data->complete_datatype_, status.MPI_SOURCE, status.MPI_TAG, handle_, &status);
      }
      else                      // null get, keep the stream flowing but don't save the result
      {
        char null_byte;
        MPI_Recv(&null_byte, 1, MPI_BYTE, status.MPI_SOURCE, status.MPI_TAG, handle_, &status);
      }
    }
  }
}

// completes nonblocking sends
void
decaf::
Comm::flush()
{
  if (reqs.size())
    MPI_Waitall(reqs.size(), &reqs[0], MPI_STATUSES_IGNORE);
  reqs.clear();
}

#endif
