/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#ifndef SYNC_TRACK_H
#define SYNC_TRACK_H

#include <map>

namespace sync
{
	class Track
	{
	public:
		explicit Track(const std::string &name) : name(name) { }
		
		struct KeyFrame
		{
			enum InterpolationType
			{
				IT_STEP,
				IT_LERP,
				IT_COSINE,
				IT_RAMP,
				IT_COUNT // max value
			};
			
			KeyFrame() : value(0.0f), interpolationType(IT_STEP) {}
			KeyFrame(float value, InterpolationType interpolationType) :
				value(value),
				interpolationType(interpolationType)
			{
			}
			
			float value;
			InterpolationType interpolationType;
		};
		
		float  getValue(float time) const;
		
		bool   isKeyFrame(size_t row) const;
		const  KeyFrame *getKeyFrame(size_t row) const;
		
		void deleteKeyFrame(size_t row);
		void setKeyFrame(size_t row, const KeyFrame &keyFrame);
		
		void truncate();
		
		const std::string &getName() const { return name; }
		
		typedef std::map<size_t, struct KeyFrame> KeyFrameContainer;
		KeyFrameContainer::const_iterator keyFramesBegin() const { return keyFrames.begin(); }
		KeyFrameContainer::const_iterator keyFramesEnd() const { return keyFrames.end(); }
		size_t getKeyFrameCount() const { return keyFrames.size(); }
		
		KeyFrame::InterpolationType getInterpolationType(int row) const
		{
			KeyFrame::InterpolationType interpolationType = KeyFrame::IT_STEP;
			{
				KeyFrameContainer::const_iterator upper = keyFrames.upper_bound(row);
				KeyFrameContainer::const_iterator lower = upper;
				if (lower != keyFrames.end())
				{
					lower--;
					if (lower != keyFrames.end()) interpolationType = lower->second.interpolationType;
				}
			}
			return interpolationType;
		}
	private:
		KeyFrameContainer keyFrames;
		std::string name;
	};
}

#endif // SYNC_TRACK_H
