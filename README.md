# faketouch
Linux kernel module simulating touch sensor with the spacebar key

# Why ?
Sometimes ago I had some work to do with a GPIO-based proximity sensor
but I had forgotten to take the sensor and the board with me, so I hacked
this thing up simulating the `/sys/class/gpio/gpioN/value` interface

# How does it work ?
Compile and insert the module

```
$ make && sudo insmod ./faketouch.ko
```

If everything went alright, there should be a folder under `/sys/kernel/debug/faketouch`.
It supports polling and reading.
