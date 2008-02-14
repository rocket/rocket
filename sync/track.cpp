#include "track.h"
#include "data.h"
using namespace sync;

#include <cmath>
#include <cstddef>

float Track::getValue(float time) const
{
	if (keyFrames.size() == 0) return 0.0f;
	
	int currRow = int(floor(time));
	
	// find bounding keyframes
	KeyFrameContainer::const_iterator upper = keyFrames.upper_bound(currRow);
	KeyFrameContainer::const_iterator lower = upper;
	lower--;
	
	// bounds check
	if (lower == keyFrames.end()) return upper->second.value;
	if (upper == keyFrames.end()) return lower->second.value;
	
	float delta = upper->second.value - lower->second.value;
	
	// lerp, bitch
	float d = (time - lower->first) / (upper->first - lower->first);
	return lower->second.value + delta * d;
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

size_t Track::getFrameCount() const
{
	if (keyFrames.empty()) return 0;
	KeyFrameContainer::const_iterator iter = keyFrames.end();
	iter--;
	return iter->first;
}
