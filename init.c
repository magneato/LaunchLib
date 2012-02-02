#include "global.h"
#include "init.h"
#include "hardware.h"

void HardwareInit(void)
{
#ifdef BARE_LAUNCHPAD
    MAKE_OUTPUT(RED_LED);
    RED_LED_OFF();

    MAKE_OUTPUT(GREEN_LED);
    GREEN_LED_OFF();

    MAKE_OUTPUT(PWM_0);
    MAKE_SPECIAL(PWM_0);

    MAKE_OUTPUT(PWM_1);
    MAKE_SPECIAL(PWM_1);
#endif

#ifdef LINE_FOLLOWER_R1
    MAKE_INPUT(SW_1);

    MAKE_INPUT(IR_LEFT_CORNER);
    MAKE_INPUT(IR_LEFT_MIDDLE);
    MAKE_INPUT(IR_RIGHT_CORNER);
    MAKE_INPUT(IR_RIGHT_MIDDLE);

    MAKE_OUTPUT(MOTOR_1_DIR_1);
    MAKE_OUTPUT(MOTOR_1_DIR_2);
    MAKE_OUTPUT(MOTOR_1_PWM);
    MAKE_SPECIAL(MOTOR_1_PWM);

    MAKE_OUTPUT(MOTOR_2_DIR_1);
    MAKE_OUTPUT(MOTOR_2_DIR_2);
    MAKE_OUTPUT(MOTOR_2_PWM);
    MAKE_SPECIAL(MOTOR_2_PWM);
#endif
#ifdef LINE_FOLLOWER_R2
    MAKE_INPUT(SW_1);

    MAKE_INPUT(IR_LEFT_CORNER);
    MAKE_INPUT(IR_LEFT_MIDDLE);
    MAKE_INPUT(IR_RIGHT_CORNER);
    MAKE_INPUT(IR_RIGHT_MIDDLE);

    MAKE_OUTPUT(MOTOR_1_DIR_1);
    MAKE_OUTPUT(MOTOR_1_DIR_2);
    MAKE_OUTPUT(MOTOR_1_PWM);
    MAKE_SPECIAL(MOTOR_1_PWM);

    MAKE_OUTPUT(MOTOR_2_DIR_1);
    MAKE_OUTPUT(MOTOR_2_DIR_2);
    MAKE_OUTPUT(MOTOR_2_PWM);
    MAKE_SPECIAL(MOTOR_2_PWM);
#endif

}
