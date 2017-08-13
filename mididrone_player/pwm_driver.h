#ifndef PWM_DRIVER_H_INCLUDED
#define PWM_DRIVER_H_INCLUDED

#include "driver.h"

class PwmDriver : public Driver
{
public:
	PwmDriver();
	virtual ~PwmDriver();
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
	int mFds[mNumChans];
};

#endif
