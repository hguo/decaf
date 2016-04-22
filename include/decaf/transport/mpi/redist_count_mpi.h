//---------------------------------------------------------------------------
//
// data interface
//
// Matthieu Dreher
// Argonne National Laboratory
// 9700 S. Cass Ave.
// Argonne, IL 60439
// mdreher@anl.gov
//
//--------------------------------------------------------------------------

#ifndef DECAF_REDIST_COUNT_MPI_HPP
#define DECAF_REDIST_COUNT_MPI_HPP

#include <iostream>
#include <assert.h>

#include <decaf/transport/mpi/types.h>
#include <decaf/transport/mpi/redist_mpi.h>

namespace decaf
{

    class RedistCountMPI : public RedistMPI
    {
    public:

    RedistCountMPI() :
        RedistMPI() {}
    RedistCountMPI(int rankSource,
                   int nbSources,
                   int rankDest,
                   int nbDests,
                   CommHandle communicator,
                   RedistCommMethod commMethod  = DECAF_REDIST_COLLECTIVE) :
        RedistMPI(rankSource, nbSources, rankDest, nbDests, communicator, commMethod) {}
        virtual ~RedistCountMPI(){}

    protected:

        // Compute the values necessary to determine how the data should be splitted
        // and redistributed.
        virtual void computeGlobal(std::shared_ptr<BaseData> data, RedistRole role);

        // Seperate the Data into chunks for each destination involve in the component
        // and fill the splitChunks vector
        virtual void splitData(std::shared_ptr<BaseData> data, RedistRole role);

        // We keep these values so we can reuse them between 2 iterations
        int global_item_rank_;    // Index of the first item in the global array
        int global_nb_items_;     // Number of items in the global array

    };

} // namespace

#endif
