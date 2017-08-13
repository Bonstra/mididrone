#ifndef STDOUT_DRIVER_H_INCLUDED
#define STDOUT_DRIVER_H_INCLUDED

#include "driver.h"

class StdoutDriver : public Driver
{
public:
	StdoutDriver();
	virtual ~StdoutDriver();
	virtual int channels();
	virtual ChannelState channelState(int idx);
	virtual bool setFirstFreeChannelState(ChannelState state);
	virtual bool setChannelState(int idx, ChannelState state);
	virtual void releaseChannel(int idx);
	virtual bool releaseLChannel(int lchan);
	virtual void panic();
private:
	static const int mNumChans = 4;
	ChannelState mChans[mNumChans];
};

#endif
