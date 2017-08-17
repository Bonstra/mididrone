#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <arpa/inet.h>

#define MUSICIAN_PORT 5555
#define TIME_INTERVAL_SEC 5

static int send_timestamp(int sock, uint32_t curtime)
{
	int res;
	uint32_t ts = htonl(curtime);
	res = send(sock, &ts, sizeof(ts), 0);
	if (res == -1) {
		printf("Failed to send message: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	int res;
	struct sockaddr_in dst_sin;
	int timer = -1;
	int sock = -1;
	int broadcast = 1;
	uint64_t timer_value = 0;
	uint32_t ts = 0;
	struct itimerspec timer_spec = {
		.it_interval = {
			.tv_sec = TIME_INTERVAL_SEC,
			.tv_nsec = 0
		},
		.it_value = {
			.tv_sec = TIME_INTERVAL_SEC,
			.tv_nsec = 0
		}
	};

	if (argc != 2) {
		printf("Usage: mididrone_conductor IP_ADDR\n"
		       "Where IP_ADDR is the IPv4 address of a "
		       "mididrone_musician,\nor a broadcast address to several "
		       "mididrone_musicians.\n"
		       "e.g. mididrone_conductor 192.168.20.255\n");
		return EXIT_FAILURE;
	}

	res = inet_aton(argv[1], &dst_sin.sin_addr);
	if (res == 0) {
		printf("Invalid IPv4 address.\n");
		return EXIT_FAILURE;
	}
	dst_sin.sin_family = AF_INET;
	dst_sin.sin_port = htons(MUSICIAN_PORT);

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1) {
		printf("Could not create socket: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	res = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast,
			sizeof(broadcast));
	if (res == -1) {
		printf("Failed to enable broadcast flag: %s\n",
				strerror(errno));
		return EXIT_FAILURE;
	}
	res = connect(sock, (const struct sockaddr*)&dst_sin, sizeof(dst_sin));
	if (res == -1) {
		printf("Could not set peer address: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	timer = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
	if (timer == -1) {
		printf("Failed to create timer: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	/* Send initial timestamp (0) */
	send_timestamp(sock, ts);
	printf("Time: %u\n", ts);

	res = timerfd_settime(timer, 0, &timer_spec, NULL);
	if (res == -1) {
		printf("Failed to set timer: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	while (1) {
		res = read(timer, &timer_value, sizeof(timer_value));
		if (res == -1) {
			printf("Failed to read timer: %s\n", strerror(errno));
			return EXIT_FAILURE;
		}
		if (timer_value > 5) {
			printf("More than 5 timer ticks missed! Aborting.\n");
			return EXIT_FAILURE;
		}

		ts += (uint32_t)timer_value * TIME_INTERVAL_SEC;
		send_timestamp(sock, ts);
		printf("Time: %u\n", ts);
	}

	return EXIT_SUCCESS;
}
