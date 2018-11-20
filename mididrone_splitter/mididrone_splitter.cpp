#include <math.h>
#include <getopt.h>
#include <libgen.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <climits>
#include <cstring>
#include <cstdio>
#include <memory>
#include <allegro.h>
#include "dispatcher.hpp"

#define ROUND(x) (int) ((x)+0.5)

static sig_atomic_t quit = false;

static const char *pressure_attr;
static const char *bend_attr;
static const char *program_attr;

enum opts_dispatcher {
	UNDEFINED,
	SIMPLE_FIFO,
	SIMPLE_CHANNEL_AFFINITY,
	CHANNEL_PRIO_MAP
};

struct rule {
	unsigned int prio;
	std::set<unsigned int> channels;
	bool exclusive;
};

struct opts {
	bool ok;
	enum opts_dispatcher dispatcher;
	unsigned int polyphony;
	unsigned int skip;
	const char* filename;
	std::unique_ptr<std::vector<struct rule>> rules;
};

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

static void split(Alg_seq &seq, Dispatcher& disp)
{
	// prepare by doing lookup of important symbols
	pressure_attr = symbol_table.insert_string("pressurer") + 1;
	bend_attr = symbol_table.insert_string("bendr") + 1;
	program_attr = symbol_table.insert_string("programi") + 1;

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

static void usage(char* arg0)
{
	printf("Usage: %s [-n POLYPHONY] [-s N] (-f|-r|-p SPEC) MIDIFILE\n", basename(arg0));
	printf("General options:\n");
	printf("  -h    Show full usage screen and exit\n");
	printf("  -n POLYPHONY Number of notes each musician can \n");
	printf("               play at once (default: 4)\n");
	printf("  -s N  Skip the N first seconds\n");
	printf("\n");
	printf("Dispatcher algorithm selection:\n");
	printf("  -a    Simple channel affinity algorithm\n");
	printf("  -f    Simple FIFO algorithm\n");
	printf("  -p SPEC Priority mapping algorithm\n");
	printf("\n");
}

static void extended_usage(void)
{
	printf("Dispatcher algorithms details\n");
	printf("Simple FIFO algorithm:\n");
	printf("  This algorithm minimizes the number of musicians needed\n");
	printf("  to play the MIDI sequence. It iterates over each\n");
	printf("  musician in order, until it finds one who can play the\n");
	printf("  note. If all musicians are busy, a new one is allocated\n");
	printf("  to play the note.\n");
	printf("  The consequence is that the first musician will almost\n");
	printf("  always have notes to play, while the last musician will\n");
	printf("  only play a few notes.\n");
	printf("Simple channel affinity algorithm:\n");
	printf("  It is a variant of the simple FIFO algorithm which\n");
	printf("  assigns one or more preferred channels to each\n");
	printf("  musician. A note is first given to the musician whose\n");
	printf("  preferred channels match the note's channel. If that\n");
	printf("  musician is busy, the simple FIFO algorithm is applied.\n");
	printf("  This algorithm uses the same number of musicians as the\n");
	printf("  simple FIFO, and better assigns each instruments to a\n");
	printf("  specific musician, provided the one channel does not\n");
	printf("  have too much polyphony, and that channels in the MIDI\n");
	printf("  sequence are used sequentially.\n");
	printf("Priority mapping algorithm:\n");
	printf("  This algorithm uses a set of user-provided rules, one\n");
	printf("  per musician. A rule defines a priority, a set of\n");
	printf("  preferred channels, and a flag telling whether to reject\n");
	printf("  notes from other channels than the preferred ones.\n");
	printf("  A musician without rule has the lowest priority, and\n");
	printf("  accepts notes from any channel. Such musicians are\n");
	printf("  automatically created when the other musicians are busy.\n");
	printf("  When a note needs to be played, the algorithm looks up\n");
	printf("  among all the musicians which have the channel in their\n");
	printf("  list of preferred channels, and chooses the one with the\n");
	printf("  highest priority to play the note. If that musician is\n");
	printf("  busy, the next one with the highest priority is tried,\n");
	printf("  until no more has a rule for the channel. When this\n");
	printf("  happens, the note is added in FIFO mode, from lowest to\n");
	printf("  highest priority. If still no musician is available, a\n");
	printf("  new one is created with no rule.\n");
	printf("  If some musicians have the same priority for a specific\n");
	printf("  channel, the first one in the list is chosen.\n");
	printf("  SPEC format: [RULE[:RULE ...]]\n");
	printf("  RULE format: priority[(+|-)channel[,channel ...]]\n");
	printf("  + means rule allows other channels than the ones\n");
	printf("  specified. - means the opposite.");
}

static const char* parse_rule(const char* str, struct rule& rule)
{
	long int prio;
	char *endptr = NULL;

	if (!str || !*str)
		return NULL;

	// Parse priority
	prio = strtol(str, &endptr, 10);
	if (str == endptr || ((*endptr) != '+' && (*endptr) != '-')) {
		printf("Invalid priority or missing -/+ flag\n");
		return NULL;
	}
	if (prio < 0) {
		printf("Priority must be a positive integer\n");
		return NULL;
	}
	if (prio > UINT_MAX) {
		printf("Priority out of range [0, %u]\n", UINT_MAX);
		return NULL;
	}
	rule.prio = (unsigned int)prio;

	// Exclusive flag
	str = endptr;
	rule.exclusive = *str == '-';
	str++;

	// Parse channels list
	while (*str && *str != ':') {
		long chan = strtol(str, &endptr, 10);
		if (str == endptr ||
		    (*endptr && (*endptr) != ',' && (*endptr) != ':')
		   ) {
			printf("Invalid channel specification\n");
			return NULL;
		}
		if (chan < 0) {
			printf("Channel must be a positive integer\n");
			return NULL;
		}
		if (chan > UINT_MAX) {
			printf("Channel out of range [0, %u]\n", UINT_MAX);
			return NULL;
		}
		rule.channels.insert((unsigned int)chan);

		// Seek next channel
		str = endptr;
		if (*str == ',') {
			str++;
			if (!*str || *str == ':') {
				printf("Extraneous ',' after channel list\n");
				return NULL;
			}
		}
	}
	return str;
}

static std::unique_ptr<std::vector<struct rule>> parse_spec(const char* spec)
{
	auto nullval = std::unique_ptr<std::vector<struct rule>>(nullptr);
	if (!spec)
		return nullval;

	auto rules = std::unique_ptr<std::vector<struct rule>>(new std::vector<struct rule>());
	while(*spec) {
		struct rule rule;
		const char* newspec = parse_rule(spec, rule);
		if (!newspec)
			return nullval;
		rules->push_back(rule);
		spec = newspec;
		if (*spec == ':') {
			spec++;
			if (!*spec) {
				printf("Expected rule after ':'\n");
				return nullval;
			}
		}

	}
	return rules;
}

static struct opts parse_opts(int argc, char* argv[])
{
	struct opts opts = {
		.ok = false,
		.dispatcher = UNDEFINED,
		.polyphony = 4,
		.skip = 0,
		.filename = nullptr,
		.rules = std::unique_ptr<std::vector<struct rule>>(nullptr)
	};
	int opt = -1;

	while((opt = getopt(argc, argv, ":afhn:p:s:")) != -1) {
		switch(opt) {
		case 'a':
			if (opts.dispatcher != UNDEFINED) {
				printf("Only a single alrogithm may be chosen.\n");
				return opts;
			}
			opts.dispatcher = SIMPLE_CHANNEL_AFFINITY;
			break;
		case 'f':
			if (opts.dispatcher != UNDEFINED) {
				printf("Only a single alrogithm may be chosen.\n");
				return opts;
			}
			opts.dispatcher = SIMPLE_FIFO;
			break;
		case 'h':
			usage(argv[0]);
			extended_usage();
			return opts;
		case 'n':
		{
			long int val = strtol(optarg, NULL, 0);
			if (val <= 0 || val > UINT_MAX) {
				printf("Invalid value for -n: %ld\n", val);
				return opts;
			}
			opts.polyphony = (unsigned int)val;
			break;
		}
		case 'p':
			if (opts.dispatcher != UNDEFINED) {
				printf("Only a single alrogithm may be chosen.\n");
				return opts;
			}
			opts.dispatcher = CHANNEL_PRIO_MAP;
			opts.rules = parse_spec(optarg);
			if (!opts.rules) {
				printf("Failed to parse priority mapping specification.\n");
				return opts;
			}
			break;
		case 's':
		{
			long int val = strtol(optarg, NULL, 0);
			if (val < 0 || val > UINT_MAX) {
				printf("Invalid value for -s: %ld\n", val);
				return opts;
			}
			opts.skip = (unsigned int)val;
			break;
		}
		case '?':
			printf("Unknown option: %s\n", argv[optind - 1]);
			usage(argv[0]);
			return opts;
		case ':':
			printf("Option %s expects an argument.\n", argv[optind - 1]);
			usage(argv[0]);
			return opts;
		default:
			printf("Unexpected option: %d\n", opt);
			return opts;
		}
	}

	if (opts.dispatcher == UNDEFINED) {
		printf("No dispatcher algorithm specified.\n");
		usage(argv[0]);
		return opts;
	}

	if ((argc - optind) == 0) {
		printf("Expected MIDI file.\n");
		usage(argv[0]);
		return opts;
	} else if ((argc - optind) > 1) {
		printf("Too many arguments.\n");
		usage(argv[0]);
		return opts;
	}

	opts.filename = argv[optind];
	opts.ok = true;

	return opts;
}

static void sig_handler(int sig)
{
	quit = true;
}

int main(int argc, char* argv[])
{
	auto opts = parse_opts(argc, argv);
	if (!opts.ok)
		return EXIT_FAILURE;

	signal(SIGINT, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGHUP, sig_handler);

	Alg_seq seq(opts.filename, true);
	seq.convert_to_seconds();
	if (opts.skip > 0)
		seq.cut(0.0, (double)opts.skip, true);

	printf("Splitting: %s\n", opts.filename);
	auto disp = std::unique_ptr<Dispatcher>(nullptr);
	switch(opts.dispatcher) {
	case SIMPLE_FIFO:
		disp.reset(new SimpleDispatcher(opts.polyphony));
		break;
	case SIMPLE_CHANNEL_AFFINITY:
		disp.reset(new ChannelDispatcher(opts.polyphony));
		break;
	case CHANNEL_PRIO_MAP:
	{
		PriorityChannelDispatcher *pcd = new PriorityChannelDispatcher(opts.polyphony);
		for (auto it = opts.rules->begin(); it != opts.rules->end(); ++it) {
			pcd->appendRule(it->prio, it->channels, it->exclusive);
		}
		disp.reset(pcd);
		break;
	}
	default:
		printf("BUG\n");
		abort();
	}

	try {
		split(seq, *disp);
	} catch (char const* s) {
		printf("Exception: %s\n", s);
		return EXIT_FAILURE;
	}

	return 0;
}



