/**
 * PWM/Device.cpp
 *
 * Copyright 2022 mikee47 <mike@sillyhouse.net>
 *
 * This file is part of the IOControl Library
 *
 * This library is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, version 3 or later.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this library.
 * If not, see <https://www.gnu.org/licenses/>.
 *
 ****/

#include <IO/PWM/Device.h>
#include <IO/PWM/Request.h>
#include <Data/Range.h>

#ifdef ARCH_ESP32
#include <driver/ledc.h>
#endif

namespace IO
{
namespace PWM
{
DEFINE_FSTR(ATTR_CHANNEL, "channel")
DEFINE_FSTR(ATTR_FREQ, "freq")
DEFINE_FSTR(ATTR_PIN, "pin")
DEFINE_FSTR(ATTR_INVERT, "invert")

const Device::Factory Device::factory;

#ifdef ARCH_ESP32
// TODO: Move some/all of these into config
constexpr ledc_mode_t mode{LEDC_LOW_SPEED_MODE};
constexpr ledc_timer_t timerNum{LEDC_TIMER_0};
constexpr ledc_channel_t channel{LEDC_CHANNEL_0};
#else
#define SOC_LEDC_CHANNEL_NUM 8
#endif

ErrorCode Device::init(const Config& config)
{
	auto err = IO::Device::init(config.base);
	if(err) {
		return err;
	}

	if(config.channel < 0 || config.channel >= SOC_LEDC_CHANNEL_NUM || config.pin == 255) {
		return Error::bad_param;
	}

#ifdef ARCH_ESP32

	channel = config.channel;

	ledc_timer_config_t ledc_timer = {
		.speed_mode = mode,
		.duty_resolution = LEDC_TIMER_8_BIT,
		.timer_num = timerNum,
		.freq_hz = config.freq,
		.clk_cfg = LEDC_AUTO_CLK,
	};
	if(ledc_timer_config(&ledc_timer) != ESP_OK) {
		return Error::bad_config;
	}

	ledc_channel_config_t ledc_channel = {
		.gpio_num = config.pin,
		.speed_mode = mode,
		.channel = ledc_channel_t(config.channel),
		.intr_type = LEDC_INTR_DISABLE,
		.timer_sel = timerNum,
		.duty = 0,
		.hpoint = 0,
	};
	if(ledc_channel_config(&ledc_channel) != ESP_OK) {
		return Error::bad_config;
	}

	// TODO: SDK version 4.4 adds support for pin inversion to LEDC API
	if(config.invert) {
		gpio_matrix_out(gpio_num_t(config.pin), LEDC_LS_SIG_OUT0_IDX, true, false);
	}
#endif

	return Error::success;
}

ErrorCode Device::init(JsonObjectConst config)
{
	Config cfg{};
	parseJson(config, cfg);
	return init(cfg);
}

void Device::parseJson(JsonObjectConst json, Config& cfg)
{
	IO::Device::parseJson(json, cfg.base);

	cfg.freq = json[ATTR_FREQ];
	cfg.pin = 255;
	Json::getValue(json[ATTR_PIN], cfg.pin);
	cfg.invert = json[ATTR_INVERT];
	cfg.channel = -1;
	Json::getValue(json[ATTR_CHANNEL], cfg.channel);
}

IO::Request* Device::createRequest()
{
	return new Request(*this);
}

ErrorCode Device::execute(Request& request)
{
#ifdef ARCH_ESP32
	auto chan = ledc_channel_t(channel);
	int value = request.getValue();

	switch(request.getCommand()) {
	case Command::query:
		value = ledc_get_duty(mode, chan);
		request.setValue(value);
		return Error::success;

	case Command::adjust:
		value += ledc_get_duty(mode, chan);
		break;

	case Command::set:
		break;

	default:
		return Error::bad_command;
	}

	value = TRange(0, 511).clip(value);

	ledc_set_duty(mode, chan, value);
	ledc_update_duty(mode, chan);
#else
	int value = request.getValue();

	switch(request.getCommand()) {
	case Command::query:
		request.setValue(this->value);
		return Error::success;

	case Command::adjust:
		value += this->value;
		break;

	case Command::set:
		break;

	default:
		return Error::bad_command;
	}

	this->value = TRange(0, 511).clip(value);
#endif

	return Error::success;
}

} // namespace PWM
} // namespace IO
