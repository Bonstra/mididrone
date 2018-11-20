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

PriorityChannelDispatcher::PriorityChannelDispatcher(unsigned int polyphony) :
	Dispatcher(),
	polyphony(polyphony)
{
}

PriorityChannelDispatcher::~PriorityChannelDispatcher()
{
}

PriorityChannelDispatcher::RuledMusician::RuledMusician(unsigned int polyphony,
		PriorityChannelRule* rule) :
	rule(rule),
	musician(polyphony)
{
}

bool PriorityChannelDispatcher::RuledMusician::operator<(const RuledMusician& b)
{
	// RuledMusicians are sorted by priority (lower priority first).
	// Musicians without rule are lower in rank than any musician with a
	// rule.
	if (this->rule && b.rule)
		return this->rule->priority < b.rule->priority;
	if (b.rule)
		return true;
	return false;
}

void PriorityChannelDispatcher::appendRule(unsigned int priority,
		std::set<unsigned int> channels,
		bool exclusive)
{
	auto rule = new PriorityChannelRule();
	rule->priority = priority;
	rule->channels = channels;
	rule->exclusive = exclusive;

	musicians.push_back(RuledMusician(polyphony, rule));
	musicians.sort();
}

bool PriorityChannelDispatcher::playNoteByTheRules(const Alg_event_ptr evt)
{
	Alg_note_ptr note(static_cast<Alg_note_ptr>(evt));

	// Iterate in reverse order so we try musicians with the higher
	// priority first.
	for (auto it = musicians.rbegin(); it != musicians.rend(); ++it) {
		if (!it->rule)
			return false;

		if (it->rule->channels.count((unsigned int)note->chan) != 0) {
			if (it->musician.playNote(evt))
				return true;
		}
	}
	return false;
}

bool PriorityChannelDispatcher::playNoteFifo(const Alg_event_ptr evt)
{
	// Iterate in normal order so that the musicians with the higher
	// priority are less affected by the notes added in FIFO mode.
	for (auto it = musicians.begin(); it != musicians.end(); ++it) {
		if ((*it).rule && (*it).rule->exclusive)
			continue;
		if ((*it).musician.playNote(evt))
			return true;
	}
	return false;
}

void PriorityChannelDispatcher::playNote(const Alg_event_ptr evt)
{
	if (!evt->is_note())
		return;

	if (!playNoteByTheRules(evt) && !playNoteFifo(evt)) {
		// Not enough musicians to play this note, spawn a new one.
		musicians.push_front(RuledMusician(polyphony));
		// Ugly hack to force the new musician to play the note
		musicians.front().musician.playNote(evt);
	}
}

void PriorityChannelDispatcher::stopNote(const Alg_event_ptr evt)
{
	if (!evt->is_note())
		return;

	for (auto it = musicians.begin(); it != musicians.end(); ++it) {
		if ((*it).musician.stopNote(evt))
			return;
	}
}

void PriorityChannelDispatcher::finalize()
{
	unsigned int idx = 0;
	for (auto it = musicians.rbegin(); it != musicians.rend(); ++it) {
		char filename[64];
		snprintf(filename, sizeof(filename), "drone_%.2d.mid", idx);
		(*it).musician.writeToFile(filename);
		++idx;
	}
	printf("Created %u files.\n", idx);
}
