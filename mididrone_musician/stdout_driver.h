#ifndef STDOUT_DRIVER_H_INCLUDED
#define STDOUT_DRIVER_H_INCLUDED

#include "driver.h"

class StdoutDriver : public Driver
{
public:
	StdoutDriver();
	virtual ~StdoutDriver();
	virtual int channels();
	virtual bool addNote(double ts, double starrtime, int lchannel,
			int freq, int ratio, double duration);
	virtual void update(double ts);
	virtual double nextEventTime();
	virtual void releaseChannel(int idx);
	virtual bool releaseLChannel(int lchan);
	virtual void panic();
private:
	static const int mNumChans = 4;
	ChannelState mChans[mNumChans];
};

#endif
