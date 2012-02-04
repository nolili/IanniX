#ifndef NXGROUP_H
#define NXGROUP_H

#include <QObject>
#include <QTreeWidgetItem>
#include "nxtrigger.h"
#include "nxcurve.h"
#include "nxcursor.h"

inline uint qHash(const QRect &rect) {
    uint code = 0;

    code |= (rect.x() << 16);
    code |= (rect.y() & 0x0000ffff);
    code ^= (rect.width() << 16);
    code ^= (rect.height() & 0x0000ffff);

    return code;
}

class NxGroup : public QObject, public NxObjectDispatchProperty, public QTreeWidgetItem {
    Q_OBJECT
public:
    explicit NxGroup(NxObjectFactoryInterface *parent, QTreeWidget *parentItem);
    inline void dispatchProperty(const QString & property, const QVariant & value) {
        //Browse active/inactive objects
        for(quint16 activityIterator = 0 ; activityIterator < ObjectsActivityLenght ; activityIterator++) {

            //Browse all types of objects
            for(quint16 typeIterator = 0 ; typeIterator < ObjectsTypeLength ; typeIterator++) {

                //Browse locals cursors
                QHashIterator< QRect, QHash<quint16, NxObject*> > localIterator(objects[activityIterator][typeIterator]);
                while (localIterator.hasNext()) {
                    localIterator.next();
                    QRect local = localIterator.key();

                    //Browse objects
                    QHashIterator<quint16, NxObject*> objectIterator(objects[activityIterator][typeIterator].value(local));
                    while (objectIterator.hasNext()) {
                        objectIterator.next();
                        NxObject *object = objectIterator.value();
                        object->dispatchProperty(property, value);
                    }
                }
            }
        }
    }
    inline const QVariant getProperty(const QString &) const {
        return id;
    }
    inline quint8 getType() const {
        return ObjectsTypeGroup;
    }
    inline const QString getTypeStr() const {
        return "group";
    }
    inline const NxRect getBoundingRect() const {
        NxRect boundingRect;

        //Browse active/inactive objects
        for(quint16 activityIterator = 0 ; activityIterator < ObjectsActivityLenght ; activityIterator++) {

            //Browse all types of objects
            for(quint16 typeIterator = 0 ; typeIterator < ObjectsTypeLength ; typeIterator++) {

                //Browse locals cursors
                QHashIterator< QRect, QHash<quint16, NxObject*> > localIterator(objects[activityIterator][typeIterator]);
                while (localIterator.hasNext()) {
                    localIterator.next();
                    QRect local = localIterator.key();

                    //Browse objects
                    QHashIterator<quint16, NxObject*> objectIterator(objects[activityIterator][typeIterator].value(local));
                    while (objectIterator.hasNext()) {
                        objectIterator.next();
                        NxObject *object = objectIterator.value();
                        boundingRect = boundingRect.united(object->getBoundingRect());
                    }
                }
            }
        }
        return boundingRect;
    }
    inline quint16 getCount() const {
        quint16 counter = 0;
        //Browse active/inactive objects
        for(quint16 activityIterator = 0 ; activityIterator < ObjectsActivityLenght ; activityIterator++) {
            //Browse all types of objects
            for(quint16 typeIterator = 0 ; typeIterator < ObjectsTypeLength ; typeIterator++) {
                //Browse locals cursors
                QHashIterator< QRect, QHash<quint16, NxObject*> > localIterator(objects[activityIterator][typeIterator]);
                while (localIterator.hasNext()) {
                    localIterator.next();
                    QRect local = localIterator.key();
                    //Browse objects
                    QHashIterator<quint16, NxObject*> objectIterator(objects[activityIterator][typeIterator].value(local));
                    while (objectIterator.hasNext()) {
                        objectIterator.next();
                        counter++;
                    }
                }
            }
        }
        return counter;
    }

public:
    //activity + type + 3D grid localisation (x,y,width) + objectID = object !
    QHash<QRect, QHash<quint16, NxObject* > > objects[ObjectsActivityLenght][ObjectsTypeLength];

protected:
    QString id;
public slots:
    inline void setId(const QString & _id) {
        id = _id;
        setText(0, tr("Group") + " " + id);
    }
    inline const QString & getId() const {
        return id;
    }

};


#endif // NXGROUP_H
