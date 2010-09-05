#include "trackview.h"

#include <QtGui/QApplication>
#include <QtGui/QPainter>
#include <QtGui/QMenuBar>
#include <QtGui/QStatusBar>
#include <QtGui/QKeyEvent>
#include <QtGui/QScrollBar>


TrackModel::TrackModel(QObject *parent)
 : QAbstractTableModel(parent)
{
}

int TrackModel::rowCount(const QModelIndex & /* parent */) const
{
	return 128;
}

int TrackModel::columnCount(const QModelIndex & /* parent */) const
{
	return 3;
}

QVariant TrackModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || role != Qt::DisplayRole)
		return QVariant();
	if (index.row() % 4 != 0)
		return QVariant();
	return QVariant(float(index.row()) / 16);
}

QVariant TrackModel::headerData(int section,
	Qt::Orientation orientation,
	int role) const
{
/*	if (role == Qt::SizeHintRole)
		return QSize(1, 1); */
	if (role == Qt::DisplayRole) {
		if (orientation == Qt::Horizontal)
			return QVariant(QString("header %1").arg(section));
		else
			return QVariant(QString("%1").arg(section, 4, 16, QLatin1Char('0')));
	}
	return QVariant();
}

TrackView::TrackView(QWidget *parent) :
	QTableView(parent)
{
	this->setModel(new TrackModel());
}
