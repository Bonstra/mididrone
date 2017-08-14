#include "pwm_driver.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/pwm_ioctl.h>
#include <cstring>
#include <cerrno>
#include <cstdio>

#define PWM_DEV "/dev/pwm"

PwmDriver::PwmDriver()
{
	for (int i = 0; i < mNumChans; i ++) {
		int fd;
		int res;
		int pwmdata;

		mFds[i] = -1;

		fd = open(PWM_DEV, O_RDWR | O_NONBLOCK | O_NOCTTY | O_CLOEXEC);
		if (fd == -1) {
			printf("open() failed (%i): %s\n", i, strerror(errno));
			continue;
		}

		/* Request PWM channel */
		pwmdata = i;
		res = ioctl(fd, PWM_REQUEST, &pwmdata);
		if (res == -1) {
			printf("ioctl PWM_REQUEST failed (%i): %s\n", i, strerror(errno));
			close(fd);
			continue;
		}
		mFds[i] = fd;
	}
	panic();
}

PwmDriver::~PwmDriver()
{
	for (int i = 0; i < mNumChans; i ++) {
		int fd = mFds[i];
		if (fd != -1) {
			ioctl(fd, PWM_STOP, 0);
			close(fd);
			mFds[i] = -1;
		}
	}
}

int PwmDriver::channels()
{
	return mNumChans;
}

ChannelState PwmDriver::channelState(int idx)
{
	return mChans[idx];
}

bool PwmDriver::setFirstFreeChannelState(ChannelState state)
{
	for (int i = 0; i < mNumChans; i ++) {
		if (!mChans[i].busy || mChans[i].lchannel == state.lchannel)
			return setChannelState(i, state);
	}
	return false;
}

bool PwmDriver::setChannelState(int idx, ChannelState state)
{
	int res;
	int pwmdata;
	ChannelState& chan = mChans[idx];

	chan = state;
	chan.busy = true;

	pwmdata = 0;
	res = ioctl(mFds[idx], PWM_SET_WIDTH, &pwmdata);
	if (res == -1) {
		printf("ioctl PWM_SET_WIDTH failed: %s\n", strerror(errno));
	}

	pwmdata = state.freq;
	res = ioctl(mFds[idx], PWM_SET_FREQ, &pwmdata);
	if (res == -1) {
		printf("ioctl PWM_SET_FREQ failed: %s\n", strerror(errno));
	}

	pwmdata = state.ratio;
	res = ioctl(mFds[idx], PWM_SET_WIDTH, &pwmdata);
	if (res == -1) {
		printf("ioctl PWM_SET_WIDTH failed: %s\n", strerror(errno));
	}

	res = ioctl(mFds[idx], PWM_START, 0);
	if (res == -1) {
		printf("ioctl PWM_START failed: %s\n", strerror(errno));
	}

	return true;
}

void PwmDriver::releaseChannel(int idx)
{
	int res;
	int pwmdata;
	mChans[idx].busy = false;
	mChans[idx].lchannel = -1;

	pwmdata = 0;
	res = ioctl(mFds[idx], PWM_SET_WIDTH, &pwmdata);
	if (res == -1) {
		printf("ioctl PWM_SET_WIDTH failed: %s\n", strerror(errno));
	}

	res = ioctl(mFds[idx], PWM_STOP, 0);
	if (res == -1) {
		printf("ioctl PWM_STOP failed: %s\n", strerror(errno));
	}
}

bool PwmDriver::releaseLChannel(int lchan)
{
	for (int i = 0; i < mNumChans; i ++) {
		if (mChans[i].lchannel == lchan) {
			releaseChannel(i);
			return true;
		}
	}
	return false;
}

void PwmDriver::panic()
{
	for (int i = 0; i < mNumChans; i ++) {
		releaseChannel(i);
		mChans[i].freq = 0;
		mChans[i].ratio = 0;
	}
}

