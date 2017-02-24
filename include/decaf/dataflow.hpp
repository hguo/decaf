﻿//---------------------------------------------------------------------------
// dataflow interface
//
// Tom Peterka
// Argonne National Laboratory
// 9700 S. Cass Ave.
// Argonne, IL 60439
// tpeterka@mcs.anl.gov
//
//--------------------------------------------------------------------------

#ifndef DECAF_DATAFLOW_HPP
#define DECAF_DATAFLOW_HPP

#include <map>
#include <string>

#include <decaf/data_model/pconstructtype.h>
#include <decaf/data_model/simpleconstructdata.hpp>
#include <decaf/data_model/msgtool.hpp>

#include <boost/type_index.hpp>

// transport layer specific types
#ifdef TRANSPORT_MPI
#include <decaf/transport/mpi/types.h>
#include <decaf/transport/mpi/comm.hpp>
#include <decaf/transport/mpi/redist_count_mpi.h>
#include <decaf/transport/mpi/redist_round_mpi.h>
#include <decaf/transport/mpi/redist_zcurve_mpi.h>
#include <decaf/transport/mpi/redist_block_mpi.h>
#include <decaf/transport/mpi/redist_proc_mpi.h>
#include <decaf/transport/mpi/tools.hpp>
#include <decaf/transport/mpi/channel.hpp>
#include <decaf/datastream/datastreamdoublefeedback.hpp>
#include <decaf/datastream/datastreamsinglefeedback.hpp>
#include <memory>
#include <queue>
#endif

#include "types.hpp"
#include "workflow.hpp"

namespace decaf
{
    // world is partitioned into {producer, dataflow, consumer, other} in increasing rank
    class Dataflow
    {
    public:
        Dataflow(CommHandle world_comm,          // world communicator
                 DecafSizes& decaf_sizes,        // sizes of producer, dataflow, consumer
                 int prod,                       // id in workflow structure of producer node
                 int dflow,                      // id in workflow structure of dataflow link
		         int con,					   	 // id in workflow structure of consumer node
		         WorkflowLink wflowLink,
		         Decomposition prod_dflow_redist, // decompositon between producer and dataflow
		         Decomposition dflow_cons_redist, // decomposition between dataflow and consumer
		         StreamPolicy streamPolicy,
		         FramePolicyManagment framePolicy,
		         vector<StorageType>& storage_types,
		         vector<unsigned int>& max_storage_sizes);

        ~Dataflow();
		bool put(pConstructData data, TaskType role);
		bool get(pConstructData data, TaskType role);

		pConstructData filterPut(pConstructData data, TaskType role, bool& data_changed, bool& filtered_empty);
		void filterGet(pConstructData data, TaskType role);

        DecafSizes* sizes()    { return &sizes_; }
        void shutdown();
        void err()             { ::all_err(err_); }
        // whether this rank is producer, dataflow, or consumer (may have multiple roles)
        bool is_prod()          { return((type_ & DECAF_PRODUCER_COMM) == DECAF_PRODUCER_COMM); }
        bool is_dflow()         { return((type_ & DECAF_DATAFLOW_COMM) == DECAF_DATAFLOW_COMM); }
        bool is_con()           { return((type_ & DECAF_CONSUMER_COMM) == DECAF_CONSUMER_COMM); }
        bool is_prod_root()     { return world_rank_ == sizes_.prod_start; }
        bool is_dflow_root()     { return world_rank_ == sizes_.dflow_start; }
        bool is_con_root()     { return world_rank_ == sizes_.con_start; }
        CommHandle prod_comm_handle() { return prod_comm_->handle(); }
        CommHandle con_comm_handle()  { return con_comm_->handle();  }
        CommHandle dflow_comm_handle(){ return dflow_comm_->handle();}
        Comm* prod_comm()             { return prod_comm_; }
        Comm* dflow_comm()            { return dflow_comm_; }
        Comm* con_comm()              { return con_comm_;  }
        void forward();

		vector<ContractKey>& keys(){ return list_keys_; }
		bool is_contract(){		return bContract_;}

		string srcPort(){ return srcPort_; }
		string destPort(){ return destPort_; }

		bool isClose(){ return bClose_; }
		void setClose(){ bClose_ = true; }

        // Clear the buffer of the redistribution components.
        // To call if the data model change from one iteration to another or to free memory space
        void clearBuffers(TaskType role);

		// puts an iteration number into a container
		static
		void
		set_iteration(pConstructData out_data, int iteration)
		{
			if(out_data->hasData("decaf_iteration")){
				if(get_iteration(out_data) == iteration){ // The correct iteration is already set
					return;
				}
				std::cout << "WARNING: iteration already set with different value: "<< get_iteration(out_data) << " " << iteration << std::endl;
				out_data->removeData("decaf_iteration");
			}
			shared_ptr<SimpleConstructData<int> > data  =
			    make_shared<SimpleConstructData<int> >(iteration);
			out_data->appendData(string("decaf_iteration"), data,
			                     DECAF_NOFLAG, DECAF_SYSTEM,
			                     DECAF_SPLIT_KEEP_VALUE, DECAF_MERGE_FIRST_VALUE);
		}

		static
		int
		get_iteration(pConstructData data)
		{
			shared_ptr<SimpleConstructData<int> > ptr =
			        data->getTypedData<SimpleConstructData<int> >("decaf_iteration");
			if(ptr){
				return ptr->getData();
			}
			return -1;
		}

        // sets a quit message into a container; caller still needs to send the message
        static
        void
        set_quit(pConstructData out_data)   // output message
		{
			shared_ptr<SimpleConstructData<int> > data  =
			    make_shared<SimpleConstructData<int> >(1);
			out_data->appendData(string("decaf_quit"), data,
			                     DECAF_NOFLAG, DECAF_SYSTEM,
			                     DECAF_SPLIT_KEEP_VALUE, DECAF_MERGE_FIRST_VALUE);
			out_data->setSystem(true);
		}

        // tests whether a message is a quit command
        static
        bool
        test_quit(pConstructData in_data)   // input message
        {
            return in_data->hasData(string("decaf_quit"));
        }

    private:
        CommHandle world_comm_;          // handle to original world communicator
        Comm* prod_comm_;                // producer communicator
        Comm* con_comm_;                 // consumer communicator
        Comm* dflow_comm_;               // dflow communicator
        int world_rank_;
        int world_size_;
        RedistComp* redist_prod_dflow_;  // Redistribution component between producer and dataflow
        RedistComp* redist_dflow_con_;   // Redestribution component between a dataflow and consumer
        RedistComp* redist_prod_con_;    // Redistribution component between producer and consumer
        DecafSizes sizes_;               // sizes of communicators, time steps
        int err_;                        // last error
        CommTypeDecaf type_;             // whether this instance is producer, consumer,
                                         // dataflow, or other
        int wflow_prod_id_;              // index of corresponding producer in the workflow
        int wflow_con_id_;               // index of corresponding consumer in the workflow
        int wflow_dflow_id_;             // index of corresponding link in the workflow
		bool no_link_;                   // True if the Dataflow doesn't have a Link
		bool use_stream_;                // True if the Dataflow manages a buffer.

		// Buffer infos
		StreamPolicy stream_policy_;         // Type of stream to use
		Datastream* stream_;

		int iteration;					// Iterations counter
		bool prod_link_overlap;		// Whether the is overlaping between producer and link

		string srcPort_;				// Portname of the source
		string destPort_;				// Portname of the destination
		bool bClose_;					// Determines if a quit message has been received when calling a get

		bool bContract_;				// boolean to say if the dataflow has a contract or not
		Check_level check_level_;			 // level of typechecking used; Relevant if bContract_ is set to true
		vector<ContractKey> list_keys_;   // keys of the data to be exchanged b/w the producer and consumer; Relevant if bContract_ is set to true
	};// End of class Dataflow

} // namespace

decaf::
Dataflow::Dataflow(CommHandle world_comm,
                   DecafSizes& decaf_sizes,
                   int prod,
                   int dflow,
                   int con,
                   WorkflowLink wflowLink,
                   Decomposition prod_dflow_redist,
                   Decomposition dflow_cons_redist,
                   StreamPolicy stream_policy,
                   FramePolicyManagment framePolicy,
                   vector<StorageType>& storage_types,
                   vector<unsigned int>& max_storage_sizes) :
    world_comm_(world_comm),
    sizes_(decaf_sizes),
    wflow_prod_id_(prod),
    wflow_dflow_id_(dflow),
    wflow_con_id_(con),
    redist_prod_dflow_(NULL),
    redist_dflow_con_(NULL),
    redist_prod_con_(NULL),
    type_(DECAF_OTHER_COMM),
    check_level_(wflowLink.check_level),
    srcPort_(wflowLink.srcPort),
    destPort_(wflowLink.destPort),
    bClose_(false),
    no_link_(false),
    use_stream_(false),
    stream_policy_(stream_policy)
{
    // DEPRECATED
    // sizes is a POD struct, initialization was not allowed in C++03; used assignment workaround
    // in C++11, apparently able to initialize as above by saying sizes_(decaf_sizes)
    // sizes_.prod_size = decaf_sizes.prod_size;
    // sizes_.dflow_size = decaf_sizes.dflow_size;
    // sizes_.con_size = decaf_sizes.con_size;
    // sizes_.prod_start = decaf_sizes.prod_start;
    // sizes_.dflow_start = decaf_sizes.dflow_start;
    // sizes_.con_start = decaf_sizes.con_start;
    // sizes_.con_nsteps = decaf_sizes.con_nsteps;

    // debug, confirming that above initialization works in all compilers
    assert(sizes_.prod_size == decaf_sizes.prod_size);
    assert(sizes_.dflow_size == decaf_sizes.dflow_size);
    assert(sizes_.con_size == decaf_sizes.con_size);
    assert(sizes_.prod_start == decaf_sizes.prod_start);
    assert(sizes_.dflow_start == decaf_sizes.dflow_start);
    assert(sizes_.con_start == decaf_sizes.con_start);

    world_rank_ = CommRank(world_comm); // my place in the world
    world_size_ = CommSize(world_comm);

    // ensure sizes and starts fit in the world
    if (sizes_.prod_start + sizes_.prod_size > world_size_   ||
        sizes_.dflow_start + sizes_.dflow_size > world_size_ ||
        sizes_.con_start + sizes_.con_size > world_size_)
    {
        err_ = DECAF_COMM_SIZES_ERR;
        return;
	}

	// Sets the boolean value to true/false whether the dataflow is related to a contract or not
	if(wflowLink.list_keys.size() == 0){
		bContract_ = false;
	}
	else{
		bContract_ = true;
		list_keys_ = wflowLink.list_keys;
		iteration = 0;
	}

    // debug
    // if (world_rank == 0)
    //     fprintf(stderr, "prod: start %d size %d dflow: start %d size %d con: start %d size %d\n",
    //             sizes_.prod_start, sizes_.prod_size, sizes_.dflow_start, sizes_.dflow_size,
    //             sizes_.con_start, sizes_.con_size);

    // communicators

    if (world_rank_ >= sizes_.prod_start &&                   // producer
        world_rank_ < sizes_.prod_start + sizes_.prod_size)
    {
        type_ |= DECAF_PRODUCER_COMM;
        prod_comm_ = new Comm(world_comm, sizes_.prod_start, sizes_.prod_start + sizes_.prod_size - 1);
	}

    if (world_rank_ >= sizes_.dflow_start &&                  // dataflow
        world_rank_ < sizes_.dflow_start + sizes_.dflow_size)
    {
        type_ |= DECAF_DATAFLOW_COMM;
        dflow_comm_ = new Comm(world_comm, sizes_.dflow_start, sizes_.dflow_start + sizes_.dflow_size - 1);
    }

	if (world_rank_ >= sizes_.con_start &&                    // consumer
        world_rank_ < sizes_.con_start + sizes_.con_size)
    {
        type_ |= DECAF_CONSUMER_COMM;
        con_comm_ = new Comm(world_comm, sizes_.con_start, sizes_.con_start + sizes_.con_size - 1);
    }

	if( (type_ & DECAF_PRODUCER_COMM) == DECAF_PRODUCER_COMM && (type_ & DECAF_DATAFLOW_COMM) == DECAF_DATAFLOW_COMM){
		prod_link_overlap = true;
	}
	else{
		prod_link_overlap = false;
	}


    // producer and dataflow
    if (sizes_.dflow_size > 0 && (
        (world_rank_ >= sizes_.prod_start && world_rank_ < sizes_.prod_start + sizes_.prod_size) ||
        (world_rank_ >= sizes_.dflow_start && world_rank_ < sizes_.dflow_start + sizes_.dflow_size)))
    {
        switch(prod_dflow_redist)
        {
        case DECAF_ROUND_ROBIN_DECOMP:
        {
            // fprintf(stderr, "Using Round for prod -> dflow\n");
            redist_prod_dflow_ = new RedistRoundMPI(sizes_.prod_start,
                                                    sizes_.prod_size,
                                                    sizes_.dflow_start,
                                                    sizes_.dflow_size,
                                                    world_comm);
            break;
        }
        case DECAF_CONTIG_DECOMP:
        {
            // fprintf(stderr, "Using Count for prod -> dflow prod\n");
            redist_prod_dflow_ = new RedistCountMPI(sizes_.prod_start,
                                                    sizes_.prod_size,
                                                    sizes_.dflow_start,
                                                    sizes_.dflow_size,
                                                    world_comm);
            break;
        }
        case DECAF_ZCURVE_DECOMP:
        {
            // fprintf(stderr, "Using ZCurve for prod -> dflow\n");
            redist_prod_dflow_ = new RedistZCurveMPI(sizes_.prod_start,
                                                     sizes_.prod_size,
                                                     sizes_.dflow_start,
                                                     sizes_.dflow_size,
                                                     world_comm);
            break;
        }
        case DECAF_BLOCK_DECOMP:
        {
            redist_prod_dflow_ = new RedistBlockMPI(sizes_.prod_start,
                                                    sizes_.prod_size,
                                                    sizes_.dflow_start,
                                                    sizes_.dflow_size,
                                                    world_comm);
            break;
        }
        case DECAF_PROC_DECOMP:
        {
            redist_prod_dflow_ = new RedistProcMPI(sizes_.prod_start,
                                                   sizes_.prod_size,
                                                   sizes_.dflow_start,
                                                   sizes_.dflow_size,
                                                   world_comm);
            break;
        }
        default:
        {
            fprintf(stderr, "ERROR: policy %d unrecognized to select a redistribution component. "
                    "Using RedistCountMPI instead\n", prod_dflow_redist);
            redist_prod_dflow_ = new RedistCountMPI(sizes_.prod_start,
                                                    sizes_.prod_size,
                                                    sizes_.dflow_start,
                                                    sizes_.dflow_size,
                                                    world_comm);
            break;
        }

        }
    }
    else
    {
        //fprintf(stderr, "No redistribution between producer and dflow needed.\n");
        redist_prod_dflow_ = NULL;
    }

    // consumer and dataflow
    if (sizes_.dflow_size > 0 && (
        (world_rank_ >= sizes_.dflow_start && world_rank_ < sizes_.dflow_start + sizes_.dflow_size) ||
        (world_rank_ >= sizes_.con_start && world_rank_ < sizes_.con_start + sizes_.con_size)))
    {
        switch(dflow_cons_redist)
        {
        case DECAF_ROUND_ROBIN_DECOMP:
        {
            // fprintf(stderr, "Using Round for dflow -> cons\n");
            redist_dflow_con_ = new RedistRoundMPI(sizes_.dflow_start,
                                                   sizes_.dflow_size,
                                                   sizes_.con_start,
                                                   sizes_.con_size,
                                                   world_comm);
            break;
        }
        case DECAF_CONTIG_DECOMP:
        {
            // fprintf(stderr, "Using Count for dflow -> cons\n");
            redist_dflow_con_ = new RedistCountMPI(sizes_.dflow_start,
                                                   sizes_.dflow_size,
                                                   sizes_.con_start,
                                                   sizes_.con_size,
                                                   world_comm);
            break;
        }
        case DECAF_ZCURVE_DECOMP:
        {
            // fprintf(stderr, "Using ZCurve for dflow -> cons\n");
            redist_dflow_con_ = new RedistZCurveMPI(sizes_.dflow_start,
                                                    sizes_.dflow_size,
                                                    sizes_.con_start,
                                                    sizes_.con_size,
                                                    world_comm);
            break;
        }
        case DECAF_BLOCK_DECOMP:
        {
            redist_dflow_con_ = new RedistBlockMPI(sizes_.dflow_start,
                                                   sizes_.dflow_size,
                                                   sizes_.con_start,
                                                   sizes_.con_size,
                                                   world_comm);
            break;
        }
        case DECAF_PROC_DECOMP:
        {

            redist_dflow_con_ = new RedistProcMPI(sizes_.dflow_start,
                                                  sizes_.dflow_size,
                                                  sizes_.con_start,
                                                  sizes_.con_size,
                                                  world_comm);
            break;
        }
        default:
        {
            fprintf(stderr, "ERROR: policy %d unrecognized to select a redistribution component. "
                    "Using RedistCountMPI instead\n", prod_dflow_redist);
            redist_dflow_con_ = new RedistCountMPI(sizes_.dflow_start,
                                                   sizes_.dflow_size,
                                                   sizes_.con_start,
                                                   sizes_.con_size,
                                                   world_comm);
            break;
        }

        }
    }
    else
    {
        //fprintf(stderr, "No redistribution between dflow and cons needed.\n");
        redist_dflow_con_ = NULL;
    }

    // producer and consumer
    if (sizes_.dflow_size == 0 && (
        (world_rank_ >= sizes_.prod_start && world_rank_ < sizes_.prod_start + sizes_.prod_size) ||
        (world_rank_ >= sizes_.con_start && world_rank_ < sizes_.con_start + sizes_.con_size)))
    {
        no_link_ = true;

        switch(prod_dflow_redist)
        {
        case DECAF_ROUND_ROBIN_DECOMP:
        {
            // fprintf(stderr, "Using Round for dflow -> cons\n");
            redist_prod_con_ = new RedistRoundMPI(sizes_.prod_start,
                                                   sizes_.prod_size,
                                                   sizes_.con_start,
                                                   sizes_.con_size,
                                                   world_comm);
            break;
        }
        case DECAF_CONTIG_DECOMP:
        {
            // fprintf(stderr, "Using Count for dflow -> cons\n");
            redist_prod_con_ = new RedistCountMPI(sizes_.prod_start,
                                                  sizes_.prod_size,
                                                  sizes_.con_start,
                                                  sizes_.con_size,
                                                  world_comm);
            break;
        }
        case DECAF_ZCURVE_DECOMP:
        {
            // fprintf(stderr, "Using ZCurve for dflow -> cons\n");
            redist_prod_con_ = new RedistZCurveMPI(sizes_.prod_start,
                                                   sizes_.prod_size,
                                                   sizes_.con_start,
                                                   sizes_.con_size,
                                                   world_comm);
            break;
        }
        case DECAF_BLOCK_DECOMP:
        {
            redist_prod_con_ = new RedistBlockMPI(sizes_.prod_start,
                                                  sizes_.prod_size,
                                                  sizes_.con_start,
                                                  sizes_.con_size,
                                                  world_comm);
            break;
        }
        case DECAF_PROC_DECOMP:
        {

            redist_prod_con_ = new RedistProcMPI(sizes_.prod_start,
                                                 sizes_.prod_size,
                                                 sizes_.con_start,
                                                 sizes_.con_size,
                                                 world_comm);
            break;
        }
        default:
        {
            fprintf(stderr, "ERROR: policy %d unrecognized to select a redistribution component. "
                    "Using RedistCountMPI instead\n", prod_dflow_redist);
            redist_dflow_con_ = new RedistCountMPI(sizes_.prod_start,
                                                   sizes_.prod_size,
                                                   sizes_.con_start,
                                                   sizes_.con_size,
                                                   world_comm);
            break;
        }

        }
    }
    else
        redist_prod_con_ = NULL;

    if(stream_policy_ != DECAF_STREAM_NONE && !no_link_ && storage_types.size() > 0)
    {
        fprintf(stderr, "Stream mode activated.\n");
        use_stream_ = true;
    }

    // Buffering setup
    if(use_stream_)
    {
        switch(stream_policy_)
        {
            case DECAF_STREAM_SINGLE:
            {
                stream_ = new DatastreamSingleFeedback(world_comm_,
                                                   sizes_.prod_start,
                                                   sizes_.prod_size,
                                                   sizes_.dflow_start,
                                                   sizes_.dflow_size,
                                                   sizes_.con_start,
                                                   sizes_.con_size,
                                                   redist_prod_dflow_,
                                                   redist_dflow_con_,
                                                   framePolicy,
                                                   storage_types,
                                                   max_storage_sizes);
                break;
            }
            case DECAF_STREAM_DOUBLE:
            {
                stream_ = new DatastreamDoubleFeedback(world_comm_,
                                                   sizes_.prod_start,
                                                   sizes_.prod_size,
                                                   sizes_.dflow_start,
                                                   sizes_.dflow_size,
                                                   sizes_.con_start,
                                                   sizes_.con_size,
                                                   redist_prod_dflow_,
                                                   redist_dflow_con_,
                                                   framePolicy,
                                                   storage_types,
                                                   max_storage_sizes);
                break;
            }
            default:
            {
                fprintf(stderr, "ERROR: Unknown Stream policy. Disabling streaming.\n");
                use_stream_ = false;
                break;
            }
        }


    }

    err_ = DECAF_OK;
}

decaf::
Dataflow::~Dataflow()
{
    if (is_prod())
        delete prod_comm_;
    if (is_con())
        delete con_comm_;
    if (redist_dflow_con_)
        // TODO: Following crashes on my mac, for the hacc example, in rank 8 (dflow)
        // but not on other machines so far. Will see if it happens in other contexts,
        // or if it is just my outdated software stack -- TP
        delete redist_dflow_con_;
    if (redist_prod_dflow_)
        delete redist_prod_dflow_;

	if (redist_prod_con_)
        delete redist_prod_con_;

    if(stream_) delete stream_;
}

// puts data into the dataflow
// tags the data with the:
// - source type (producer/consumer or dataflow)
// - workflow link id corresponding to this dataflow
// - destination workflow node id or link id, depending on type of destination
// NB: source is *link* id while destination is *either node or link* id
// Returns true if ok, false if an ERROR occured
bool
decaf::
Dataflow::put(pConstructData data, TaskType role)
{
	pConstructData data_filtered; // container to be sent
	bool data_removed_by_period; // i.e.: Has some data field been removed due to the periodicity during the call of filterAndCheck?
	bool data_filtered_empty;

	data_filtered = filterPut(data, role, data_removed_by_period, data_filtered_empty);

	// If the filtered message does not contain user data, no need to send
	if(data_filtered_empty){
		iteration++;
		return false;
	}

	if(role==DECAF_LINK || !prod_link_overlap){
		iteration++;
	}

	data_filtered->removeData("src_type");
	data_filtered->removeData("link_id");
	data_filtered->removeData("dest_id");

    // encode type into message (producer/consumer or dataflow)
    shared_ptr<SimpleConstructData<TaskType> > value  =
        make_shared<SimpleConstructData<TaskType> >(role);
	data_filtered->appendData(string("src_type"), value,
                    DECAF_NOFLAG, DECAF_SYSTEM,
                    DECAF_SPLIT_KEEP_VALUE, DECAF_MERGE_FIRST_VALUE);


    // encode dataflow link id into message
    shared_ptr<SimpleConstructData<int> > value1  =
        make_shared<SimpleConstructData<int> >(wflow_dflow_id_);
	data_filtered->appendData(string("link_id"), value1,
                     DECAF_NOFLAG, DECAF_SYSTEM,
                     DECAF_SPLIT_KEEP_VALUE, DECAF_MERGE_FIRST_VALUE);


    if (role == DECAF_NODE)
    {
        if(no_link_)
        {
            // encode destination link id into message
            shared_ptr<SimpleConstructData<int> > value2  =
                make_shared<SimpleConstructData<int> >(wflow_con_id_);
			data_filtered->appendData(string("dest_id"), value2,
                             DECAF_NOFLAG, DECAF_SYSTEM,
                             DECAF_SPLIT_KEEP_VALUE, DECAF_MERGE_FIRST_VALUE);

            // send the message
			if(redist_prod_con_ == NULL){
                fprintf(stderr, "Dataflow::put() trying to access a null communicator\n");
				MPI_Abort(MPI_COMM_WORLD, 0);
			}
			if(data_removed_by_period){
				redist_prod_con_->clearBuffers(); // We clear the buffers before and after the sending
			}
			redist_prod_con_->process(data_filtered, DECAF_REDIST_SOURCE);
            redist_prod_con_->flush();
			if(data_removed_by_period){
				redist_prod_con_->clearBuffers();
			}
        }
        else
        {
            // encode destination link id into message
            shared_ptr<SimpleConstructData<int> > value2  =
                make_shared<SimpleConstructData<int> >(wflow_dflow_id_);
			data_filtered->appendData(string("dest_id"), value2,
                             DECAF_NOFLAG, DECAF_SYSTEM,
                             DECAF_SPLIT_KEEP_VALUE, DECAF_MERGE_FIRST_VALUE);

			if(redist_prod_dflow_ == NULL){
				fprintf(stderr, "Dataflow::put() trying to access a null communicator\n");
				MPI_Abort(MPI_COMM_WORLD, 0);
			}
			if(data_removed_by_period){
				redist_prod_dflow_->clearBuffers(); // We clear the buffers before and after the sending
			}

			/// send the message
            if(use_stream_)
			{
                stream_->processProd(data);
            }
            else
            {
				redist_prod_dflow_->process(data_filtered, DECAF_REDIST_SOURCE);
				redist_prod_dflow_->flush();

			}
			if(data_removed_by_period){
				redist_prod_dflow_->clearBuffers();
			}
        }
    }
    else if (role == DECAF_LINK)
    {
        // encode destination node id into message
        shared_ptr<SimpleConstructData<int> > value2  =
            make_shared<SimpleConstructData<int> >(wflow_con_id_);
		data_filtered->appendData(string("dest_id"), value2,
                         DECAF_NOFLAG, DECAF_SYSTEM,
                         DECAF_SPLIT_KEEP_VALUE, DECAF_MERGE_FIRST_VALUE);


        // send the message
		if(redist_dflow_con_ == NULL){
            fprintf(stderr, "Dataflow::put() trying to access a null communicator\n");
			MPI_Abort(MPI_COMM_WORLD, 0);
		}
		if(data_removed_by_period){
			redist_dflow_con_->clearBuffers(); // We clear the buffers before and after the sending
		}
		redist_dflow_con_->process(data_filtered, DECAF_REDIST_SOURCE);
        redist_dflow_con_->flush();
		if(data_removed_by_period){
			redist_dflow_con_->clearBuffers();
		}
	}

	// TODO remove this? It is data_filtered, the variale lives only in this function call
	data_filtered->removeData("src_type");
	data_filtered->removeData("link_id");
	data_filtered->removeData("dest_id");
	data_filtered->removeData("decaf_iteration");

	return true;
}


// gets data from the dataflow
// return true if ok, false if an ERROR occured
bool
decaf::
Dataflow::get(pConstructData data, TaskType role)
{
    if (role == DECAF_LINK)
    {
		if (redist_prod_dflow_ == NULL){
            fprintf(stderr, "Dataflow::get() trying to access a null communicator\n");
			MPI_Abort(MPI_COMM_WORLD, 0);
		}

        if(use_stream_)
        {
            stream_->processDflow(data);
        }
        else    // No buffer, blocking get
        {
            redist_prod_dflow_->process(data, DECAF_REDIST_DEST);
            redist_prod_dflow_->flush();
		}

    }
    else if (role == DECAF_NODE)
    {
        if(no_link_)
        {
			if (redist_prod_con_ == NULL){
                fprintf(stderr, "Dataflow::get() trying to access a null communicator\n");
				MPI_Abort(MPI_COMM_WORLD, 0);
			}
            redist_prod_con_->process(data, DECAF_REDIST_DEST);
            redist_prod_con_->flush();
        }
        else
        {
			if (redist_dflow_con_ == NULL){
                fprintf(stderr, "Dataflow::get() trying to access a null communicator\n");
				MPI_Abort(MPI_COMM_WORLD, 0);
			}
            redist_dflow_con_->process(data, DECAF_REDIST_DEST);
            redist_dflow_con_->flush();

            // Comnsumer side
            if(use_stream_)
                stream_->processCon(data);
        }


		if(test_quit(data)){
			setClose();
			return true;
		}
    }

	// Performs the filtering and checking of data if needed
	filterGet(data, role);

	return true;
}


decaf::pConstructData
decaf::
Dataflow::filterPut(pConstructData data, TaskType role, bool& data_changed, bool& filtered_empty){
	pConstructData data_filtered;
	data_changed = data->isEmpty();
	filtered_empty = false;

	if(data->hasData("decaf_quit")){
		set_quit(data_filtered);
		return data_filtered;
	}

	if(data->isEmpty() || check_level_ < CHECK_PY_AND_SOURCE){ // If there is no user data or if there is no need to perform the check/filtering
		return data;
	}

	if(role == DECAF_LINK){// iteration value is wrong if filtered data leads to an empty message at producer put, need to update it with the message from the producer node
		int i = get_iteration(data);
		if(i != -1){
			iteration = i;
		}
	}

	if(is_contract()){
	    for(ContractKey field : keys()){
			if( (iteration % field.period) == 0){ // This field is sent during this iteration
				if(!data->hasData(field.name)){// If the field is not present in the data the contract is not respected.
					fprintf(stderr, "ERROR: Contract not respected, the field \"%s\" is not present in the data model to send. Aborting.\n", field.name.c_str());
					MPI_Abort(MPI_COMM_WORLD, 0);
				}
				// Performing typechecking
				string typeName = data->getTypename(field.name);
				if(typeName.compare(field.type) != 0){ //The two types do not match
					fprintf(stderr, "ERROR: Contract not respected, sent type %s does not match the type %s of the field \"%s\". Aborting.\n", typeName.c_str(), field.type.c_str(), field.name.c_str());
					MPI_Abort(MPI_COMM_WORLD, 0);
				}
				data_filtered->appendData(data, field.name); // The field is present of correct type, send it
			}
			else{ // The field should not be sent in this iteration, we clear the buffers of the RedistComp
				//TODO need a clearBuffer on a specific field name
				data_changed = true;
			}
		}
	}
	else{
		data_filtered = data;
	}

	// If all fields are filtered during this iteration, the message is empty and should not be sent
	if(data_filtered->isEmpty()){
		filtered_empty = true;
		return data_filtered;
	}

	// In case there were system fields in the original message
	data_filtered->copySystemFields(data);

	// We attach the current iteration needed for the filtering at the get side or by a link for the put
	if(check_level_ >= CHECK_EVERYWHERE || (role==DECAF_NODE && !no_link_) ){
		set_iteration(data_filtered, iteration);
	}

	return data_filtered;
}

void
decaf::
Dataflow::filterGet(pConstructData data, TaskType role){
	if(data->isEmpty() || check_level_ < CHECK_EVERYWHERE){ // If there is no user data or if there is no need to perform the check/filtering
		return;
	}

	int it = get_iteration(data);

	if(is_contract()){
	    for(ContractKey field : keys()){
			if( (it % field.period) == 0){ // This field should be received during this iteration
				if(! data->hasData(field.name)){
					fprintf(stderr, "ERROR: Contract not respected, the field \"%s\" is not received in the data model. Aborting.\n", field.name.c_str());
					MPI_Abort(MPI_COMM_WORLD, 0);
				}
				string typeName = data->getTypename(field.name);
				if(typeName.compare(field.type) != 0){ //The two types do not match
					fprintf(stderr, "ERROR: Contract not respected, received type %s does not match the type %s of the field \"%s\". Aborting.\n", typeName.c_str(), field.type.c_str(), field.name.c_str());
					MPI_Abort(MPI_COMM_WORLD, 0);
				}
			}
			else{// The field is not supposed to be received in this iteration, remove it
				data->removeData(field.name);
			}
		}
	}
}


// cleanup
void
decaf::
Dataflow::shutdown()
{
    if (redist_prod_dflow_)
        redist_prod_dflow_->shutdown();
    if (redist_dflow_con_)
        redist_dflow_con_->shutdown();
    if(redist_prod_con_)
        redist_prod_con_->shutdown();
}


void
decaf::
Dataflow::clearBuffers(TaskType role)
{
    if (role == DECAF_LINK)
        redist_dflow_con_->clearBuffers();
    else if (role == DECAF_NODE && no_link_)
        redist_prod_con_->clearBuffers();
    else if (role == DECAF_NODE && !no_link_)
        redist_prod_dflow_->clearBuffers();
}

#endif
