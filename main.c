#include "architecture.h"
#include "thread.h"
#include "xtimer.h"
#include "ztimer.h"

#include "uuid.h"

#include "clk.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "shell.h"

#include "micropython.h"
#include "py/stackctrl.h"
#include "py/builtin.h"
#include "py/compile.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "lib/utils/pyexec.h"

#include "blob/boot.py.h"

#include "progress_bar.h"

#include "board.h"
#include "periph/gpio.h"
#include "periph_conf.h"

#define LD_MSK (1 << LED0_PIN_NUM)

#define TEST_FLANK      GPIO_FALLING

char threadA_stack [THREAD_STACKSIZE_MAIN];
char threadB_stack [THREAD_STACKSIZE_MAIN];
char threadC_stack [THREAD_STACKSIZE_MAIN];

void *threadA_func(void *arg);
void *threadB_func(void *arg);
void *threadC_func(void *arg);

/*
    DON'T change for ODR bit check as in some moments blinking LED could be disabled
    and it looks like it not enabled (not blinking)
*/
int ledAlreadyBlinking = 0;
int stopLedBlinking = 0;

kernel_pid_t pid, pid2, pid3;

static char mp_heap[MP_RIOT_HEAPSIZE];

int btnsCnt = 0;
int ledsCnt = 0;

// leave it here just for future purposes
int isNthBitSet(int value, int nthBit)
{
    if (value & (1 << nthBit)) return 1; // bit is set
    else return 0; // bit is not set
}

int msgSend(int argc, char **argv)
{
    msg_t m;
    m.content.ptr = "blah ... blah ...";
    msg_send(&m, pid2);

    return 0;
}

int uuidGen(int argc, char **argv)
{
    uuid_t uuid;
    char res[100];

    uuid_v4(&uuid);
    uuid_to_string(&uuid, &res);

    printf("UU: %s\n", res);

    return 0;
}

int ledBlinking(int argc, char **argv) {
    if(argc < 2) {
        puts("usage: blink <command>");
        puts("commands");
        puts("\ton\tenable LED blinking");
        puts("\toff\tdisable LED blinking");

        return 1;
    }
    else if(strncmp(argv[1], "on", 2) == 0) {
        if(ledAlreadyBlinking == 1) {
            puts("LED already blinking");
            return 0;
        }

        puts("Enable LED");

        stopLedBlinking = 0;
        ledAlreadyBlinking = 1;

        msg_t m;
        m.content.value = 1;
        msg_send(&m, pid);
    }
    else if(strncmp(argv[1], "off", 3) == 0) {
        if(ledAlreadyBlinking == 0) {
            puts("LED already switched off");
            return 0;
        }

        puts("Disable LED");

        stopLedBlinking = 1;
    }
    else {
        return 1;
    }

    return 0;
}

int e() {
    // on stm32f405 its correct sequence
    // on stm32f411ce on disable should be written 0 instead of 1
    // LED0_PORT->ODR = LD_MSK;

    /*
    #ifdef BOARD_WEACT_F411CE
        LED0_PORT->ODR = (0 << LED0_PIN_NUM);
    #else
        LED0_PORT->ODR = (1 << LED0_PIN_NUM);
    #endif
    */

    // or better
    #ifdef LED0_ON
        LED0_ON;
    #endif

    return 0;
}

int d() {
    // on stm32f405 its correct sequence
    // on stm32f411ce on disable should be written 1 instead of 0

    /*
    #ifdef BOARD_WEACT_F411CE
        LED0_PORT->ODR = (1 << LED0_PIN_NUM);
    #else
        LED0_PORT->ODR = (0 << LED0_PIN_NUM);
    #endif
    */

    // or better
    #ifdef LED0_OFF
        LED0_OFF;
    #endif

    return 0;
}

int mpyRun()
{
    msg_t m;

    m.content.value = 1;
    msg_send(&m, pid3);

    return 0;
}

// test function. Not really usable yet
int mpy2Run()
{
    uint32_t stack_dummy;
    mp_stack_set_top((char*)&stack_dummy);

    mp_stack_set_limit(THREAD_STACKSIZE_MAIN - MP_STACK_SAFEAREA);

    mp_riot_init(mp_heap, sizeof(mp_heap));

    while (1) {
        if (pyexec_mode_kind == PYEXEC_MODE_RAW_REPL) {
            if (pyexec_raw_repl() != 0) break;
        }
        else {
            if (pyexec_friendly_repl() != 0) break;
        }
    }

    puts("MPY exit");

    return 0;
}

int pbTest()
{
    for(int i=0; i<=100; i++) {
        progress_bar_print("", "", i);

        xtimer_msleep(100);
    }

    puts("");

    // progress_bar_print("", "", 50);

    return 0;
}

int btnStats()
{
    puts("On-board button test\n");

    /* cppcheck-suppress knownConditionTrueFalse
     * rationale: board-dependent ifdefs */

    if (btnsCnt == 0) {
        puts("[FAILED] no buttons available!");
    }
    else {
        printf(" -- Available buttons: %i\n\n", btnsCnt);
        puts(" -- Try pressing buttons to test.\n");
        puts("[SUCCESS]");
    }

    return 0;
}

int ledStats()
{
    puts("On-board LED test\n");

    if (ledsCnt == 0) {
        puts("NO LEDs AVAILABLE");
    }
    else {
        printf("Available LEDs: %i\n", ledsCnt);
    }

    return 0;
}

const shell_command_t commands[] = {
    {"msg", "send message to the secondary thread", msgSend},
    {"uuid", "Generate UUID", uuidGen},
    {"blink", "Controls LED blinking", ledBlinking},

    {"b", "Stats of how many buttons available", btnStats},

    {"e", "Enable LED", e},
    {"d", "Disable LED", d},

    {"l", "LED stats", ledStats},

    {"py", "MicroPython test run", mpyRun},

    {"pb", "Progress bar test", pbTest},

    // {"p", "TEST MicroPython run as command line interpreter", mpy2Run},

    {NULL, NULL, NULL}
};

static void cb(void *arg)
{
    printf("Pressed BTN%d\n", (int)arg);
}

int main(void)
{
    puts("Welcome to RIOT!");

    pid = thread_create(
        threadA_stack,
        sizeof(threadA_stack),
        THREAD_PRIORITY_MAIN - 1,
        THREAD_CREATE_STACKTEST,
        threadA_func,
        NULL,
        "thread LEDs"
    );

    pid2 = thread_create(
        threadB_stack,
        sizeof(threadB_stack),
        THREAD_PRIORITY_MAIN - 1,
        THREAD_CREATE_STACKTEST,
        threadB_func,
        NULL,
        "thread Msgs"
    );

    pid3 = thread_create(
        threadC_stack,
        sizeof(threadC_stack),
        THREAD_PRIORITY_MAIN - 1,
        THREAD_CREATE_STACKTEST,
        threadC_func,
        NULL,
        "thread MicroPython"
    );

// -----------------------------------------------------------------------------

    // LEDs code: https://github.com/RIOT-OS/RIOT/blob/master/tests/leds/main.c

    #ifdef LED0_ON
        ++ledsCnt;
        LED0_OFF;
    #endif

// -----------------------------------------------------------------------------

    // BUTTONs handling code from: https://github.com/RIOT-OS/RIOT/tree/master/tests/buttons

    /* get the number of available buttons and init interrupt handler */
#ifdef BTN0_PIN
    if (gpio_init_int(BTN0_PIN, BTN0_MODE, TEST_FLANK, cb, (void *)btnsCnt) < 0) {
        puts("[FAILED] init BTN0!");
        return 1;
    }
    ++btnsCnt;
#endif


    /*
#ifdef BTN1_PIN
    if (gpio_init_int(BTN1_PIN, BTN1_MODE, TEST_FLANK, cb, (void *)cnt) < 0) {
        puts("[FAILED] init BTN1!");
        return 1;
    }
    ++cnt;
#endif

#ifdef BTN2_PIN
    if (gpio_init_int(BTN2_PIN, BTN2_MODE, TEST_FLANK, cb, (void *)cnt) < 0) {
        puts("[FAILED] init BTN2!");
        return 1;
    }
    ++cnt;
#endif

#ifdef BTN3_PIN
    if (gpio_init_int(BTN3_PIN, BTN3_MODE, TEST_FLANK, cb, (void *)cnt) < 0) {
        puts("[FAILED] init BTN3!");
        return 1;
    }
    ++cnt;
#endif
    */

// -----------------------------------------------------------------------------

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    // shell_run_once(commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    // mpy2Run();

    // puts("AFTER shell_run");

    return 0;
}

void *threadA_func(void *arg)
{
    (void) arg;
    msg_t m;

    while(1) {
        msg_receive(&m);

        if (m.content.value == 1) {
            while (1) {
                if (IS_USED(MODULE_XTIMER)) {
                    // ztimer_sleep(ZTIMER_USEC, 1 * US_PER_SEC);
                }

                // LED0_PORT->ODR ^= LD_MSK;

                #ifdef LED0_TOGGLE
                    LED0_TOGGLE;

                    // LED1_TOGGLE;
                    // LED2_TOGGLE;
                    // LED3_TOGGLE;
                #else
                    puts("Blink! (No LED present or configured...)");
                #endif

                if(stopLedBlinking == 1) {
                    d();
                    ledAlreadyBlinking = 0;
                    break;
                }

                xtimer_msleep(500);
            }
        }
    }

    return NULL;
}

void *threadB_func(void *arg)
{
    (void) arg;
    msg_t m;

    while (1) {
        msg_receive(&m);
        printf("Got msg from %" PRIkernel_pid " with data %s\n", m.sender_pid, (char *) m.content.ptr);
    }

    return NULL;
}

void *threadC_func(void *arg)
{
    (void) arg;
    msg_t m;

    uint32_t stack_dummy;
    mp_stack_set_top((char*)&stack_dummy);

    mp_stack_set_limit(THREAD_STACKSIZE_MAIN - MP_STACK_SAFEAREA);

    mp_riot_init(mp_heap, sizeof(mp_heap));

    while (1) {
        msg_receive(&m);

        puts("-- Executing boot.py");
        mp_do_str((const char *) boot_py, boot_py_len);
        puts("-- boot.py exited.");

        /*
        while (1) {
            if (pyexec_mode_kind == PYEXEC_MODE_RAW_REPL) {
                if (pyexec_raw_repl() != 0) {
                    break;
                }
            }
            else {
                if (pyexec_friendly_repl() != 0) {
                    break;
                }
            }
        }

        puts("soft reboot");
        */
    }

    return NULL;
}
