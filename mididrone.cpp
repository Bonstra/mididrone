#include <math.h>
#include <libgen.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <cstring>
#include <cstdio>
#include <allegro.h>
#include "driver.h"
#include "stdout_driver.h"
#include "pwm_driver.h"

#define ROUND(x) (int) ((x)+0.5)

static volatile bool quit = false;

static double time_sec_offset = 0;

static const char *pressure_attr;
static const char *bend_attr;
static const char *program_attr;

static void init_time()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	time_sec_offset = (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0);
}

static double get_time()
{
	struct timeval tv;
	double res;
	gettimeofday(&tv, NULL);
	res = (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0);
	res -= time_sec_offset;
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

static void midi_note_on(Driver *driver, double when, int chan, int key, int loud)
{
	unsigned long timestamp = (unsigned long) (when * 1000);
	chan = chan & 15;
	if (key > 127) key = 127;
	if (key < 0) key = 0;
	if (loud > 127) loud = 127;
	if (loud < 0) loud = 0;

	ChannelState cs;
	cs.freq = key2freq(key);
	cs.ratio = loud2ratio(loud);
	cs.lchannel = chan;
	if (loud == 0) {
		driver->releaseLChannel(chan);
	}
	driver->setFirstFreeChannelState(cs);
}

#if UPDATE
void send_midi_update(Alg_update_ptr u, Driver* driver)
{
	if (u->get_attribute() == pressure_attr) {
		if (u->get_identifier() < 0) {
			midi_channel_message_2(midi, u->time, MIDI_TOUCH, u->chan,
					(int) (u->get_real_value() * 127));
		} else {
			midi_channel_message(midi, u->time, MIDI_POLY_TOUCH, u->chan, 
					u->get_identifier(), 
					(int) (u->get_real_value() * 127));
		}
	} else if (u->get_attribute() == bend_attr) {
		int bend = ROUND((u->get_real_value() + 1) * 8192);
		if (bend > 8191) bend = 8191;
		if (bend < 0) bend = 0;
		midi_channel_message(midi, u->time, MIDI_BEND, u->chan, 
				bend >> 7, bend & 0x7F);
	} else if (u->get_attribute() == program_attr) {
		midi_channel_message_2(midi, u->time, MIDI_CH_PROGRAM, 
				u->chan, u->get_integer_value());
	} else if (strncmp("control", u->get_attribute(), 7) == 0 &&
			u->get_update_type() == 'r') {
		int control = atoi(u->get_attribute() + 7);
		int val = ROUND(u->get_real_value() * 127);
		midi_channel_message(midi, u->time, MIDI_CTRL, u->chan, control, val);
	}
}
#endif

static void seq2midi(Alg_seq &seq, Driver *driver)
{
	// prepare by doing lookup of important symbols
	pressure_attr = symbol_table.insert_string("pressurer") + 1;
	bend_attr = symbol_table.insert_string("bendr") + 1;
	program_attr = symbol_table.insert_string("programi") + 1;

	Alg_iterator iterator(&seq, true);
	iterator.begin();
	bool note_on;
	Alg_event_ptr e = iterator.next(&note_on);
	driver->panic(); // Reset driver

	while (e && !quit) {
		double next_time = (note_on ? e->time : e->get_end_time());
		wait_until(next_time);
		if (e->is_note() && note_on) { // process notes here
			// printf("Note at %g: chan %d key %d loud %d\n",
			//        next_time, e->chan, e->key, (int) e->loud);
			midi_note_on(driver, next_time, e->chan, e->get_identifier(),
					(int) e->get_loud());
		} else if (e->is_note()) { // must be a note off
			midi_note_on(driver, next_time, e->chan, e->get_identifier(), 0);
#if UPDATE
		} else if (e->is_update()) { // process updates here
			Alg_update_ptr u = (Alg_update_ptr) e; // coerce to proper type
			send_midi_update(u, driver);
#endif
		} 
		// add next note
		e = iterator.next(&note_on);
	}
	iterator.end();
}

void sig_handler(int sig)
{
	quit = true;
}

int main(int argc, char* argv[])
{
	if (argc != 2) {
		printf("Usage: %s MIDIFILE\n", basename(argv[0]));
		return 1;
	}

	signal(SIGINT, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGHUP, sig_handler);

	init_time();
	Alg_seq seq(argv[1], true);
	seq.convert_to_seconds();
	Driver* driver = new PwmDriver();

	printf("Playing: %s\n", argv[1]);
	printf("Available channels: %i\n", driver->channels());

	seq2midi(seq, driver);

	delete(driver);
	return 0;
}


