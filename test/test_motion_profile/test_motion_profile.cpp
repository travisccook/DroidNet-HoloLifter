#include <unity.h>
#include "MotionProfile.h"
#include "motion_config.h"

using namespace motionprofile;

void setUp() {}
void tearDown() {}

// The firmware's constants; cruise 255 unless a test says otherwise
static const uint8_t CRUISE = 255, FLOOR = MOTOR_APPROACH_SPEED, SAFE = MOTOR_HOMING_SPEED;
static const int32_t RAMP = DECEL_RAMP_COUNTS, DEADBAND = POSITION_DEADBAND;

// floor + (255 - floor) * d / ramp: the documented ramp
static uint8_t ramp(int32_t d) { return (uint8_t)(FLOOR + (int32_t)(255 - FLOOR) * d / RAMP); }

void test_full_cruise_outside_ramp() {
  TEST_ASSERT_EQUAL_UINT8(255, approachSpeed(10 * RAMP, CRUISE, FLOOR, RAMP));
  TEST_ASSERT_EQUAL_UINT8(255, approachSpeed(RAMP, CRUISE, FLOOR, RAMP));
}

void test_ramps_down_linearly_inside_ramp() {
  // halfway through the ramp: halfway between floor and cruise
  TEST_ASSERT_EQUAL_UINT8(ramp(RAMP / 2), approachSpeed(RAMP / 2, CRUISE, FLOOR, RAMP));
  TEST_ASSERT_TRUE(approachSpeed(RAMP / 3, CRUISE, FLOOR, RAMP) < approachSpeed(2 * RAMP / 3, CRUISE, FLOOR, RAMP));
}

void test_never_below_floor_before_stop_point() {
  // the old P-loop fell to ~23 here and stalled going up
  TEST_ASSERT_EQUAL_UINT8(ramp(1), approachSpeed(1, CRUISE, FLOOR, RAMP));
  TEST_ASSERT_TRUE(ramp(1) < FLOOR + 2);
  for (int32_t d = 1; d <= RAMP; d++)
    TEST_ASSERT_TRUE(approachSpeed(d, CRUISE, FLOOR, RAMP) >= FLOOR);
}

void test_zero_at_or_past_stop_point() {
  TEST_ASSERT_EQUAL_UINT8(0, approachSpeed(0, CRUISE, FLOOR, RAMP));
  TEST_ASSERT_EQUAL_UINT8(0, approachSpeed(-5, CRUISE, FLOOR, RAMP));
}

void test_low_cruise_is_never_exceeded() {
  const uint8_t slow = FLOOR + 30;
  TEST_ASSERT_EQUAL_UINT8(slow, approachSpeed(10 * RAMP, slow, FLOOR, RAMP));
  TEST_ASSERT_EQUAL_UINT8(slow, approachSpeed(RAMP / 3, slow, FLOOR, RAMP));  // ramp(RAMP/3) > slow
  // a cruise below the floor wins: the user's speed is the ceiling
  TEST_ASSERT_EQUAL_UINT8(FLOOR / 2, approachSpeed(5, FLOOR / 2, FLOOR, RAMP));
}

void test_zero_ramp_means_cruise_until_stop() {
  TEST_ASSERT_EQUAL_UINT8(255, approachSpeed(1, CRUISE, FLOOR, 0));
}

void test_goto_speed_sign_follows_direction() {
  TEST_ASSERT_EQUAL_INT16(255, gotoSpeed(10 * RAMP, +1, CRUISE, FLOOR, RAMP, DEADBAND));
  TEST_ASSERT_EQUAL_INT16(-255, gotoSpeed(-10 * RAMP, -1, CRUISE, FLOOR, RAMP, DEADBAND));
}

void test_goto_holds_floor_on_final_approach() {
  // 78 counts short going up: the bench stall point of the old loop
  TEST_ASSERT_TRUE(gotoSpeed(78, +1, CRUISE, FLOOR, RAMP, DEADBAND) >= FLOOR);
  TEST_ASSERT_TRUE(gotoSpeed(-78, -1, CRUISE, FLOOR, RAMP, DEADBAND) <= -FLOOR);
}

void test_goto_arrives_inside_deadband() {
  TEST_ASSERT_EQUAL_INT16(0, gotoSpeed(DEADBAND, +1, CRUISE, FLOOR, RAMP, DEADBAND));
  TEST_ASSERT_EQUAL_INT16(0, gotoSpeed(-DEADBAND, -1, CRUISE, FLOOR, RAMP, DEADBAND));
  TEST_ASSERT_EQUAL_INT16(0, gotoSpeed(0, +1, CRUISE, FLOOR, RAMP, DEADBAND));
}

void test_goto_never_reverses_after_crossing_target() {
  // overshot the target: arrive, do not hunt back
  TEST_ASSERT_EQUAL_INT16(0, gotoSpeed(-40, +1, CRUISE, FLOOR, RAMP, DEADBAND));
  TEST_ASSERT_EQUAL_INT16(0, gotoSpeed(40, -1, CRUISE, FLOOR, RAMP, DEADBAND));
}

void test_manual_unhomed_runs_at_safe_speed() {
  // before homing there are no soft limits: never the set speed (255)
  TEST_ASSERT_EQUAL_UINT8(SAFE, manualSpeed(false, 5000, 255, FLOOR, RAMP, SAFE));
  TEST_ASSERT_EQUAL_UINT8(SAFE, manualSpeed(false, -5, 255, FLOOR, RAMP, SAFE));
  // a set speed below the safe speed is kept
  TEST_ASSERT_EQUAL_UINT8(SAFE / 2, manualSpeed(false, 5000, SAFE / 2, FLOOR, RAMP, SAFE));
}

void test_manual_homed_ramps_to_soft_limit() {
  TEST_ASSERT_EQUAL_UINT8(255, manualSpeed(true, 10 * RAMP, 255, FLOOR, RAMP, SAFE));
  TEST_ASSERT_EQUAL_UINT8(ramp(RAMP / 2), manualSpeed(true, RAMP / 2, 255, FLOOR, RAMP, SAFE));
  TEST_ASSERT_EQUAL_UINT8(0, manualSpeed(true, 0, 255, FLOOR, RAMP, SAFE));
}

void test_v1_margin_doubles_on_load() {
  // config v1 applied the margin twice; v2 applies it once
  TEST_ASSERT_EQUAL_INT32(40, marginFromStored(1, 20));
  TEST_ASSERT_EQUAL_INT32(40, marginFromStored(2, 40));
  TEST_ASSERT_EQUAL_INT32(20, marginFromStored(2, 20));
}

void test_up_before_homing_homes_first() {
  // a closed panel may be above: never drive up blind
  TEST_ASSERT_TRUE(homeBeforeManual(+1, false));
  TEST_ASSERT_FALSE(homeBeforeManual(+1, true));
}

void test_down_never_homes_first() {
  // down runs slowly onto the lower switch instead
  TEST_ASSERT_FALSE(homeBeforeManual(-1, false));
  TEST_ASSERT_FALSE(homeBeforeManual(-1, true));
}

void test_target_in_range_is_inclusive() {
  TEST_ASSERT_TRUE(targetInRange(40, 40, 2945));
  TEST_ASSERT_TRUE(targetInRange(2945, 40, 2945));
  TEST_ASSERT_TRUE(targetInRange(1500, 40, 2945));
}

void test_target_outside_soft_limits_is_refused() {
  // a goto queued before homing is checked against the new limits
  TEST_ASSERT_FALSE(targetInRange(39, 40, 2945));
  TEST_ASSERT_FALSE(targetInRange(2946, 40, 2945));
  TEST_ASSERT_FALSE(targetInRange(-120, 40, 2945));
}

void test_both_switches_triggered_is_a_wiring_fault() {
  // NC switches: a broken wire or lost common GND reads triggered
  TEST_ASSERT_TRUE(wiringFault(true, true));
  TEST_ASSERT_FALSE(wiringFault(true, false));
  TEST_ASSERT_FALSE(wiringFault(false, true));
  TEST_ASSERT_FALSE(wiringFault(false, false));
}

void test_backoff_continues_while_on_upper_switch() {
  TEST_ASSERT_EQUAL_INT((int)Backoff::CONTINUE, (int)backoffStep(true, false, 10, HOMING_BACKOFF_MAX_COUNTS));
}

void test_backoff_done_when_upper_switch_releases() {
  TEST_ASSERT_EQUAL_INT((int)Backoff::DONE, (int)backoffStep(false, false, 10, HOMING_BACKOFF_MAX_COUNTS));
}

void test_backoff_faults_on_lower_switch() {
  // driving down onto the lower switch: the upper "trigger" was a fault
  TEST_ASSERT_EQUAL_INT((int)Backoff::FAULT_LOWER, (int)backoffStep(true, true, 10, HOMING_BACKOFF_MAX_COUNTS));
  TEST_ASSERT_EQUAL_INT((int)Backoff::FAULT_LOWER, (int)backoffStep(false, true, 10, HOMING_BACKOFF_MAX_COUNTS));
}

void test_backoff_faults_past_the_distance_cap() {
  TEST_ASSERT_EQUAL_INT((int)Backoff::FAULT_DISTANCE,
                        (int)backoffStep(true, false, HOMING_BACKOFF_MAX_COUNTS, HOMING_BACKOFF_MAX_COUNTS));
  TEST_ASSERT_EQUAL_INT((int)Backoff::CONTINUE,
                        (int)backoffStep(true, false, HOMING_BACKOFF_MAX_COUNTS - 1, HOMING_BACKOFF_MAX_COUNTS));
}

void test_backoff_cap_ignores_encoder_sign() {
  // an inverted encoder counts the back-off up instead of down
  TEST_ASSERT_EQUAL_INT((int)Backoff::FAULT_DISTANCE,
                        (int)backoffStep(true, false, -HOMING_BACKOFF_MAX_COUNTS, HOMING_BACKOFF_MAX_COUNTS));
  TEST_ASSERT_EQUAL_INT((int)Backoff::CONTINUE,
                        (int)backoffStep(true, false, -(HOMING_BACKOFF_MAX_COUNTS - 1), HOMING_BACKOFF_MAX_COUNTS));
}

void test_drive_write_on_change() {
  TEST_ASSERT_TRUE(driveWriteDue(0, 80, 0, MOTOR_REFRESH_MS));
  TEST_ASSERT_TRUE(driveWriteDue(80, -80, 0, MOTOR_REFRESH_MS));
}

void test_drive_write_refreshes_unchanged_speed() {
  // SCMD writes are fire-and-forget: re-send so a lost write is repaired
  TEST_ASSERT_FALSE(driveWriteDue(80, 80, MOTOR_REFRESH_MS - 1, MOTOR_REFRESH_MS));
  TEST_ASSERT_TRUE(driveWriteDue(80, 80, MOTOR_REFRESH_MS, MOTOR_REFRESH_MS));
}

void test_rest_stop_refreshes_periodically() {
  TEST_ASSERT_FALSE(restStopDue(MOTOR_REFRESH_MS - 1, 0, false, MOTOR_REFRESH_MS, REST_DRIFT_COUNTS));
  TEST_ASSERT_TRUE(restStopDue(MOTOR_REFRESH_MS, 0, false, MOTOR_REFRESH_MS, REST_DRIFT_COUNTS));
}

void test_rest_stop_now_on_drift_or_limit() {
  TEST_ASSERT_FALSE(restStopDue(0, REST_DRIFT_COUNTS, false, MOTOR_REFRESH_MS, REST_DRIFT_COUNTS));
  TEST_ASSERT_TRUE(restStopDue(0, REST_DRIFT_COUNTS + 1, false, MOTOR_REFRESH_MS, REST_DRIFT_COUNTS));
  TEST_ASSERT_TRUE(restStopDue(0, -(REST_DRIFT_COUNTS + 1), false, MOTOR_REFRESH_MS, REST_DRIFT_COUNTS));
  TEST_ASSERT_TRUE(restStopDue(0, 0, true, MOTOR_REFRESH_MS, REST_DRIFT_COUNTS));
}

void test_margin_range_with_known_travel() {
  // [SOFT_LIMIT_MARGIN_MIN, travel/2]: never at or past the switches
  TEST_ASSERT_TRUE(marginInRange(SOFT_LIMIT_MARGIN_MIN, 2985));
  TEST_ASSERT_TRUE(marginInRange(2985 / 2, 2985));
  TEST_ASSERT_FALSE(marginInRange(SOFT_LIMIT_MARGIN_MIN - 1, 2985));
  TEST_ASSERT_FALSE(marginInRange(0, 2985));
  TEST_ASSERT_FALSE(marginInRange(-200, 2985));
  TEST_ASSERT_FALSE(marginInRange(2985 / 2 + 1, 2985));
}

void test_margin_range_before_homing_uses_fixed_bounds() {
  // travel unknown: [SOFT_LIMIT_MARGIN_MIN, SOFT_LIMIT_MARGIN_MAX_UNHOMED]
  TEST_ASSERT_TRUE(marginInRange(SOFT_LIMIT_MARGIN_MAX_UNHOMED, 0));
  TEST_ASSERT_FALSE(marginInRange(SOFT_LIMIT_MARGIN_MAX_UNHOMED + 1, 0));
  TEST_ASSERT_FALSE(marginInRange(100000, 0));
  TEST_ASSERT_FALSE(marginInRange(0, 0));
}

void test_margin_clamped_into_range() {
  TEST_ASSERT_EQUAL_INT32(SOFT_LIMIT_MARGIN_MIN, clampMargin(0, 2985));
  TEST_ASSERT_EQUAL_INT32(SOFT_LIMIT_MARGIN_MIN, clampMargin(-200, 2985));
  TEST_ASSERT_EQUAL_INT32(2985 / 2, clampMargin(100000, 2985));
  TEST_ASSERT_EQUAL_INT32(60, clampMargin(60, 2985));
  TEST_ASSERT_EQUAL_INT32(SOFT_LIMIT_MARGIN_MIN, clampMargin(20, 0));   // corrupt/old value before homing
}

void test_loaded_margin_only_clamped_to_minimum_before_homing() {
  // the 500 cap is for #HLM before homing only; homing enforces travel/2
  TEST_ASSERT_EQUAL_INT32(700, clampMargin(700, 0));                    // homed #HLM700, after a reboot
  TEST_ASSERT_EQUAL_INT32(700, clampMargin(clampMargin(700, 0), 2985)); // ...survives homing
  TEST_ASSERT_EQUAL_INT32(600, clampMargin(marginFromStored(1, 300), 0)); // v1 300 doubled, not cut to 500
  TEST_ASSERT_EQUAL_INT32(2985 / 2, clampMargin(clampMargin(100000, 0), 2985));
}

void test_limit_blocks_commanded_direction() {
  // by the commanded direction, so the first tick never drives into a switch
  TEST_ASSERT_TRUE(limitBlocks(+1, true, false));
  TEST_ASSERT_TRUE(limitBlocks(-1, false, true));
  TEST_ASSERT_FALSE(limitBlocks(+1, false, true));   // moving away from the lower switch
  TEST_ASSERT_FALSE(limitBlocks(-1, true, false));   // moving away from the upper switch
  TEST_ASSERT_FALSE(limitBlocks(0, true, true));
}

void test_reversal_pauses_after_recent_opposite_drive() {
  TEST_ASSERT_EQUAL_UINT32(REVERSAL_PAUSE_MS, reversalPauseMs(+1, 0, -80, REVERSAL_PAUSE_MS));
  TEST_ASSERT_EQUAL_UINT32(REVERSAL_PAUSE_MS - 30, reversalPauseMs(-1, 30, 255, REVERSAL_PAUSE_MS));
}

void test_no_pause_same_direction_stop_or_settled() {
  TEST_ASSERT_EQUAL_UINT32(0, reversalPauseMs(+1, 0, 80, REVERSAL_PAUSE_MS));     // same direction
  TEST_ASSERT_EQUAL_UINT32(0, reversalPauseMs(+1, 0, 0, REVERSAL_PAUSE_MS));      // stopping
  TEST_ASSERT_EQUAL_UINT32(0, reversalPauseMs(0, 0, -80, REVERSAL_PAUSE_MS));     // never driven
  TEST_ASSERT_EQUAL_UINT32(0, reversalPauseMs(+1, REVERSAL_PAUSE_MS, -80, REVERSAL_PAUSE_MS));  // coasted out
}

void test_speed_clamped_to_approach_floor() {
  // below the approach speed the lift stalls uphill
  TEST_ASSERT_EQUAL_UINT8(MOTOR_APPROACH_SPEED, clampSpeed(0));
  TEST_ASSERT_EQUAL_UINT8(MOTOR_APPROACH_SPEED, clampSpeed(20));
  TEST_ASSERT_EQUAL_UINT8(MOTOR_APPROACH_SPEED, clampSpeed(-5));
  TEST_ASSERT_EQUAL_UINT8(128, clampSpeed(128));
  TEST_ASSERT_EQUAL_UINT8(MOTOR_MAX_SPEED, clampSpeed(255));
  TEST_ASSERT_EQUAL_UINT8(MOTOR_MAX_SPEED, clampSpeed(300));   // not truncated to 44
}

void test_backoff_start_kept_until_switch_releases() {
  // re-homing (#HLH, stop + #HLH) mid back-off must not restart the cap
  TEST_ASSERT_EQUAL_INT32(2900, backoffStart(true, 2900, 2800));
  TEST_ASSERT_EQUAL_INT32(2800, backoffStart(false, 2900, 2800));
}

void test_homing_travel_must_fit_two_margins() {
  // a broken lower wire makes stage 1 "find" the lower switch at the top
  TEST_ASSERT_FALSE(travelPlausible(0));
  TEST_ASSERT_FALSE(travelPlausible(2 * SOFT_LIMIT_MARGIN_MIN - 1));
  TEST_ASSERT_TRUE(travelPlausible(2 * SOFT_LIMIT_MARGIN_MIN));
  TEST_ASSERT_TRUE(travelPlausible(2985));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_full_cruise_outside_ramp);
  RUN_TEST(test_ramps_down_linearly_inside_ramp);
  RUN_TEST(test_never_below_floor_before_stop_point);
  RUN_TEST(test_zero_at_or_past_stop_point);
  RUN_TEST(test_low_cruise_is_never_exceeded);
  RUN_TEST(test_zero_ramp_means_cruise_until_stop);
  RUN_TEST(test_goto_speed_sign_follows_direction);
  RUN_TEST(test_goto_holds_floor_on_final_approach);
  RUN_TEST(test_goto_arrives_inside_deadband);
  RUN_TEST(test_goto_never_reverses_after_crossing_target);
  RUN_TEST(test_manual_unhomed_runs_at_safe_speed);
  RUN_TEST(test_manual_homed_ramps_to_soft_limit);
  RUN_TEST(test_v1_margin_doubles_on_load);
  RUN_TEST(test_up_before_homing_homes_first);
  RUN_TEST(test_down_never_homes_first);
  RUN_TEST(test_target_in_range_is_inclusive);
  RUN_TEST(test_target_outside_soft_limits_is_refused);
  RUN_TEST(test_both_switches_triggered_is_a_wiring_fault);
  RUN_TEST(test_backoff_continues_while_on_upper_switch);
  RUN_TEST(test_backoff_done_when_upper_switch_releases);
  RUN_TEST(test_backoff_faults_on_lower_switch);
  RUN_TEST(test_backoff_faults_past_the_distance_cap);
  RUN_TEST(test_backoff_cap_ignores_encoder_sign);
  RUN_TEST(test_drive_write_on_change);
  RUN_TEST(test_drive_write_refreshes_unchanged_speed);
  RUN_TEST(test_rest_stop_refreshes_periodically);
  RUN_TEST(test_rest_stop_now_on_drift_or_limit);
  RUN_TEST(test_margin_range_with_known_travel);
  RUN_TEST(test_margin_range_before_homing_uses_fixed_bounds);
  RUN_TEST(test_margin_clamped_into_range);
  RUN_TEST(test_loaded_margin_only_clamped_to_minimum_before_homing);
  RUN_TEST(test_limit_blocks_commanded_direction);
  RUN_TEST(test_reversal_pauses_after_recent_opposite_drive);
  RUN_TEST(test_no_pause_same_direction_stop_or_settled);
  RUN_TEST(test_speed_clamped_to_approach_floor);
  RUN_TEST(test_backoff_start_kept_until_switch_releases);
  RUN_TEST(test_homing_travel_must_fit_two_margins);
  return UNITY_END();
}
