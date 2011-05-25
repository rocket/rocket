#ifndef TRACKVIEW_H
#define TRACKVIEW_H

#include <QtGui/QTableView>
#include <QMap>

class TrackModel : public QAbstractTableModel
{
	Q_OBJECT

public:
	TrackModel(QObject *parent = 0);

	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	int columnCount(const QModelIndex &parent = QModelIndex()) const;

	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
	Qt::ItemFlags flags(const QModelIndex &index) const;

	bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
private:
    QMap<QModelIndex, float> datacontent;
};

class TrackView : public QTableView {
	Q_OBJECT

public:
	TrackView(QWidget *parent = 0);

protected:
};

#endif // TRACKVIEW_H
