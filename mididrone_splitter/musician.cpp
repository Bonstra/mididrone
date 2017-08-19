#include "musician.hpp"
#include <cstdio>

struct MusicianNoteState
{
	bool busy;
	unsigned int note;
	unsigned int channel;

	MusicianNoteState();
};

MusicianNoteState::MusicianNoteState() :
	busy(false),
	note(0),
	channel(0)
{
}

Musician::Musician(unsigned int max) :
	seq(new Alg_seq()),
	max_notes(max),
	note_states(new MusicianNoteState[max])
{
}

Musician::~Musician()
{
	delete seq;
	delete[] note_states;
}

Musician::Musician(const Musician& mus) :
	seq(new Alg_seq(mus.seq)),
	max_notes(mus.max_notes),
	note_states(new MusicianNoteState[mus.max_notes])
{
	for (unsigned int i = 0; i < max_notes; i ++) {
		note_states[i] = mus.note_states[i];
	}
}

Musician& Musician::operator= (const Musician& mus)
{
	max_notes = mus.max_notes;
	delete seq;
	seq = new Alg_seq(mus.seq);
	delete[] note_states;
	note_states = new MusicianNoteState[mus.max_notes];
	for (unsigned int i = 0; i < max_notes; i ++) {
		note_states[i] = mus.note_states[i];
	}
	return *this;
}

unsigned int Musician::maxNotes()
{
	return max_notes;
}

unsigned int Musician::usedNotes()
{
	unsigned int used = 0;
	for (unsigned int i = 0; i < max_notes; i ++) {
		if (note_states[i].busy)
			used++;
	}
	return used;
}

bool Musician::playNote(const Alg_event_ptr evt)
{
	if (usedNotes() >= max_notes)
		return false;
	if (!evt->is_note()) {
		throw "Event is not a note!\n";
	}

	unsigned int idx;
	for (idx = 0; idx < max_notes; idx++) {
		if (!note_states[idx].busy) {
			break;
		}
	}
	if (idx >= max_notes)
		return false;

	Alg_note_ptr note(dynamic_cast<Alg_note_ptr>(evt));
	MusicianNoteState& state(note_states[idx]);
	state.busy = true;
	state.note = note->get_identifier();
	state.channel = note->chan;

	seq->add_event(new Alg_note(*note), 0);

	return true;
}

bool Musician::stopNote(const Alg_event_ptr evt)
{
	if (usedNotes() == 0)
		return false;
	if (!evt->is_note()) {
		throw "Event is not a note!\n";
	}

	Alg_note_ptr note(static_cast<Alg_note_ptr>(evt));

	for (unsigned int idx = 0; idx < max_notes; idx++) {
		MusicianNoteState& state(note_states[idx]);
		if (!state.busy)
			continue;
		if (state.channel == note->chan &&
		    state.note == note->get_identifier()) {
			state.busy = false;
			state.channel = 0;
			state.note = 0;
			return true;
		}
	}
	return false;
}

bool Musician::writeToFile(const char* filename)
{
	return seq->smf_write(filename);
}

