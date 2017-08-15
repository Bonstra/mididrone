#ifndef MUSICIAN_H
#define MUSICIAN_H

#include <cstring>
#include <allegro.h>

struct MusicianNoteState;
class Musician
{
private:
	Alg_seq *seq;
	unsigned int max_notes;
	struct MusicianNoteState *note_states;
public:
	Musician(unsigned int max_notes);
	Musician(const Musician& mus);
	~Musician();
	unsigned int maxNotes();
	unsigned int usedNotes();
	bool playNote(const Alg_event_ptr evt);
	bool stopNote(const Alg_event_ptr evt);
	bool writeToFile(const char* filename);
};

#endif
