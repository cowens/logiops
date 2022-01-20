/*
 * Copyright 2019-2020 PixlOne
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <cmath>
#include "AxisGesture.h"
#include "../../Device.h"
#include "../../InputDevice.h"
#include "../../util/log.h"

using namespace logid::actions;

AxisGesture::AxisGesture(Device *device, config::AxisGesture& config) :
    Gesture (device), _multiplier (1), _config (config)
{
    if(std::holds_alternative<uint>(_config.axis)) {
        _input_axis = std::get<uint>(_config.axis);
    } else {
        const auto& axis = std::get<std::string>(_config.axis);
        try {
            _axis = _device->virtualInput()->toAxisCode(axis);
            _device->virtualInput()->registerAxis(_axis);
        } catch(InputDevice::InvalidEventCode& e) {
            logPrintf(WARN, "Invalid axis %s.");
            throw InvalidGesture();
        }
    }
    _device->virtualInput()->registerAxis(_input_axis);
}

void AxisGesture::press(bool init_threshold)
{
    _axis = init_threshold ?
            _config.threshold.value_or(defaults::gesture_threshold) : 0;
    _axis_remainder = 0;
    _hires_remainder = 0;
}

void AxisGesture::release(bool primary)
{
    // Do nothing
    (void)primary; // Suppress unused warning
}

void AxisGesture::move(int16_t axis)
{
    const auto threshold = _config.threshold.value_or(
            defaults::gesture_threshold);
    int16_t new_axis = _axis+axis;
    int low_res_axis = InputDevice::getLowResAxis(axis);
    int hires_remainder = _hires_remainder;

    if(new_axis > threshold) {
        double move = axis;
        if(_axis < threshold)
            move = new_axis - threshold;
        bool negative_multiplier = _config.axis_multiplier.value_or(1) < 0;
        if(negative_multiplier)
            move *= -_config.axis_multiplier.value_or(1);
        else
            move *= _config.axis_multiplier.value_or(1);
        // Handle hi-res multiplier
        move *= _multiplier;

        double move_floor = floor(move);
        _axis_remainder = move - move_floor;
        if(_axis_remainder >= 1) {
            double int_remainder = floor(_axis_remainder);
            move_floor += int_remainder;
            _axis_remainder -= int_remainder;
        }

        if(negative_multiplier)
            move_floor = -move_floor;

        if(low_res_axis != -1) {
            int lowres_movement = 0, hires_movement = move_floor;
            _device->virtualInput()->moveAxis(_input_axis, hires_movement);
            hires_remainder += hires_movement;
            if(abs(hires_remainder) >= 60) {
                lowres_movement = hires_remainder/120;
                if(lowres_movement == 0)
                    lowres_movement = hires_remainder > 0 ? 1 : -1;
                hires_remainder -= lowres_movement*120;
                _device->virtualInput()->moveAxis(low_res_axis, lowres_movement);
            }

            _hires_remainder = hires_remainder;
        } else {
            _device->virtualInput()->moveAxis(_input_axis, move_floor);
        }
    }
    _axis = new_axis;
}

bool AxisGesture::metThreshold() const
{
    return _axis >= _config.threshold.value_or(defaults::gesture_threshold);
}

bool AxisGesture::wheelCompatibility() const
{
    return true;
}

void AxisGesture::setHiresMultiplier(double multiplier)
{
    if(InputDevice::getLowResAxis(_axis) != -1) {
        _multiplier = multiplier;
    }
}
