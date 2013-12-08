#if __APPLE__
// In Mac OS X 10.5 and later trying to use the daemon function gives a “‘daemon’ is deprecated”
// error, which prevents compilation because we build with "-Werror".
// Since this is supposed to be portable cross-platform code, we don't care that daemon is
// deprecated on Mac OS X 10.5, so we use this preprocessor trick to eliminate the error message.
#define daemon yes_we_know_that_daemon_is_deprecated_in_os_x_10_5_thankyou
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <stdarg.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <pwd.h>

#if __APPLE__
#undef daemon
extern int daemon(int, int);
#endif

#include <mosquitto.h>
#include <libsureelec.h>

#include "ini.h"
#include "config.h"

#ifdef __GLIBC__
#define POSSIBLY_UNUSED __attribute__((unused))
#else
#define POSSIBLY_UNUSED
#endif

#define INI_NAME ".mqttdisplay"

int STOPPING = 0;
int VERBOSE = 0;
int BRIGHTNESS = 128;

struct md_config {
	char *topic;
	char *display;
	char *broker;
	long broker_port;
	int foreground;
};

static struct md_config DEFAULTS = {
	"mqttdisplay/#",
	"/dev/tty.SLAB_USBtoUART",
	"localhost",
	1883,
	0
};

static struct option OPTIONS[] = {
	{ "host", required_argument, 0, 'h' },
	{ "display", required_argument, 0, 'd' },
	{ "port", required_argument, 0, 'p' },
	{ "topic", required_argument, 0, 't' },
	{ "verbose", no_argument, 0, 'v' },
	{ "foreground", no_argument, 0, 'f' },
	{ 0, 0, 0, 0 }
};

static void debug_print(const char *format, ...) {
	if (VERBOSE) {
		va_list ap;
		va_start(ap, format); /* measure the required size (the number of elements of format) */

		fprintf(stderr, "mqttdisplay: ");
		vfprintf(stderr, format, ap);
		fprintf(stderr, "\n");

		va_end(ap);
	}
}

static char *strerror_wrapper(int err) {
	char *buf = calloc(256, sizeof(char));
	POSSIBLY_UNUSED char *bbuf = buf;
#ifdef STRERROR_R_CHAR_P
	bbuf =
#endif
		strerror_r(err, buf, 256);
	return bbuf;
}


void handle_errno(int retval, int err) {
	const char *message = NULL;

	switch (retval) {
		case MOSQ_ERR_SUCCESS:
			return;

		case MOSQ_ERR_ERRNO:
			message = strerror_wrapper(err);
			break;

		default:
			message = mosquitto_strerror(err);
			break;
	}

	if (message) {
		fprintf(stderr, "%s\n", message);
		exit(EXIT_FAILURE);
	}
}

static char *wrap_text(const char *buffer, size_t width, size_t min_lines) {
	size_t count, buflen;
	int lines = 0;
	const char *ptr, *endptr;
	char result[32 * sizeof(char) * width];
	char *result_ptr = result, *chopped_result = NULL;

	count = 0;
	buflen = strlen(buffer);
	memset(result, ' ', 32 * sizeof(char) * width);

	do {
		ptr = buffer + count;

		/* don't set endptr beyond the end of the buffer */
		if (ptr - buffer + width <= buflen)
			endptr = ptr + width;
		else
			endptr = buffer + buflen;

		/* back up EOL to a null terminator or space */
		while (*(endptr) && !isspace(*(endptr)) )
			endptr--;

		if (isspace(*(endptr))) {
			endptr++;
		}

		strncpy(result_ptr, ptr, (endptr - ptr));
		count += endptr - ptr;
		lines++;

		result_ptr = result_ptr + width;
	} while (*endptr);

	if (lines < min_lines) {
		lines = min_lines;
	}

	chopped_result = calloc(lines * width, sizeof(char));
	memcpy(chopped_result, result, lines * width);
	return chopped_result;
}

static void message_callback(struct mosquitto *client, void *obj, const struct mosquitto_message *message) {
	char *wrapped_text;

	libsureelec_ctx *display = (libsureelec_ctx *) obj;
	debug_print("Got message: %s", message->payload);

	if (message->payloadlen > 0) {
		wrapped_text = wrap_text(message->payload, display->device_info.width, display->device_info.height);

		memcpy(display->framebuffer, wrapped_text, display->device_info.width * display->device_info.height);
		BRIGHTNESS = 254;
		alarm(10);
		libsureelec_refresh(display);
	} else {
		libsureelec_clear_display(display);
	}
}

static void interrupt_handler(int signal) {
	switch (signal) {
		case SIGINT:
			debug_print("Caught signal - stopping\n");
			STOPPING = 1;
			break;
		case SIGALRM:
			debug_print("Caught alarm signal - dimming display\n");
			BRIGHTNESS = 128;
			break;
		default:
			debug_print("Caught unknown signal\n");
			break;
	}
}

static struct mosquitto *init_mosquitto(const char *broker, long port, const char *topic, libsureelec_ctx *display) {
	struct mosquitto *client = NULL;
	int retval = 0;

	mosquitto_lib_init();
	client = mosquitto_new("mqttdisplay", 1, display);

	retval = mosquitto_connect(client, broker, port, 10);
	handle_errno(retval, errno);

	mosquitto_message_callback_set(client, message_callback);

	mosquitto_subscribe(client, NULL, topic, 0);
	handle_errno(retval, errno);

	return client;
}

static libsureelec_ctx *init_display(const char *device, int verbose) {
	libsureelec_ctx *display = libsureelec_create(device, verbose);

	if (!display) {
		fprintf(stderr, "Failed to initialize display");
		exit(EXIT_FAILURE);
	}

	libsureelec_clear_display(display);
	libsureelec_set_contrast(display, 1);
	libsureelec_set_brightness(display, 128);

	return display;
}

static int ini_handler(void *userdata, const char *section, const char *name, const char *value) {
	struct md_config *config = (struct md_config *) userdata;

	#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
	if (MATCH("mqttdisplay", "broker")) {
		config->broker = strdup(value);
	} else if (MATCH("mqttdisplay", "port")) {
		config->broker_port = strtol(value, NULL, 10);
	} else if (MATCH("mqttdisplay", "display")) {
		config->display = strdup(value);
	} else if (MATCH("mqttdisplay", "topic")) {
		config->topic = strdup(value);
	} else if (MATCH("mqttdisplay", "verbose")) {
		VERBOSE = strtol(value, NULL, 10) ? 1 : 0;
	} else if (MATCH("mqttdisplay", "foreground")) {
		config->foreground = strtol(value, NULL, 10) ? 1 : 0;
	}

	return 1;
}

static int read_config(const char *path, struct md_config *config) {
	const char *message;

	debug_print("Loading INI file %s\n", path);
	if (access(path, R_OK) < 0) {
		message = strerror_wrapper(errno);
		debug_print("Could not access INI file %s: %s\n", path, message);
		return -1;
	}

	if (ini_parse(path, ini_handler, config) < 0) {
		debug_print("Could not parse INI file %s\n", path);
		return 0;
	}

	return 1;
}

static int read_arguments(int argc, char **argv, struct md_config *config) {
	int opt = 0, index = 0;

	while (opt != -1) {
		opt = getopt_long(argc, argv, "h:d:p:t:vf", OPTIONS, &index);

		switch (opt) {
			case 'f':
				config->foreground = 1;
				break;

			case 'h':
				config->broker = strdup(optarg);
				break;

			case 'd':
				config->display = strdup(optarg);
				break;

			case 't':
				config->topic = strdup(optarg);
				break;

			case 'p':
				config->broker_port = strtol(optarg, NULL, 10);
				break;

			case 'v':
				VERBOSE = 1;
				break;
		}
	}

	return 1;
}

static char *get_home_directory(void) {
	const char *dir;
	struct passwd *pwd;

	dir = getenv("HOME");
	if (!dir) {
		pwd = getpwuid(getuid());

		if (pwd) {
			dir = pwd->pw_dir;
		}
	}

	return strdup(dir);
}

int main(int argc, char **argv) {
	struct md_config config = DEFAULTS;
	struct mosquitto *client = NULL;
	libsureelec_ctx *display = NULL;
	char ini_path[256];
	char *home_dir = NULL;
	int i, brightness;

	/* We read the config before we read the CLI arguments, but do a quick
	 * check to see if '-v' is an argument to enable verbose mode ahead of
	 * time */

	for (i = 0; i < argc; i++) {
		if (strcmp("-v", argv[i]) == 0) {
			VERBOSE = 1;
		}
	}

	home_dir = get_home_directory();
	snprintf(ini_path, sizeof(ini_path), "%s/%s", home_dir, INI_NAME);

	read_config(ini_path, &config);
	read_arguments(argc, argv, &config);

	if (signal(SIGINT, interrupt_handler) == SIG_ERR) {
		fprintf(stderr, "Failed to install signal handler\n");
		exit(EXIT_FAILURE);
	}

	if (signal(SIGALRM, interrupt_handler) == SIG_ERR) {
		fprintf(stderr, "Failed to install signal handler\n");
		exit(EXIT_FAILURE);
	}

	debug_print("Starting with broker %s:%ld, topic \"%s\", and display %s",
			config.broker, config.broker_port, config.topic, config.display);

	if (config.foreground == 0) {
		if (daemon(0, 0) == -1) {
			fprintf(stderr, "Failed to daemonize\n");
			exit(EXIT_FAILURE);
		}
	}

	display = init_display(config.display, VERBOSE);

	client = init_mosquitto(config.broker, config.broker_port, config.topic, (void *) display);

	brightness = BRIGHTNESS;
	while (!STOPPING) {
		printf("Brightness is %d, %d\n", brightness, BRIGHTNESS);
		if (brightness != BRIGHTNESS) {
			brightness = BRIGHTNESS;
			libsureelec_set_brightness(display, BRIGHTNESS);
		}

		mosquitto_loop(client, 100, 1);
	}

	return 0;
}

