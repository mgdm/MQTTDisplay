#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <stdarg.h>
#include <errno.h>
#include <signal.h>

#include <mosquitto.h>
#include <libsureelec.h>

#include "config.h"

#ifdef __GLIBC__
#define POSSIBLY_UNUSED __attribute__((unused))
#else
#define POSSIBLY_UNUSED
#endif

char *TOPIC = "/mqttdisplay/#";
char *DISPLAY = "/dev/tty.SLAB_USBtoUART";
char *BROKER = "localhost";
long BROKER_PORT = 1883;
long VERBOSE = 0;
long STOPPING = 0;

static struct option OPTIONS[] = {
	{ "broker", required_argument, 0, 'b' },
	{ "display", required_argument, 0, 'd' },
	{ "port", required_argument, 0, 'p' },
	{ "topic", required_argument, 0, 't' },
	{ "verbose", no_argument, 0, 'v' },
	{ 0, 0, 0, 0 }
};

static void debug_print(const char *format, ...) {
	if (VERBOSE) {
		va_list ap;
		va_start(ap, format); /* measure the required size (the number of elements of format) */

		fprintf(stderr, "mqttdisplay: ");
		vfprintf(stderr, format, ap);
		fprintf(stderr, "\n");
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
	char *message = NULL;

	switch (retval) {
		case MOSQ_ERR_SUCCESS:
		default:
			return;

		case MOSQ_ERR_INVAL:
			message = strdup("Invalid input parameter");
			break;

		case MOSQ_ERR_NOMEM:
			message = strdup("Insufficient memory");
			break;

		case MOSQ_ERR_NO_CONN:
			message = strdup("The client is not connected to a broker");
			break;

		case MOSQ_ERR_CONN_LOST:
			message = strdup("Connection lost");
			break;

		case MOSQ_ERR_PROTOCOL:
			message = strdup("There was a protocol error communicating with the broker.");
			break;

		case MOSQ_ERR_PAYLOAD_SIZE:
			message = strdup("Payload is too large");
			break;

		case MOSQ_ERR_ERRNO:
			message = strerror_wrapper(err);
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

	wrapped_text = wrap_text(message->payload, display->device_info.width, display->device_info.height);
	memcpy(display->framebuffer, wrapped_text, display->device_info.width * display->device_info.height);
	libsureelec_refresh(display);
}

static void interrupt_handler(int signal) {
	switch (signal) {
		case SIGINT:
			debug_print("Caught signal - stopping\n");
			STOPPING = 1;
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

static libsureelec_ctx *init_display(const char *device) {
	libsureelec_ctx *display = libsureelec_create(device, VERBOSE);
	
	if (!display) {
		fprintf(stderr, "Failed to initialize display");
		exit(EXIT_FAILURE);
	}

	libsureelec_clear_display(display);
	libsureelec_set_contrast(display, 1);
	libsureelec_set_brightness(display, 254);

	return display;
}

int main(int argc, char **argv) {
	int opt = 0, index = 0;
	struct mosquitto *client = NULL;
	libsureelec_ctx *display = NULL;

	while (opt != -1) {
		opt = getopt_long(argc, argv, "b:d:p:t:v", OPTIONS, &index);

		switch (opt) {
			case 'b':
				BROKER = strdup(optarg);
				break;

			case 'd':
				DISPLAY = strdup(optarg);
				break;

			case 'p':
				BROKER_PORT = strtol(optarg, NULL, 10);
				break;

			case 'v':
				VERBOSE = 1;
				break;
		}
	}

	if (signal(SIGINT, interrupt_handler) == SIG_ERR) {
		fprintf(stderr, "Failed to install signal handler\n");
		exit(EXIT_FAILURE);
	}

	debug_print("Starting with broker %s:%ld and display %s", 
			BROKER, BROKER_PORT, DISPLAY);

	display = init_display(DISPLAY);

	client = init_mosquitto(BROKER, BROKER_PORT, TOPIC, (void *) display);

	while (!STOPPING) {
		mosquitto_loop(client, 100, 1);
	}

	return 0;
}

