

#pragma once

extern "C" {
	void randomx_program_aarch64(void* reg, void* mem, void* scratchpad, uint64_t iterations);
	void randomx_program_aarch64_main_loop();
	void randomx_program_aarch64_vm_instructions();
	void randomx_program_aarch64_imul_rcp_literals_end();
	void randomx_program_aarch64_vm_instructions_end();
	void randomx_program_aarch64_cacheline_align_mask1();
	void randomx_program_aarch64_cacheline_align_mask2();
	void randomx_program_aarch64_update_spMix1();
	void randomx_program_aarch64_vm_instructions_end_light();
	void randomx_program_aarch64_light_cacheline_align_mask();
	void randomx_program_aarch64_light_dataset_offset();
	void randomx_init_dataset_aarch64();
	void randomx_init_dataset_aarch64_end();
	void randomx_calc_dataset_item_aarch64();
	void randomx_calc_dataset_item_aarch64_prefetch();
	void randomx_calc_dataset_item_aarch64_mix();
	void randomx_calc_dataset_item_aarch64_store_result();
	void randomx_calc_dataset_item_aarch64_end();
}
