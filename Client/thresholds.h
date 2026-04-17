#pragma once

/**
 * Safe Operating Limit Thresholds
 *
 * These constants define the safety thresholds for aircraft telemetry monitoring.
 * Used by the ground control client to warn operators when aircraft parameters
 * exceed safe operating limits.
 */

// MISRA-CPP-2008-2-13-4: Literal suffixes must be uppercase
// MISRA-CPP-2008-7-3-1: Wrap in a namespace to avoid polluting the global namespace
namespace Thresholds
{
    constexpr float FUEL_LEVEL_MIN_PERCENT   = 20.0F;
    constexpr float ENGINE_TEMP_MAX_CELSIUS  = 950.0F;
    constexpr float ALTITUDE_MAX_FEET        = 45000.0F;
    constexpr float AIRSPEED_MAX_KNOTS       = 500.0F;
}
