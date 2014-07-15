#include "trackview.h"
#include "syncdocument.h"
#include <qdrawutil.h>
#include <QApplication>
#include <QByteArray>
#include <QClipboard>
#include <QScrollBar>
#include <QMouseEvent>
#include <QMimeData>

TrackView::TrackView(QWidget *parent) :
    QAbstractScrollArea(parent),
    windowRows(0),
    windowTracks(0),
    document(NULL),
    dragging(false)
{
	viewport()->setAutoFillBackground(false);

	setFocus(Qt::OtherFocusReason);

	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	scrollPosX = 0;
	scrollPosY = 0;
	windowWidth  = -1;
	windowHeight = -1;
	
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

int TrackView::getScreenY(int row) const
{
	return topMarginHeight + (row * rowHeight) - scrollPosY;
}

int TrackView::getScreenX(size_t track) const
{
	return int(leftMarginWidth + (track * trackWidth)) - scrollPosX;
}

inline int divfloor(int a, int b)
{
	if (a < 0)
		return -abs(a) / b - 1;
	return a / b;
}

int TrackView::getTrackFromX(int x) const
{
	return divfloor(x + scrollPosX - leftMarginWidth, trackWidth);
}

void TrackView::paintEvent(QPaintEvent *event)
{
	QPainter painter(this->viewport());
	paintTopMargin(painter, event->rect());
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
	topRightMargin.setLeft(getScreenX(getTrackCount()) - 1);
	topRightMargin.setRight(rcTracks.right() + 1);
	painter.fillRect(topRightMargin, palette().button());
	qDrawWinButton(&painter, topRightMargin, palette());


	int startTrack = scrollPosX / trackWidth;
	int endTrack  = qMin(startTrack + windowTracks + 1, int(getTrackCount()));
	
	for (int track = startTrack; track < endTrack; ++track) {
		size_t index = doc->getTrackIndexFromPos(track);
		const sync_track *t = doc->tracks[index];

		QRect topMargin(getScreenX(track), 0, trackWidth, topMarginHeight);
		if (!rcTracks.intersects(topMargin))
			continue;

		QRect fillRect = topMargin;

		QBrush bgBrush = palette().button();
		if (track == editTrack)
			bgBrush = editBrush;

		painter.fillRect(fillRect, bgBrush);
		qDrawWinButton(&painter, fillRect, palette());

		if (!doc->clientSocket.clientTracks.count(t->name))
			painter.setPen(QColor(128, 128, 128));
		else
			painter.setPen(QColor(0, 0, 0));

		painter.drawText(fillRect, QString::fromUtf8(t->name));
	}

	// make sure that the top margin isn't overdrawn by the track-data
	painter.setClipRegion(QRect(0, topMarginHeight, rcTracks.right() + 1, rcTracks.bottom() + 1));
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
	
	for (int row = firstRow; row <= lastRow; ++row) {
		QRect leftMargin(0, getScreenY(row), leftMarginWidth, rowHeight);
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
	
	int selectLeft  = qMin(selectStartTrack, selectStopTrack);
	int selectRight = qMax(selectStartTrack, selectStopTrack);
	int selectTop    = qMin(selectStartRow, selectStopRow);
	int selectBottom = qMax(selectStartRow, selectStopRow);
	
	int startTrack = scrollPosX / trackWidth;
	int endTrack  = qMin(startTrack + windowTracks + 1, int(getTrackCount()));
	
	for (int track = startTrack; track < endTrack; ++track) {
		const sync_track *t = doc->tracks[doc->getTrackIndexFromPos(track)];
		for (int row = firstRow; row <= lastRow; ++row) {
			QRect patternDataRect(getScreenX(track), getScreenY(row), trackWidth, rowHeight);
			if (!rcTracks.intersects(patternDataRect))
				continue;

			int idx = sync_find_key(t, row);
			int fidx = idx >= 0 ? idx : -idx - 2;
			key_type interpolationType = (fidx >= 0) ? t->keys[fidx].type : KEY_STEP;
			bool selected = (track >= selectLeft && track <= selectRight) && (row >= selectTop && row <= selectBottom);

			QBrush baseBrush = bgBaseBrush;
			QBrush darkBrush = bgDarkBrush;

			if (selected)
			{
				baseBrush = selectBaseBrush;
				darkBrush = selectDarkBrush;
			}
			
			QBrush bgBrush = baseBrush;
			if (row % 8 == 0) bgBrush = darkBrush;
			
			QRect fillRect = patternDataRect;
			painter.fillRect(fillRect, bgBrush);
			if (row % 8 == 0) {
				if (selected) painter.setPen(rowSelectPen);
				else          painter.setPen(rowPen);
				painter.drawLine(patternDataRect.topLeft(),
				                 patternDataRect.topRight());
			}
			
			switch (interpolationType) {
			case KEY_STEP:
				break;
			case KEY_LINEAR:
				painter.setPen(QPen(QBrush(QColor(255, 0, 0)), 2));
				break;
			case KEY_SMOOTH:
				painter.setPen(QPen(QBrush(QColor(0, 255, 0)), 2));
				break;
			case KEY_RAMP:
				painter.setPen(QPen(QBrush(QColor(0, 0, 255)), 2));
				break;
			default:
				Q_ASSERT(false);
			}

			if (interpolationType != KEY_STEP) {
				painter.drawLine(patternDataRect.topRight() + QPoint(0, 1),
				                 patternDataRect.bottomRight());
			}

			bool drawEditString = false;
			if (row == editRow && track == editTrack) {
				painter.setPen(QColor(0, 0, 0));
				painter.drawRect(fillRect.x(), fillRect.y(), fillRect.width() - 1, fillRect.height() - 1);
				if (editString.size() > 0)
					drawEditString = true;
			}
			/* format the text */
			QString text;
			if (drawEditString)
				text = editString;
			else if (idx < 0)
				text = "  ---";
			else {
				float val = t->keys[idx].value;
				text = QString::number(val, 'f', 2);
			}

			painter.setPen(selected ?
			    palette().color(QPalette::HighlightedText) :
			    palette().color(QPalette::WindowText));
			painter.drawText(patternDataRect, text);
		}
	}
	
	/* right margin */
	{
		QRect rightMargin(QPoint(getScreenX(getTrackCount()), getScreenY(0)),
		                  QPoint(rcTracks.right(), getScreenY(int(getRows())) - 1));
		painter.fillRect(rightMargin, palette().dark());
	}
	
	{
		QRect bottomPadding(QPoint(rcTracks.left(), getScreenY(int(getRows()))),
		                    QPoint(rcTracks.right(), rcTracks.bottom()));
		painter.fillRect(bottomPadding, palette().dark());
	}
	
	{
		QRect topPadding(QPoint(rcTracks.left(), qMax(int(rcTracks.top()), topMarginHeight)),
		                 QPoint(rcTracks.right(), getScreenY(0) - 1));
		painter.fillRect(topPadding, palette().dark());
	}
}

void TrackView::mouseMoveEvent(QMouseEvent *event)
{
	int track = getTrackFromX(event->pos().x());
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
	int track = getTrackFromX(event->pos().x());
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
	track_key keyFrame;
};

void TrackView::editCopy()
{
	const SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	if (0 == getTrackCount()) {
		QApplication::beep();
		return;
	}
	
	int selectLeft  = qMin(selectStartTrack, selectStopTrack);
	int selectRight = qMax(selectStartTrack, selectStopTrack);
	int selectTop    = qMin(selectStartRow, selectStopRow);
	int selectBottom = qMax(selectStartRow, selectStopRow);

	QVector<struct CopyEntry> copyEntries;
	for (int track = selectLeft; track <= selectRight; ++track) {
		const size_t trackIndex  = doc->getTrackIndexFromPos(track);
		const sync_track *t = doc->tracks[trackIndex];

		for (int row = selectTop; row <= selectBottom; ++row) {
			int idx = sync_find_key(t, row);
			if (idx >= 0) {
				CopyEntry ce;
				ce.track = track - selectLeft;
				ce.keyFrame = t->keys[idx];
				ce.keyFrame.row -= selectTop;
				copyEntries.push_back(ce);
			}
		}
	}
	
	int buffer_width  = selectRight - selectLeft + 1;
	int buffer_height = selectBottom - selectTop + 1;
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
		
		SyncDocument::MultiCommand *multiCmd = new SyncDocument::MultiCommand();
		for (int i = 0; i < buffer_width; ++i) {
			size_t trackPos = editTrack + i;
			if (trackPos >= getTrackCount()) continue;

			size_t trackIndex = doc->getTrackIndexFromPos(trackPos);
			const sync_track *t = doc->tracks[trackIndex];
			for (int j = 0; j < buffer_height; ++j) {
				int row = editRow + j;
				if (is_key_frame(t, row))
					multiCmd->addCommand(new SyncDocument::DeleteCommand(int(trackIndex), row));
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

			size_t trackPos = editTrack + ce.track;
			if (trackPos < getTrackCount())
			{
				size_t trackIndex = doc->getTrackIndexFromPos(trackPos);
				track_key key = ce.keyFrame;
				key.row += editRow;

				// since we deleted all keyframes in the edit-box already, we can just insert this one. 
				SyncDocument::Command *cmd = new SyncDocument::InsertCommand(int(trackIndex), key);
				multiCmd->addCommand(cmd);
			}
		}

		doc->exec(multiCmd);
		viewport()->update();
		dirtyCurrentValue();

		clipbuf = NULL;
	} else
		QApplication::beep();
}

void TrackView::editUndo()
{
	if (NULL == getDocument())
		return;

	if (!getDocument()->undo())
		QApplication::beep();

	// unfortunately, we don't know how much to invalidate... so we'll just invalidate it all.
	viewport()->update();
}

void TrackView::editRedo()
{
	if (NULL == getDocument())
		return;

	if (!getDocument()->redo())
		QApplication::beep();

	// unfortunately, we don't know how much to invalidate... so we'll just invalidate it all.
	viewport()->update();
}

void TrackView::selectAll()
{
	selectStartTrack = int(this->getTrackCount()) - 1;
	selectStopTrack = editTrack = 0;
	selectStartRow = int(this->getRows()) - 1;
	selectStopRow = editRow = 0;
	viewport()->update();
}

void TrackView::selectTrack()
{
	selectStartTrack = selectStopTrack = editTrack;
	selectStartRow = int(this->getRows()) - 1;
	selectStopRow = editRow = 0;
	viewport()->update();
}

void TrackView::selectRow()
{
	selectStartTrack = int(this->getTrackCount()) - 1;
	selectStopTrack = editTrack = 0;
	selectStartRow = selectStopRow = editRow;
	viewport()->update();
}


void TrackView::setupScrollBars()
{
	verticalScrollBar()->setValue(editRow);
	verticalScrollBar()->setMinimum(0);
	verticalScrollBar()->setMaximum(int(getRows()) - 1);
	verticalScrollBar()->setPageStep(windowRows);

	horizontalScrollBar()->setValue(editTrack);
	horizontalScrollBar()->setMinimum(0);
	horizontalScrollBar()->setMaximum(int(getTrackCount()) - 1 + windowTracks - 1);
	horizontalScrollBar()->setPageStep(windowTracks);
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
	
	if (newScrollPosX != scrollPosX || newScrollPosY != scrollPosY)
	{
		int scrollX = scrollPosX - newScrollPosX;
		int scrollY = scrollPosY - newScrollPosY;
		
		// update scrollPos
		scrollPosX = newScrollPosX;
		scrollPosY = newScrollPosY;
		
		scrollWindow(scrollX, scrollY);
	}
	setupScrollBars();
}

void TrackView::setEditRow(int newEditRow, bool selecting)
{
	SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	int oldEditRow = editRow;
	editRow = newEditRow;
	
	// clamp to document
	editRow = qBound(0, editRow, int(getRows()) - 1);
	
	if (oldEditRow != editRow)
	{
		if (selecting)
		{
			selectStopRow = editRow;
			invalidateRange(selectStartTrack, selectStopTrack, oldEditRow, editRow);
		}
		else
		{
			invalidateRange(selectStartTrack, selectStopTrack, selectStartRow, selectStopRow);
			selectStartRow   = selectStopRow   = editRow;
			selectStartTrack = selectStopTrack = editTrack;
		} if (doc->clientSocket.clientPaused) {
			doc->clientSocket.sendSetRowCommand(editRow);
		}
		dirtyPosition();
		dirtyCurrentValue();
	}
	
	invalidateRow(oldEditRow);
	invalidateRow(editRow);
	
	setScrollPos(scrollPosX, (editRow * rowHeight) - ((windowHeight - topMarginHeight) / 2) + rowHeight / 2);
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

	if (autoscroll && windowTracks > 0) {
		int firstTrack = scrollPosX / trackWidth;
		int lastTrack  = firstTrack + windowTracks;

		int newFirstTrack = firstTrack;
		if (editTrack >= lastTrack)
			newFirstTrack = editTrack - lastTrack + firstTrack + 1;
		if (editTrack < firstTrack)
			newFirstTrack = editTrack;
		setScrollPos(newFirstTrack * trackWidth, scrollPosY);
	} else
		setupScrollBars();
}

void TrackView::setRows(size_t rows)
{
	SyncDocument *doc = getDocument();
	Q_ASSERT(doc);

	doc->setRows(rows);
	viewport()->update();
	setEditRow(qMin(editRow, int(rows) - 1));
}

size_t TrackView::getRows() const
{
	const SyncDocument *doc = getDocument();
	if (!doc)
		return 0;
	return doc->getRows();
}

size_t TrackView::getTrackCount() const
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
	setEditTrack(value);
}

void TrackView::editEnterValue()
{
	SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	if (int(editString.size()) > 0 && editTrack < int(getTrackCount()))
	{
		size_t trackIndex = doc->getTrackIndexFromPos(editTrack);
		const sync_track *t = doc->tracks[trackIndex];

		track_key newKey;
		newKey.type = KEY_STEP;
		newKey.row = editRow;
		int idx = sync_find_key(t, editRow);
		if (idx >= 0)
			newKey = t->keys[idx]; // copy old key
		newKey.value = editString.toFloat(); // modify value
		editString.clear();

		SyncDocument::Command *cmd = doc->getSetKeyFrameCommand(int(trackIndex), newKey);
		doc->exec(cmd);

		dirtyCurrentValue();
		viewport()->update();
	}
	else
		QApplication::beep();
}

void TrackView::editToggleInterpolationType()
{
	SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	if (editTrack < int(getTrackCount())) {
		size_t trackIndex = doc->getTrackIndexFromPos(editTrack);
		const sync_track *t = doc->tracks[trackIndex];

		int idx = key_idx_floor(t, editRow);
		if (idx < 0) {
			QApplication::beep();
			return;
		}

		// copy and modify
		track_key newKey = t->keys[idx];
		newKey.type = (enum key_type)
		    ((newKey.type + 1) % KEY_TYPE_COUNT);

		// apply change to data-set
		SyncDocument::Command *cmd = doc->getSetKeyFrameCommand(int(trackIndex), newKey);
		doc->exec(cmd);

		// update user interface
		dirtyCurrentValue();
		viewport()->update();
	}
	else
		QApplication::beep();
}

void TrackView::editClear()
{
	SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	int selectLeft  = qMin(selectStartTrack, selectStopTrack);
	int selectRight = qMax(selectStartTrack, selectStopTrack);
	int selectTop    = qMin(selectStartRow, selectStopRow);
	int selectBottom = qMax(selectStartRow, selectStopRow);
	
	if (0 == getTrackCount()) return;
	Q_ASSERT(selectRight < int(getTrackCount()));
	
	SyncDocument::MultiCommand *multiCmd = new SyncDocument::MultiCommand();
	for (int track = selectLeft; track <= selectRight; ++track) {
		size_t trackIndex = doc->getTrackIndexFromPos(track);
		const sync_track *t = doc->tracks[trackIndex];

		for (int row = selectTop; row <= selectBottom; ++row) {
			if (is_key_frame(t, row)) {
				SyncDocument::Command *cmd = new SyncDocument::DeleteCommand(int(trackIndex), row);
				multiCmd->addCommand(cmd);
			}
		}
	}
	
	if (0 == multiCmd->getSize()) {
		QApplication::beep();
		delete multiCmd;
	}
	else
	{
		doc->exec(multiCmd);

		dirtyCurrentValue();
		viewport()->update();
	}
}

void TrackView::editBiasValue(float amount)
{
	SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	int selectLeft  = qMin(selectStartTrack, selectStopTrack);
	int selectRight = qMax(selectStartTrack, selectStopTrack);
	int selectTop    = qMin(selectStartRow, selectStopRow);
	int selectBottom = qMax(selectStartRow, selectStopRow);
	
	if (0 == getTrackCount()) {
		QApplication::beep();
		return;
	}
	
	SyncDocument::MultiCommand *multiCmd = new SyncDocument::MultiCommand();
	for (int track = selectLeft; track <= selectRight; ++track) {
		Q_ASSERT(track < int(getTrackCount()));
		size_t trackIndex = doc->getTrackIndexFromPos(track);
		const sync_track *t = doc->tracks[trackIndex];

		for (int row = selectTop; row <= selectBottom; ++row) {
			int idx = sync_find_key(t, row);
			if (idx >= 0) {
				struct track_key k = t->keys[idx]; // copy old key
				k.value += amount; // modify value

				// add sub-command
				SyncDocument::Command *cmd = doc->getSetKeyFrameCommand(int(trackIndex), k);
				multiCmd->addCommand(cmd);
			}
		}
	}
	
	if (0 == multiCmd->getSize()) {
		QApplication::beep();
		delete multiCmd;
	}
	else
	{
		doc->exec(multiCmd);

		dirtyCurrentValue();
		invalidateRange(selectLeft, selectRight, selectTop, selectBottom);
	}
}

void TrackView::keyPressEvent(QKeyEvent *event)
{
	SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	if (editString.length()) {
		switch(event->key()) {
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

	if (!editString.length()) {
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

	if (!editString.length() && doc->clientSocket.clientPaused) {
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
	case Qt::Key_Return: editEnterValue(); break;
	case Qt::Key_Delete: editClear(); break;

	case Qt::Key_Backspace:
		if (editString.length()) {
			editString.resize(editString.length() - 1);
			invalidatePos(editTrack, editRow);
		} else
			QApplication::beep();
		break;

	case Qt::Key_Cancel:
	case Qt::Key_Escape:
		if (editString.length()) {
			// return to old value (i.e don't clear)
			editString.clear();
			invalidatePos(editTrack, editRow);
			QApplication::beep();
		}
		break;

	case Qt::Key_Space:
		doc->clientSocket.sendPauseCommand( !doc->clientSocket.clientPaused );
		break;

	case Qt::Key_Minus:
		if (!editString.length())
		{
			editString.append(event->key());
			invalidatePos(editTrack, editRow);
		}
		break;

	case Qt::Key_Period:
		// only one '.' allowed
		if (editString.indexOf('.') >= 0) {
			QApplication::beep();
			break;
		}
	case Qt::Key_0:
	case Qt::Key_1:
	case Qt::Key_2:
	case Qt::Key_3:
	case Qt::Key_4:
	case Qt::Key_5:
	case Qt::Key_6:
	case Qt::Key_7:
	case Qt::Key_8:
	case Qt::Key_9:
		if (editTrack < int(getTrackCount()))
		{
			editString.push_back(event->key());
			invalidatePos(editTrack, editRow);
		}
		else
			QApplication::beep();
		break;

	case Qt::Key_I:
		editToggleInterpolationType();
		break;

	case Qt::Key_K:
		getDocument()->toggleRowBookmark(getEditRow());
		invalidateRow(getEditRow());
		break;
	}
}

void TrackView::resizeEvent(QResizeEvent *event)
{
	windowWidth  = event->size().width();
	windowHeight = event->size().height();
	
	windowRows   = (event->size().height() - topMarginHeight) / rowHeight;
	windowTracks = (event->size().width()  - leftMarginWidth) / trackWidth;
	
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
