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
	return 32;
}

Qt::ItemFlags TrackModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return 0;

	return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

QVariant TrackModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();

	QString str;
	switch (role) {
	case Qt::DisplayRole:
		if (index.row() % 4 != 0)
			return QVariant();
		return str.setNum(float(index.row()) / 16);

	case Qt::BackgroundRole:
		if (index.row() % 4 == 0 && index.column() % 2 == 0)
			return QBrush(QColor(255, 0, 0, 32));
		if (index.row() % 8 == 0)
			return QBrush(QColor(0, 0, 0, 16));
		return QVariant();

	case Qt::EditRole:
		if (index.row() % 4 != 0)
			return QVariant();
		return str.setNum(float(index.row()) / 16);
	}

	return QVariant();
}

bool TrackModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	bool ok;
	value.toFloat(&ok);

	if (!ok)
		return false;

	dataChanged(index, index);
	return true;
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
