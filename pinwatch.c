#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <gpiolib.h>

#define DEFAULT_SAMPLE_US 2000U
#define MIN_SAMPLE_US 250U
#define MAX_SAMPLE_US 1000000U
#define DEFAULT_CONFIG_INTERVAL_MS 1000U
#define MAX_WATCHED_PINS 40U

typedef struct
{
    unsigned physical_pin;
    unsigned gpio;
    int level;
    GPIO_FSEL_T function;
    GPIO_DIR_T direction;
    GPIO_PULL_T pull;
} watched_pin_t;

static volatile sig_atomic_t running = 1;

static void handle_signal(int signal_number)
{
    (void)signal_number;
    running = 0;
}

static void usage(FILE *stream, const char *program)
{
    fprintf(stream,
            "Usage: %s [options]\n"
            "\n"
            "Passively sample Raspberry Pi header GPIO levels without claiming\n"
            "or configuring any GPIO line. JSON events are written to stdout.\n"
            "\n"
            "Options:\n"
            "  --sample-us N               Sampling interval in microseconds\n"
            "                                (default %u, range %u-%u)\n"
            "  --config-interval-ms N      Configuration recheck interval\n"
            "                                (default %u, 0 disables rechecks)\n"
            "  --pins LIST                 Physical header pins, e.g. 11,13,15-18\n"
            "                                (default: every GPIO header pin)\n"
            "  --snapshot                  Emit the initial state before changes\n"
            "  --once                      Exit after the requested snapshot\n"
            "                                (requires --snapshot)\n"
            "  --exit-on-stdin-close       Exit when the controlling pipe closes\n"
            "  --help                      Show this help\n",
            program,
            DEFAULT_SAMPLE_US,
            MIN_SAMPLE_US,
            MAX_SAMPLE_US,
            DEFAULT_CONFIG_INTERVAL_MS);
}

static bool parse_unsigned(const char *value, unsigned *result)
{
    char *end = NULL;
    unsigned long parsed;

    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (errno || !value[0] || !end || *end != '\0' || parsed > UINT32_MAX)
        return false;

    *result = (unsigned)parsed;
    return true;
}

static bool parse_pin_list(const char *value, bool selected[MAX_WATCHED_PINS + 1])
{
    char *copy = strdup(value);
    char *save = NULL;
    char *token;

    if (!copy)
        return false;

    memset(selected, 0, sizeof(bool) * (MAX_WATCHED_PINS + 1));
    for (token = strtok_r(copy, ",", &save); token; token = strtok_r(NULL, ",", &save))
    {
        char *dash = strchr(token, '-');
        unsigned first;
        unsigned last;

        if (dash)
        {
            *dash = '\0';
            if (!parse_unsigned(token, &first) || !parse_unsigned(dash + 1, &last))
            {
                free(copy);
                return false;
            }
        }
        else
        {
            if (!parse_unsigned(token, &first))
            {
                free(copy);
                return false;
            }
            last = first;
        }

        if (first < 1 || last > MAX_WATCHED_PINS || first > last)
        {
            free(copy);
            return false;
        }

        for (unsigned pin = first; pin <= last; pin++)
            selected[pin] = true;
    }

    free(copy);
    return true;
}

static uint64_t monotonic_ns(void)
{
    struct timespec value;

    if (clock_gettime(CLOCK_MONOTONIC, &value))
        return 0;
    return ((uint64_t)value.tv_sec * 1000000000ULL) + (uint64_t)value.tv_nsec;
}

static void utc_timestamp(char *buffer, size_t size)
{
    struct timespec value;
    struct tm broken_down;
    char seconds[32];

    if (clock_gettime(CLOCK_REALTIME, &value) ||
        !gmtime_r(&value.tv_sec, &broken_down) ||
        !strftime(seconds, sizeof(seconds), "%Y-%m-%dT%H:%M:%S", &broken_down))
    {
        snprintf(buffer, size, "unknown");
        return;
    }

    snprintf(buffer, size, "%s.%03ldZ", seconds, value.tv_nsec / 1000000L);
}

static const char *direction_name(GPIO_DIR_T direction)
{
    if (direction == DIR_INPUT)
        return "input";
    if (direction == DIR_OUTPUT)
        return "output";
    return "unknown";
}

static const char *safe_name(const char *value)
{
    return value ? value : "unknown";
}

static void print_pin_event(const char *event, const watched_pin_t *pin, unsigned sample_us)
{
    char timestamp[48];
    const char *function = safe_name(gpio_get_fsel_name(pin->function));
    const char *pull = safe_name(gpio_get_pull_name(pin->pull));

    utc_timestamp(timestamp, sizeof(timestamp));
    printf("{\"version\":1,\"event\":\"%s\",\"timestamp\":\"%s\","
           "\"monotonic_ns\":%llu,\"physical_pin\":%u,\"gpio\":%u,"
           "\"level\":",
           event,
           timestamp,
           (unsigned long long)monotonic_ns(),
           pin->physical_pin,
           pin->gpio);
    if (pin->level < 0)
        printf("null");
    else
        printf("%d", pin->level);
    printf(",\"direction\":\"%s\",\"pull\":\"%s\",\"function\":\"%s\","
           "\"sample_us\":%u}\n",
           direction_name(pin->direction),
           pull,
           function,
           sample_us);
}

static void read_pin(watched_pin_t *pin)
{
    pin->level = gpio_get_level(pin->gpio);
    pin->function = gpio_get_fsel(pin->gpio);
    pin->direction = gpio_get_dir(pin->gpio);
    pin->pull = gpio_get_pull(pin->gpio);
}

static bool stdin_closed(void)
{
    struct pollfd descriptor = {
        .fd = STDIN_FILENO,
        .events = POLLIN | POLLHUP | POLLERR,
        .revents = 0,
    };
    int result = poll(&descriptor, 1, 0);

    if (result < 0)
        return errno != EINTR;
    return result > 0 && (descriptor.revents & (POLLHUP | POLLERR | POLLNVAL));
}

static void sleep_for_sample(unsigned sample_us)
{
    struct timespec requested = {
        .tv_sec = sample_us / 1000000U,
        .tv_nsec = (long)(sample_us % 1000000U) * 1000L,
    };

    while (running && nanosleep(&requested, &requested) && errno == EINTR)
        ;
}

int main(int argc, char **argv)
{
    unsigned sample_us = DEFAULT_SAMPLE_US;
    unsigned config_interval_ms = DEFAULT_CONFIG_INTERVAL_MS;
    bool selected[MAX_WATCHED_PINS + 1] = {false};
    bool pins_supplied = false;
    bool snapshot = false;
    bool once = false;
    bool exit_on_stdin_close = false;
    watched_pin_t pins[MAX_WATCHED_PINS];
    size_t pin_count = 0;
    uint64_t next_config_check_ns;
    unsigned first_header_pin;
    unsigned last_header_pin;
    int option;
    int result;
    static const struct option options[] = {
        {"sample-us", required_argument, NULL, 's'},
        {"config-interval-ms", required_argument, NULL, 'c'},
        {"pins", required_argument, NULL, 'p'},
        {"snapshot", no_argument, NULL, 'S'},
        {"once", no_argument, NULL, 'o'},
        {"exit-on-stdin-close", no_argument, NULL, 'e'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0},
    };

    while ((option = getopt_long(argc, argv, "s:c:p:Soeh", options, NULL)) != -1)
    {
        switch (option)
        {
        case 's':
            if (!parse_unsigned(optarg, &sample_us) ||
                sample_us < MIN_SAMPLE_US || sample_us > MAX_SAMPLE_US)
            {
                fprintf(stderr, "--sample-us must be between %u and %u\n",
                        MIN_SAMPLE_US, MAX_SAMPLE_US);
                return 2;
            }
            break;
        case 'c':
            if (!parse_unsigned(optarg, &config_interval_ms) || config_interval_ms > 60000U)
            {
                fprintf(stderr, "--config-interval-ms must be between 0 and 60000\n");
                return 2;
            }
            break;
        case 'p':
            if (!parse_pin_list(optarg, selected))
            {
                fprintf(stderr, "--pins must contain physical pins from 1 through 40\n");
                return 2;
            }
            pins_supplied = true;
            break;
        case 'S':
            snapshot = true;
            break;
        case 'o':
            once = true;
            break;
        case 'e':
            exit_on_stdin_close = true;
            break;
        case 'h':
            usage(stdout, argv[0]);
            return 0;
        default:
            usage(stderr, argv[0]);
            return 2;
        }
    }

    if (optind != argc)
    {
        usage(stderr, argv[0]);
        return 2;
    }
    if (once && !snapshot)
    {
        fprintf(stderr, "--once requires --snapshot\n");
        return 2;
    }

    setvbuf(stdout, NULL, _IOLBF, 0);

    result = gpiolib_init();
    if (result < 0)
    {
        fprintf(stderr, "Failed to initialize Raspberry Pi gpiolib\n");
        return 1;
    }

    gpio_get_pin_range(&first_header_pin, &last_header_pin);
    if (first_header_pin == GPIO_INVALID)
    {
        fprintf(stderr, "No physical header pin mapping was found\n");
        return 1;
    }

    result = gpiolib_mmap();
    if (result)
    {
        fprintf(stderr, "Failed to map Raspberry Pi GPIO registers: %s\n", strerror(result));
        return 1;
    }

    for (unsigned physical_pin = first_header_pin;
         physical_pin <= last_header_pin && physical_pin <= MAX_WATCHED_PINS;
         physical_pin++)
    {
        unsigned gpio;

        if (pins_supplied && !selected[physical_pin])
            continue;
        gpio = gpio_for_pin((int)physical_pin);
        if (!gpio_num_is_valid(gpio))
            continue;
        pins[pin_count].physical_pin = physical_pin;
        pins[pin_count].gpio = gpio;
        read_pin(&pins[pin_count]);
        if (snapshot)
            print_pin_event("pin_snapshot", &pins[pin_count], sample_us);
        pin_count++;
    }

    if (!pin_count)
    {
        fprintf(stderr, "No GPIO header pins matched the selection\n");
        return 1;
    }

    if (once)
        return 0;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGHUP, handle_signal);
    next_config_check_ns = monotonic_ns() + ((uint64_t)config_interval_ms * 1000000ULL);

    while (running)
    {
        bool check_configuration = config_interval_ms && monotonic_ns() >= next_config_check_ns;

        if (exit_on_stdin_close && stdin_closed())
            break;

        for (size_t index = 0; index < pin_count; index++)
        {
            watched_pin_t current = pins[index];

            current.level = gpio_get_level(current.gpio);
            if (current.level != pins[index].level)
            {
                pins[index].level = current.level;
                print_pin_event("pin_level_changed", &pins[index], sample_us);
            }

            if (check_configuration)
            {
                current.function = gpio_get_fsel(current.gpio);
                current.direction = gpio_get_dir(current.gpio);
                current.pull = gpio_get_pull(current.gpio);
                if (current.function != pins[index].function ||
                    current.direction != pins[index].direction ||
                    current.pull != pins[index].pull)
                {
                    pins[index] = current;
                    print_pin_event("pin_configuration_changed", &pins[index], sample_us);
                }
            }
        }

        if (check_configuration)
            next_config_check_ns = monotonic_ns() + ((uint64_t)config_interval_ms * 1000000ULL);
        sleep_for_sample(sample_us);
    }

    return 0;
}
