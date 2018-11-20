#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <vector>
#include <cstring>
#include <set>
#include <list>
#include <memory>
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

struct PriorityChannelRule {
	unsigned int priority;
	std::set<unsigned int> channels;
	bool exclusive;
};

class PriorityChannelDispatcher : public Dispatcher
{
	struct RuledMusician {
		std::unique_ptr<PriorityChannelRule> rule;
		Musician musician;

		RuledMusician(unsigned int polyphony, PriorityChannelRule* rule=nullptr);
		bool operator<(const RuledMusician& b);
	};
private:
	bool playNoteByTheRules(const Alg_event_ptr evt);
	bool playNoteFifo(const Alg_event_ptr evt);
protected:
	unsigned int polyphony;
	std::list<RuledMusician> musicians;
public:
	PriorityChannelDispatcher(unsigned int polyphony);
	virtual ~PriorityChannelDispatcher();

	virtual void playNote(const Alg_event_ptr evt);
	virtual void stopNote(const Alg_event_ptr evt);
	virtual void finalize();

	void appendRule(unsigned int priority, std::set<unsigned int> channels,
			bool exclusive=false);
};

#endif
