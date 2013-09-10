//
// Copyright (C) 2013 University of Kaiserslautern
// Microelectronic Systems Design Research Group
//
// Christian Brugger (brugger@eit.uni-kl.de)
// 20. August 2013
//

#include "iodev.hpp"
#include "json_helper.hpp"
#include "heston_both_acc.hpp"

#include "json/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#include <thread>
#include <chrono>
#include <iostream>
#include <cmath>
#include <fstream>


struct HestonParamsHWML {
	// call option
	float log_spot_price;
	float reversion_rate_TIMES_step_size_fine;
	float reversion_rate_TIMES_step_size_coarse;
	float long_term_avg_vola;
	float vol_of_vol_TIMES_sqrt_step_size_fine;
	float vol_of_vol_TIMES_sqrt_step_size_coarse;
	float double_riskless_rate; // = 2 * riskless_rate
	float vola_0;
	float correlation;
	// both knockout
	float log_lower_barrier_value;
	float log_upper_barrier_value;
	// simulation params
	uint32_t step_cnt_coarse;
	uint32_t ml_constant; // only 5 bit
	uint32_t do_multilevel; // boolean
	float half_step_size_fine; // = step_size_fine / 2
	float half_step_size_coarse; // = step_size_coarse / 2
	float sqrt_step_size_fine; // = sqrt(step_size_fine)
	float sqrt_step_size_coarse; // = sqrt(step_size_coarse)
	// = BARRIER_HIT_CORRECTION * sqrt_step_size
	float barrier_correction_factor_fine;
	float barrier_correction_factor_coarse;
	uint32_t path_cnt;
};


float heston_ml_hw_kernel(Json::Value bitstream, HestonParamsML ml_params, 
		uint32_t step_cnt_fine, uint32_t path_cnt, bool do_multilevel) {
	HestonParamsML p = ml_params;
	if (p.ml_constant == 0) {
		std::cerr << "ERROR: ml_constant == 0" << std::endl;
		exit(1);
	}
	if (step_cnt_fine % p.ml_constant != 0) {
		std::cerr << "ERROR: step_cnt_fine % ml_constant != 0" << std::endl;
		exit(1);
	}
	// define heston hw parameters
	// continuity correction, see Broadie, Glasserman, Kou (1997)
	float barrier_hit_correction = 0.5826;
	uint32_t step_cnt_coarse = step_cnt_fine / p.ml_constant;
	float step_size_fine = p.time_to_maturity / step_cnt_fine;
	float step_size_coarse = p.time_to_maturity / step_cnt_coarse;
	float sqrt_step_size_fine = std::sqrt(step_size_fine);
	float sqrt_step_size_coarse = std::sqrt(step_size_coarse);
	HestonParamsHWML params_hw = {
		(float) std::log(p.spot_price),
		(float) p.reversion_rate * step_size_fine,
		(float) p.reversion_rate * step_size_coarse,
		(float) p.long_term_avg_vola,
		(float) p.vol_of_vol * sqrt_step_size_fine,
		(float) p.vol_of_vol * sqrt_step_size_coarse,
		(float) (2 * p.riskless_rate),
		(float) p.vola_0,
		(float) p.correlation,
		(float) std::log(p.lower_barrier_value),
		(float) std::log(p.upper_barrier_value),
		step_cnt_coarse,
		p.ml_constant,
		do_multilevel,
		step_size_fine / 2,
		step_size_coarse / 2,
		sqrt_step_size_fine,
		sqrt_step_size_coarse,
		(float) barrier_hit_correction * sqrt_step_size_fine,
		(float) barrier_hit_correction * sqrt_step_size_coarse,
		0};

	// find accelerators
	std::vector<Json::Value> accelerators;
	std::vector<Json::Value> fifos;
	for (auto component: bitstream)
		if (component["__class__"] == "heston_ml") {
			accelerators.push_back(component);
			fifos.push_back(component["axi_fifo"]);
		}

	// start accelerators
	uint32_t acc_path_cnt = path_cnt / accelerators.size();
	int path_cnt_remainder = path_cnt % accelerators.size();
	for (auto acc: accelerators) {
		params_hw.path_cnt = acc_path_cnt + (path_cnt_remainder-- > 0 ? 1 : 0);
		start_heston_accelerator(acc, &params_hw, sizeof(params_hw));
	}

	// setup read iterator
	read_fifos_iterator<float> read_it(fifos, path_cnt + 1);

	// calculate result
	double result = 0;
	unsigned index;
	float price;
	read_it.next(price, index);
	while (read_it.next(price, index)) {
		std::cout << index << ": " << price << std::endl;
		result += std::max(0.f, std::exp(price) - (float) p.strike_price);
	}
	result *= std::exp(-p.riskless_rate * p.time_to_maturity) / path_cnt;
	return result;
}


float heston_ml_hw(Json::Value bitstream, HestonParamsML ml_params) {
	return heston_ml_hw_kernel(bitstream, ml_params, 512, 100, false);
}

