#include "trackview.h"
#include "syncdocument.h"
#include <qdrawutil.h>
#include <QApplication>
#include <QByteArray>
#include <QClipboard>
#include <QDoubleValidator>
#include <QLineEdit>
#include <QMouseEvent>
#include <QMimeData>
#include <QScrollBar>
#include <QStylePainter>

TrackView::TrackView(SyncPage *page, QWidget *parent) :
    QAbstractScrollArea(parent),
    page(page),
    windowRows(0),
    readOnly(false),
    dragging(false)
{
	Q_ASSERT(page);

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

	selectionStart = selectionEnd = QPoint(0, 0);

	updateFont(fontMetrics());
	updatePalette();

	stepPen = QPen();
	lerpPen = QPen(QBrush(Qt::red), 2);
	smoothPen = QPen(QBrush(Qt::green), 2);
	rampPen = QPen(QBrush(Qt::blue), 2);

	editBrush = Qt::yellow;
	bookmarkBrush = QColor(128, 128, 255);

	handCursor = QCursor(Qt::OpenHandCursor);
	setMouseTracking(true);

	setupScrollBars();
	QObject::connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(onHScroll(int)));
	QObject::connect(verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(onVScroll(int)));

	QObject::connect(page, SIGNAL(trackHeaderChanged(int)), this, SLOT(onTrackHeaderChanged(int)));
	QObject::connect(page, SIGNAL(trackDataChanged(int, int, int)), this, SLOT(onTrackDataChanged(int, int, int)));
}

void TrackView::onTrackHeaderChanged(int trackIndex)
{
	QRect rect(QPoint(getPhysicalX(trackIndex), 0),
	           QPoint(getPhysicalX(trackIndex + 1) - 1, topMarginHeight - 1));
	viewport()->update(rect);
}

void TrackView::onTrackDataChanged(int trackIndex, int start, int stop)
{
	QRect rect(QPoint(getPhysicalX(trackIndex),         getPhysicalY(start)),
	           QPoint(getPhysicalX(trackIndex + 1) - 1, getPhysicalY(stop + 1) - 1));
	viewport()->update(rect);
}

void TrackView::updatePalette()
{
	bgBaseBrush = palette().base();
	bgDarkBrush = palette().window();

	selectBaseBrush = palette().highlight();
	selectDarkBrush = palette().highlight().color().darker(100.0 / 0.9);

	rowPen       = QPen(QBrush(palette().base().color().darker(100.0 / 0.7)), 1);
	rowSelectPen = QPen(QBrush(palette().highlight().color().darker(100.0 / 0.7)), 1);
}

void TrackView::updateFont(const QFontMetrics &fontMetrics)
{
	rowHeight = fontMetrics.lineSpacing();
	trackWidth = fontMetrics.width('0') * 16;

	topMarginHeight = rowHeight + 4;
	leftMarginWidth = fontMetrics.width('0') * 8;
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

int TrackView::getRowFromLogicalY(int y) const
{
	return divfloor(y, rowHeight);
}

int TrackView::getRowFromPhysicalY(int y) const
{
	return getRowFromLogicalY(y - topMarginHeight + scrollPosY);
}

void TrackView::paintEvent(QPaintEvent *event)
{
	QStylePainter painter(this->viewport());

	updateFont(painter.fontMetrics()); // HACK: the fontMetrics we get from QWidget is not scaled properly

	paintTopMargin(painter, event->region());
	paintLeftMargin(painter, event->region());
	paintTracks(painter, event->region());
}

void TrackView::paintTopMargin(QStylePainter &painter, const QRegion &region)
{
	const QRect &rect = region.boundingRect();
	painter.setClipRect(QRectF(QPointF(0.0f, 0.0f),
	                    QPointF(rect.right() + 1.0f, topMarginHeight - 0.5f)));

	QRect topMargin(QPoint(-2, 0),
	                QPoint(rect.right() + 3, topMarginHeight - 1));
	painter.fillRect(topMargin.adjusted(1, 1, -1, -1), palette().button());
	qDrawWinButton(&painter, topMargin, palette());

	int startTrack = qBound(0, getTrackFromPhysicalX(qMax(rect.left(), leftMarginWidth)), getTrackCount());
	int endTrack = qBound(0, getTrackFromPhysicalX(rect.right()) + 1, getTrackCount());

	for (int track = startTrack; track < endTrack; ++track) {
		const SyncTrack *t = getTrack(track);

		QRect topMargin(getPhysicalX(track), 0, trackWidth, topMarginHeight);
		if (!region.intersects(topMargin))
			continue;

		QRect fillRect = topMargin;

		QBrush bgBrush = palette().button();
		if (track == editTrack)
			bgBrush = editBrush;

		painter.fillRect(fillRect.adjusted(1, 1, -1, -1), bgBrush);
		qDrawWinButton(&painter, fillRect, bgBrush.color());

		if (!t->isActive())
			painter.setPen(QColor(128, 128, 128));
		else
			painter.setPen(QColor(0, 0, 0));

		painter.drawText(fillRect, t->getDisplayName());
	}

	// make sure that the top margin isn't overdrawn by the track-data
	painter.setClipRegion(QRect(0, topMarginHeight, rect.right() + 1, rect.bottom() + 1));
}

void TrackView::paintLeftMargin(QStylePainter &painter, const QRegion &region)
{
	const QRect &rect = region.boundingRect();
	const SyncDocument *doc = getDocument();

	int firstRow = qBound(0, getRowFromPhysicalY(qMax(rect.top(), topMarginHeight)), getRows() - 1);
	int lastRow = qBound(0, getRowFromPhysicalY(qMax(rect.bottom(), topMarginHeight)), getRows() - 1);

	painter.setClipRect(QRectF(QPointF(0.0f, topMarginHeight - 0.5f),
	                           QPointF(leftMarginWidth - 0.5f, rect.bottom() + 1.0f)));

	QRectF padding(QPointF(rect.left(), topMarginHeight - 0.5f),
	               QPointF(leftMarginWidth - 0.5f, rect.bottom() + 1.0f));
	painter.fillRect(padding, palette().dark());

	for (int row = firstRow; row <= lastRow; ++row) {
		QRect leftMargin(0, getPhysicalY(row), leftMarginWidth, rowHeight);
		if (!region.intersects(leftMargin))
			continue;

		QBrush fillBrush;
		if (row == editRow)
			fillBrush = editBrush;
		else if (doc->isRowBookmark(row))
			fillBrush = bookmarkBrush;
		else
			fillBrush = palette().button();

		painter.fillRect(leftMargin.adjusted(1, 1, -1, -1), fillBrush);
		qDrawWinButton(&painter, leftMargin, QPalette(fillBrush.color()));

		if ((row % 8) == 0)      painter.setPen(QColor(0, 0, 0));
		else if ((row % 4) == 0) painter.setPen(QColor(64, 64, 64));
		else                     painter.setPen(QColor(128, 128, 128));

		painter.drawText(leftMargin, QString("%1").arg(row, 5, 16, QChar('0')).toUpper() + "h");
	}
}

void TrackView::paintTracks(QStylePainter &painter, const QRegion &region)
{
	const QRect &rect = region.boundingRect();
	int startTrack = qBound(0, getTrackFromPhysicalX(qMax(rect.left(), leftMarginWidth)), getTrackCount());
	int endTrack = qBound(0, getTrackFromPhysicalX(rect.right()) + 1, getTrackCount());

	painter.setClipRect(QRectF(QPointF(leftMarginWidth - 0.5f, topMarginHeight - 0.5f),
	                           QPointF(rect.right() + 1.0f, rect.bottom() + 1.0f)));
	painter.fillRect(rect, palette().dark());

	for (int track = startTrack; track < endTrack; ++track)
		paintTrack(painter, region, track);
}

QPen TrackView::getInterpolationPen(SyncTrack::TrackKey::KeyType type)
{
	switch (type) {
	case SyncTrack::TrackKey::STEP:
		return stepPen;

	case SyncTrack::TrackKey::LINEAR:
		return lerpPen;

	case SyncTrack::TrackKey::SMOOTH:
		return smoothPen;

	case SyncTrack::TrackKey::RAMP:
		return rampPen;

	default:
		Q_ASSERT(false);
		return QPen();
	}
}

void TrackView::paintTrack(QStylePainter &painter, const QRegion &region, int track)
{
	const QRect &rect = region.boundingRect();
	int firstRow = qBound(0, getRowFromPhysicalY(qMax(rect.top(), topMarginHeight)), getRows() - 1);
	int lastRow = qBound(0, getRowFromPhysicalY(qMax(rect.bottom(), topMarginHeight)), getRows() - 1);

	QRect selection = getSelection();

	const SyncTrack *t = getTrack(track);

	for (int row = firstRow; row <= lastRow; ++row) {
		QRect patternDataRect(getPhysicalX(track), getPhysicalY(row), trackWidth, rowHeight);
		if (!region.intersects(patternDataRect))
			continue;

		const SyncTrack::TrackKey *key = t->getPrevKeyFrame(row);

		SyncTrack::TrackKey::KeyType interpolationType = key ? key->type : SyncTrack::TrackKey::STEP;
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
			painter.drawLine(QPointF(patternDataRect.left() + 0.5, patternDataRect.top() + 0.5),
			                 QPointF(patternDataRect.right() + 0.5, patternDataRect.top() + 0.5));
		}

		if (interpolationType != SyncTrack::TrackKey::STEP) {
			painter.setPen(getInterpolationPen(interpolationType));
			painter.drawLine(QPoint(patternDataRect.right(), patternDataRect.top() + 1),
			                 QPoint(patternDataRect.right(), patternDataRect.bottom()));
		}

		if (row == editRow && track == editTrack) {
			QRectF selectRect = QRectF(patternDataRect).adjusted(0.5, 0.5, -0.5, -0.5);
			painter.setPen(QColor(0, 0, 0));
			painter.drawRect(selectRect);
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
		const int trackCount = getTrackCount();

		if (track < 0 || track >= trackCount)
			return;

		if (track > anchorTrack) {
			for (int i = anchorTrack; i < track; ++i)
				page->swapTrackOrder(i, i + 1);
			anchorTrack = track;
			setEditTrack(track, false);
		} else if (track < anchorTrack) {
			for (int i = anchorTrack; i > track; --i)
				page->swapTrackOrder(i, i - 1);
			anchorTrack = track;
			setEditTrack(track, false);
		}
	} else {
		if (event->pos().y() < topMarginHeight &&
		    track >= 0 && track < getTrackCount()) {
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
	    track >= 0 && track < getTrackCount()) {
		dragging = true;
		anchorTrack = track;
	}
}

void TrackView::mouseReleaseEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		dragging = false;
		setCursor(QCursor(Qt::ArrowCursor));
		setEditTrack(editTrack, false);
	}
}

struct CopyEntry
{
	int track;
	SyncTrack::TrackKey keyFrame;
};

void TrackView::editCopy()
{
	if (0 == getTrackCount()) {
		QApplication::beep();
		return;
	}

	QRect selection = getSelection();

	QVector<struct CopyEntry> copyEntries;
	for (int track = selection.left(); track <= selection.right(); ++track) {
		const SyncTrack *t = getTrack(track);

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
	data.append((char *)copyEntries.data(), sizeof(CopyEntry) * copyEntries.size());

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

			SyncTrack *t = getTrack(trackPos);
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
				SyncTrack::TrackKey key = ce.keyFrame;
				key.row += editRow;

				// since we deleted all keyframes in the edit-box already, we can just insert this one.
				doc->setKeyFrame(getTrack(trackPos), key);
			}
		}
		doc->endMacro();

		dirtyCurrentValue();

		clipbuf = NULL;
	} else
		QApplication::beep();
}

void TrackView::editUndo()
{
	SyncDocument *doc = getDocument();

	if (!doc->canUndo())
		QApplication::beep();
	else
		doc->undo();
}

void TrackView::editRedo()
{
	SyncDocument *doc = getDocument();

	if (!doc->canRedo())
		QApplication::beep();
	else
		doc->redo();
}

void TrackView::editPreviousBookmark()
{
	SyncDocument *doc = getDocument();

	int row = doc->prevRowBookmark(getEditRow());
	if (row >= 0)
		setEditRow(row);
}

void TrackView::editNextBookmark()
{
	SyncDocument *doc = getDocument();

	int row = doc->nextRowBookmark(getEditRow());
	if (row >= 0)
		setEditRow(row);
}


void TrackView::setSelection(const QRect &rect)
{
	QRect oldRect = getSelection();

	selectionStart = rect.topLeft();
	selectionEnd = rect.bottomRight();

	QRect logicalRect = oldRect.united(getSelection());
	QRect physicalRect(QPoint(getPhysicalX(logicalRect.left()),
	                          getPhysicalY(logicalRect.top())),
	                   QPoint(getPhysicalX(logicalRect.right() + 1) - 1,
	                          getPhysicalY(logicalRect.bottom() + 1) - 1));
	viewport()->update(physicalRect);
}

void TrackView::updateSelection(const QPoint &pos, bool selecting)
{
	QRect oldRect = getSelection();

	if (!selecting)
		selectionStart = pos;
	selectionEnd = pos;

	QRect logicalRect = oldRect.united(getSelection());
	QRect physicalRect(QPoint(getPhysicalX(logicalRect.left()),
	                          getPhysicalY(logicalRect.top())),
	                   QPoint(getPhysicalX(logicalRect.right() + 1) - 1,
	                          getPhysicalY(logicalRect.bottom() + 1) - 1));
	viewport()->update(physicalRect);
}

void TrackView::selectAll()
{
	setSelection(QRect(0, 0, getTrackCount() - 1, getRows() - 1));
}

void TrackView::selectTrack()
{
	setSelection(QRect(editTrack, 0, editTrack, getRows() - 1));
}

void TrackView::selectRow()
{
	setSelection(QRect(0, editRow, getTrackCount() - 1, editRow));
}

void TrackView::setupScrollBars()
{
	verticalScrollBar()->setValue(editRow);
	verticalScrollBar()->setMinimum(0);
	verticalScrollBar()->setMaximum(getRows() - 1);
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
	int oldEditRow = editRow;
	editRow = newEditRow;

	// clamp to document
	editRow = qBound(0, editRow, getRows() - 1);

	if (oldEditRow != editRow) {
		updateSelection(QPoint(editTrack, editRow), selecting);
		dirtyPosition();
		dirtyCurrentValue();

		invalidateLeftMarginRow(oldEditRow);
		invalidateLeftMarginRow(editRow);
	}

	setScrollPos(scrollPosX, (editRow * rowHeight) - ((viewport()->height() - topMarginHeight) / 2) + rowHeight / 2);
}

void TrackView::setEditTrack(int newEditTrack, bool selecting)
{
	if (0 == getTrackCount()) return;

	int oldEditTrack = editTrack;
	editTrack = newEditTrack;

	// clamp to document
	editTrack = qBound(0, editTrack, getTrackCount() - 1);

	if (oldEditTrack != editTrack) {
		updateSelection(QPoint(editTrack, editRow), selecting);
		onTrackHeaderChanged(oldEditTrack);
		onTrackHeaderChanged(editTrack);
		dirtyPosition();
		dirtyCurrentValue();
	}

	if (viewport()->width() > 0) {
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

	doc->setRows(rows);
	viewport()->update();
	setEditRow(qMin(editRow, rows - 1));
}

int TrackView::getRows() const
{
	return page->getDocument()->getRows();
}

SyncTrack *TrackView::getTrack(int index)
{
	return page->getTrack(index);
}

int TrackView::getTrackCount() const
{
	return page->getTrackCount();
}

SyncDocument *TrackView::getDocument()
{
	return page->getDocument();
}

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
	if (!lineEdit->isVisible())
		return;

	if (lineEdit->text().length() > 0 && editTrack < getTrackCount()) {
		SyncTrack *t = getTrack(editTrack);

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
	} else
		QApplication::beep();

	lineEdit->hide();
	setFocus();
}

void TrackView::editToggleInterpolationType()
{
	SyncDocument *doc = getDocument();

	if (editTrack < getTrackCount()) {
		SyncTrack *t = getTrack(editTrack);
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
	} else
		QApplication::beep();
}

void TrackView::editClear()
{
	SyncDocument *doc = getDocument();

	QRect selection = getSelection();

	if (0 == getTrackCount()) return;
	Q_ASSERT(selection.right() < getTrackCount());

	doc->beginMacro("clear");
	for (int track = selection.left(); track <= selection.right(); ++track) {
		SyncTrack *t = getTrack(track);

		for (int row = selection.top(); row <= selection.bottom(); ++row) {
			if (t->isKeyFrame(row))
				doc->deleteKeyFrame(t, row);
		}
	}

	doc->endMacro();
	dirtyCurrentValue();
}

void TrackView::editBiasValue(float amount)
{
	SyncDocument *doc = getDocument();

	if (0 == getTrackCount()) {
		QApplication::beep();
		return;
	}

	QRect selection = getSelection();

	doc->beginMacro("bias");
	for (int track = selection.left(); track <= selection.right(); ++track) {
		Q_ASSERT(track < getTrackCount());
		SyncTrack *t = getTrack(track);

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
}

void TrackView::keyPressEvent(QKeyEvent *event)
{
	SyncDocument *doc = getDocument();

	if (!readOnly && lineEdit->isVisible()) {
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
			return;
		}
	}

	bool shiftDown = (event->modifiers() & Qt::ShiftModifier) != 0;
	bool ctrlDown = (event->modifiers() & Qt::ControlModifier) != 0;
	bool selecting = shiftDown;

	if (lineEdit->isHidden()) {
		switch (event->key()) {
		case Qt::Key_Backtab:
			if (ctrlDown) {
				// the main window will handle this
				event->ignore();
				return;
			}
			selecting = false;
			// FALLTHROUGH
		case Qt::Key_Left:
			if (ctrlDown) {
				if (0 < editTrack)
					page->swapTrackOrder(editTrack, editTrack - 1);
				else
					QApplication::beep();
			}
			if (0 != getTrackCount())
				setEditTrack(editTrack - 1, selecting);
			else
				QApplication::beep();
			return;

		case Qt::Key_Tab:
			if (ctrlDown) {
				// the main window will handle this
				event->ignore();
				return;
			}
			selecting = false;
			// FALLTHROUGH
		case Qt::Key_Right:
			if (ctrlDown) {
				if (getTrackCount() > editTrack + 1)
					page->swapTrackOrder(editTrack, editTrack + 1);
				else
					QApplication::beep();
			}
			if (0 != getTrackCount())
				setEditTrack(editTrack + 1, selecting);
			else
				QApplication::beep();
			return;
		}
	}

	if (!readOnly && lineEdit->isHidden()) {
		switch (event->key()) {
		case Qt::Key_Up:
			if (ctrlDown) {
				if (getTrackCount() > editTrack)
					editBiasValue(shiftDown ? 0.1f : 1.0f);
				else
					QApplication::beep();
			} else
				setEditRow(editRow - 1, selecting);
			return;

		case Qt::Key_Down:
			if (ctrlDown) {
				if (getTrackCount() > editTrack)
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
				setEditTrack(0, false);
			else
				setEditRow(0, selecting);
			return;

		case Qt::Key_End:
			if (ctrlDown)
				setEditTrack(getTrackCount() - 1, false);
			else
				setEditRow(getRows() - 1, selecting);
			return;
		}
	}

	switch (event->key()) {
	case Qt::Key_Delete:
		editClear();
		return;

	case Qt::Key_Cancel:
	case Qt::Key_Escape:
		if (!readOnly && lineEdit->isVisible()) {
			// return to old value (i.e don't clear)
			lineEdit->hide();
			setFocus();
			QApplication::beep();
		}
		return;

	case Qt::Key_I:
		editToggleInterpolationType();
		return;

	case Qt::Key_K:
		doc->toggleRowBookmark(getEditRow());
		return;
	}

	if (!readOnly && !ctrlDown && lineEdit->isHidden() && event->text().length() && doc->getTrackCount()) {
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
			return;
		}
	}

	event->ignore();
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
		updateFont(fontMetrics());
		update();
		break;

	case QEvent::PaletteChange:
		updatePalette();
		break;

	default: ;
	}
}
