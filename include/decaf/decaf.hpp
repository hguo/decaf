//---------------------------------------------------------------------------
//
// decaf top-level interface
//
// Tom Peterka
// Argonne National Laboratory
// 9700 S. Cass Ave.
// Argonne, IL 60439
// tpeterka@mcs.anl.gov
//
//--------------------------------------------------------------------------

#ifndef DECAF_HPP
#define DECAF_HPP

#include <map>
#include "dataflow.hpp"

// BFS node
struct BFSNode
{
  int index;                    // index of node in all workflow nodes
  int dist;                     // distance from start (level in bfs tree)
};

// transport layer specific types
#ifdef TRANSPORT_MPI
#include "transport/mpi/types.hpp"
#include "transport/mpi/comm.hpp"
#include "transport/mpi/data.hpp"
#endif

#include "types.hpp"
#include "data.hpp"

namespace decaf
{
  class Decaf
  {
  public:
    Decaf(CommHandle world_comm,
          Workflow& workflow,
          int prod_nsteps,
          int con_nsteps):
      world_comm_(world_comm),
      workflow_(workflow),
      prod_nsteps_(prod_nsteps),
      con_nsteps_(con_nsteps)  {}
    ~Decaf()                   {}
    void err()                 { ::all_err(err_); }
    void run(Data* data,
             map<string, void(*)(void*,
                                 int,
                                 int,
                                 int,
                                 vector<Dataflow*>&,
                                 int)> callbacks,
             void (*pipeliner)(Dataflow*),
             void (*checker)(Dataflow*));

  private:
    // builds a vector of dataflows for all links in the workflow
    void build_dataflows(vector<Dataflow*>& dataflows,
                         void (*pipeliner)(Dataflow*),
                         void (*checker)(Dataflow*),
                         decaf::Data* data)
      {
        DecafSizes decaf_sizes;
        for (size_t i = 0; i < workflow_.links.size(); i++)
        {
          int prod  = workflow_.links[i].prod;    // index into workflow nodes
          int dflow = i;                          // index into workflow links
          int con   = workflow_.links[i].con;     // index into workflow nodes
          decaf_sizes.prod_size   = workflow_.nodes[prod].nprocs;
          decaf_sizes.dflow_size  = workflow_.links[dflow].nprocs;
          decaf_sizes.con_size    = workflow_.nodes[con].nprocs;
          decaf_sizes.prod_start  = workflow_.nodes[prod].start_proc;
          decaf_sizes.dflow_start = workflow_.links[dflow].start_proc;
          decaf_sizes.con_start   = workflow_.nodes[con].start_proc;
          decaf_sizes.con_nsteps  = con_nsteps_;
          dataflows.push_back(new Dataflow(world_comm_, decaf_sizes, pipeliner, checker, data));
          dataflows[i]->err();
        }
      }

    // computes breadth-first search (BFS) traversal of the workflow nodes
    // returns number of levels in the bfs tree (distance of farthest node from source)
    int bfs(vector<int>& sources,       // starting nodes (indices into workflow nodes)
            vector<BFSNode>& bfs_nodes) // output bfs node order (indices into workflow nodes)
      {
        vector<bool> visited_nodes;
        visited_nodes.reserve(workflow_.nodes.size());
        queue<BFSNode> q;
        BFSNode bfs_node;

        // init
        bfs_node.index = -1;
        bfs_node.dist  = 0;
        for (size_t i = 0; i < workflow_.nodes.size(); i++)
          visited_nodes[i] = false;

        // init source nodes
        for (size_t i = 0; i < sources.size(); i++)
        {
          visited_nodes[sources[i]] = true;
          bfs_node.index = sources[i];
          bfs_node.dist = 0;
          q.push(bfs_node);
        }

        while (!q.empty())
        {
          int u = q.front().index;
          int d = q.front().dist;
          q.pop();
          bfs_node.index = u;
          bfs_node.dist = d;
          bfs_nodes.push_back(bfs_node);
          for (size_t i = 0; i < workflow_.nodes[u].out_links.size(); i++)
          {
            int v = workflow_.links[workflow_.nodes[u].out_links[i]].con;
            if (visited_nodes[v] == false)
            {
              visited_nodes[v] = true;
              bfs_node.index = v;
              bfs_node.dist = d + 1;
              q.push(bfs_node);
            }
          }
        }
        return bfs_node.dist;
      }

    CommHandle world_comm_;     // handle to original world communicator
    Workflow workflow_;         // workflow
    int prod_nsteps_;           // total number of producer time steps
    int con_nsteps_;            // number of time steps that are consumed (sent through decaf)
    int err_;                   // last error
  };
} // namespace


// runs the workflow
//
// computes a BFS order of the workflow nodes and calls the producer, consumer, and
// dataflow callbacks each time the order grows by another level (distance from the source nodes)
void
decaf::
Decaf::run(decaf::Data* data,                      // data model
           map<string, void(*)(void*,
                               int,
                               int,
                               int,
                               vector<decaf::Dataflow*>&,
                               int)> callbacks,    // map of callback functions
           void (*pipeliner)(decaf::Dataflow*),    // custom pipeliner code
           void (*checker)(decaf::Dataflow*))      // custom resilience code
{
  vector<decaf::Dataflow*> dataflows;

  // create decaf instances for all links in the workflow
  build_dataflows(dataflows, pipeliner, checker, data);

  // TODO: assuming the same consumer interval for the entire workflow
  // this should not be necessary, need to experiment
  int con_interval;                                      // consumer interval
  if (con_nsteps_)
    con_interval = prod_nsteps_ / con_nsteps_;           // consume every so often
  else
    con_interval = -1;                                   // don't consume

  // find the sources of the workflow
  vector<int> sources;
  for (size_t i = 0; i < workflow_.nodes.size(); i++)
  {
    if (workflow_.nodes[i].in_links.size() == 0)
      sources.push_back(i);
  }

  // compute a BFS of the graph
  vector<BFSNode> bfs_order;
  int nlevels = bfs(sources, bfs_order);

  // debug: print the bfs order
//   for (int i = 0; i < bfs_order.size(); i++)
//     fprintf(stderr, "nlevels = %d bfs[%d] = index %d dist %d\n",
//             nlevels, i, bfs_order[i].index, bfs_order[i].dist);

  // start the dataflows
  for (size_t i = 0; i < workflow_.links.size(); i++)
    dataflows[i]->run();

  // run the main loop
  for (int t = 0; t < prod_nsteps_; t++)
  {

    // execute the workflow, calling nodes in BFS order
    int n = 0;
    for (int level = 0; level <= nlevels; level++)
    {
      while (1)
      {
        if (n >= bfs_order.size() || bfs_order[n].dist > level)
          break;
        int u = bfs_order[n].index;
        // debug
//         fprintf(stderr, "level = %d n = %d u = %d\n", level, n, u);

        // fill dataflows
        vector<decaf::Dataflow*> out_dataflows;
        vector<decaf::Dataflow*> in_dataflows;
        for (size_t j = 0; j < workflow_.nodes[u].out_links.size(); j++)
          out_dataflows.push_back(dataflows[workflow_.nodes[u].out_links[j]]);
        for (size_t j = 0; j < workflow_.nodes[u].in_links.size(); j++)
          in_dataflows.push_back(dataflows[workflow_.nodes[u].in_links[j]]);

        // consumer
        if (in_dataflows.size() && in_dataflows[0]->is_con())
          callbacks[workflow_.nodes[u].con_func](workflow_.nodes[u].con_args, t,
                                                con_interval, prod_nsteps_, in_dataflows, -1);
        // producer
        if (out_dataflows.size() && out_dataflows[0]->is_prod())
          callbacks[workflow_.nodes[u].prod_func](workflow_.nodes[u].prod_args, t,
                                                 con_interval, prod_nsteps_, out_dataflows, -1);

        // dataflow
        for (size_t j = 0; j < out_dataflows.size(); j++)
        {
          int l = workflow_.nodes[u].out_links[j];
          if (out_dataflows[j]->is_dflow())
            callbacks[workflow_.links[l].dflow_func](workflow_.links[l].dflow_args, t,
                                                    con_interval, prod_nsteps_, out_dataflows, j);
        }
        n++;
      }
    }
  }

  // cleanup
  for (size_t i = 0; i < dataflows.size(); i++)
    delete dataflows[i];
}

#endif
