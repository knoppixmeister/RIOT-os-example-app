# RIOT os example app

With two threads, interactive shell console and blinking led

Instructions:

```
git clone https://github.com/RIOT-OS/RIOT.git
cd RIOT/examples

git clone https://github.com/knoppixmeister/RIOT-os-example-app.git
cd RIOT-os-example-app

make BOARD=<your_board> (e.g. https://github.com/knoppixmeister/RIOT-os-WeAct-STM32F405RG-board )
make BOARD=<your_board> flash

make BOARD=<your_board> term (optional)
```
---

Based on code:

https://github.com/RIOT-OS/RIOT/blob/master/examples/default/main.c

https://github.com/RIOT-OS/RIOT/blob/master/examples/blinky/main.c

https://github.com/RIOT-OS/RIOT/blob/master/examples/thread_duel/main.c

http://codingadventures.org/2018/11/23/riot-os-tutorial-to-use-command-handlers-and-multi-threading/

