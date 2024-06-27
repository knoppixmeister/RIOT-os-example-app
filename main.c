#include "architecture.h"
#include "thread.h"
#include "xtimer.h"
#include "ztimer.h"

#include "uuid.h"

#include "clk.h"

#include <stdio.h>
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

#define LD_MSK (1 << LED0_PIN_NUM)

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
    LED0_PORT->ODR = LD_MSK;

    return 0;
}

int d() {
    LED0_PORT->ODR = (0 << LED0_PIN_NUM);

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

const shell_command_t commands[] = {
    {"msg", "send message to the secondary thread", msgSend},
    {"uuid", "Generate UUID", uuidGen},
    {"blink", "Controls LED blinking", ledBlinking},

    {"e", "Enable LED", e},
    {"d", "Disable LED", d},

    {"py", "MicroPython test run", mpyRun},

    {"pb", "Progress bar test", pbTest},

    // {"p", "TEST MicroPython run as command line interpreter", mpy2Run},

    {NULL, NULL, NULL}
};

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

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    // shell_run_once(commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    puts("AFTER shell_run");

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

                LED0_PORT->ODR ^= LD_MSK;

                #ifdef LED0_TOGGLE
                    // LED0_TOGGLE;

                    // LED1_TOGGLE;
                    // LED2_TOGGLE;
                    // LED3_TOGGLE;
                #else
                    puts("Blink! (No LED present or configured...)");
                #endif

                if(stopLedBlinking == 1) {
                    LED0_PORT->ODR = (0 << LED0_PIN_NUM);
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
