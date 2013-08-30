//
// Copyright (C) 2013 University of Kaiserslautern
// Microelectronic Systems Design Research Group
//
// Christian Brugger (brugger@eit.uni-kl.de)
// 19. August 2013
//
// Using: Xilinx Vivado HLS 2013.2
//

#include <hls_stream.h>

#include <stdint.h>

#ifndef __MERSENNE_TWISTER_HLS_HPP__
#define __MERSENNE_TWISTER_HLS_HPP__

// how many random numbers are generated for simulation
#define _SIM_RN_CNT_ 10000

void mersenne_twister(
		uint32_t seeds[624],
		hls::stream<uint32_t> &random_numbers);


#endif
