// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-

/// @file	AP_MotorsHeli.h
/// @brief	Motor control class for Traditional Heli

#ifndef __AP_MOTORS_HELI_H__
#define __AP_MOTORS_HELI_H__

#include <inttypes.h>
#include <AP_Common.h>
#include <AP_Math.h>        // ArduPilot Mega Vector/Matrix math Library
#include <RC_Channel.h>     // RC Channel Library
#include "AP_Motors.h"

// output channels
#define AP_MOTORS_HELI_EXT_GYRO                 CH_7    // tail servo uses channel 7
#define AP_MOTORS_HELI_EXT_RSC                  CH_8    // main rotor controlled with channel 8

// maximum number of swashplate servos
#define AP_MOTORS_HELI_NUM_SWASHPLATE_SERVOS    3

// servo output rates
#define AP_MOTORS_HELI_SPEED_DEFAULT            125     // default servo update rate for helicopters
#define AP_MOTORS_HELI_SPEED_DIGITAL_SERVOS     125     // update rate for digital servos
#define AP_MOTORS_HELI_SPEED_ANALOG_SERVOS      125     // update rate for analog servos

// servo position defaults
#define AP_MOTORS_HELI_SERVO1_POS               -60
#define AP_MOTORS_HELI_SERVO2_POS                60
#define AP_MOTORS_HELI_SERVO3_POS               180

// swash type definitions
#define AP_MOTORS_HELI_SWASH_CCPM               0
#define AP_MOTORS_HELI_SWASH_H1                 1

// default swash min and max angles and positions
#define AP_MOTORS_HELI_SWASH_ROLL_MAX           4500
#define AP_MOTORS_HELI_SWASH_PITCH_MAX          4500
#define AP_MOTORS_HELI_COLLECTIVE_MIN           1250
#define AP_MOTORS_HELI_COLLECTIVE_MAX           1750
#define AP_MOTORS_HELI_COLLECTIVE_MID           1500

// swash min and max position (expressed as percentage) while in stabilize mode
#define AP_MOTORS_HELI_MANUAL_COLLECTIVE_MIN    0
#define AP_MOTORS_HELI_MANUAL_COLLECTIVE_MAX    100

// default external gyro gain (ch7 out)
#define AP_MOTORS_HELI_EXT_GYRO_GAIN            1350

// main rotor speed control types (ch8 out)
#define AP_MOTORS_HELI_RSC_MODE_NONE            0       // main rotor ESC is directly connected to receiver
#define AP_MOTORS_HELI_RSC_MODE_CH8_PASSTHROUGH 1       // main rotor ESC is connected to RC8 (out) but pilot still directly controls speed with a passthrough from CH8 (in)
#define AP_MOTORS_HELI_RSC_MODE_EXT_GOVERNOR    2       // main rotor ESC is connected to RC8 and controlled by arducopter

// default main rotor governor set-point (ch8 out)
#define AP_MOTORS_HELI_EXT_GOVERNOR_SETPOINT    1500

// default main rotor ramp up rate in 100th of seconds
#define AP_MOTORS_HELI_RSC_RATE                 1000    // 1000 = 10 seconds
// motor run-up time default in 100th of seconds
#define AP_MOTORS_HELI_MOTOR_RUNUP_TIME         500     // 500 = 5 seconds

// flybar types
#define AP_MOTORS_HELI_NOFLYBAR                 0
#define AP_MOTORS_HELI_FLYBAR                   1

class AP_HeliControls;

/// @class      AP_MotorsHeli
class AP_MotorsHeli : public AP_Motors {
public:

    /// Constructor
    AP_MotorsHeli( RC_Channel*      rc_roll,
                   RC_Channel*      rc_pitch,
                   RC_Channel*      rc_throttle,
                   RC_Channel*      rc_yaw,
                   RC_Channel*      rc_8,
                   RC_Channel*      swash_servo_1,
                   RC_Channel*      swash_servo_2,
                   RC_Channel*      swash_servo_3,
                   RC_Channel*      yaw_servo,
                   uint16_t         speed_hz = AP_MOTORS_HELI_SPEED_DEFAULT) :
        AP_Motors(rc_roll, rc_pitch, rc_throttle, rc_yaw, speed_hz),
        _servo_1(swash_servo_1),
        _servo_2(swash_servo_2),
        _servo_3(swash_servo_3),
        _servo_4(yaw_servo),
        _rc_8(rc_8),
        _roll_scaler(1),
        _pitch_scaler(1),
        _collective_scalar(1),
        _collective_scalar_manual(1),
        _collective_out(0),
        _collective_mid_pwm(0),
        _rsc_output(0),
        _rsc_ramp(0),
        _motor_runup_timer(0)
    {
		AP_Param::setup_object_defaults(this, var_info);

        // initialise flags
        _heliflags.swash_initialised = 0;
        _heliflags.manual_collective = 0;
        _heliflags.landing_collective = 0;
        _heliflags.motor_runup_complete = 0;
    };

    // init
    void Init();

    // set update rate to motors - a value in hertz
    // you must have setup_motors before calling this
    void set_update_rate( uint16_t speed_hz );

    // enable - starts allowing signals to be sent to motors
    void enable();

    // output_min - sends minimum values out to the motors
    void output_min();

    // output_test - wiggle servos in order to show connections are correct
    void output_test();

    //
    // heli specific methods
    //

    // allow_arming - returns true if main rotor is spinning and it is ok to arm
    bool allow_arming();

    // ext_gyro_enabled - returns true if we have an external gyro for yaw control
    bool ext_gyro_enabled() { return _ext_gyro_enabled; }

    // ext_gyro_gain - gets and sets external gyro gain output on ch7
    int16_t ext_gyro_gain() { return _ext_gyro_gain; }
    void ext_gyro_gain(int16_t gain) { _ext_gyro_gain = gain; }

    // has_flybar - returns true if we have a mechical flybar
    bool has_flybar() { return _flybar_mode; }

    // get_collective_mid - returns collective mid position as a number from 0 ~ 1000
    int16_t get_collective_mid() { return  _collective_mid; }

    // get_collective_out - returns collective position from last output as a number from 0 ~ 1000
    int16_t get_collective_out() { return _collective_out; }

    // set_collective_for_manual_control - limits collective to reduced range for stabilize (i.e. manual) flying
    void set_collective_for_manual_control(bool true_false) { _heliflags.manual_collective = true_false; }

    // get min/max collective when controlled manually as a number from 0 ~ 1000 (note that parameter is stored as percentage)
    int16_t get_manual_collective_min() { return _manual_collective_min*10; }
    int16_t get_manual_collective_max() { return _manual_collective_max*10; }

    // set_collective_for_landing - limits collective from going too low if we know we are landed
    void set_collective_for_landing(bool landing) { _heliflags.landing_collective = landing; }

    // return true if the main rotor is up to speed
    bool motor_runup_complete() { return _heliflags.motor_runup_complete; }

    // var_info for holding Parameter information
    static const struct AP_Param::GroupInfo var_info[];

protected:

    // output - sends commands to the motors
    void output_armed();
    void output_disarmed();

private:

    // heli_move_swash - moves swash plate to attitude of parameters passed in
    void move_swash(int16_t roll_out, int16_t pitch_out, int16_t coll_in, int16_t yaw_out);

    // reset_swash - free up swash for maximum movements. Used for set-up
    void reset_swash();

    // init_swash - initialise the swash plate
    void init_swash();

    // calculate_roll_pitch_collective_factors - calculate factors based on swash type and servo position
    void calculate_roll_pitch_collective_factors();

    // rsc_control - update value to send to main rotor's ESC
    void rsc_control();

    // external objects we depend upon
    RC_Channel      *_servo_1;
    RC_Channel      *_servo_2;
    RC_Channel      *_servo_3;
    RC_Channel      *_servo_4;
    RC_Channel      *_rc_8;

    // flags bitmask
    struct heliflags_type {
        uint8_t swash_initialised       : 1;    // true if swash has been initialised
        uint8_t manual_collective       : 1;    // true if pilot is manually controlling the collective.  If true then we reduce the swash range
        uint8_t landing_collective      : 1;    // true if collective is setup for landing which has much higher minimum
        uint8_t motor_runup_complete    : 1;    // true if the rotors have had enough time to wind up
    } _heliflags;

    // parameters
    AP_Int16        _servo1_pos;                // Angular location of swash servo #1
    AP_Int16        _servo2_pos;                // Angular location of swash servo #2
    AP_Int16        _servo3_pos;                // Angular location of swash servo #3
    AP_Int16        _roll_max;                  // Maximum roll angle of the swash plate in centi-degrees
    AP_Int16        _pitch_max;                 // Maximum pitch angle of the swash plate in centi-degrees
    AP_Int16        _collective_min;            // Lowest possible servo position for the swashplate
    AP_Int16        _collective_max;            // Highest possible servo position for the swashplate
    AP_Int16        _collective_mid;            // Swash servo position corresponding to zero collective pitch (or zero lift for Assymetrical blades)
    AP_Int16        _ext_gyro_enabled;          // Enabled/Disable an external rudder gyro connected to channel 7.  With no external gyro a more complex yaw controller is used
    AP_Int8         _swash_type;                // Swash Type Setting - either 3-servo CCPM or H1 Mechanical Mixing
    AP_Int16        _ext_gyro_gain;             // PWM sent to the external gyro on Ch7
    AP_Int8         _servo_manual;              // Pass radio inputs directly to servos during set-up through mission planner
    AP_Int16        _phase_angle;               // Phase angle correction for rotor head.  If pitching the swash forward induces a roll, this can be correct the problem
    AP_Int16        _collective_yaw_effect;     // Feed-forward compensation to automatically add rudder input when collective pitch is increased. Can be positive or negative depending on mechanics.    
    AP_Int16        _ext_gov_setpoint;          // PWM passed to the external motor governor when external governor is enabledv
    AP_Int8         _rsc_mode;                  // Sets which main rotor ESC control mode is active
    AP_Int16        _rsc_ramp_up_rate;          // The time in 100th seconds the RSC takes to ramp up to speed
    AP_Int8         _flybar_mode;               // Flybar present or not.  Affects attitude controller used during ACRO flight mode
    AP_Int8         _manual_collective_min;     // Minimum collective position while pilot directly controls the collective
    AP_Int8         _manual_collective_max;     // Maximum collective position while pilot directly controls the collective

    // internal variables
    float           _rollFactor[AP_MOTORS_HELI_NUM_SWASHPLATE_SERVOS];
    float           _pitchFactor[AP_MOTORS_HELI_NUM_SWASHPLATE_SERVOS];
    float           _collectiveFactor[AP_MOTORS_HELI_NUM_SWASHPLATE_SERVOS];
    float           _roll_scaler;               // scaler to convert roll input from radio (i.e. -4500 ~ 4500) to max roll range
    float           _pitch_scaler;              // scaler to convert pitch input from radio (i.e. -4500 ~ 4500) to max pitch range
    float           _collective_scalar;         // collective scalar to convert pwm form (i.e. 0 ~ 1000) passed in to actual servo range (i.e 1250~1750 would be 500)
    float           _collective_scalar_manual;  // collective scalar to reduce the range of the collective movement while collective is being controlled manually (i.e. directly by the pilot)
    int16_t         _collective_out;            // actual collective pitch value.  Required by the main code for calculating cruise throttle
    int16_t         _collective_mid_pwm;        // collective mid parameter value converted to pwm form (i.e. 0 ~ 1000)
    int16_t         _rsc_output;                // final output to the external motor governor 1000-2000
    int16_t         _rsc_ramp;                  // current state of ramping
    int16_t         _motor_runup_timer;         // timer to determine if motor has run up fully
};

#endif  // AP_MOTORSHELI
