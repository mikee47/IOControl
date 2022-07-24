#include "Fan.h"
#include <hardware/pwm.h>
#include "hardware/clocks.h"

#define PWM_REF_CLOCK 125e6
#define PWM_CLOCKDIV 1
#define PWM_FREQ 20e3
#define PWM_CLOCK (PWM_REF_CLOCK / PWM_CLOCKDIV)
#define PWM_COUNT_TOP (round(PWM_CLOCK / PWM_FREQ) + 1)

void Fan::setLevel(uint8_t percent)
{
	level = PWM_COUNT_TOP * percent / 100;
	pwm_set_gpio_level(pwmPin, level);
}

uint8_t Fan::getLevel() const
{
	return 100 * level / PWM_COUNT_TOP;
}

uint16_t Fan::readRpm()
{
	auto slice_num = pwm_gpio_to_slice_num(tachoPin);
	uint16_t pulseCount = pwm_get_counter(slice_num);
	auto elapsedTicks = us.elapsedTicks();
	pwm_set_counter(slice_num, 0);
	us.start();
	if(pulseCount == 0) {
		rpm = 0;
	} else {
		rpm = (60 * 1000000 / 2) / us.ticksToTime(elapsedTicks / pulseCount);
	}
	return rpm;
}

bool Fan::configureInput()
{
	if(pwm_gpio_to_channel(tachoPin) != PWM_CHAN_B) {
		debug_e("Input must be PWM channel B");
		return false;
	}

	auto slice_num = pwm_gpio_to_slice_num(tachoPin);
	pwm_config cfg = pwm_get_default_config();
	pwm_config_set_clkdiv_mode(&cfg, PWM_DIV_B_RISING);
	pwm_config_set_clkdiv(&cfg, 1);
	pwm_init(slice_num, &cfg, true);
	gpio_set_function(tachoPin, GPIO_FUNC_PWM);
	gpio_set_dir(tachoPin, GPIO_IN);
	gpio_set_pulls(tachoPin, true, false);

	pwm_set_counter(slice_num, 0);
	us.start();

	return true;
}

bool Fan::configureOutput()
{
	auto slice_num = pwm_gpio_to_slice_num(pwmPin);
	pwm_config cfg = pwm_get_default_config();
	pwm_config_set_clkdiv_mode(&cfg, PWM_DIV_FREE_RUNNING);
	pwm_config_set_clkdiv(&cfg, PWM_CLOCKDIV);
	pwm_config_set_wrap(&cfg, PWM_COUNT_TOP);
	pwm_init(slice_num, &cfg, true);
	gpio_set_function(pwmPin, GPIO_FUNC_PWM);
	gpio_set_dir(pwmPin, GPIO_OUT);
	pwm_set_gpio_level(pwmPin, 0);

	return true;
}
