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
			int pwmdata = 0;
			ioctl(fd, PWM_SET_WIDTH, &pwmdata);
			usleep(10000);
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

void PwmDriver::applyChannelCfg(int idx)
{
	int res;
	int pwmdata;
	ChannelState& chan = mChans[idx];

	pwmdata = 0;
	res = ioctl(mFds[idx], PWM_SET_WIDTH, &pwmdata);
	if (res == -1) {
		printf("ioctl PWM_SET_WIDTH failed: %s\n", strerror(errno));
	}

	pwmdata = chan.freq;
	res = ioctl(mFds[idx], PWM_SET_FREQ, &pwmdata);
	if (res == -1) {
		printf("ioctl PWM_SET_FREQ failed: %s\n", strerror(errno));
	}

	pwmdata = chan.ratio;
	res = ioctl(mFds[idx], PWM_SET_WIDTH, &pwmdata);
	if (res == -1) {
		printf("ioctl PWM_SET_WIDTH failed: %s\n", strerror(errno));
	}

	res = ioctl(mFds[idx], PWM_START, 0);
	if (res == -1) {
		printf("ioctl PWM_START failed: %s\n", strerror(errno));
	}
}

void PwmDriver::applyChannelRelease(int idx)
{
	int res;
	int pwmdata;

	pwmdata = 0;
	res = ioctl(mFds[idx], PWM_SET_WIDTH, &pwmdata);
	if (res == -1) {
		printf("ioctl PWM_SET_WIDTH failed: %s\n", strerror(errno));
	}
}

bool PwmDriver::addNote(double ts, double starttime, int lchannel, int freq,
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
		applyChannelCfg(i);
		return true;
	}
	return false;
}

void PwmDriver::update(double ts)
{
	for (int i = 0; i < mNumChans; ++i) {
		ChannelState& chan(mChans[i]);
		if (chan.busy && chan.stoptime <= ts) {
			releaseChannel(i);
		}
	}
}

double PwmDriver::nextEventTime()
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

void PwmDriver::releaseChannel(int idx)
{
	mChans[idx].busy = false;
	mChans[idx].lchannel = -1;
	applyChannelRelease(idx);
}

bool PwmDriver::releaseLChannel(int lchan)
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

void PwmDriver::panic()
{
	for (int i = 0; i < mNumChans; i ++) {
		releaseChannel(i);
		mChans[i].freq = 0;
		mChans[i].ratio = 0;
	}
}

