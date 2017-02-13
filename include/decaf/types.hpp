﻿//---------------------------------------------------------------------------
//
// decaf typedefs, enums
//
// Tom Peterka
// Argonne National Laboratory
// 9700 S. Cass Ave.
// Argonne, IL 60439
// tpeterka@mcs.anl.gov
//
//--------------------------------------------------------------------------

#ifndef DECAF_TYPES_HPP
#define DECAF_TYPES_HPP

#include "decaf.hpp"

#include <stdio.h>
#include <vector>
#include <queue>
#include <string>
#include <iostream>

using namespace std;

enum Decomposition
{
	DECAF_ROUND_ROBIN_DECOMP,
	DECAF_CONTIG_DECOMP,
	DECAF_ZCURVE_DECOMP,
	DECAF_BLOCK_DECOMP,
	DECAF_PROC_DECOMP,
	DECAF_NUM_DECOMPS,
};

// workflow entity types
typedef unsigned char TaskType;
#define DECAF_NONE      0x00
#define DECAF_NODE      0x01
#define DECAF_LINK      0x02

// communicator types
typedef unsigned char CommTypeDecaf;
#define DECAF_OTHER_COMM      0x00
#define DECAF_PRODUCER_COMM   0x01
#define DECAF_DATAFLOW_COMM   0x02
#define DECAF_CONSUMER_COMM   0x04
#define DECAF_PROD_DFLOW_COMM 0x08
#define DECAF_DFLOW_CON_COMM  0x10

enum DecafError
{
	DECAF_OK,
	DECAF_COMM_SIZES_ERR,
	DECAF_NUM_ERRS,
};

struct DecafSizes
{
	int prod_size;         // size (number of processes) of producer communicator
	int dflow_size;        // size (number of processes) of dataflow communicator
	int con_size;          // size (number of processes) of consumer communicator
	int prod_start;        // starting world process rank of producer communicator
	int dflow_start;       // starting world process rank of dataflow communicator
	int con_start;         // starting world process rank of consumer communicator
	int con_nsteps;        // number of consumer timesteps
};

struct ContractKey
{
	std::string name;		// Name of the data field
	std::string type;		// Type of the data field
	int period;				// The data field is sent every "period" iteration
};

enum Check_level // Level of checking/filtering for the contracts, types, periodicity
{
	CHECK_NONE,				// NO filtering or typechecking
	CHECK_PYTHON,			// Only at python script
	CHECK_PY_AND_SOURCE,	// Python script AND in Dataflow->put
	CHECK_EVERYWHERE,		// PYthon script, Dataflow->put AND Dataflow->get
};


void
all_err(int err_code)
{
	switch (err_code) {
	case DECAF_OK :
		break;
	case DECAF_COMM_SIZES_ERR :
		fprintf(stderr, "Decaf error: Group sizes of producer, consumer, and dataflow exceed total "
		        "size of world communicator\n");
		break;
	default:
		break;
	}
}

Decomposition stringToDecomposition(std::string name)
{
	if(name.compare(std::string("round")) == 0)
		return DECAF_ROUND_ROBIN_DECOMP;
	else if (name.compare(std::string("count")) == 0)
		return DECAF_CONTIG_DECOMP;
	else if (name.compare(std::string("zcurve")) == 0)
		return DECAF_ZCURVE_DECOMP;
	else if (name.compare(std::string("block")) == 0)
		return DECAF_BLOCK_DECOMP;
	else if (name.compare(std::string("proc")) == 0)
		return DECAF_PROC_DECOMP;
	else if (name.compare(std::string("")) == 0)
		return DECAF_CONTIG_DECOMP;
	else
	{
		std::cerr<<"ERROR : unknown Decomposition name : "<<name<<". Using count instead."<<std::endl;
		return DECAF_CONTIG_DECOMP;
	}
}

Check_level stringToCheckLevel(string check){
	if(!check.compare("PYTHON"))
		return CHECK_PYTHON;
	if(!check.compare("PY_AND_SOURCE"))
		return CHECK_PY_AND_SOURCE;
	if(!check.compare("EVERYWHERE"))
		return CHECK_EVERYWHERE;

	return CHECK_NONE;
}

#endif
