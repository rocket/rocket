#include "syncpage.h"

#include <syncdocument.h>

SyncPage::SyncPage(SyncDocument *document, const QString &name) :
    QObject(document),
    document(document),
    name(name)
{
}

SyncTrack *SyncPage::getTrack(int index)
{
	Q_ASSERT(index >= 0 && index < tracks.size());
	return tracks[index];
}

const SyncTrack *SyncPage::getTrack(int index) const
{
	Q_ASSERT(index >= 0 && index < tracks.size());
	return tracks[index];
}

void SyncPage::addTrack(SyncTrack *track)
{
	tracks.push_back(track);
}
