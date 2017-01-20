//---------------------------------------------------------------------------
//
// 2-node producer-consumer coupling example with buffering mechanism
//
// prod (1 procs) - con (1 procs)
//
// entire workflow takes 3 procs (1 dataflow procs between prod and con)
//
// Matthieu Dreher
// Argonne National Laboratory
// 9700 S. Cass Ave.
// Argonne, IL 60439
// mdreher@anl.gov
//
//--------------------------------------------------------------------------

#include <decaf/decaf.hpp>
#include <decaf/data_model/pconstructtype.h>
#include <decaf/data_model/simplefield.hpp>
#include <decaf/data_model/boost_macros.h>

#include <assert.h>
#include <math.h>
#include <mpi.h>
#include <map>
#include <cstdlib>

using namespace decaf;
using namespace std;

// link callback function
extern "C"
{
    // dataflow just forwards everything that comes its way in this example
    void dflow(void* args,                          // arguments to the callback
               Dataflow* dataflow,                  // dataflow
               pConstructData in_data)   // input data
    {
        fprintf(stderr, "Forwarding data in dflow\n");
        dataflow->put(in_data, DECAF_LINK);
    }
} // extern "C"

int main(int argc,
         char** argv)
{
    Workflow workflow;
    Workflow::make_wflow_from_json(workflow, "buffer.json");

    MPI_Init(NULL, NULL);

    // create decaf
    Decaf* decaf = new Decaf(MPI_COMM_WORLD, workflow);

    // cleanup
    delete decaf;
    MPI_Finalize();

    return 0;
}
