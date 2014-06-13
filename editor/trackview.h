/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in COPYING
 */

#pragma once

#include <string>

#include <QAbstractScrollArea>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QPainter>

class SyncDocument;

class TrackView : public QAbstractScrollArea
{
	Q_OBJECT
public:
	TrackView(QWidget *parent);
	~TrackView();

	void setDocument(SyncDocument *document)
	{
		this->document = document;
		this->setupScrollBars();
	}

	const SyncDocument *getDocument() const { return document; }
	SyncDocument *getDocument() { return document; }

	void setRows(size_t rows);
	size_t getRows() const;

	void editEnterValue();
	void editBiasValue(float amount);
	void editToggleInterpolationType();
	
	void setEditRow(int newEditRow, bool selecting = false);
	int  getEditRow() const { return editRow; }
	
	void setEditTrack(int newEditTrack, bool autoscroll = true, bool selecting = false);
	int  getEditTrack() const { return editTrack; }

	void selectNone()
	{
		selectStartTrack = selectStopTrack = editTrack;
		selectStartRow = selectStopRow = editRow;
		update();
	}

	void dirtyCurrentValue()
	{
		emit currValDirty();
	}

	void dirtyPosition()
	{
		emit posChanged();
	}

signals:
	void posChanged();
	void currValDirty();

private slots:
	void onHScroll(int value);
	void onVScroll(int value);

public slots:
	void editUndo();
	void editRedo();
	void editCopy();
	void editCut();
	void editPaste();
	void editClear();

	void selectAll();
	void selectTrack();
	void selectRow();

private:

	/* paint helpers */
	void paintTopMargin(QPainter &painter, const QRect &rcTracks);
	void paintTracks(QPainter &painter, const QRect &rcTracks);

	void paintEvent(QPaintEvent *);
	void keyPressEvent(QKeyEvent *);
	void resizeEvent(QResizeEvent *);
	void mouseMoveEvent(QMouseEvent *);
	void mousePressEvent(QMouseEvent *);
	void mouseReleaseEvent(QMouseEvent *);

	void setupScrollBars();
	void setScrollPos(int newScrollPosX, int newScrollPosY);
	void scrollWindow(int newScrollPosX, int newScrollPosY);

	void invalidateRange(int startTrack, int stopTrack, int startRow, int stopRow)
	{
		QRect rect(QPoint(getScreenX(qMin(startTrack, stopTrack)),
		                  getScreenY(qMin(startRow, stopRow))),
		           QPoint(getScreenX(qMax(startTrack, stopTrack) + 1) - 1,
		                  getScreenY(qMax(startRow, stopRow) + 1) - 1));
		viewport()->update(rect);
	}

	void invalidatePos(int track, int row)
	{
		invalidateRange(track, track, row, row);
	}

	void invalidateRow(int row)
	{
		invalidateRange(0, getTrackCount(), row, row);
	}

	void invalidateTrack(int track)
	{
		invalidateRange(track, track, 0, getRows());
	}

	int getScreenY(int row) const;
	int getScreenX(size_t track) const;
	int getTrackFromX(int x) const;

	size_t getTrackCount() const;

	int selectStartTrack, selectStopTrack;
	int selectStartRow, selectStopRow;

	int rowHeight;
	int fontWidth;
	int trackWidth;
	int topMarginHeight;
	int leftMarginWidth;

	QBrush bgBaseBrush, bgDarkBrush;
	QBrush selectBaseBrush, selectDarkBrush;
	QPen rowPen, rowSelectPen;
	QBrush editBrush, bookmarkBrush;
	QPen lerpPen, cosinePen, rampPen;
	QCursor handCursor;

	/* cursor position */
	int editRow, editTrack;
	
	int scrollPosX,  scrollPosY;
	int windowWidth, windowHeight;
	int windowRows,  windowTracks;
	
	SyncDocument *document;
	
	QString editString;

	bool dragging;
	int anchorTrack;
};
