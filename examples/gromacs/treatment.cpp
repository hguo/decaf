//---------------------------------------------------------------------------
//
// 2-node producer-consumer coupling example
//
// prod (4 procs) - con (2 procs)
//
// entire workflow takes 8 procs (2 dataflow procs between prod and con)
// this file contains the consumer (2 procs)
//
// Tom Peterka
// Argonne National Laboratory
// 9700 S. Cass Ave.
// Argonne, IL 60439
// mdreher@anl.gov
//
//--------------------------------------------------------------------------

#include <decaf/decaf.hpp>
#include <decaf/data_model/simplefield.hpp>
#include <decaf/data_model/arrayfield.hpp>
#include <decaf/data_model/blockfield.hpp>
//#include <decaf/data_model/array3dconstructdata.hpp>
#include <boost/multi_array.hpp>
#include <decaf/data_model/boost_macros.h>

#include "decaf/data_model/morton.h"

#include <assert.h>
#include <math.h>
#include <mpi.h>
#include <map>
#include <cstdlib>
#include <sstream>
#include <fstream>


#include "wflow_gromacs.hpp"                         // defines the workflow for this example

using namespace decaf;
using namespace std;
using namespace boost;

template <typename T>
T clip(const T& n, const T& lower, const T& upper) {
  return max(lower, min(n, upper));
}

void posToFile(float* pos, int nbParticules, const string filename)
{
    ofstream file;
    cout<<"Filename : "<<filename<<endl;
    file.open(filename.c_str());

    unsigned int r,g,b;
    r = rand() % 255;
    g = rand() % 255;
    b = rand() % 255;

    unsigned int ur,ug,ub;
    ur = r;
    ug = g;
    ub = b;
    ur = clip<unsigned int>(ur, 0, 255);
    ug = clip<unsigned int>(ug, 0, 255);
    ub = clip<unsigned int>(ub, 0, 255);
    //cout<<"UColor : "<<ur<<","<<ug<<","<<ub<<endl;

    //cout<<"Number of particules to save : "<<nbParticules<<endl;
    file<<"ply"<<endl;
    file<<"format ascii 1.0"<<endl;
    file<<"element vertex "<<nbParticules<<endl;
    file<<"property float x"<<endl;
    file<<"property float y"<<endl;
    file<<"property float z"<<endl;
    file<<"property uchar red"<<endl;
    file<<"property uchar green"<<endl;
    file<<"property uchar blue"<<endl;
    file<<"end_header"<<endl;
    for(int i = 0; i < nbParticules; i++)
        file<<pos[3*i]<<" "<<pos[3*i+1]<<" "<<pos[3*i+2]
            <<" "<<ur<<" "<<ug<<" "<<ub<<endl;
    file.close();
}

void computeBBox(float* pos, int nbParticles)
{
    if(nbParticles > 0)
    {
        float xmin,ymin,zmin,xmax,ymax,zmax;
        xmin = pos[0];
        xmax = pos[0];
        ymin = pos[1];
        ymax = pos[1];
        zmin = pos[2];
        zmax = pos[2];

        for(int i = 1; i < nbParticles; i++)
        {
            if(xmin > pos[3*i])
                xmin = pos[3*i];
            if(ymin > pos[3*i+1])
                ymin = pos[3*i+1];
            if(zmin > pos[3*i+2])
                zmin = pos[3*i+2];
            if(xmax < pos[3*i])
                xmax = pos[3*i];
            if(ymax < pos[3*i+1])
                ymax = pos[3*i+1];
            if(zmax < pos[3*i+2])
                zmax = pos[3*i+2];
        }

        std::cout<<"["<<xmin<<","<<ymin<<","<<zmin<<"]["<<xmax<<","<<ymax<<","<<zmax<<"]"<<std::endl;
    }
}

// consumer
void treatment1(Decaf* decaf)
{
    vector< pConstructData > in_data;
    fprintf(stderr, "Launching treatment\n");
    fflush(stderr);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int iteration = 0;

    while (decaf->get(in_data))
    {
        // get the atom positions
        fprintf(stderr, "Reception of %u messages.\n", in_data.size());

        fprintf(stderr, "Number of particles received : %i\n", in_data[0]->getNbItems());
        if(in_data[0]->getNbItems() > 0)
        {
            for (size_t i = 0; i < in_data.size(); i++)
            {


                // Getting the grid info
                BlockField blockField  = in_data[i]->getFieldData<BlockField>("domain_block");
                Block<3>* block = blockField.getBlock();
                //block->printExtends();
                //block->printBoxes();
            }

            ArrayFieldf posField = in_data[0]->getFieldData<ArrayFieldf>("pos");

            stringstream filename;
            filename<<"pos_"<<rank<<"_"<<iteration<<"_treat.ply";
            posToFile(posField.getArray(), posField->getNbItems(), filename.str());

            computeBBox(posField.getArray(), posField->getNbItems());

        }

        // Now each process has a sub domain of the global grid



        //Building the grid
        /*unsigned int* lExtends = block->getLocalExtends();
        multi_array<float,3>* grid = new multi_array<float,3>(
                    extents[lExtends[3]][lExtends[4]][lExtends[5]]
                );


        ArrayFieldu mortonField = atoms->getFieldData<ArrayFieldu>("morton");
        unsigned int *morton = mortonField.getArray();
        int nbMorton = mortonField->getNbItems();

        for(int i = 0; i < nbMorton; i++)
        {
            unsigned int x,y,z;
            Morton_3D_Decode_10bit(morton[i], x, y, z);

            // Checking if the particle should be here
            if(!block->isInLocalBlock(x,y,z))
                fprintf(stderr, "ERROR : particle not belonging to the local block. FIXME\n");

            unsigned int localx, localy, localz;
            localx = x - lExtends[0];
            localy = y - lExtends[1];
            localz = z - lExtends[2];

            (*grid)[localx][localy][localz] += 1.0f; // TODO : get the full formulation
        }


        delete grid;
        */

        iteration++;
    }

    // terminate the task (mandatory) by sending a quit message to the rest of the workflow
    fprintf(stderr, "Treatment terminating\n");
    //decaf->terminate();
}

// every user application needs to implement the following run function with this signature
// run(Workflow&) in the global namespace
void run(Workflow& workflow)                             // workflow
{
    MPI_Init(NULL, NULL);

    char processorName[MPI_MAX_PROCESSOR_NAME];
    int size_world, rank, nameLen;

    MPI_Comm_size(MPI_COMM_WORLD, &size_world);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Get_processor_name(processorName,&nameLen);

    srand(time(NULL) + rank * size_world + nameLen);

    fprintf(stderr, "treatment rank %i\n", rank);

    // create decaf
    Decaf* decaf = new Decaf(MPI_COMM_WORLD, workflow);

    // start the task
    treatment1(decaf);

    // cleanup
    delete decaf;
    MPI_Finalize();
}

// test driver for debugging purposes
// normal entry point is run(), called by python
int main(int argc,
         char** argv)
{
    fprintf(stderr, "Hello treatment\n");
    // define the workflow
    Workflow workflow;
    make_wflow(workflow);

    // run decaf
    run(workflow);

    return 0;
}
