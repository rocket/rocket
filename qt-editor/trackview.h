#ifndef TRACKVIEW_H
#define TRACKVIEW_H

#include <QtGui/QAbstractScrollArea>

class TrackView : public QAbstractScrollArea {
	Q_OBJECT

public:
	TrackView(QWidget *parent = 0);

protected:
	void paintEvent(QPaintEvent *event);
	void keyPressEvent(QKeyEvent *event);

	void scrollContentsBy(int dx, int dy);

	QFont font;
	int currRow, currCol;
};

#endif // TRACKVIEW_H
