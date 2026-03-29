#pragma once

/**
 * Safe Operating Limit Thresholds
 *
 * These constants define the safety thresholds for aircraft telemetry monitoring.
 * Used by the ground control client to warn operators when aircraft parameters
 * exceed safe operating limits.
 */

constexpr float FUEL_LEVEL_MIN_PERCENT = 20.0f;
constexpr float ENGINE_TEMP_MAX_CELSIUS = 950.0f;
constexpr float ALTITUDE_MAX_FEET = 45000.0f;
constexpr float AIRSPEED_MAX_KNOTS = 500.0f;
