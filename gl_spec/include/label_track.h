#ifndef LABEL_TRACK_H
#define LABEL_TRACK_H
#pragma once
#include <QGraphicsView>
#include <QGraphicsObject>

struct LabelSpan { double start, end; QString tag; };

class SpanItem : public QGraphicsObject {
    Q_OBJECT
public:
    SpanItem(double pps, double start, double end, QString tag, QGraphicsItem* parent=nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

    double start() const { return s_; }
    double end()   const { return e_;  }
    void setNewLength(double length) {
        this->_length_ = length;
        prepareGeometryChange();
        update();
    }

    void setPxPerSec(double pps);

// signals:
    // void spanChanged(double start, double end);

private:
    // state
    double s_, e_, pps_, _ori_length_, _length_;
    QString tag_;
    enum Drag { None, Move, Left, Right } drag_ = None;
    QPointF grab_;

};

class LabelTrackView : public QGraphicsView {
    Q_OBJECT
public:
    explicit LabelTrackView(QWidget* parent=nullptr);

    void setStartSec(double sec, double viewport);

    static inline double pxPerSecOf(const QGraphicsView* v, double viewport_sec) {
        const int w = v->viewport() ? v->viewport()->width() : v->width();
        return w > 0 ? double(w) / viewport_sec : 1.0;
    }

    SpanItem* addSpan(double startSec, double endSec, const QString& tag);

protected:
    void drawBackground(QPainter*, const QRectF&) override;
private:
    QGraphicsScene scene_;
    double start_sec = 0.0;
    double viewport_sec = 5.0;
    double widthRec = 100;
};

#endif // LABEL_TRACK_H
