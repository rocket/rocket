/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#define _USE_MATH_DEFINES
#include <cmath>

#include "track.h"
#include "data.h"
using namespace sync;

#include <cstddef>

/*#ifndef M_PI
#define M_PI
#endif */

static inline float step(Track::KeyFrameContainer::const_iterator lower, Track::KeyFrameContainer::const_iterator upper, float time)
{
	return lower->second.value;
}

static inline float lerp(Track::KeyFrameContainer::const_iterator lower, Track::KeyFrameContainer::const_iterator upper, float time)
{
	// find local time
	float t = (time - lower->first) / (upper->first - lower->first);
	
	// lerp, bitch
	float delta = upper->second.value - lower->second.value;
	return lower->second.value + delta * t;
}

static inline float cosine(Track::KeyFrameContainer::const_iterator lower, Track::KeyFrameContainer::const_iterator upper, float time)
{
	// find local time
	float t = (time - lower->first) / (upper->first - lower->first);
	t = float((1.0 - cos(t * M_PI)) * 0.5);
	
	// lerp, bitch
	float delta = upper->second.value - lower->second.value;
	return lower->second.value + delta * t;
}

static inline float ramp(Track::KeyFrameContainer::const_iterator lower, Track::KeyFrameContainer::const_iterator upper, float time)
{
	// find local time
	float t = (time - lower->first) / (upper->first - lower->first);
	t = powf(t, 2.0f);
	
	// lerp, bitch
	float delta = upper->second.value - lower->second.value;
	return lower->second.value + delta * t;
}

/*float cosine(float time) const;
float ramp(float time) const; */

float Track::getValue(float time) const
{
	if (keyFrames.size() == 0) return 0.0f;
	
	// find bounding keyframes
	int currRow = int(floor(time));
	KeyFrameContainer::const_iterator upper = keyFrames.upper_bound(currRow);
	KeyFrameContainer::const_iterator lower = upper;
	lower--;
	
	// bounds check
	if (lower == keyFrames.end()) return upper->second.value;
	if (upper == keyFrames.end()) return lower->second.value;
	
	switch (lower->second.interpolationType)
	{
	case Track::KeyFrame::IT_STEP:   return step(  lower, upper, time);
	case Track::KeyFrame::IT_LERP:   return lerp(  lower, upper, time);
	case Track::KeyFrame::IT_COSINE: return cosine(lower, upper, time);
	case Track::KeyFrame::IT_RAMP:   return ramp(  lower, upper, time);
	default:
		assert(false);
		return step(lower, upper, time);
	}
}

bool Track::isKeyFrame(size_t row) const
{
	return keyFrames.find(row) != keyFrames.end();
}

const Track::KeyFrame *Track::getKeyFrame(size_t row) const
{
	KeyFrameContainer::const_iterator iter = keyFrames.find(row);
	if (iter == keyFrames.end()) return NULL;
	return &iter->second;
}

void Track::deleteKeyFrame(size_t row)
{
	keyFrames.erase(row);
}

void Track::setKeyFrame(size_t row, const KeyFrame &keyFrame)
{
	keyFrames[row] = keyFrame;
}
