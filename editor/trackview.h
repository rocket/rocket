#ifndef TRACKVIEW_H
#define TRACKVIEW_H

#include <QAbstractScrollArea>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QPainter>

class QLineEdit;
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

	void setRows(int rows);
	int getRows() const;

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
		emit posChanged(editTrack, editRow);
	}

	bool paused, connected;

signals:
	void posChanged(int col, int row);
	void pauseChanged(bool paused);
	void currValDirty();

private slots:
	void onHScroll(int value);
	void onVScroll(int value);
	void onEditingFinished();

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
	void paintLeftMargin(QPainter &painter, const QRect &rcTracks);
	void paintTracks(QPainter &painter, const QRect &rcTracks);
	void paintTrack(QPainter &painter, const QRect &rcTracks, int track);

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

	void invalidateRange(int startTrack, int stopTrack, int startRow, int stopRow)
	{
		QRect rect(QPoint(getPhysicalX(qMin(startTrack, stopTrack)),
		                  getPhysicalY(qMin(startRow, stopRow))),
		           QPoint(getPhysicalX(qMax(startTrack, stopTrack) + 1) - 1,
		                  getPhysicalY(qMax(startRow, stopRow) + 1) - 1));
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

	void invalidateAll()
	{
		invalidateRange(0, getTrackCount(), 0, getRows());
	}

	QRect getSelection() const
	{
		return QRect(QPoint(qMin(selectStartTrack, selectStopTrack),
		                    qMin(selectStartRow, selectStopRow)),
		             QPoint(qMax(selectStartTrack, selectStopTrack),
		                    qMax(selectStartRow, selectStopRow)));
	}

	int getLogicalX(int track) const;
	int getLogicalY(int row) const;
	int getPhysicalX(int track) const;
	int getPhysicalY(int row) const;

	int getTrackFromLogicalX(int x) const;
	int getTrackFromPhysicalX(int x) const;

	int getTrackCount() const;

	int selectStartTrack, selectStopTrack;
	int selectStartRow, selectStopRow;

	int rowHeight;
	int trackWidth;
	int topMarginHeight;
	int leftMarginWidth;
	void updateFont();

	QBrush bgBaseBrush, bgDarkBrush;
	QBrush selectBaseBrush, selectDarkBrush;
	QPen rowPen, rowSelectPen;
	QBrush editBrush, bookmarkBrush;
	QPen lerpPen, cosinePen, rampPen;
	QCursor handCursor;
	void updatePalette();

	/* cursor position */
	int editRow, editTrack;
	
	int scrollPosX,  scrollPosY;
	int windowRows;
	
	SyncDocument *document;

	QLineEdit *lineEdit;

	bool dragging;
	int anchorTrack;
};

#endif // !defined(TRACKVIEW_H)
