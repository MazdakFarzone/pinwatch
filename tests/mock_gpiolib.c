#include "gpiolib.h"

int gpiolib_init(void)
{
    return 0;
}

int gpiolib_mmap(void)
{
    return 0;
}

void gpio_get_pin_range(unsigned *first, unsigned *last)
{
    *first = 1;
    *last = 3;
}

unsigned gpio_for_pin(int physical_pin)
{
    if (physical_pin == 2)
        return 2;
    if (physical_pin == 3)
        return 3;
    return GPIO_INVALID;
}

bool gpio_num_is_valid(unsigned gpio)
{
    return gpio != GPIO_INVALID;
}

int gpio_get_level(unsigned gpio)
{
    return (int)(gpio % 2U);
}

GPIO_FSEL_T gpio_get_fsel(unsigned gpio)
{
    (void)gpio;
    return GPIO_FSEL_INPUT;
}

GPIO_DIR_T gpio_get_dir(unsigned gpio)
{
    (void)gpio;
    return DIR_INPUT;
}

GPIO_PULL_T gpio_get_pull(unsigned gpio)
{
    (void)gpio;
    return PULL_NONE;
}

const char *gpio_get_fsel_name(GPIO_FSEL_T function)
{
    (void)function;
    return "input";
}

const char *gpio_get_pull_name(GPIO_PULL_T pull)
{
    (void)pull;
    return "none";
}
