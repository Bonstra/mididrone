#include "dispatcher.hpp"

SimpleDispatcher::SimpleDispatcher(unsigned int max_notes) :
	musicians(),
	notes_per_musician(max_notes)
{
}

SimpleDispatcher::~SimpleDispatcher()
{
}

void SimpleDispatcher::playNote(const Alg_event_ptr evt)
{
	if (!evt->is_note())
		return;

	for (auto it = musicians.begin(); it != musicians.end(); ++it) {
		if ((*it).playNote(evt))
			return;
	}
	// Not enough musicians to play this note, spawn a new one.
	Musician m(notes_per_musician);
	m.playNote(evt);
	musicians.push_back(m);
}

void SimpleDispatcher::stopNote(const Alg_event_ptr evt)
{
	if (!evt->is_note())
		return;

	for (auto it = musicians.begin(); it != musicians.end(); ++it) {
		if ((*it).stopNote(evt))
			return;
	}
}

void SimpleDispatcher::finalize()
{
	unsigned int idx = 0;
	for (auto it = musicians.begin(); it != musicians.end(); ++it) {
		char filename[64];
		snprintf(filename, sizeof(filename), "drone_%.2d.mid", idx);
		(*it).writeToFile(filename);
		++idx;
	}
	printf("Created %u files.\n", idx);
}

ChannelDispatcher::ChannelDispatcher(unsigned int max_notes) :
	SimpleDispatcher(max_notes),
	full_seq(),
	final_musicians()
{
}

ChannelDispatcher::~ChannelDispatcher()
{
}

void ChannelDispatcher::playNote(const Alg_event_ptr evt)
{
	if (evt->is_note()) {
		Alg_note* note(new Alg_note(dynamic_cast<Alg_note*>(evt)));
		full_seq.add_event(note, 0);
	}
	SimpleDispatcher::playNote(evt);
}

void ChannelDispatcher::stopNote(const Alg_event_ptr evt)
{
	SimpleDispatcher::stopNote(evt);
}

void ChannelDispatcher::finalize()
{
	unsigned int num_musicians = musicians.size();
	if (num_musicians == 0)
		return;

	/* Distribute events among musicians, trying the preferred musician
	 * for the channel first. */
	final_musicians.clear();
	final_musicians.resize(num_musicians, Musician(notes_per_musician));
	Alg_iterator seq_iter(&full_seq, true);
	seq_iter.begin();
	bool on;
	for (Alg_event_ptr evt = seq_iter.next(&on);
			evt;
			evt = seq_iter.next(&on)) {
		if (!evt->is_note())
			continue;
		if (on) {
			final_note_on(dynamic_cast<Alg_note_ptr>(evt));
		} else {
			final_note_off(dynamic_cast<Alg_note_ptr>(evt));
		}
	}
	seq_iter.end();
	musicians = final_musicians;
	SimpleDispatcher::finalize();
}

void ChannelDispatcher::final_note_on(const Alg_note_ptr note)
{
	unsigned int chan = static_cast<unsigned int>(note->chan);
	unsigned int num_musicians = final_musicians.size();
	Musician& preferred_mus =
		final_musicians[chan % num_musicians];
	if (preferred_mus.usedNotes() < preferred_mus.maxNotes()) {
		preferred_mus.playNote(note);
		return;
	}
	for (auto it = final_musicians.begin();
			it != final_musicians.end(); ++it) {
		Musician& mus(*it);
		if (mus.usedNotes() < mus.maxNotes()) {
			mus.playNote(note);
			return;
		}
	}
}

void ChannelDispatcher::final_note_off(const Alg_note_ptr note)
{
	for (auto it = final_musicians.begin();
			it != final_musicians.end(); ++it) {
		Musician& mus(*it);
		mus.stopNote(note);
	}
}

