#include "stdout_driver.h"
#include <cstdio>

StdoutDriver::StdoutDriver()
{
	panic();
}

StdoutDriver::~StdoutDriver()
{
}

int StdoutDriver::channels()
{
	return mNumChans;
}

bool StdoutDriver::addNote(double ts, double starttime, int lchannel, int freq,
		int ratio, double duration)
{
	update(ts);
	for (int i = 0; i < mNumChans; ++i) {
		ChannelState& chan(mChans[i]);
		if (chan.busy)
			continue;
		chan.busy = true;
		chan.freq = freq;
		chan.ratio = ratio;
		chan.lchannel = lchannel;
		chan.stoptime = starttime + duration;
		printf("%.4f: Add chan=%d lchan=%d freq=%d ratio=%d "
				"start=%.4f duration=%.4f\n", ts, i, lchannel,
				freq, ratio, starttime, duration);
		return true;
	}
	printf("%.4f: !!! No free channel!\n", ts);
	return false;
}

void StdoutDriver::update(double ts)
{
	for (int i = 0; i < mNumChans; ++i) {
		ChannelState& chan(mChans[i]);
		if (chan.busy && chan.stoptime <= ts) {
			printf("%.4f: Release chan=%d lchan=%d "
					"stoptime=%.4f\n", ts, i, chan.lchannel,
					chan.stoptime);
			releaseChannel(i);
		}
	}
}

double StdoutDriver::nextEventTime()
{
	double closest = -1.0;
	for (int i = 0; i < mNumChans; ++i) {
		ChannelState& chan(mChans[i]);
		if (chan.busy) {
			if (closest < 0.0 || chan.stoptime < closest) {
				closest = chan.stoptime;
			}
		}
	}
	return closest;
}

void StdoutDriver::releaseChannel(int idx)
{
	mChans[idx].busy = false;
	mChans[idx].lchannel = -1;
}

bool StdoutDriver::releaseLChannel(int lchan)
{
	bool success = false;
	for (int i = 0; i < mNumChans; i ++) {
		ChannelState& chan(mChans[i]);
		if (chan.busy && chan.lchannel == lchan) {
			releaseChannel(i);
			success = true;
		}
	}
	return success;
}

void StdoutDriver::panic()
{
	for (int i = 0; i < mNumChans; i ++) {
		releaseChannel(i);
		mChans[i].freq = 0;
		mChans[i].ratio = 0;
	}
}

