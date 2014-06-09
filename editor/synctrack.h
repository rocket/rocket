#ifndef SYNCTRACK_H
#define SYNCTRACK_H

#include <QMap>

class SyncTrack {
public:
	SyncTrack(const QString &name) :
	    name(name)
	{
	}

	struct TrackKey {
		int row;
		float value;
		enum KeyType {
			STEP,   /* stay constant */
			LINEAR, /* lerp to the next value */
			SMOOTH, /* smooth curve to the next value */
			RAMP,   /* ramp up */
			KEY_TYPE_COUNT
		} type;
	};

	void setKey(const TrackKey &key)
	{
		keys[key.row] = key;
	}

	void removeKey(int row)
	{
		Q_ASSERT(keys.find(row) != keys.end());
		keys.remove(row);
	}

	bool isKeyFrame(int row) const
	{
		QMap<int, TrackKey>::const_iterator it = keys.lowerBound(row);
		return it != keys.end() && it.key() == row;
	}

	TrackKey getKeyFrame(int row) const
	{
		Q_ASSERT(isKeyFrame(row));
		QMap<int, TrackKey>::const_iterator it = keys.lowerBound(row);
		return it.value();
	}

	const TrackKey *getPrevKeyFrame(int row) const
	{
		QMap<int, TrackKey>::const_iterator it = keys.lowerBound(row);
		if (it != keys.constBegin() && (it == keys.constEnd() || it.key() != row))
			--it;

		if (it == keys.constEnd() || it.key() > row)
			return NULL;

		return &it.value();
	}

	const TrackKey *getNextKeyFrame(int row) const
	{
		QMap<int, TrackKey>::const_iterator it = keys.lowerBound(row);

		if (it == keys.constEnd() || it.key() < row)
			return NULL;

		return &it.value();
	}

	double getValue(int row) const
	{
		if (!keys.size())
			return 0.0;

		const TrackKey *prevKey = getPrevKeyFrame(row);
		const TrackKey *nextKey = getNextKeyFrame(row);

		Q_ASSERT(prevKey != NULL || nextKey != NULL);

		if (!prevKey)
			return nextKey->value;
		if (!nextKey)
			return prevKey->value;
		if (prevKey == nextKey)
			return prevKey->value;

		float a = prevKey->value, b, c, d;
		switch (prevKey->type) {
		case TrackKey::STEP:
			b = c = d = 0.0f;
			break;

		case TrackKey::LINEAR:
			b = 1.0f;
			c = d = 0.0f;
			break;

		case TrackKey::SMOOTH:
			b =  0.0f;
			c =  3.0f;
			d = -2.0f;
			break;

		case TrackKey::RAMP:
			b = d = 0.0f;
			c = 1.0f;
			break;

		default:
			Q_ASSERT(0);
			a = 0.0f;
			b = 0.0f;
			c = 0.0f;
			d = 0.0f;
		}

		float x = double(row - prevKey->row) /
			  double(nextKey->row - prevKey->row);
		float mag = nextKey->value - prevKey->value;
		return a + (b + (c + d * x) * x) * x * mag;
	}

	const QMap<int, TrackKey> getKeyMap() const
	{
		return keys;
	}

	QString name;
private:
	QMap<int, TrackKey> keys;
};

#endif // !defined(SYNCTRACK_H)
