#include <math.h>
#include <libgen.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <cstring>
#include <cstdio>
#include <allegro.h>
#include "dispatcher.hpp"

#define ROUND(x) (int) ((x)+0.5)

static sig_atomic_t quit = false;

static const char *pressure_attr;
static const char *bend_attr;
static const char *program_attr;

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

static void split(Alg_seq &seq)
{
	// prepare by doing lookup of important symbols
	pressure_attr = symbol_table.insert_string("pressurer") + 1;
	bend_attr = symbol_table.insert_string("bendr") + 1;
	program_attr = symbol_table.insert_string("programi") + 1;

	SimpleDispatcher disp(4);

	Alg_iterator iterator(&seq, true);
	iterator.begin();
	bool note_on;
	Alg_event_ptr e = iterator.next(&note_on);

	while (e && !quit) {
		
		double next_time = (note_on ? e->time : e->get_end_time());
		if (e->is_note() && note_on) { // process notes here
			// printf("Note at %g: chan %d key %d loud %d\n",
			//        next_time, e->chan, e->key, (int) e->loud);
			/*midi_note_on(driver, next_time, e->chan, e->get_identifier(),
					(int) e->get_loud());*/
			disp.playNote(e);
		} else if (e->is_note()) { // must be a note off
			/* midi_note_on(driver, next_time, e->chan, e->get_identifier(), 0); */
			disp.stopNote(e);
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
	disp.finalize();
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

	Alg_seq seq(argv[1], true);
	seq.convert_to_seconds();

	printf("Splitting: %s\n", argv[1]);

	try {
		split(seq);
	} catch (char const* s) {
		printf("Exception: %s\n", s);
		return EXIT_FAILURE;
	}

	return 0;
}



