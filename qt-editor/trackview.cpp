#include "trackview.h"

#include <QtGui/QApplication>
#include <QtGui/QPainter>
#include <QtGui/QMenuBar>
#include <QtGui/QStatusBar>
#include <QtGui/QKeyEvent>
#include <QtGui/QScrollBar>

TrackView::TrackView(QWidget *parent) :
	QAbstractScrollArea(parent),
	font("Fixedsys"),
	currRow(0),
	currCol(0)
{
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	verticalScrollBar()->setPageStep(1);
	verticalScrollBar()->setRange(0, 100);
	setViewportMargins(16 * 4, 16, 0, 0);
}

void TrackView::scrollContentsBy(int dx, int dy)
{
	viewport()->scroll(dx, dy);
}

void TrackView::keyPressEvent(QKeyEvent *event)
{
	switch (event->key()) {
	case Qt::Key_Up:
		if (currRow > 0) {
			currRow--;
			viewport()->update();
		} else
			QApplication::beep();
		break;

	case Qt::Key_Down:
		currRow++;
		viewport()->update();
		break;

	case Qt::Key_PageUp:
		if (currRow > 0) {
			currRow = qMax(0, currRow - 16);
			viewport()->update();
		} else
			QApplication::beep();
		break;

	case Qt::Key_PageDown:
		currRow = currRow + 16;
		viewport()->update();
		break;

	case Qt::Key_Left:
		if (currCol > 0) {
			currCol--;
			viewport()->update();
		} else
			QApplication::beep();
		break;

	case Qt::Key_Right:
		currCol++;
		viewport()->update();
		break;
	}
}

void TrackView::paintEvent(QPaintEvent *event)
{
	QPainter painter(viewport());

	painter.fillRect(0, 0, width(), height(), palette().background());

	painter.setFont(font);

	int scrollX = -horizontalScrollBar()->value();
	int scrollY = -verticalScrollBar()->value();

	for (int c = 0; c < 3; ++c) {

		QRect rect;
		rect.setLeft(scrollX + c * painter.fontMetrics().width(' ') * 8);
		rect.setRight(scrollX + (c + 1) * painter.fontMetrics().width(' ') * 8);

		for (int r = 0; r < 32; ++r) {
			rect.setTop(scrollY + r * painter.fontMetrics().lineSpacing());
			rect.setBottom(scrollY + (r + 1) * painter.fontMetrics().lineSpacing());

			if (c == currCol && r == currRow)
				painter.fillRect(rect, palette().highlight());
			else
				painter.fillRect(rect, r % 8 ?
					palette().base() :
					palette().alternateBase());

			QString temp = QString("---");
			if (!(r % 4))
				temp.setNum(float(r) / 8, 'f', 4);
			painter.drawText(rect, temp);
		}
	}
}
