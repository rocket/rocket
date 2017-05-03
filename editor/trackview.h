#ifndef TRACKVIEW_H
#define TRACKVIEW_H

#include <QAbstractScrollArea>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QPen>

#include "synctrack.h"
#include "syncpage.h"

class QFontMetrics;
class QLineEdit;
class QStylePainter;
class SyncDocument;
class SyncPage;

class TrackView : public QAbstractScrollArea
{
	Q_OBJECT
public:
	TrackView(SyncPage *page, QWidget *parent);

	void setRows(int rows);
	int getRows() const;

	SyncDocument *getDocument();
	SyncTrack *getTrack(int index);
	int getTrackCount() const;

	void editEnterValue();
	void editBiasValue(float amount);
	void editToggleInterpolationType();

	int getEditRow() const { return editRow; }
	int getEditTrack() const { return editTrack; }

	void updateRow(int row)
	{
		internalSetEditRow(row, false);
	}

	void selectNone()
	{
		setSelection(QRect(QPoint(editTrack, editRow),
		                   QPoint(editTrack, editRow)));
	}

	void dirtyCurrentValue()
	{
		emit currValDirty();
	}

	void dirtyPosition()
	{
		emit posChanged(editTrack, editRow);
	}

	void setReadOnly(bool readOnly)
	{
		this->readOnly = readOnly;
	}

signals:
	void editRowChanged(int row);
	void posChanged(int col, int row);
	void currValDirty();

private slots:
	void onHScroll(int value);
	void onVScroll(int value);
	void onEditingFinished();
	void onTrackHeaderChanged(int trackIndex);
	void onTrackDataChanged(int trackIndex, int start, int stop);

public slots:
	void editUndo();
	void editRedo();
	void editCopy();
	void editCut();
	void editPaste();
	void editClear();

	void editPreviousBookmark();
	void editNextBookmark();

	void selectAll();
	void selectTrack();
	void selectRow();

private:

	void setEditTrack(int newEditTrack, bool selecting);
	void setEditRow(int newEditRow, bool selecting)
	{
		if (internalSetEditRow(newEditRow, selecting))
			emit editRowChanged(editRow);
	}

	bool internalSetEditRow(int row, bool selecting);

	/* paint helpers */
	void paintTopMargin(QStylePainter &painter, const QRegion &region);
	void paintLeftMargin(QStylePainter &painter, const QRegion &region);
	void paintTracks(QStylePainter &painter, const QRegion &region);
	void paintTrack(QStylePainter &painter, const QRegion &region, int track);

	void paintEvent(QPaintEvent *);
	void keyPressEvent(QKeyEvent *);
	void resizeEvent(QResizeEvent *);
	void mouseMoveEvent(QMouseEvent *);
	void mousePressEvent(QMouseEvent *);
	void mouseReleaseEvent(QMouseEvent *);
	void changeEvent(QEvent *);

	void setupScrollBars();
	void setScrollPos(int newScrollPosX, int newScrollPosY);
	void scrollWindow(int newScrollPosX, int newScrollPosY);

	void invalidateLeftMarginRow(int row)
	{
		QRect rect(QPoint(0,               getPhysicalY(row)),
		           QPoint(leftMarginWidth, getPhysicalY(row + 1) - 1));
		viewport()->update(rect);
	}

	QRect getSelection() const
	{
		return QRect(selectionStart, selectionStart).united(QRect(selectionEnd, selectionEnd));
	}

	void setSelection(const QRect &rect);
	void updateSelection(const QPoint &pos, bool selecting);

	QPen getInterpolationPen(SyncTrack::TrackKey::KeyType type);

	int getLogicalX(int track) const;
	int getLogicalY(int row) const;
	int getPhysicalX(int track) const;
	int getPhysicalY(int row) const;

	int getTrackFromLogicalX(int x) const;
	int getTrackFromPhysicalX(int x) const;
	int getRowFromLogicalY(int y) const;
	int getRowFromPhysicalY(int y) const;

	SyncPage *page;

	QPoint selectionStart;
	QPoint selectionEnd;

	int rowHeight;
	int trackWidth;
	int topMarginHeight;
	int leftMarginWidth;
	void updateFont(const QFontMetrics &fontMetrics);

	QBrush bgBaseBrush, bgDarkBrush;
	QBrush selectBaseBrush, selectDarkBrush;
	QPen rowPen, rowSelectPen;
	QBrush editBrush, bookmarkBrush;
	QPen stepPen, lerpPen, smoothPen, rampPen;
	QCursor handCursor;
	void updatePalette();

	/* cursor position */
	int editRow, editTrack;

	int scrollPosX,  scrollPosY;
	int windowRows;

	QLineEdit *lineEdit;

	bool readOnly;
	bool dragging;
	int anchorTrack;
};

#endif // !defined(TRACKVIEW_H)
