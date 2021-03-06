/**
@file schedule.c
@brief Generic scheduling mechanisms
@author Joe Brown
*/
#include "global.h"
#include "schedule.h"
#include "hardware_init.h"
#include "config.h"

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//                            __                        __
//                           / /   ____   _____ ____ _ / /
//                          / /   / __ \ / ___// __ `// /
//                         / /___/ /_/ // /__ / /_/ // /
//                        /_____/\____/ \___/ \__,_//_/
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
#define WDT_INT_ENABLE IE1
#define SCHEDULE_VECTOR WDT_VECTOR

// global time
uint32_t now = 0;

// multiplier for timing
volatile uint8_t g_timing_multiplier = 0;

/** @brief number of registered callouts*/
static uint8_t event_count;
/** @brief configured callback list*/
static CallbackEvent callback_store[MAX_CALLBACK_CNT];

/** @brief bit array represending occupied or vacant callouts*/
static uint16_t callout_map;

/** @brief Congiruation for callout which holds the function pointer and next
time it will run.*/
typedef struct
{
    CalloutFn func;
    uint32_t run_time;
} CalloutEvent;

/** @brief Array of function callouts. If a callout is enabled it will have a
function pointer stored and if it is disabled the pointer will be replaced
with a null*/
static CalloutEvent callout_store[MAX_CALLOUT_CNT];

/**
@brief Check the callback list for functions that are ready to run
@details
Search the callback store for functions that enabled with a time that is equal
to the current global time. If we find the function call it and reset the
run_time based on the stored value.
@param[in] current_time current global time from now variable
*/
static void CallbackService(uint32_t current_time);

/**
@brief Check the callout list for functions that are ready to run
@details
Search the callout store for functions that enabled with a time that is equal
to the current global time. If we find the function call it and vacate the slot
in the map.
@param[in] current_time current global time from now variable
*/
static void CalloutService(uint32_t current_time);

/**
@brief Return the number of occupied slots in the callout map (pending callouts)
@details
Use K&R method to run through the callout map and count the number of bits. This
represents the number of slot occupied in the callout store.
@return the number of functions pending in the callout map
*/
static uint8_t get_callout_map_size(void);

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//                            ____        _  __
//                           /  _/____   (_)/ /_
//                           / / / __ \ / // __/
//                         _/ / / / / // // /_
//                        /___//_/ /_//_/ \__/
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void ScheduleTimerInit(void)
{
    // divide the mclk by 512
    WDTCTL = WDT_MDLY_0_5;
    // calculate the multiplier (number of clks to get to 1ms)
    g_timing_multiplier = (g_clock_speed / 1000000) * 2;
    WDT_INT_ENABLE |= WDTIE;    // Enable WDT interrupt
}

uint32_t TimeNow(void)
{
    return now;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//            ____        __                                   __
//           /  _/____   / /_ ___   _____ _____ __  __ ____   / /_
//           / / / __ \ / __// _ \ / ___// ___// / / // __ \ / __/
//         _/ / / / / // /_ /  __// /   / /   / /_/ // /_/ // /_
//        /___//_/ /_/ \__/ \___//_/   /_/    \__,_// .___/ \__/
//                                                 /_/
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
#pragma vector=SCHEDULE_VECTOR
__interrupt void ScheduleTimerOverflow(void)
{
    now++;
    CallbackService(now);
    CalloutService(now);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//               ______        __ __ __                  __
//              / ____/____ _ / // // /_   ____ _ _____ / /__
//             / /    / __ `// // // __ \ / __ `// ___// //_/
//            / /___ / /_/ // // // /_/ // /_/ // /__ / ,<
//            \____/ \__,_//_//_//_.___/ \__,_/ \___//_/|_|
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
int8_t CallbackRegister(CallbackFn func, uint32_t run_time)
{
    if (event_count < sizeof(callback_store)/sizeof(CallbackFn))
    {
        // Callbacks are initialized disabled
        callback_store[event_count].enabled       = FALSE;
        callback_store[event_count].func          = func;
        callback_store[event_count].run_time      = run_time - 1;
        callback_store[event_count].next_run_time = now + (run_time * _MILLISECOND);
        event_count++;
        return (SUCCESS);
    }
    return (FAILURE);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void CallbackService(uint32_t current_time)
{
    uint8_t i = 0;
    uint8_t callbacks_remaining = event_count;
    if (callbacks_remaining == 0)
    {
        goto service_complete;
    }
    for (i = 0;i < event_count;i++)
    {
        if (callback_store[i].enabled == TRUE &&
            current_time == callback_store[i].next_run_time)
        {
            callback_store[i].next_run_time = current_time +
                                              (callback_store[i].run_time * _MILLISECOND);
            callback_store[i].func();
            if (--callbacks_remaining == 0)
            {
                goto service_complete;
            }
        }
    }
service_complete:
    return;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void CallbackMode(CallbackFn func, enum ScheduleMode mode)
{
    uint8_t i = 0;
    for (i = 0;i < event_count;i++)
    {
        if (func == callback_store[i].func)
        {
            callback_store[i].enabled = mode;
            if (mode)
            {
                callback_store[i].next_run_time = now +
                                                  (callback_store[i].run_time * _MILLISECOND);
            }
            break;
        }
    }
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//                   ______        __ __               __
//                  / ____/____ _ / // /____   __  __ / /_
//                 / /    / __ `// // // __ \ / / / // __/
//                / /___ / /_/ // // // /_/ // /_/ // /_
//                \____/ \__,_//_//_/ \____/ \__,_/ \__/
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
int8_t CalloutRegister(CalloutFn func, uint32_t run_time)
{
    // event queue full?
    if (get_callout_map_size() != MAX_CALLOUT_CNT)
    {
        uint8_t i = 0;
        for (i = 0;i < MAX_CALLOUT_CNT;i++)
        {
            // find the first open slot
            if (!(callout_map & _BV(i)))
            {
                // claim the slot
                callout_map |= _BV(i);
                // save our data
                callout_store[i].func = func;
                callout_store[i].run_time = now + (run_time * _MILLISECOND);
                return (SUCCESS);
            }
        }
    }
    return (FAILURE);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void CalloutCancel(CalloutFn func)
{
    uint8_t i = 0;
    for (i = 0;i < MAX_CALLOUT_CNT;i++)
    {
        // find the slot
        if (callout_store[i].func == func)
        {
            // clear
            callout_map &= ~_BV(i);
            break;
        }
    }
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
uint8_t get_callout_map_size(void)
{
    uint8_t sum = 0;
    uint16_t map = callout_map;
    for (sum = 0; map; sum++)
    {
        map &= map - 1;
    }
    return sum;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void CalloutService(uint32_t current_time)
{
    uint8_t i = 0;
    // get the number of callouts
    uint8_t callouts_remaining = get_callout_map_size();
    // if there are none to service
    if (!callouts_remaining)
    {
        goto service_complete;
    }
    for (i = 0;i < MAX_CALLOUT_CNT;i++)
    {
        // find occupied slots and see if the function there is ready
        if ((callout_map & _BV(i)) && current_time == callout_store[i].run_time)
        {
            // run the function
            callout_store[i].func();
            // vacate the slot
            callout_map &= ~_BV(i);
            // decrement the number left and see if we can stop
            if (--callouts_remaining == 0)
            {
                goto service_complete;
            }
        }
    }
service_complete:
    return;
}
