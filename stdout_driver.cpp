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

ChannelState StdoutDriver::channelState(int idx)
{
	return mChans[idx];
}

bool StdoutDriver::setFirstFreeChannelState(ChannelState state)
{
	for (int i = 0; i < mNumChans; i ++) {
		if (!mChans[i].busy || mChans[i].lchannel == state.lchannel)
			return setChannelState(i, state);
	}
	printf("No free channel!\n");
	return false;
}

bool StdoutDriver::setChannelState(int idx, ChannelState state)
{
	ChannelState& chan = mChans[idx];
	if (chan.busy)
		return false;

	chan = state;
	chan.busy = true;
	printf("chan %i: lchan: %i freq: %i ratio: %i\n", idx, chan.lchannel, chan.freq, chan.ratio);
	return true;
}

void StdoutDriver::releaseChannel(int idx)
{
	printf("chan %i: lchan: %i busy: 0\n", idx, mChans[idx].lchannel);
	mChans[idx].busy = false;
	mChans[idx].lchannel = -1;
}

bool StdoutDriver::releaseLChannel(int lchan)
{
	for (int i = 0; i < mNumChans; i ++) {
		if (mChans[i].lchannel == lchan) {
			releaseChannel(i);
			return true;
		}
	}
	return false;
}

void StdoutDriver::panic()
{
	for (int i = 0; i < mNumChans; i ++) {
		releaseChannel(i);
		mChans[i].freq = 0;
		mChans[i].ratio = 0;
	}
}

