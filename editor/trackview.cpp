#include "trackview.h"
#include "syncdocument.h"
#include <qdrawutil.h>
#include <QApplication>
#include <QByteArray>
#include <QClipboard>
#include <QScrollBar>
#include <QMouseEvent>
#include <QMimeData>
#include <QLineEdit>
#include <QDoubleValidator>

TrackView::TrackView(QWidget *parent) :
    QAbstractScrollArea(parent),
    paused(true),
    connected(false),
    windowRows(0),
    document(NULL),
    dragging(false)
{
#ifdef Q_OS_WIN
	setFont(QFont("Fixedsys"));
#else
	QFont font("Monospace");
	font.setStyleHint(QFont::TypeWriter);
	setFont(font);
#endif

	lineEdit = new QLineEdit(this);
	lineEdit->setAutoFillBackground(true);
	lineEdit->hide();
	QDoubleValidator *lineEditValidator = new QDoubleValidator();
	lineEditValidator->setNotation(QDoubleValidator::StandardNotation);
	lineEditValidator->setLocale(QLocale::c());
	lineEdit->setValidator(lineEditValidator);

	QObject::connect(lineEdit, SIGNAL(editingFinished()), this, SLOT(onEditingFinished()));

	viewport()->setAutoFillBackground(false);

	setFocus(Qt::OtherFocusReason);

	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	scrollPosX = 0;
	scrollPosY = 0;

	editRow = 0;
	editTrack = 0;
	
	selectStartTrack = selectStopTrack = 0;
	selectStartRow = selectStopRow = 0;

	updateFont();
	updatePalette();

	handCursor = QCursor(Qt::OpenHandCursor);
	setMouseTracking(true);

	setupScrollBars();
	QObject::connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(onHScroll(int)));
	QObject::connect(verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(onVScroll(int)));
}

void TrackView::updatePalette()
{
	bgBaseBrush = palette().base();
	bgDarkBrush = palette().window();

	selectBaseBrush = palette().highlight();
	selectDarkBrush = palette().highlight().color().darker(100.0 / 0.9);

	rowPen       = QPen(QBrush(palette().base().color().darker(100.0 / 0.7)), 1);
	rowSelectPen = QPen(QBrush(palette().highlight().color().darker(100.0 / 0.7)), 1);

	lerpPen   = QPen(QBrush(QColor(255, 0, 0)), 2);
	cosinePen = QPen(QBrush(QColor(0, 255, 0)), 2);
	rampPen   = QPen(QBrush(QColor(0, 0, 255)), 2);

	editBrush = Qt::yellow;
	bookmarkBrush = QColor(128, 128, 255);
}

void TrackView::updateFont()
{
	rowHeight = fontMetrics().lineSpacing();
	trackWidth = fontMetrics().width('0') * 16;

	topMarginHeight = rowHeight + 4;
	leftMarginWidth = fontMetrics().width('0') * 8;
}

TrackView::~TrackView()
{
	if (document)
		delete document;
}

int TrackView::getLogicalX(int track) const
{
	return track * trackWidth;
}

int TrackView::getLogicalY(int row) const
{
	return row * rowHeight;
}

int TrackView::getPhysicalX(int track) const
{
	return leftMarginWidth + getLogicalX(track) - scrollPosX;
}

int TrackView::getPhysicalY(int row) const
{
	return topMarginHeight + getLogicalY(row) - scrollPosY;
}

inline int divfloor(int a, int b)
{
	if (a < 0)
		return -abs(a) / b - 1;
	return a / b;
}

int TrackView::getTrackFromLogicalX(int x) const
{
	return divfloor(x, trackWidth);
}

int TrackView::getTrackFromPhysicalX(int x) const
{
	return getTrackFromLogicalX(x - leftMarginWidth + scrollPosX);
}

void TrackView::paintEvent(QPaintEvent *event)
{
	QPainter painter(this->viewport());
	paintTopMargin(painter, event->rect());
	paintLeftMargin(painter, event->rect());
	paintTracks(painter, event->rect());
}

void TrackView::paintTopMargin(QPainter &painter, const QRect &rcTracks)
{
	QRect topLeftMargin;
	const SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	topLeftMargin.setTop(-1);
	topLeftMargin.setBottom(topMarginHeight - 1);
	topLeftMargin.setLeft(-1);
	topLeftMargin.setRight(leftMarginWidth + 1);
	painter.fillRect(topLeftMargin, palette().button());
	qDrawWinButton(&painter, topLeftMargin, palette());

	QRect topRightMargin;
	topRightMargin.setTop(-1);
	topRightMargin.setBottom(topMarginHeight - 1);
	topRightMargin.setLeft(getPhysicalX(getTrackCount()) - 1);
	topRightMargin.setRight(rcTracks.right() + 1);
	painter.fillRect(topRightMargin, palette().button());
	qDrawWinButton(&painter, topRightMargin, palette());

	int startTrack = qBound(0, getTrackFromPhysicalX(qMax(rcTracks.left(), leftMarginWidth)), int(getTrackCount()));
	int endTrack   = qBound(0, getTrackFromPhysicalX(rcTracks.right()) + 1, int(getTrackCount()));

	for (int track = startTrack; track < endTrack; ++track) {
		int index = doc->getTrackIndexFromPos(track);
		const SyncTrack *t = doc->getTrack(index);

		QRect topMargin(getPhysicalX(track), 0, trackWidth, topMarginHeight);
		if (!rcTracks.intersects(topMargin))
			continue;

		QRect fillRect = topMargin;

		QBrush bgBrush = palette().button();
		if (track == editTrack)
			bgBrush = editBrush;

		painter.fillRect(fillRect, bgBrush);
		qDrawWinButton(&painter, fillRect, palette());

		if (!t->isActive())
			painter.setPen(QColor(128, 128, 128));
		else
			painter.setPen(QColor(0, 0, 0));

		painter.drawText(fillRect, t->name);
	}

	// make sure that the top margin isn't overdrawn by the track-data
	painter.setClipRegion(QRect(0, topMarginHeight, rcTracks.right() + 1, rcTracks.bottom() + 1));
}

void TrackView::paintLeftMargin(QPainter &painter, const QRect &rcTracks)
{
	const SyncDocument *doc = getDocument();
	if (NULL == doc) return;

	int firstRow = editRow - windowRows / 2 - 1;
	int lastRow  = editRow + windowRows / 2 + 1;

	/* clamp first & last row */
	firstRow = qBound(0, firstRow, int(getRows()) - 1);
	lastRow  = qBound(0, lastRow,  int(getRows()) - 1);

	for (int row = firstRow; row <= lastRow; ++row) {
		QRect leftMargin(0, getPhysicalY(row), leftMarginWidth, rowHeight);
		if (!rcTracks.intersects(leftMargin))
			continue;

		QBrush fillBrush;
		if (row == editRow)
			fillBrush = editBrush;
		else if (doc->isRowBookmark(row))
			fillBrush = bookmarkBrush;
		else
			fillBrush = palette().button();
		painter.fillRect(leftMargin, fillBrush);

		qDrawWinButton(&painter, leftMargin, palette());
		if ((row % 8) == 0)      painter.setPen(QColor(0, 0, 0));
		else if ((row % 4) == 0) painter.setPen(QColor(64, 64, 64));
		else                     painter.setPen(QColor(128, 128, 128));

		painter.drawText(leftMargin, QString("%1").arg(row, 5, 16, QChar('0')).toUpper() + "h");
	}
}

void TrackView::paintTracks(QPainter &painter, const QRect &rcTracks)
{
	const SyncDocument *doc = getDocument();
	if (NULL == doc) return;

	int firstRow = editRow - windowRows / 2 - 1;
	int lastRow  = editRow + windowRows / 2 + 1;

	/* clamp first & last row */
	firstRow = qBound(0, firstRow, int(getRows()) - 1);
	lastRow  = qBound(0, lastRow,  int(getRows()) - 1);

	int startTrack = qBound(0, getTrackFromPhysicalX(qMax(rcTracks.left(), leftMarginWidth)), int(getTrackCount()));
	int endTrack   = qBound(0, getTrackFromPhysicalX(rcTracks.right()) + 1, int(getTrackCount()));

	QRect topPadding(QPoint(rcTracks.left(), qMax(int(rcTracks.top()), topMarginHeight)),
			 QPoint(rcTracks.right(), getPhysicalY(0) - 1));
	painter.fillRect(topPadding, palette().dark());

	QRect bottomPadding(QPoint(rcTracks.left(), getPhysicalY(int(getRows()))),
			    QPoint(rcTracks.right(), rcTracks.bottom()));
	painter.fillRect(bottomPadding, palette().dark());

	painter.setClipRect(leftMarginWidth,
			    topMarginHeight,
			    viewport()->width() - leftMarginWidth,
			    viewport()->height() - topMarginHeight);

	for (int track = startTrack; track < endTrack; ++track)
		paintTrack(painter, rcTracks, track);

	QRect rightMargin(QPoint(getPhysicalX(getTrackCount()), getPhysicalY(0)),
	                  QPoint(rcTracks.right(), getPhysicalY(int(getRows())) - 1));
	painter.fillRect(rightMargin, palette().dark());
}

static QPen getInterpolationBrush(SyncTrack::TrackKey::KeyType type)
{
	switch (type) {
	case SyncTrack::TrackKey::STEP:
		return QPen();

	case SyncTrack::TrackKey::LINEAR:
		return QPen(QBrush(QColor(255, 0, 0)), 2);

	case SyncTrack::TrackKey::SMOOTH:
		return QPen(QBrush(QColor(0, 255, 0)), 2);

	case SyncTrack::TrackKey::RAMP:
		return QPen(QBrush(QColor(0, 0, 255)), 2);

	default:
		Q_ASSERT(false);
		return QPen();
	}
}

void TrackView::paintTrack(QPainter &painter, const QRect &rcTracks, int track)
{
	int firstRow = editRow - windowRows / 2 - 1;
	int lastRow  = editRow + windowRows / 2 + 1;

	/* clamp first & last row */
	firstRow = qBound(0, firstRow, int(getRows()) - 1);
	lastRow  = qBound(0, lastRow,  int(getRows()) - 1);

	QRect selection = getSelection();

	const SyncTrack *t = getDocument()->getTrack(getDocument()->getTrackIndexFromPos(track));
	QMap<int, SyncTrack::TrackKey> keyMap = t->getKeyMap();

	for (int row = firstRow; row <= lastRow; ++row) {
		QRect patternDataRect(getPhysicalX(track), getPhysicalY(row), trackWidth, rowHeight);
		if (!rcTracks.intersects(patternDataRect))
			continue;

		QMap<int, SyncTrack::TrackKey>::const_iterator it = keyMap.lowerBound(row);
		if (it != keyMap.constBegin() && it.key() != row)
			--it;

		SyncTrack::TrackKey::KeyType interpolationType =
				(it != keyMap.constEnd() && it.key() <= row) ?
				it->type : SyncTrack::TrackKey::STEP;
		bool selected = selection.contains(track, row);

		QBrush baseBrush = bgBaseBrush;
		QBrush darkBrush = bgDarkBrush;

		if (selected) {
			baseBrush = selectBaseBrush;
			darkBrush = selectDarkBrush;
		}

		QBrush bgBrush = (row % 8 == 0) ? darkBrush : baseBrush;

		QRect fillRect = patternDataRect;
		painter.fillRect(fillRect, bgBrush);
		if (row % 8 == 0) {
			painter.setPen(selected ? rowSelectPen : rowPen);
			painter.drawLine(patternDataRect.topLeft(),
			                 patternDataRect.topRight());
		}

		if (interpolationType != SyncTrack::TrackKey::STEP) {
			painter.setPen(getInterpolationBrush(interpolationType));
			painter.drawLine(patternDataRect.topRight(),
			                 patternDataRect.bottomRight());
		}

		if (row == editRow && track == editTrack) {
			painter.setPen(QColor(0, 0, 0));
			painter.drawRect(fillRect.x(), fillRect.y(), fillRect.width() - 1, fillRect.height() - 1);
		}

		painter.setPen(selected ?
		    palette().color(QPalette::HighlightedText) :
		    palette().color(QPalette::WindowText));
		painter.drawText(patternDataRect, t->isKeyFrame(row) ?
		                 QString::number(t->getKeyFrame(row).value, 'f', 2) :
		                 "  ---");
	}
}

void TrackView::mouseMoveEvent(QMouseEvent *event)
{
	int track = getTrackFromPhysicalX(event->pos().x());
	if (dragging) {
		SyncDocument *doc = getDocument();
		const int trackCount = getTrackCount();

		if (!doc || track < 0 || track >= trackCount)
			return;

		if (track > anchorTrack) {
			for (int i = anchorTrack; i < track; ++i)
				doc->swapTrackOrder(i, i + 1);
			anchorTrack = track;
			setEditTrack(track);
			viewport()->update();
		} else if (track < anchorTrack) {
			for (int i = anchorTrack; i > track; --i)
				doc->swapTrackOrder(i, i - 1);
			anchorTrack = track;
			setEditTrack(track);
			viewport()->update();
		}
	} else {
		if (event->pos().y() < topMarginHeight &&
		    track >= 0 && track < int(getTrackCount())) {
			setCursor(handCursor);
		} else
			setCursor(QCursor(Qt::ArrowCursor));
	}
}

void TrackView::mousePressEvent(QMouseEvent *event)
{
	int track = getTrackFromPhysicalX(event->pos().x());
	if (event->button() == Qt::LeftButton &&
	    event->pos().y() < topMarginHeight &&
	    track >= 0 && track < int(getTrackCount())) {
		dragging = true;
		anchorTrack = track;
	}
}

void TrackView::mouseReleaseEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		dragging = false;
		setCursor(QCursor(Qt::ArrowCursor));
		setEditTrack(editTrack);
	}
}

struct CopyEntry
{
	int track;
	SyncTrack::TrackKey keyFrame;
};

void TrackView::editCopy()
{
	const SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	if (0 == getTrackCount()) {
		QApplication::beep();
		return;
	}

	QRect selection = getSelection();

	QVector<struct CopyEntry> copyEntries;
	for (int track = selection.left(); track <= selection.right(); ++track) {
		const int trackIndex  = doc->getTrackIndexFromPos(track);
		const SyncTrack *t = doc->getTrack(trackIndex);

		for (int row = selection.top(); row <= selection.bottom(); ++row) {
			if (t->isKeyFrame(row)) {
				CopyEntry ce;
				ce.track = track - selection.left();
				ce.keyFrame = t->getKeyFrame(row);
				ce.keyFrame.row -= selection.top();
				copyEntries.push_back(ce);
			}
		}
	}
	
	int buffer_width  = selection.width();
	int buffer_height = selection.height();
	size_t buffer_size = copyEntries.size();

	QByteArray data;
	data.append((char *)&buffer_width, sizeof(int));
	data.append((char *)&buffer_height, sizeof(int));
	data.append((char *)&buffer_size, sizeof(size_t));
	data.append((char *)&copyEntries[0], sizeof(CopyEntry) * copyEntries.size());

	QMimeData *mimeData = new QMimeData;
	mimeData->setData("application/x-gnu-rocket", data);
	QApplication::clipboard()->setMimeData(mimeData);
}

void TrackView::editCut()
{
	if (0 == getTrackCount()) {
		QApplication::beep();
		return;
	}
	
	editCopy();
	editClear();
}

void TrackView::editPaste()
{
	SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	if (0 == getTrackCount()) {
		QApplication::beep();
		return;
	}

	const QMimeData *mimeData = QApplication::clipboard()->mimeData();
	if (mimeData->hasFormat("application/x-gnu-rocket")) {
		const QByteArray mimeDataBuffer = mimeData->data("application/x-gnu-rocket");
		const char *clipbuf = mimeDataBuffer.data();
		
		// copy data
		int buffer_width, buffer_height, buffer_size;
		memcpy(&buffer_width,  clipbuf + 0,               sizeof(int));
		memcpy(&buffer_height, clipbuf + sizeof(int),     sizeof(int));
		memcpy(&buffer_size,   clipbuf + 2 * sizeof(int), sizeof(int));

		doc->beginMacro("paste");
		for (int i = 0; i < buffer_width; ++i) {
			int trackPos = editTrack + i;
			if (trackPos >= getTrackCount()) continue;

			int trackIndex = doc->getTrackIndexFromPos(trackPos);
			SyncTrack *t = doc->getTrack(trackIndex);
			for (int j = 0; j < buffer_height; ++j) {
				int row = editRow + j;
				if (t->isKeyFrame(row))
					doc->deleteKeyFrame(t, row);
			}
		}
		
		const char *src = clipbuf + 2 * sizeof(int) + sizeof(size_t);
		for (int i = 0; i < buffer_size; ++i)
		{
			struct CopyEntry ce;
			memcpy(&ce, src, sizeof(CopyEntry));
			src += sizeof(CopyEntry);

			Q_ASSERT(ce.track >= 0);
			Q_ASSERT(ce.track < buffer_width);
			Q_ASSERT(ce.keyFrame.row >= 0);
			Q_ASSERT(ce.keyFrame.row < buffer_height);

			int trackPos = editTrack + ce.track;
			if (trackPos < getTrackCount()) {
				int track = doc->getTrackIndexFromPos(trackPos);
				SyncTrack::TrackKey key = ce.keyFrame;
				key.row += editRow;

				// since we deleted all keyframes in the edit-box already, we can just insert this one. 
				doc->setKeyFrame(doc->getTrack(track), key);
			}
		}
		doc->endMacro();

		invalidateRange(editTrack, editTrack + buffer_width - 1, editRow, editRow + buffer_height - 1);
		dirtyCurrentValue();

		clipbuf = NULL;
	} else
		QApplication::beep();
}

void TrackView::editUndo()
{
	SyncDocument *doc = getDocument();
	if (!doc)
		return;

	if (!doc->canUndo())
		QApplication::beep();
	else
		doc->undo();

	// unfortunately, we don't know how much to invalidate... so we'll just invalidate it all.
	invalidateAll();
}

void TrackView::editRedo()
{
	SyncDocument *doc = getDocument();
	if (!doc)
		return;

	if (!doc->canRedo())
		QApplication::beep();
	else
		doc->redo();

	// unfortunately, we don't know how much to invalidate... so we'll just invalidate it all.
	invalidateAll();
}

void TrackView::selectAll()
{
	selectStartTrack = int(this->getTrackCount()) - 1;
	selectStopTrack = editTrack = 0;
	selectStartRow = int(this->getRows()) - 1;
	selectStopRow = editRow = 0;
	invalidateAll();
}

void TrackView::selectTrack()
{
	selectStartTrack = selectStopTrack = editTrack;
	selectStartRow = int(this->getRows()) - 1;
	selectStopRow = editRow = 0;
	invalidateAll();
}

void TrackView::selectRow()
{
	selectStartTrack = int(this->getTrackCount()) - 1;
	selectStopTrack = editTrack = 0;
	selectStartRow = selectStopRow = editRow;
	invalidateAll();
}


void TrackView::setupScrollBars()
{
	verticalScrollBar()->setValue(editRow);
	verticalScrollBar()->setMinimum(0);
	verticalScrollBar()->setMaximum(int(getRows()) - 1);
	verticalScrollBar()->setPageStep(windowRows);

	int contentWidth = getTrackCount() * trackWidth;
	int viewWidth = qMax(viewport()->width() - leftMarginWidth, 0);
	horizontalScrollBar()->setValue(editTrack * trackWidth);
	horizontalScrollBar()->setRange(0, contentWidth - viewWidth);
	horizontalScrollBar()->setSingleStep(20);
	horizontalScrollBar()->setPageStep(viewWidth);
}

void TrackView::scrollWindow(int scrollX, int scrollY)
{
	QRect clip = viewport()->geometry();

	if (scrollX == 0) clip.setTop(topMarginHeight); // don't scroll the top margin
	if (scrollY == 0) clip.setLeft(leftMarginWidth); // don't scroll the left margin

	viewport()->scroll(scrollX, scrollY, clip);
}

void TrackView::setScrollPos(int newScrollPosX, int newScrollPosY)
{
	// clamp newscrollPosX
	newScrollPosX = qMax(newScrollPosX, 0);
	
	if (newScrollPosX != scrollPosX || newScrollPosY != scrollPosY) {
		int deltaX = scrollPosX - newScrollPosX;
		int deltaY = scrollPosY - newScrollPosY;

		// update scrollPos
		scrollPosX = newScrollPosX;
		scrollPosY = newScrollPosY;

		scrollWindow(deltaX, deltaY);
	}

	horizontalScrollBar()->setValue(newScrollPosX);
	verticalScrollBar()->setValue(editRow);
}

void TrackView::setEditRow(int newEditRow, bool selecting)
{
	SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	int oldEditRow = editRow;
	editRow = newEditRow;
	
	// clamp to document
	editRow = qBound(0, editRow, int(getRows()) - 1);
	
	if (oldEditRow != editRow) {
		if (selecting) {
			selectStopRow = editRow;
			invalidateRange(selectStartTrack, selectStopTrack, oldEditRow, editRow);
		} else {
			invalidateRange(selectStartTrack, selectStopTrack, selectStartRow, selectStopRow);
			selectStartRow   = selectStopRow   = editRow;
			selectStartTrack = selectStopTrack = editTrack;
		}
		dirtyPosition();
		dirtyCurrentValue();
	}
	
	invalidateRow(oldEditRow);
	invalidateRow(editRow);
	
	setScrollPos(scrollPosX, (editRow * rowHeight) - ((viewport()->height() - topMarginHeight) / 2) + rowHeight / 2);
}

void TrackView::setEditTrack(int newEditTrack, bool autoscroll, bool selecting)
{
	if (0 == getTrackCount()) return;
	
	int oldEditTrack = editTrack;
	editTrack = newEditTrack;
	
	// clamp to document
	editTrack = qBound(0, editTrack, int(getTrackCount()) - 1);
	
	if (oldEditTrack != editTrack)
	{
		if (selecting)
		{
			selectStopTrack = editTrack;
			invalidateRange(oldEditTrack, editTrack, selectStartRow, selectStopRow);
		}
		else
		{
			invalidateRange(selectStartTrack, selectStopTrack, selectStartRow, selectStopRow);
			selectStartRow   = selectStopRow   = editRow;
			selectStartTrack = selectStopTrack = editTrack;
		}
		dirtyPosition();
		dirtyCurrentValue();
		invalidateTrack(oldEditTrack);
		invalidateTrack(editTrack);
	}

	if (autoscroll && viewport()->width() > 0) {
		int viewportWidth = viewport()->width() - leftMarginWidth;
		int minX = getLogicalX(editTrack);
		int maxX = getLogicalX(editTrack + 1);

		if (minX < scrollPosX)
			setScrollPos(minX, scrollPosY);
		else if (maxX > scrollPosX + viewportWidth)
			setScrollPos(maxX - viewportWidth, scrollPosY);
	} else
		setupScrollBars();
}

void TrackView::setRows(int rows)
{
	SyncDocument *doc = getDocument();
	Q_ASSERT(doc);

	doc->setRows(rows);
	viewport()->update();
	setEditRow(qMin(editRow, int(rows) - 1));
}

int TrackView::getRows() const
{
	const SyncDocument *doc = getDocument();
	if (!doc)
		return 0;
	return doc->getRows();
}

int TrackView::getTrackCount() const
{
	const SyncDocument *doc = getDocument();
	if (!doc)
		return 0;
	return doc->getTrackCount();
};

void TrackView::onVScroll(int value)
{
	setEditRow(value);
}

void TrackView::onHScroll(int value)
{
	setScrollPos(value, scrollPosY);
}

void TrackView::onEditingFinished()
{
	editEnterValue();
}

void TrackView::editEnterValue()
{
	SyncDocument *doc = getDocument();
	if (!doc || !lineEdit->isVisible())
		return;

	if (lineEdit->text().length() > 0 && editTrack < int(getTrackCount())) {
		int track = doc->getTrackIndexFromPos(editTrack);
		SyncTrack *t = doc->getTrack(track);

		SyncTrack::TrackKey newKey;
		newKey.type = SyncTrack::TrackKey::STEP;
		newKey.row = editRow;
		if (t->isKeyFrame(editRow))
			newKey = t->getKeyFrame(editRow); // copy old key
		QString text = lineEdit->text();
		text.remove(lineEdit->validator()->locale().groupSeparator()); // workaround QTBUG-40456
		newKey.value = lineEdit->validator()->locale().toFloat(text); // modify value

		doc->setKeyFrame(t, newKey);

		dirtyCurrentValue();
		invalidateTrack(editTrack);
	} else
		QApplication::beep();

	lineEdit->hide();
}

void TrackView::editToggleInterpolationType()
{
	SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	if (editTrack < int(getTrackCount())) {
		int track = doc->getTrackIndexFromPos(editTrack);
		SyncTrack *t = doc->getTrack(track);
		QMap<int, SyncTrack::TrackKey> keyMap = t->getKeyMap();

		QMap<int, SyncTrack::TrackKey>::const_iterator it = keyMap.lowerBound(editRow);
		if (it != keyMap.constBegin() && it.key() != editRow)
			--it;

		if (it.key() > editRow || it == keyMap.constEnd()) {
			QApplication::beep();
			return;
		}

		// copy and modify
		SyncTrack::TrackKey newKey = *it;
		newKey.type = (SyncTrack::TrackKey::KeyType)
		    ((newKey.type + 1) % SyncTrack::TrackKey::KEY_TYPE_COUNT);

		// apply change to data-set
		doc->setKeyFrame(t, newKey);

		// update user interface
		dirtyCurrentValue();
		invalidateTrack(editTrack);
	}
	else
		QApplication::beep();
}

void TrackView::editClear()
{
	SyncDocument *doc = getDocument();
	if (NULL == doc) return;

	QRect selection = getSelection();

	if (0 == getTrackCount()) return;
	Q_ASSERT(selection.right() < int(getTrackCount()));
	
	doc->beginMacro("clear");
	for (int track = selection.left(); track <= selection.right(); ++track) {
		int trackIndex = doc->getTrackIndexFromPos(track);
		SyncTrack *t = doc->getTrack(trackIndex);

		for (int row = selection.top(); row <= selection.bottom(); ++row) {
			if (t->isKeyFrame(row))
				doc->deleteKeyFrame(t, row);
		}
	}

	doc->endMacro();
	dirtyCurrentValue();
	invalidateRange(selection.left(), selection.right(), selection.top(), selection.bottom());
}

void TrackView::editBiasValue(float amount)
{
	SyncDocument *doc = getDocument();
	if (NULL == doc) return;

	if (0 == getTrackCount()) {
		QApplication::beep();
		return;
	}

	QRect selection = getSelection();

	doc->beginMacro("bias");
	for (int track = selection.left(); track <= selection.right(); ++track) {
		Q_ASSERT(track < int(getTrackCount()));
		int trackIndex = doc->getTrackIndexFromPos(track);
		SyncTrack *t = doc->getTrack(trackIndex);

		for (int row = selection.top(); row <= selection.bottom(); ++row) {
			if (t->isKeyFrame(row)) {
				SyncTrack::TrackKey k = t->getKeyFrame(row); // copy old key
				k.value += amount; // modify value

				// add sub-command
				doc->setKeyFrame(t, k);
			}
		}
	}
	doc->endMacro();

	dirtyCurrentValue();
	invalidateRange(selection.left(), selection.right(), selection.top(), selection.bottom());
}

void TrackView::keyPressEvent(QKeyEvent *event)
{
	SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	if (paused && lineEdit->isVisible()) {
		switch (event->key()) {
		case Qt::Key_Up:
		case Qt::Key_Down:
		case Qt::Key_Left:
		case Qt::Key_Right:
		case Qt::Key_PageUp:
		case Qt::Key_PageDown:
		case Qt::Key_Home:
		case Qt::Key_End:
		case Qt::Key_Space:
			editEnterValue();
		}
	}

	bool shiftDown = (event->modifiers() & Qt::ShiftModifier) != 0;
	bool ctrlDown = (event->modifiers() & Qt::ControlModifier) != 0;
	bool selecting = shiftDown;

	if (lineEdit->isHidden()) {
		switch (event->key()) {
		case Qt::Key_Backtab:
			ctrlDown = false;
			selecting = false;
			// FALLTHROUGH
		case Qt::Key_Left:
			if (ctrlDown) {
				if (0 < editTrack)
					doc->swapTrackOrder(editTrack, editTrack - 1);
				else
					QApplication::beep();
			}
			if (0 != getTrackCount())
				setEditTrack(editTrack - 1, true, selecting);
			else
				QApplication::beep();
			return;

		case Qt::Key_Tab:
			ctrlDown = false;
			selecting = false;
			// FALLTHROUGH
		case Qt::Key_Right:
			if (ctrlDown) {
				if (int(getTrackCount()) > editTrack + 1)
					doc->swapTrackOrder(editTrack, editTrack + 1);
				else
					QApplication::beep();
			}
			if (0 != getTrackCount())
				setEditTrack(editTrack + 1, true, selecting);
			else
				QApplication::beep();
			return;
		}
	}

	if (lineEdit->isHidden() && paused) {
		switch (event->key()) {
		case Qt::Key_Up:
			if (ctrlDown) {
				if (int(getTrackCount()) > editTrack)
					editBiasValue(shiftDown ? 0.1f : 1.0f);
				else
					QApplication::beep();
			} else
				setEditRow(editRow - 1, selecting);
			return;

		case Qt::Key_Down:
			if (ctrlDown) {
				if (int(getTrackCount()) > editTrack)
					editBiasValue(shiftDown ? -0.1f : -1.0f);
				else
					QApplication::beep();
			} else
				setEditRow(editRow + 1, selecting);
			return;

		case Qt::Key_PageUp:
			if (ctrlDown)
				editBiasValue(shiftDown ? 100.0f : 10.0f);
			else
				setEditRow(editRow - 0x10, selecting);
			return;

		case Qt::Key_PageDown:
			if (ctrlDown)
				editBiasValue(shiftDown ? -100.0f : -10.0f);
			else
				setEditRow(editRow + 0x10, selecting);
			return;

		case Qt::Key_Home:
			if (ctrlDown)
				setEditTrack(0);
			else
				setEditRow(0, selecting);
			return;

		case Qt::Key_End:
			if (ctrlDown)
				setEditTrack(int(getTrackCount()) - 1);
			else
				setEditRow(int(getRows()) - 1, selecting);
			return;
		}
	}

	switch (event->key()) {
	case Qt::Key_Delete: editClear(); return;

	case Qt::Key_Cancel:
	case Qt::Key_Escape:
		if (paused && lineEdit->isVisible()) {
			// return to old value (i.e don't clear)
			lineEdit->hide();
			QApplication::beep();
		}
		return;

	case Qt::Key_Space:
		if (connected) {
			paused = !paused;
			emit pauseChanged(paused);
		}
		return;

	case Qt::Key_I:
		editToggleInterpolationType();
		return;

	case Qt::Key_K:
		getDocument()->toggleRowBookmark(getEditRow());
		invalidateRow(getEditRow());
		return;
	}

	if (paused && lineEdit->isHidden() && event->text().length() && doc->getTrackCount()) {
		// no line-edit, check if input matches a double
		QString str = event->text();
		int pos = 0;
		if (lineEdit->validator()->validate(str, pos) != QValidator::Invalid) {
			lineEdit->move(getPhysicalX(getEditTrack()), getPhysicalY(getEditRow()));
			lineEdit->resize(trackWidth, rowHeight);
			lineEdit->setText("");
			lineEdit->show();
			lineEdit->event(event);
			lineEdit->setFocus();
		}
	}
}

void TrackView::resizeEvent(QResizeEvent *event)
{
	windowRows   = (event->size().height() - topMarginHeight) / rowHeight;
	setEditRow(editRow);
	setupScrollBars();
}

void TrackView::changeEvent(QEvent *event)
{
	switch (event->type()) {
	case QEvent::FontChange:
		updateFont();
		break;

	case QEvent::PaletteChange:
		updatePalette();
		break;

	default: ;
	}
}
