#ifndef SYNCTRACK_H
#define SYNCTRACK_H

#include <QObject>
#include <QMap>

class SyncTrack : public QObject {
	Q_OBJECT
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
		emit keyFrameChanged(*this, key.row);
	}

	void removeKey(int row)
	{
		Q_ASSERT(keys.find(row) != keys.end());
		keys.remove(row);
		emit keyFrameChanged(*this, row);
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

	static void getPolynomial(float coeffs[4], const TrackKey *key)
	{
		coeffs[0] = key->value;
		switch (key->type) {
		case TrackKey::STEP:
			coeffs[1] = coeffs[2] = coeffs[3] = 0.0f;
			break;

		case TrackKey::LINEAR:
			coeffs[1] = 1.0f;
			coeffs[2] = coeffs[3] = 0.0f;
			break;

		case TrackKey::SMOOTH:
			coeffs[1] =  0.0f;
			coeffs[2] =  3.0f;
			coeffs[3] = -2.0f;
			break;

		case TrackKey::RAMP:
			coeffs[1] = coeffs[3] = 0.0f;
			coeffs[2] = 1.0f;
			break;

		default:
			Q_ASSERT(0);
			coeffs[0] = 0.0f;
			coeffs[1] = 0.0f;
			coeffs[2] = 0.0f;
			coeffs[3] = 0.0f;
		}
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

		float coeffs[4];
		getPolynomial(coeffs, prevKey);

		float x = double(row - prevKey->row) /
			  double(nextKey->row - prevKey->row);
		float mag = nextKey->value - prevKey->value;
		return coeffs[0] + (coeffs[1] + (coeffs[2] + coeffs[3] * x) * x) * x * mag;
	}

	const QMap<int, TrackKey> getKeyMap() const
	{
		return keys;
	}

	bool isActive() const
	{
		return receivers(SIGNAL(keyFrameChanged(const SyncTrack &, int))) > 0;
	}

	QString name;
private:
	QMap<int, TrackKey> keys;

signals:
	void keyFrameChanged(const SyncTrack &track, int row);
};

#endif // !defined(SYNCTRACK_H)
