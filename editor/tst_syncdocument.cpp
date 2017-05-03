#include <QString>
#include <QtTest>
#include "syncdocument.h"

class SyncDocumentTest : public QObject
{
	Q_OBJECT

private Q_SLOTS:
	void prevRowBookmark();
	void nextRowBookmark();
};

void SyncDocumentTest::prevRowBookmark()
{
	SyncDocument doc;

	QVERIFY(doc.prevRowBookmark(10) < 0);

	doc.toggleRowBookmark(10);
	QVERIFY(doc.prevRowBookmark(9) < 0);
	QVERIFY(doc.prevRowBookmark(10) < 0);
	QVERIFY(doc.prevRowBookmark(11) == 10);
	QVERIFY(doc.prevRowBookmark(12) == 10);

	doc.toggleRowBookmark(11);
	QVERIFY(doc.prevRowBookmark(11) == 10);
	QVERIFY(doc.prevRowBookmark(12) == 11);
}

void SyncDocumentTest::nextRowBookmark()
{
	SyncDocument doc;

	QVERIFY(doc.nextRowBookmark(10) < 0);

	doc.toggleRowBookmark(10);
	QVERIFY(doc.nextRowBookmark(8) == 10);
	QVERIFY(doc.nextRowBookmark(9) == 10);
	QVERIFY(doc.nextRowBookmark(10) < 0);
	QVERIFY(doc.nextRowBookmark(11) < 0);

	doc.toggleRowBookmark(11);
	QVERIFY(doc.nextRowBookmark(9) == 10);
	QVERIFY(doc.nextRowBookmark(10) == 11);
}

QTEST_APPLESS_MAIN(SyncDocumentTest)

#include "tst_syncdocument.moc"
