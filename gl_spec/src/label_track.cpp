#include "label_track.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <cmath>

#include <QDebug>

// -------- SpanItem --------
SpanItem::SpanItem(double pps, double start, double end, QString tag, QGraphicsItem* parent)
    : QGraphicsObject(parent), pps_(pps), s_(start), e_(end), tag_(std::move(tag))
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    this->_ori_length_ = std::max(8.0, (e_-s_)*pps_);
    this->_length_ = this->_ori_length_;
    //setPos(s_*pps_, 20);
}

QRectF SpanItem::boundingRect() const {
    return QRectF(0, 0, this->_length_, 28);
}

void SpanItem::setPxPerSec(double pps){
    if (qFuzzyCompare(pps_, pps)) return;
    pps_ = pps;
    prepareGeometryChange();          // width depends on pps_
    // setPos(s_ * pps_, 20);             // left depends on pps_
    update();
}

void SpanItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) {
    const QRectF r = boundingRect();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(QPen(Qt::black, 1));
    p->setBrush(isSelected()? QColor("#aaccff") : QColor("#cfe3ff"));
    p->drawRoundedRect(r.adjusted(0.5,0.5,-0.5,-0.5), 4, 4);

    p->setBrush(Qt::white);
    p->drawRect(QRectF(0, 0, 6, r.height()));
    p->drawRect(QRectF(r.width()-6, 0, 6, r.height()));

    QFontMetrics fm(p->font());
    QString text = fm.elidedText(tag_, Qt::ElideRight, int(r.width()-12));
    p->drawText(r.adjusted(8,0,-8,0), Qt::AlignVCenter|Qt::AlignHCenter, text);
}


LabelTrackView::LabelTrackView(QWidget* parent) : QGraphicsView(parent) {

    setScene(&scene_);

    scene_.setSceneRect(0, 0, this->widthRec, 30);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);

    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    setViewportUpdateMode(BoundingRectViewportUpdate);
    setCacheMode(CacheBackground);

}

SpanItem* LabelTrackView::addSpan(double startSec, double endSec, const QString& tag) {

    double view_port_sec = 5.0;
    double pxPerSec = this->widthRec / view_port_sec;
    auto* it = new SpanItem(pxPerSec, startSec, endSec, tag);
    scene_.addItem(it);

    resetCachedContent();
    update();

    return it;
}

void LabelTrackView::setStartSec(double sec, double viewport) {
    this->start_sec = sec;
    this->viewport_sec = viewport;
    resetCachedContent();
    update();

    double pxPerSec = widthRec / viewport_sec;
    double S = start_sec;
    int i = 0;
    for (QGraphicsItem* gi : scene_.items()) {
        if (auto* it = qgraphicsitem_cast<SpanItem*>(gi)) {
            it->setPxPerSec(pxPerSec);

            double A = it->start();
            double B = it->end();
            bool visible = (B > S) && (A < S + 5.0);
            it->setVisible(visible);

            double L = std::max(A, S);
            double R = std::min(B, S + 5.0);
            double x = (L - S) * pxPerSec;
            double w = (R - L) * pxPerSec;

            i++;
            /*
            qDebug() << std::to_string(i).c_str() << ":"
                     << std::to_string(it->start()).c_str() << " - " << std::to_string(it->end()).c_str() << " > "
                     << std::to_string(x).c_str() << " ~ " << std::to_string(w).c_str();
            */
            it->setPos(x, 20);
        }
    }
}

// if (endSec <= startSec) endSec = startSec + 0.25; // guard: min 250ms
void LabelTrackView::drawBackground(QPainter* p, const QRectF& r) {
    p->fillRect(r, QColor("#d9d9d9"));
    this->widthRec = r.width();
    scene_.setSceneRect(0, 0, this->widthRec, 30);

    double pxPerSec = r.width() / viewport_sec;
    double pxForHalfSec = 0.5 * pxPerSec;

    //qDebug() << "pps__:" << std::to_string(pxPerSec).c_str();

    double curr_start_sec = start_sec + 0.5;
    p->setPen(QPen(Qt::black, 1));
    for (double t = r.left() + pxForHalfSec; t < r.right(); t += pxForHalfSec) {
        p->drawLine(t, r.top(), t, r.bottom());
        p->drawText(t+3, r.top()+12, QString::number(curr_start_sec)+"s");
        curr_start_sec += 0.5;
    }

    double S = start_sec;
    int i = 0;
    for (QGraphicsItem* gi : scene_.items()) {
        if (auto* it = qgraphicsitem_cast<SpanItem*>(gi)) {


            double A = it->start();
            double B = it->end();
            bool visible = (B > S) && (A < S + 5.0);
            it->setVisible(visible);

            double L = std::max(A, S);
            double R = std::min(B, S + 5.0);
            double x = (L - S) * pxPerSec;
            double w = (R - L) * pxPerSec;

            i++;
            /*
            qDebug() << std::to_string(i).c_str() << ":"
                     << std::to_string(it->start()).c_str() << " - " << std::to_string(it->end()).c_str() << " > "
                     << std::to_string(x).c_str() << " ~ " << std::to_string(w).c_str();
            */
            it->setPos(x, 20);
            it->setPxPerSec(pxPerSec);
            it->setNewLength(w);
        }
    }
}
