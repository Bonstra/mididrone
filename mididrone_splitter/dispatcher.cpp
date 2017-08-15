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
