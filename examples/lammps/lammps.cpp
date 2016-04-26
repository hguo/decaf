//---------------------------------------------------------------------------
//
// lammps example
//
// 4-node workflow
//
//          print (1 proc)
//        /
//    lammps (4 procs)
//        \
//          print2 (1 proc) - print (1 proc)
//
//  entire workflow takes 10 procs (1 dataflow proc between each producer consumer pair)
//
// Tom Peterka
// Argonne National Laboratory
// 9700 S. Cass Ave.
// Argonne, IL 60439
// tpeterka@mcs.anl.gov
//
//--------------------------------------------------------------------------
#include <decaf/decaf.hpp>
#include <decaf/data_model/constructtype.h>

#include <assert.h>
#include <math.h>
#include <mpi.h>
#include <string.h>
#include <utility>
#include <map>

// lammps includes
#include "lammps.h"
#include "input.h"
#include "atom.h"
#include "library.h"

using namespace decaf;
using namespace LAMMPS_NS;
using namespace std;

struct lammps_args_t                         // custom args for running lammps
{
    LAMMPS* lammps;
    string infile;
};

struct pos_args_t                            // custom args for atom positions
{
    int natoms;                              // number of atoms
    double* pos;                             // atom positions
};

// runs lammps and puts the atom positions to the dataflow at the consumer intervals
void lammps(Decaf* decaf, int nsteps, int analysis_interval, string infile)
{
    LAMMPS* lps = new LAMMPS(0, NULL, decaf->prod_comm_handle());
    lps->input->file(infile.c_str());

    for (int timestep = 0; timestep < nsteps; timestep++)
    {
        fprintf(stderr, "lammps\n");

        lps->input->one("run 1");
        int natoms = static_cast<int>(lps->atom->natoms);
        double* x = new double[3 * natoms];
        lammps_gather_atoms(lps, (char*)"x", 1, 3, x);

        if (!((timestep + 1) % analysis_interval))
        {
            shared_ptr<ConstructData> container = make_shared<ConstructData>();

            // lammps gathered all positions to rank 0
            if (decaf->prod_comm()->rank() == 0)
            {
                fprintf(stderr, "lammps producing time step %d with %d atoms\n",
                        timestep, natoms);
                // debug
                //         for (int i = 0; i < 10; i++)         // print first few atoms
                //           fprintf(stderr, "%.3lf %.3lf %.3lf\n",
                // x[3 * i], x[3 * i + 1], x[3 * i + 2]);

                shared_ptr<VectorConstructData<double> > data  =
                    make_shared<VectorConstructData<double> >(x, 3 * natoms, 3);
                container->appendData(string("pos"), data,
                                      DECAF_NOFLAG, DECAF_PRIVATE,
                                      DECAF_SPLIT_DEFAULT, DECAF_MERGE_DEFAULT);
            }
            else
            {
                vector<double> pos;
                shared_ptr<VectorConstructData<double> > data  =
                    make_shared<VectorConstructData<double> >(pos, 3);
                container->appendData(string("pos"), data,
                                      DECAF_NOFLAG, DECAF_PRIVATE,
                                      DECAF_SPLIT_DEFAULT, DECAF_MERGE_DEFAULT);
            }
            decaf->put(container);
        }
        delete[] x;
    }
    delete lps;
}

// gets the atom positions and prints them
void print(Decaf* decaf)
{
    fprintf(stderr, "print\n");

    while (1)
    {
        // receive data from all inbound dataflows
        // in this example there is only one inbound dataflow, but in general there could be more
        vector< shared_ptr<ConstructData> > in_data;
        decaf->get(in_data);

        // get the values
        for (size_t i = 0; i < in_data.size(); i++)
        {
            if (Dataflow::test_quit(in_data[i]))
                return;

            shared_ptr<BaseConstructData> ptr = in_data[i]->getData(string("pos"));
            if (ptr)
            {
                shared_ptr<VectorConstructData<double> > pos =
                    dynamic_pointer_cast<VectorConstructData<double> >(ptr);

                // debug
                fprintf(stderr, "consumer print1 or print3 printing %d atoms\n",
                        pos->getNbItems());
                for (int i = 0; i < 10; i++)               // print first few atoms
                    fprintf(stderr, "%.3lf %.3lf %.3lf\n",
                            pos->getVector()[3 * i],
                            pos->getVector()[3 * i + 1],
                            pos->getVector()[3 * i + 2]);
            }
            else
                fprintf(stderr, "Error: null pointer in node2\n");
        }
    }
}

// forwards the atom positions in this example
// in a more realistic example, could filter them and only forward some subset of them
void print2(Decaf* decaf)
{
    while (1)
    {
        int sum = 0;
        bool done = false;

        // receive data from all inbound dataflows
        // in this example there is only one inbound dataflow, but in general there could be more
        vector< shared_ptr<ConstructData> > in_data;
        decaf->get(in_data);

        // get the values and add them
        for (size_t i = 0; i < in_data.size(); i++)
        {
            if (Dataflow::test_quit(in_data[i]))
            {
                done = true;
                break;
            }
            fprintf(stderr, "print2 forwarding positions\n");
            decaf->put(in_data[i]);
        }

        if (done)
        {
            // create a quit message and send it on all outbound dataflows
            shared_ptr<ConstructData> container = make_shared<ConstructData>();
            Dataflow::set_quit(container);
            decaf->put(container);

            return;
        }
    }
}

extern "C"
{
    // dataflow just forwards everything that comes its way in this example
    void dflow(void* args,                          // arguments to the callback
               Dataflow* dataflow,                  // dataflow
               shared_ptr<ConstructData> in_data)   // input data
    {
        dataflow->put(in_data, DECAF_LINK);
    }
} // extern "C"

void run(Workflow& workflow,                 // workflow
         int lammps_nsteps,                  // number of lammps timesteps to execute
         int analysis_interval,              // number of lammps timesteps to skip analyzing
         string infile)                      // lammps input config file
{
    MPI_Init(NULL, NULL);
    Decaf* decaf = new Decaf(MPI_COMM_WORLD, workflow);

    // run workflow node tasks
    // decaf simply tells the user whether this rank belongs to a workflow node
    // how the tasks are called is entirely up to the user
    // e.g., if they overlap in rank, it is up to the user to call them in an order that makes
    // sense (threaded, alternting, etc.)
    // also, the user can define any function signature she wants
    if (decaf->my_node("lammps"))
        lammps(decaf, lammps_nsteps, analysis_interval, infile);
    if (decaf->my_node("print"))
        print(decaf);
    if (decaf->my_node("print2"))
        print2(decaf);

    // MPI_Barrier(MPI_COMM_WORLD);

    // cleanup
    delete decaf;
    MPI_Finalize();
}

// test driver for debugging purposes
// normal entry point is run(), called by python
int main(int argc,
         char** argv)
{
    Workflow workflow;
    int lammps_nsteps     = 1;
    int analysis_interval = 1;
    char * prefix         = getenv("DECAF_PREFIX");
    if (prefix == NULL)
    {
        fprintf(stderr, "ERROR: environment variable DECAF_PREFIX not defined. Please export "
                "DECAF_PREFIX to point to the root of your decaf install directory.\n");
        exit(1);
    }
    string path = string(prefix , strlen(prefix));
    path.append(string("/examples/lammps/mod_lammps.so"));
    string infile = argv[1];


    // fill workflow nodes
    WorkflowNode node;
    node.in_links.push_back(1);              // print1
    node.start_proc = 5;
    node.nprocs = 1;
    node.func = "print";
    node.path = path;
    workflow.nodes.push_back(node);

    node.out_links.clear();
    node.in_links.clear();
    node.in_links.push_back(0);              // print3
    node.start_proc = 9;
    node.nprocs = 1;
    node.func = "print";
    node.path = path;
    workflow.nodes.push_back(node);

    node.out_links.clear();
    node.in_links.clear();
    node.out_links.push_back(0);             // print2
    node.in_links.push_back(2);
    node.start_proc = 7;
    node.nprocs = 1;
    node.func = "print2";
    node.path = path;
    workflow.nodes.push_back(node);

    node.out_links.clear();
    node.in_links.clear();
    node.out_links.push_back(1);             // lammps
    node.out_links.push_back(2);
    node.start_proc = 0;
    node.nprocs = 4;
    node.func = "lammps";
    node.path = path;
    workflow.nodes.push_back(node);

    // fill workflow links
    WorkflowLink link;
    link.prod = 2;                           // print2 - print3
    link.con = 1;
    link.start_proc = 8;
    link.nprocs = 1;
    link.func = "dflow";
    link.path = path;
    link.prod_dflow_redist = "count";
    link.dflow_con_redist = "count";
    workflow.links.push_back(link);

    link.prod = 3;                           // lammps - print1
    link.con = 0;
    link.start_proc = 4;
    link.nprocs = 1;
    link.func = "dflow";
    link.path = path;
    link.prod_dflow_redist = "count";
    link.dflow_con_redist = "count";
    workflow.links.push_back(link);

    link.prod = 3;                           // lammps - print2
    link.con = 2;
    link.start_proc = 6;
    link.nprocs = 1;
    link.func = "dflow";
    link.path = path;
    link.prod_dflow_redist = "count";
    link.dflow_con_redist = "count";
    workflow.links.push_back(link);

    // run decaf
    run(workflow, lammps_nsteps, analysis_interval, infile);

    return 0;
}
