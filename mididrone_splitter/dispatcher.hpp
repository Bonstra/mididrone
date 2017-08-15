#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <vector>
#include <cstring>
#include <allegro.h>
#include "musician.hpp"

class SimpleDispatcher
{
private:
	std::vector<Musician> musicians;
	unsigned int notes_per_musician;
public:
	SimpleDispatcher(unsigned int notes_per_musician);
	~SimpleDispatcher();

	void playNote(const Alg_event_ptr evt);
	void stopNote(const Alg_event_ptr evt);
	void finalize();
};

#endif
