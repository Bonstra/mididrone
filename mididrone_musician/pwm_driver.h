#ifndef PWM_DRIVER_H_INCLUDED
#define PWM_DRIVER_H_INCLUDED

#include "driver.h"

class PwmDriver : public Driver
{
public:
	PwmDriver();
	virtual ~PwmDriver();
	virtual int channels();
	virtual bool addNote(double ts, double starttime, int lchannel,
			int freq, int ratio, double duration);
	virtual void update(double ts);
	virtual double nextEventTime();
	virtual void releaseChannel(int idx);
	virtual bool releaseLChannel(int lchan);
	virtual void panic();
private:
	void applyChannelCfg(int idx);
	void applyChannelRelease(int idx);
	static const int mNumChans = 4;
	ChannelState mChans[mNumChans];
	int mFds[mNumChans];
};

#endif
