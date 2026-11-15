#define _POSIX_C_SOURCE 200809L

#include <mosquitto.h>

#include <math.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CLIENT_ID "sensor-01"
#define TELEMETRY_TOPIC "home/livingroom/sensor-01/telemetry"
#define STATUS_TOPIC "home/livingroom/sensor-01/status"
#define SAMPLE_INTERVAL_SECONDS 2

static atomic_bool broker_connected = ATOMIC_VAR_INIT(false);
static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int signal_number)
{
	(void)signal_number;
	keep_running = 0;
}

static int publish_status(struct mosquitto *mosq, const char *status)
{
	return mosquitto_publish(mosq, NULL, STATUS_TOPIC, (int)strlen(status),
			status, 1, true);
}

static void on_connect(struct mosquitto *mosq, void *userdata, int result)
{
	(void)userdata;
	if(result == 0){
		atomic_store(&broker_connected, true);
		(void)publish_status(mosq, "online");
		fprintf(stdout, "Connected to broker; device status is online.\n");
	}else{
		atomic_store(&broker_connected, false);
		fprintf(stderr, "Broker rejected connection: %s\n",
			mosquitto_connack_string(result));
	}
}

static void on_disconnect(struct mosquitto *mosq, void *userdata, int result)
{
	(void)mosq;
	(void)userdata;
	atomic_store(&broker_connected, false);
	if(result != 0){
		fprintf(stderr, "Disconnected from broker; reconnect will be attempted.\n");
	}
}

static void publish_sample(struct mosquitto *mosq, unsigned long sample,
		const char *mode)
{
	double temperature;
	double humidity;
	char payload[192];
	int length;
	int rc;
	const double phase = (double)sample;

	if(strcmp(mode, "alert") == 0){
		temperature = 33.4;
		humidity = 78.0;
	}else{
		temperature = 25.0 + 8.0 * sin(phase * 0.18);
		humidity = 58.0 + 22.0 * cos(phase * 0.11);
	}

	length = snprintf(payload, sizeof(payload),
		"{\"deviceId\":\"%s\",\"timestamp\":%lld,"
		"\"temperature\":%.1f,\"humidity\":%.1f}",
		CLIENT_ID, (long long)time(NULL), temperature, humidity);
	if(length < 0 || (size_t)length >= sizeof(payload)){
		fprintf(stderr, "Could not encode telemetry payload.\n");
		return;
	}

	rc = mosquitto_publish(mosq, NULL, TELEMETRY_TOPIC, length, payload, 1, true);
	if(rc == MOSQ_ERR_SUCCESS){
		fprintf(stdout, "Published %s\n", payload);
	}else{
		fprintf(stderr, "Publish failed: %s\n", mosquitto_strerror(rc));
	}
}

int main(void)
{
	const char *host = getenv("MQTT_HOST");
	const char *port_text = getenv("MQTT_PORT");
	const char *mode = getenv("SENSOR_MODE");
	char *end = NULL;
	long port = port_text == NULL ? 1883 : strtol(port_text, &end, 10);
	struct mosquitto *mosq;
	unsigned long sample = 0;
	int rc;

	if(host == NULL || host[0] == '\0') host = "127.0.0.1";
	if(mode == NULL || mode[0] == '\0') mode = "normal";
	if(port_text != NULL && (end == port_text || *end != '\0' || port < 1 || port > 65535)){
		fprintf(stderr, "MQTT_PORT must be between 1 and 65535.\n");
		return EXIT_FAILURE;
	}
	if(strcmp(mode, "normal") != 0 && strcmp(mode, "alert") != 0){
		fprintf(stderr, "SENSOR_MODE must be 'normal' or 'alert'.\n");
		return EXIT_FAILURE;
	}

	if(mosquitto_lib_init() != MOSQ_ERR_SUCCESS){
		fprintf(stderr, "Could not initialize libmosquitto.\n");
		return EXIT_FAILURE;
	}

	mosq = mosquitto_new(CLIENT_ID, true, NULL);
	if(mosq == NULL){
		fprintf(stderr, "Could not create MQTT client.\n");
		mosquitto_lib_cleanup();
		return EXIT_FAILURE;
	}

	rc = mosquitto_will_set(mosq, STATUS_TOPIC, 7, "offline", 1, true);
	if(rc != MOSQ_ERR_SUCCESS){
		fprintf(stderr, "Could not configure MQTT will: %s\n", mosquitto_strerror(rc));
		mosquitto_destroy(mosq);
		mosquitto_lib_cleanup();
		return EXIT_FAILURE;
	}

	mosquitto_connect_callback_set(mosq, on_connect);
	mosquitto_disconnect_callback_set(mosq, on_disconnect);

	rc = mosquitto_connect_async(mosq, host, (int)port, 10);
	if(rc != MOSQ_ERR_SUCCESS){
		fprintf(stderr, "Could not start broker connection: %s\n", mosquitto_strerror(rc));
		mosquitto_destroy(mosq);
		mosquitto_lib_cleanup();
		return EXIT_FAILURE;
	}

	rc = mosquitto_loop_start(mosq);
	if(rc != MOSQ_ERR_SUCCESS){
		fprintf(stderr, "Could not start MQTT network loop: %s\n", mosquitto_strerror(rc));
		mosquitto_destroy(mosq);
		mosquitto_lib_cleanup();
		return EXIT_FAILURE;
	}

	(void)signal(SIGINT, handle_signal);
	(void)signal(SIGTERM, handle_signal);
	fprintf(stdout, "Sensor started in '%s' mode; publishing every %d seconds.\n",
		mode, SAMPLE_INTERVAL_SECONDS);

	while(keep_running){
		if(atomic_load(&broker_connected)){
			publish_sample(mosq, sample++, mode);
		}
		(void)sleep(SAMPLE_INTERVAL_SECONDS);
	}

	if(atomic_load(&broker_connected)){
		(void)publish_status(mosq, "offline");
		(void)sleep(1);
		(void)mosquitto_disconnect(mosq);
	}
	(void)mosquitto_loop_stop(mosq, true);
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
	return EXIT_SUCCESS;
}

