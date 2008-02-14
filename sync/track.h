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
			KeyFrame() : lerp(false) {}
			KeyFrame(float value) : value(value), lerp(false) {}
			float value;
			bool lerp;
		};
		
		float  getValue(float time) const;
		bool   isKeyFrame(size_t row) const;
		const  KeyFrame *getKeyFrame(size_t row) const;
		size_t getFrameCount() const;
		
		void deleteKeyFrame(size_t row);
		void setKeyFrame(size_t row, const KeyFrame &keyFrame);
		void setKeyFrame(size_t row, const float value);
		
	// private:
		
		typedef std::map<size_t, struct KeyFrame> KeyFrameContainer;
		KeyFrameContainer keyFrames;
	};
}

#endif // SYNC_TRACK_H
