#include "si5351.h"

#include <limits.h>

#include "pico/time.h"

volatile uint32_t si5351_debug_pll_n = 0;
volatile uint32_t si5351_debug_ms_m = 0;
volatile uint32_t si5351_debug_actual_hz = 0;
volatile uint8_t si5351_debug_pll_bytes[8] = {0};
volatile uint8_t si5351_debug_ms_bytes[8] = {0};
volatile uint8_t si5351_debug_clk_ctrl = 0;
volatile uint8_t si5351_debug_phase_offset = 0;
volatile uint8_t si5351_debug_clock = 0;
volatile uint8_t si5351_debug_quadrature_partner = SI5351_CLK_NONE;
volatile bool si5351_debug_pair_mode = false;
volatile bool si5351_debug_use_pllb = false;
volatile bool si5351_debug_programmed = false;

#define SI5351_N_MIN 25u
#define SI5351_N_MAX 36u
#define SI5351_M_MIN 8u
#define SI5351_M_MAX 127u

static const uint8_t k_clk_ctrl_regs[3] = {
	SI5351_REG_CLK0_CTRL,
	SI5351_REG_CLK1_CTRL,
	SI5351_REG_CLK2_CTRL,
};

static const uint8_t k_ms_base_regs[3] = {
	SI5351_REG_MS0_BASE,
	SI5351_REG_MS1_BASE,
	SI5351_REG_MS2_BASE,
};

static const uint8_t k_phase_regs[3] = {
	SI5351_REG_CLK0_PHOFF,
	SI5351_REG_CLK1_PHOFF,
	SI5351_REG_CLK2_PHOFF,
};

static bool i2c_write_reg_checked(i2c_inst_t *i2c, uint8_t reg, uint8_t val) {
	uint8_t tx[2] = {reg, val};
	return i2c_write_blocking(i2c, SI5351_ADDR, tx, 2, false) == 2;
}

static bool i2c_read_reg_checked(i2c_inst_t *i2c, uint8_t reg, uint8_t *val) {
	if (i2c_write_blocking(i2c, SI5351_ADDR, &reg, 1, true) != 1) {
		return false;
	}
	return i2c_read_blocking(i2c, SI5351_ADDR, val, 1, false) == 1;
}

static bool si5351_write_block(i2c_inst_t *i2c, uint8_t base_reg, const uint8_t *data, size_t len) {
	uint8_t tx[9];
	if (len == 0 || len > 8) {
		return false;
	}
	tx[0] = base_reg;
	for (size_t i = 0; i < len; ++i) {
		tx[i + 1] = data[i];
	}
	return i2c_write_blocking(i2c, SI5351_ADDR, tx, len + 1, false) == (int)(len + 1);
}

static void si5351_pack_integer_params(uint32_t a, uint8_t out[8]) {
	const uint32_t p1 = 128u * a - 512u;
	const uint32_t p2 = 0u;
	const uint32_t p3 = 1u;

	out[0] = (uint8_t)((p3 >> 8) & 0xFF);
	out[1] = (uint8_t)(p3 & 0xFF);
	out[2] = (uint8_t)(((p3 >> 12) & 0xF0) | ((p1 >> 16) & 0x03));
	out[3] = (uint8_t)((p1 >> 8) & 0xFF);
	out[4] = (uint8_t)(p1 & 0xFF);
	out[5] = (uint8_t)((p2 >> 16) & 0x0F);
	out[6] = (uint8_t)((p2 >> 8) & 0xFF);
	out[7] = (uint8_t)(p2 & 0xFF);
}

static bool si5351_program_pll(i2c_inst_t *i2c, uint8_t pll_base, uint32_t n) {
	uint8_t data[8];
	si5351_pack_integer_params(n, data);
	for (size_t i = 0; i < sizeof(data); ++i) {
		si5351_debug_pll_bytes[i] = data[i];
	}
	return si5351_write_block(i2c, pll_base, data, 8);
}

static bool si5351_program_ms(i2c_inst_t *i2c, si5351_clock_t clock, uint32_t m) {
	uint8_t data[8];
	si5351_pack_integer_params(m, data);
	for (size_t i = 0; i < sizeof(data); ++i) {
		si5351_debug_ms_bytes[i] = data[i];
	}
	return si5351_write_block(i2c, k_ms_base_regs[(uint8_t)clock], data, 8);
}

static uint8_t make_clk_ctrl_reg(bool use_pllb) {
	// Keep output enabled, integer mode, 8mA drive; select PLLA/B source.
	return (uint8_t)(0x4Fu | (use_pllb ? 0x20u : 0x00u));
}

static bool is_valid_clock(si5351_clock_t clock) {
	return clock == SI5351_CLK0 || clock == SI5351_CLK1 || clock == SI5351_CLK2;
}

static bool find_integer_solution(
	uint32_t target_hz,
	si5351_integer_mode_t mode,
	uint32_t *best_n,
	uint32_t *best_m,
	uint32_t *best_actual_hz
) {
	bool found = false;
	uint32_t n_best = 0;
	uint32_t m_best = 0;
	uint32_t actual_best = 0;
	uint32_t best_err_hz = UINT_MAX;

	if (target_hz == 0) {
		return false;
	}

	for (uint32_t n = SI5351_N_MIN; n <= SI5351_N_MAX; ++n) {
		uint32_t vco = SI5351_XTAL_HZ * n;
		if (vco < SI5351_PLL_VCO_MIN_HZ || vco > SI5351_PLL_VCO_MAX_HZ) {
			continue;
		}

		for (uint32_t m = SI5351_M_MIN; m <= SI5351_M_MAX; m += 2) {
			uint32_t actual_hz = (vco + (m / 2u)) / m;
			uint32_t err_hz = (actual_hz > target_hz) ? (actual_hz - target_hz) : (target_hz - actual_hz);
			bool exact = (vco % m == 0u) && (actual_hz == target_hz);

			if (mode == SI5351_INTEGER_EXACT) {
				if (exact) {
					*best_n = n;
					*best_m = m;
					*best_actual_hz = actual_hz;
					return true;
				}
			} else if (!found || err_hz < best_err_hz) {
				found = true;
				best_err_hz = err_hz;
				n_best = n;
				m_best = m;
				actual_best = actual_hz;
			}
		}
	}

	if (mode == SI5351_INTEGER_EXACT || !found) {
		return false;
	}

	*best_n = n_best;
	*best_m = m_best;
	*best_actual_hz = actual_best;
	return true;
}

static bool update_output_enable(i2c_inst_t *i2c, uint8_t clear_mask_bits) {
	uint8_t en;
	if (!i2c_read_reg_checked(i2c, SI5351_REG_OUTPUT_ENABLE_CTRL, &en)) {
		return false;
	}
	en &= (uint8_t)(~clear_mask_bits);
	return i2c_write_reg_checked(i2c, SI5351_REG_OUTPUT_ENABLE_CTRL, en);
}

static bool reset_selected_pll(i2c_inst_t *i2c, bool use_pllb) {
	uint8_t reset_val = use_pllb ? 0x80u : 0x20u;
	return i2c_write_reg_checked(i2c, SI5351_REG_PLL_RESET, reset_val);
}

bool si5351_init(i2c_inst_t *i2c) {
	const uint32_t timeout_ms = 1000;
	absolute_time_t deadline = make_timeout_time_ms(timeout_ms);

	if (!i2c_write_reg_checked(i2c, SI5351_REG_OUTPUT_ENABLE_CTRL, 0xFFu)) {
		return false;
	}
	if (!i2c_write_reg_checked(i2c, SI5351_REG_XTAL_CL, SI5351_XTAL_LOAD_10PF)) {
		return false;
	}

	while (!time_reached(deadline)) {
		uint8_t status;
		if (!i2c_read_reg_checked(i2c, SI5351_REG_DEVICE_STATUS, &status)) {
			return false;
		}
		if ((status & 0x80u) == 0u) {
			return true;
		}
		sleep_ms(10);
	}

	return false;
}

void si5351_write_reg(i2c_inst_t *i2c, uint8_t reg, uint8_t val) {
	(void)i2c_write_reg_checked(i2c, reg, val);
}

uint8_t si5351_read_reg(i2c_inst_t *i2c, uint8_t reg) {
	uint8_t val = 0;
	(void)i2c_read_reg_checked(i2c, reg, &val);
	return val;
}

uint32_t si5351_set_frequency(
	i2c_inst_t *i2c,
	si5351_clock_t clock,
	uint32_t target_hz,
	si5351_integer_mode_t mode,
	si5351_clock_t quadrature_partner
) {
	uint32_t n = 0;
	uint32_t m = 0;
	uint32_t actual_hz = 0;
	bool pair_mode = quadrature_partner != SI5351_CLK_NONE;
	bool use_pllb = (clock == SI5351_CLK2);
	uint8_t output_clear_mask;

	if (!is_valid_clock(clock)) {
		return 0;
	}
	if (pair_mode) {
		if (!is_valid_clock(quadrature_partner) || quadrature_partner == clock) {
			return 0;
		}
		// Quadrature requires a shared PLL and same divider; use PLLA.
		use_pllb = false;
	}

	if (!find_integer_solution(target_hz, mode, &n, &m, &actual_hz)) {
		return 0;
	}

	si5351_debug_pll_n = n;
	si5351_debug_ms_m = m;
	si5351_debug_actual_hz = actual_hz;
	si5351_debug_clock = (uint8_t)clock;
	si5351_debug_quadrature_partner = (uint8_t)quadrature_partner;
	si5351_debug_pair_mode = pair_mode;
	si5351_debug_use_pllb = use_pllb;
	si5351_debug_programmed = false;

	if (!si5351_program_pll(i2c, use_pllb ? SI5351_REG_PLLB_BASE : SI5351_REG_PLLA_BASE, n)) {
		return 0;
	}

	if (!si5351_program_ms(i2c, clock, m)) {
		return 0;
	}
	si5351_debug_clk_ctrl = make_clk_ctrl_reg(use_pllb);
	if (!i2c_write_reg_checked(i2c, k_clk_ctrl_regs[(uint8_t)clock], si5351_debug_clk_ctrl)) {
		return 0;
	}
	si5351_debug_phase_offset = 0x00u;
	if (!i2c_write_reg_checked(i2c, k_phase_regs[(uint8_t)clock], si5351_debug_phase_offset)) {
		return 0;
	}

	output_clear_mask = (uint8_t)(1u << (uint8_t)clock);

	if (pair_mode) {
		if (!si5351_program_ms(i2c, quadrature_partner, m)) {
			return 0;
		}
		si5351_debug_clk_ctrl = make_clk_ctrl_reg(false);
		if (!i2c_write_reg_checked(i2c, k_clk_ctrl_regs[(uint8_t)quadrature_partner], si5351_debug_clk_ctrl)) {
			return 0;
		}
		si5351_debug_phase_offset = (uint8_t)m;
		if (!i2c_write_reg_checked(i2c, k_phase_regs[(uint8_t)quadrature_partner], si5351_debug_phase_offset)) {
			return 0;
		}
		output_clear_mask |= (uint8_t)(1u << (uint8_t)quadrature_partner);
	}

	if (!reset_selected_pll(i2c, use_pllb)) {
		return 0;
	}
	if (!update_output_enable(i2c, output_clear_mask)) {
		return 0;
	}

	si5351_debug_programmed = true;
	return actual_hz;
}