#ifndef DRIVER_H_INCLUDED
#define DRIVER_H_INCLUDED

struct ChannelState
{
public:
	bool busy;
	int lchannel; // Logical channel
	int freq;
	int ratio;
};

class Driver {
public:
	virtual ~Driver() {};
	virtual int channels() = 0;
	virtual ChannelState channelState(int idx) = 0;
	virtual bool setFirstFreeChannelState(ChannelState state) = 0;
	virtual bool setChannelState(int idx, ChannelState state) = 0;
	virtual void releaseChannel(int idx) = 0;
	virtual bool releaseLChannel(int lchan) = 0;
	virtual void panic() = 0;
};

#endif

