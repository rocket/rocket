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
			explicit KeyFrame(float value, InterpolationType interpolationType) :
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
		
	// private:
		
		typedef std::map<size_t, struct KeyFrame> KeyFrameContainer;
		KeyFrameContainer keyFrames;
	};
}

#endif // SYNC_TRACK_H
