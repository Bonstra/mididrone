#include <math.h>
#include <libgen.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <libpomp.h>
#include <futils/futils.h>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <allegro.h>
#include "driver.h"
#include "stdout_driver.h"
#include "pwm_driver.h"

#define ROUND(x) (int) ((x)+0.5)

static sig_atomic_t quit = 0;

static bool go_received = false;
static double time_sec_offset = 0.0;
static double time_error = 0.0;

static struct pomp_loop *loop = NULL;
static struct pomp_timer *timer = NULL;
static int conductor_sock = -1;

static Alg_seq* seq = NULL;
static Alg_iterator* seq_iter = NULL;
static Alg_event* next_event = NULL;
static Driver* driver = NULL;

static void init_time()
{
	struct timespec ts;
	time_get_monotonic(&ts);
	time_sec_offset = (double)ts.tv_sec + ((double)ts.tv_nsec / 1000000000.0);
}

static double get_time()
{
	struct timespec ts;
	double res;
	time_get_monotonic(&ts);
	res = (double)ts.tv_sec + ((double)ts.tv_nsec / 1000000000.0);
	res -= time_sec_offset;
	res += time_error;
	return res;
}

static void wait_until(double time)
{
	double now;
	do {
		now = get_time();
		//printf("now: %f\n", now);
		if (time - now >= 1.0)
			sleep((int)(time - now));
		else if (time - now > 0.0)
			usleep((time - now) * 1000000.0);
	} while (now < time);
}

static int key2freq(int key)
{
	double freq = pow(2.0, ((double)key - 69.0) / 12.0) * 440.0;
	//printf("key2freq: %i -> %i\n", key, (int)round(freq));
	return (int)round(freq);
}

static int loud2ratio(int loud)
{
	return loud * 2;
}

static void midi_note_on(Driver *driver, double ts, double starttime, int chan,
		int key, int loud, double duration)
{
	chan = chan & 15;
	if (key > 127) key = 127;
	if (key < 0) key = 0;
	if (loud > 127) loud = 127;
	if (loud < 0) loud = 0;

	int freq = key2freq(key);
	int ratio = loud2ratio(loud);
	driver->addNote(ts, starttime, chan, freq, ratio, duration);
}

static void process_one_seq_event(double ts)
{
	assert(next_event != NULL);
	// Process notes here
	if (next_event->is_note()) {
		midi_note_on(driver, ts, next_event->time, next_event->chan,
				next_event->get_identifier(),
				(int) next_event->get_loud(),
				next_event->get_duration());
	}
}

static void process_seq_event(double ts)
{
	while(next_event && ts >= next_event->time) {
		process_one_seq_event(ts);
		next_event = seq_iter->next();
	}
}

static void schedule_timer(double ts)
{
	long delay_min = -1;
	/* Determine time to next seq event. */
	if (next_event) {
		long next_evt_delay = (next_event->time - ts) * 1000.0;
		if (next_evt_delay <= 0)
			next_evt_delay = 1;
		if (delay_min < 0 || delay_min > next_evt_delay)
			delay_min = next_evt_delay;
	}

	/* Determine time to next driver event. */
	double driver_ts = driver->nextEventTime();
	if (driver_ts >= 0.0) {
		if (driver_ts <= ts) {
			delay_min = 0;
		} else {
			long drv_delay = (driver_ts - ts) * 1000.0;
			if (drv_delay < delay_min)
				delay_min = drv_delay;
		}
	}

	if (delay_min == 0)
		delay_min = 1; // Minimum to get the timer running
	if (delay_min < 0) {
		/* No more events, end of song */
		seq_iter->end();
		quit = 1;
		pomp_loop_wakeup(loop);
		return;
	}
	pomp_timer_set(timer, delay_min);
}

static void process_and_schedule(double ts)
{
	process_seq_event(ts);
	driver->update(ts);
	schedule_timer(ts);
}

static void sig_handler(int sig)
{
	quit = 1;
	pomp_loop_wakeup(loop);
}

static void timer_handler(struct pomp_timer *t, void *userdata)
{
	double ts = get_time();
	process_and_schedule(ts);
}

static void conductor_handler(int fd, uint32_t revents, void *userdata)
{
	if (revents & POMP_FD_EVENT_IN) {
		int res;
		uint32_t buf;
		uint32_t new_ts;
		struct sockaddr_in saddr;
		socklen_t saddr_sz = sizeof(saddr);

		res = recvfrom(fd, &buf, sizeof(buf), 0, (struct sockaddr*)&saddr, &saddr_sz);
		if (res == -1) {
			printf("recvfrom() failed: %s\n", strerror(errno));
			quit = 1;
			pomp_loop_wakeup(loop);
		}
		new_ts = ntohl(buf);
		if (!go_received && new_ts == 0) {
			printf("Go received from %s:%d\n", inet_ntoa(saddr.sin_addr), ntohs(saddr.sin_port));
			go_received = true;
			init_time();
			process_and_schedule(0.0);
		} else if (go_received) {
			double local_ts;
			/* Recalculate time error */
			time_error = 0.0;
			local_ts = get_time();
			time_error = new_ts - local_ts;

			printf("Conductor timestamp: %u Local: %.4f "
					"Error: %4f\n", new_ts, local_ts,
					time_error);
			pomp_timer_clear(timer);
			process_and_schedule(new_ts);
		}
	}
	if (revents & (POMP_FD_EVENT_ERR | POMP_FD_EVENT_HUP)) {
			printf("ERR or HUP event.\n");
			quit = 1;
			pomp_loop_wakeup(loop);
	}
}

static int conductor_listener_setup(void)
{
	int res;
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(5555);
	addr.sin_addr.s_addr = INADDR_ANY;

	conductor_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (conductor_sock == -1) {
		printf("socket() failed: %s\n", strerror(errno));
		return -1;
	}
	res = bind(conductor_sock, (const struct sockaddr*)&addr, sizeof(addr));
	if (res == -1) {
		printf("bind() failed: %s\n", strerror(errno));
		close(conductor_sock);
		conductor_sock = -1;
		return -1;
	}

	res = pomp_loop_add(loop, conductor_sock, POMP_FD_EVENT_IN,
			conductor_handler, NULL);
	if (res) {
		printf("pomp_loop_add() failed: %s\n", strerror(-res));
		close(conductor_sock);
		conductor_sock = -1;
		return -1;
	}

	return 0;
}

int main(int argc, char* argv[])
{
	loop = pomp_loop_new();
	timer = pomp_timer_new(loop, timer_handler, NULL);

	if (argc != 2) {
		printf("Usage: %s MIDIFILE\n", basename(argv[0]));
		return 1;
	}

	signal(SIGINT, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGHUP, sig_handler);

	seq = new Alg_seq(argv[1], true);
	seq->convert_to_seconds();
	seq_iter = new Alg_iterator(seq, false);

#ifdef USE_MINIDRONES_PWM_DRIVER
	driver = new PwmDriver();
#else
	driver = new StdoutDriver();
#endif

	if (conductor_listener_setup()) {
		printf("conductor_listener_setup() failed!\n");
		return EXIT_FAILURE;
	}

	seq_iter->begin();
	next_event = seq_iter->next();

	printf("Playing: %s\n", argv[1]);
	printf("Available channels: %i\n", driver->channels());

	while(!quit) {
		pomp_loop_wait_and_process(loop, -1);
	}

	driver->panic();

	pomp_loop_remove(loop, conductor_sock);
	close(conductor_sock);
	conductor_sock = -1;

	pomp_timer_destroy(timer);
	timer = NULL;
	pomp_loop_destroy(loop);
	loop = NULL;

	delete driver;
	driver = NULL;
	delete seq_iter;
	seq_iter = NULL;
	delete seq;
	seq = NULL;
	return 0;
}


