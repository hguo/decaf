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

#include <iostream>
#include <assert.h>
#include <strings.h>
#include <stdio.h>
#include <string.h>

#include <decaf/transport/mpi/redist_round_mpi.h>



void
decaf::
RedistRoundMPI::computeGlobal(std::shared_ptr<BaseData> data, RedistRole role)
{
    //Nothing to do here
    return;
}

void
decaf::
RedistRoundMPI::splitData(std::shared_ptr<BaseData> data, RedistRole role)
{
    if(role == DECAF_REDIST_SOURCE){

        //Computing how to split the data

        //Compute the split vector and the destination ranks
        std::vector<std::vector<int> > split_ranges = std::vector<std::vector<int> >( nbDests_);
        int nbItems = data->getNbItems();

        // Create the array which represents where the current source will emit toward
        // the destinations rank. 0 is no send to that rank, 1 is send
        if( summerizeDest_) delete  summerizeDest_;
        summerizeDest_ = new int[ nbDests_];
        bzero( summerizeDest_,  nbDests_ * sizeof(int)); // First we don't send anything

        //Distributing the data in a round robin fashion
        for(int i = 0; i < nbItems; i++)
        {
            split_ranges[i % nbDests_].push_back(i);
        }

        //Updating the informations about messages to send
        for(unsigned int i = 0; i < split_ranges.size(); i++)
        {
            if(split_ranges.at(i).size() > 0)
                destList_.push_back(i + local_dest_rank_);

            //We won't send a message if we send to self
            if(i + local_dest_rank_ != rank_)
                summerizeDest_[i] = 1;
        }


        splitChunks_ =  data->split( split_ranges );

        for(unsigned int i = 0; i < splitChunks_.size(); i++)
        {
            // TODO : Check the rank for the destination.
            // Not necessary to serialize if overlapping
            if(!splitChunks_.at(i)->serialize())
                std::cout<<"ERROR : unable to serialize one object"<<std::endl;
        }

        // DEPRECATED
        // Everything is done, now we can clean the data.
        // Data might be rewriten if producers and consummers are overlapping

        // data->purgeData();

    }
}
