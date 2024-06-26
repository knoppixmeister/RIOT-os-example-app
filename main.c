#include "architecture.h"
#include "thread.h"
#include "xtimer.h"
#include "ztimer.h"

#include "uuid.h"

#include "clk.h"

#include <stdio.h>
#include <string.h>

#include "shell.h"

char threadA_stack [THREAD_STACKSIZE_MAIN];
char threadB_stack [THREAD_STACKSIZE_MAIN];

void *threadA_func(void *arg);
void *threadB_func(void *arg);

kernel_pid_t pid, pid2;

int msgSend(int argc, char **argv)
{
    msg_t m;
    m.content.value = "bla ... blah ...";
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

const shell_command_t commands[] = {
    {"msg", "send message to the secondary thread", msgSend},
    {"uuid", "Generate UUID", uuidGen},
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
        "thread A"
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

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}

void *threadA_func(void *arg)
{
    (void) arg;

    while (1) {
        if (IS_USED(MODULE_XTIMER)) {
            // ztimer_sleep(ZTIMER_USEC, 1 * US_PER_SEC);
        }

        #ifdef LED0_TOGGLE
            LED0_TOGGLE;

            // LED1_TOGGLE;
            // LED2_TOGGLE;
            // LED3_TOGGLE;
        #else
            puts("Blink! (No LED present or configured...)");
        #endif

        xtimer_msleep(500);
    }

    return NULL;
}

void *threadB_func(void *arg)
{
    (void) arg;
    msg_t m;

    while (1) {
        msg_receive(&m);
        printf("Got msg from %" PRIkernel_pid " with data %s\n", m.sender_pid, m.content.value);
    }

    return NULL;
}

