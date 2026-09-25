#ifndef TEST_GPIOLIB_H
#define TEST_GPIOLIB_H

#include <stdbool.h>
#include <stdint.h>

#define GPIO_INVALID UINT32_MAX

typedef enum
{
    GPIO_FSEL_INPUT = 0,
    GPIO_FSEL_OUTPUT = 1,
} GPIO_FSEL_T;

typedef enum
{
    DIR_INPUT = 0,
    DIR_OUTPUT = 1,
} GPIO_DIR_T;

typedef enum
{
    PULL_NONE = 0,
    PULL_UP = 1,
    PULL_DOWN = 2,
} GPIO_PULL_T;

int gpiolib_init(void);
int gpiolib_mmap(void);
void gpio_get_pin_range(unsigned *first, unsigned *last);
unsigned gpio_for_pin(int physical_pin);
bool gpio_num_is_valid(unsigned gpio);
int gpio_get_level(unsigned gpio);
GPIO_FSEL_T gpio_get_fsel(unsigned gpio);
GPIO_DIR_T gpio_get_dir(unsigned gpio);
GPIO_PULL_T gpio_get_pull(unsigned gpio);
const char *gpio_get_fsel_name(GPIO_FSEL_T function);
const char *gpio_get_pull_name(GPIO_PULL_T pull);

#endif
