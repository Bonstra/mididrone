#ifndef DRIVER_H_INCLUDED
#define DRIVER_H_INCLUDED

struct ChannelState
{
public:
	bool busy;
	int lchannel; // Logical channel
	int freq;
	int ratio;
	double stoptime;
};

class Driver {
public:
	virtual ~Driver() {};
	virtual int channels() = 0;
	virtual bool addNote(double ts, double starttime, int lchannel,
			int freq, int ratio, double duration) = 0;
	virtual void update(double ts) = 0;
	virtual double nextEventTime() = 0;
	virtual void releaseChannel(int idx) = 0;
	virtual bool releaseLChannel(int lchan) = 0;
	virtual void panic() = 0;
};

#endif

