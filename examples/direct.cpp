//---------------------------------------------------------------------------
//
// example of direct coupling
//
// Tom Peterka
// Argonne National Laboratory
// 9700 S. Cass Ave.
// Argonne, IL 60439
// tpeterka@mcs.anl.gov
//
//--------------------------------------------------------------------------
#include <decaf/decaf.hpp>

// user-defined producer code
void prod_code(void* args)
{
}
// user-defined selector code
void con_code(void* args)
{
}
// user-defined selector code
void sel_type(void *args)
{
}
// user-defined pipeliner code
void pipe_type(void *args)
{
}
// user-defined aggregator code
void aggr_type(void *args)
{
}
// user-defined resilience code
void fault_check(void *args)
{
}
//
// main is the producer (eg. simulation)
//
int main(int argc, char** argv)
{
  // create (split) new communicators
  int prod_size = 8;  // fake some communicator sizes: producer
  int con_size = 4;   // consumer
  int dflow_size = 2; // dataflow size defines number of aggregator and other intermediate nodes
  decaf::Comm prod_comm(MPI_COMM_WORLD, prod_size);
  decaf::Comm con_comm(MPI_COMM_WORLD, con_size);
  decaf::Comm dflow_comm(MPI_COMM_WORLD, dflow_size);

  // describe producer, consumer intrinsic data properties
  decaf::Data prod_data;
  decaf::Data con_data;

  // create producer, consumer, dataflow
  decaf::Producer prod(prod_comm,
                       &prod_code,
                       prod_data);
  decaf::Consumer con(con_comm,
                      &con_code,
                      &sel_type,
                      &pipe_type,
                      &aggr_type,
                      &fault_check,
                      con_data);
  decaf::Dataflow dflow(dflow_comm);

  // simulation loop
  // producer runs multiple time steps and calls the consumer at some interval
  int tot_time_steps = 100; // some fake values
  int con_interval = 10;
  void* data; // pointer to base address of data generated by producer

  for (int t = 0; t < tot_time_steps; t++)
  {
    // custom producer code
    prod.exec(data);

    // run the consumer code
    if (!(t % con_interval))
      con.exec(data);
  }
}
