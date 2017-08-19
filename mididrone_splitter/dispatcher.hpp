#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <vector>
#include <cstring>
#include <allegro.h>
#include "musician.hpp"

class Dispatcher
{
public:
	virtual void playNote(const Alg_event_ptr evt) = 0;
	virtual void stopNote(const Alg_event_ptr evt) = 0;
	virtual void finalize() = 0;
};

class SimpleDispatcher : public Dispatcher
{
protected:
	std::vector<Musician> musicians;
	unsigned int notes_per_musician;
public:
	SimpleDispatcher(unsigned int notes_per_musician);
	virtual ~SimpleDispatcher();

	virtual void playNote(const Alg_event_ptr evt);
	virtual void stopNote(const Alg_event_ptr evt);
	virtual void finalize();
};

class ChannelDispatcher : public SimpleDispatcher
{
public:
	ChannelDispatcher(unsigned int notes_per_musician);
	virtual ~ChannelDispatcher();

	virtual void playNote(const Alg_event_ptr evt);
	virtual void stopNote(const Alg_event_ptr evt);
	virtual void finalize();
protected:
	Alg_seq full_seq;
	std::vector<Musician> final_musicians;
private:
	void final_note_on(const Alg_note_ptr note);
	void final_note_off(const Alg_note_ptr note);
};

#endif
