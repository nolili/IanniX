#include "uirender.h"
#include "ui_uirender.h"

UiRender::UiRender(QWidget *parent) :
    QGLWidget(parent),
    ui(new Ui::UiRender) {
    //Initialize view
    ui->setupUi(this);
    setCursor(Qt::OpenHandCursor);
    grabGesture(Qt::PinchGesture);

    //Initialisations
    factory = 0;
    document = 0;
    setDocument(0);
    setMouseTracking(true);
    editing = false;
    mousePressed = false;
    mouseShift = false;
    mouseObjectDrag = false;
    mouseSnap = false;
    selectedHover = 0;
    scale = 3;
    setCanObjectDrag(true);

    //Render options
    renderOptions = new UiRenderOptions(this);
    renderOptions->render = this;
    renderOptions->renderFont = QFont("Arial");
    renderOptions->renderFont.setPixelSize(11);
    setTriggerAutosize(true);
    renderOptions->paintAxisGrid = true;
    renderOptions->paintAxisMain = true;
    renderOptions->paintBackground = true;
    renderOptions->paintCursors = true;
    renderOptions->paintCurves = true;
    renderOptions->paintTriggers = true;
    renderOptions->paintLabel = true;
    renderOptions->axisGrid = 1;

    renderOptions->colors["background"] = QColor(255, 255, 255, 255);
    renderOptions->colors["grid"] = QColor(63, 63, 63, 255);
    renderOptions->colors["axis"] = renderOptions->colors["grid"].lighter(150);
    renderOptions->colors["selection"] = QColor(128, 128, 128, 255);
    renderOptions->colors["object_selection"] = QColor(255, 240, 34, 255);
    renderOptions->colors["object_hover"] = QColor(33, 255, 166, 255);
    renderOptions->colors["debug"] = QColor(100, 100, 100, 255);

    renderOptions->colors["trigger_active"] = QColor(0, 187, 255, 255);
    renderOptions->colors["trigger_inactive"] = QColor(255, 255, 255, 92);
    renderOptions->colors["trigger_active_message"] = QColor(0, 187, 255, 255);
    renderOptions->colors["trigger_inactive_message"] = QColor(255, 255, 255, 92);

    renderOptions->colors["curve_active"] = QColor(255, 255, 255, 175);
    renderOptions->colors["curve_inactive"] = QColor(255, 255, 255, 92);
    renderOptions->colors["cursor_active"] = QColor(255, 81, 31, 255);
    renderOptions->colors["cursor_inactive"] = QColor(255, 255, 255, 92);

    zoom(100);

    //Refresh
    timePerfRefresh = 0;
    timePerfCounter = 0;
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(timerEvent()));
    timer->start(25);
}
UiRender::~UiRender() {
    delete ui;
}
void UiRender::changeEvent(QEvent *e) {
    QGLWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}


//Force refresh
void UiRender::timerEvent() {
    updateGL();
}

//Setters
void UiRender::setDocument(NxDocument *_document) {
    document = _document;
}

void UiRender::loadTexture(const QString & name, const QString & filename, const NxRect & mapping) {
    if(QFile::exists(filename)) {
        GLuint texture = 0;

        glEnable(GL_TEXTURE_2D);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        QImage tex = QGLWidget::convertToGLFormat(QImage(filename));
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex.width(), tex.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.bits());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glDisable(GL_TEXTURE_2D);

        renderOptions->textures[name].texture = texture;
        renderOptions->textures[name].mapping = mapping;
    }
    else
        qDebug("Can't load %s", qPrintable(filename));
}

void UiRender::centerOn(const NxPoint & center) {
    if(!renderOptions->axisArea.contains(center)) {
        renderOptions->axisCenter = -center;
        zoom();
        resizeGL();
        updateGL();
    }
}

void UiRender::rotateTo(const NxPoint & rotation) {
    setRotation(rotation);
    zoom();
    resizeGL();
    updateGL();
}

//Initialize event
void UiRender::initializeGL() {
    //OpenGL options
    glEnable(GL_POLYGON_SMOOTH);
    glEnable(GL_LINE_SMOOTH);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    //Force resize
    resizeGL();
}

//Resize event
void UiRender::resizeGL(int width, int height) {
    //Calculate area
    renderOptions->axisArea = NxRect(NxPoint(), NxSize(10, 10));
    if(width > height) {
        if(renderOptions->axisArea.width() > -renderOptions->axisArea.height())
            renderOptions->axisArea.setHeight(-renderOptions->axisArea.width() * (qreal)height/(qreal)width);
        else
            renderOptions->axisArea.setWidth(-renderOptions->axisArea.height() * (qreal)width/(qreal)height);
    }
    else {
        if(renderOptions->axisArea.width() > -renderOptions->axisArea.height())
            renderOptions->axisArea.setHeight(-renderOptions->axisArea.width() * (qreal)height/(qreal)width);
        else
            renderOptions->axisArea.setWidth(-renderOptions->axisArea.height() * (qreal)width/(qreal)height);
    }
    renderOptions->axisArea.setWidth(renderOptions->axisArea.width()   * renderOptions->zoomLinear);
    renderOptions->axisArea.setHeight(renderOptions->axisArea.height() * renderOptions->zoomLinear);
    renderOptions->axisArea.translate(-NxPoint(renderOptions->axisArea.size().width()/2, renderOptions->axisArea.size().height()/2));

    renderOptions->axisArea.translate(-renderOptions->axisCenter);

    //With other parameters
    renderOptions->axisAreaSearch.setTopLeft(QPoint(floor(renderOptions->axisArea.topLeft().x() / 10.0F) * 10 - 1, ceil(renderOptions->axisArea.topLeft().y() / 10.0F) * 10 + 1));
    renderOptions->axisAreaSearch.setBottomRight(QPoint(ceil(renderOptions->axisArea.bottomRight().x() / 10.0F) * 10 + 1, floor(renderOptions->axisArea.bottomRight().y() / 10.0F) * 10 - 1));

    //Set axis
    glViewport(0, 0, (GLint)width, (GLint)height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(renderOptions->axisArea.left(), renderOptions->axisArea.right(), renderOptions->axisArea.bottom(), renderOptions->axisArea.top(), 50, 250.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0, 0.0, -150);
    glRotatef(rotation.y(), 1, 0, 0);
    glRotatef(rotation.x(), 0, 1, 0);
    glRotatef(rotation.z(), 0, 0, 1);
    glScalef(scale, scale, scale);
    glTranslatef(translation.x(), translation.y(), 0);
    if((rotation.x() == 0) && (rotation.y() == 0) && (rotation.z() == 0))
        renderOptions->allowSelection = true;
    else
        renderOptions->allowSelection = false;

    renderOptions->axisArea.translate(renderOptions->axisCenter);
}

//Paint event
void UiRender::paintGL() {
    //Start measure
    timePerfRefresh += renderMeasure.elapsed() / 1000.0F;
    timePerfCounter++;
    renderMeasure.start();
    /*
    if(factory)
        factory->onDraw();
    */

    //Clear
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if((factory) && (document)) {
        //Background
        paintBackground();

        //Paint axis
        paintAxisGrid();

        //Paint selection
        paintSelection();

        //Draw objects
        //Browse documents
        QRect local;
        QMapIterator<QString, NxGroup*> groupIterator(document->groups);
        while (groupIterator.hasNext()) {
            groupIterator.next();
            NxGroup *group = groupIterator.value();
            if(group->checkState(0) == Qt::Checked)
                renderOptions->paintThisGroup = true;
            else
                renderOptions->paintThisGroup = false;

            //Browse active/inactive objects
            for(quint16 activityIterator = 0 ; activityIterator < ObjectsActivityLenght ; activityIterator++) {

                //Browse all types of objects
                for(quint16 typeIterator = 0 ; typeIterator < ObjectsTypeLength ; typeIterator++) {

                    //Browse locals cursors
                    QHashIterator< QRect, QHash<quint16, NxObject*> > localIterator(group->objects[activityIterator][typeIterator]);
                    while (localIterator.hasNext()) {
                        localIterator.next();
                        local = localIterator.key();

                        //Is object in viewport ?
                        if((typeIterator != ObjectsTypeTrigger) || (renderOptions->axisAreaSearch.contains(local.x(), local.y()))) {
                            //Browse objects
                            QHashIterator<quint16, NxObject*> objectIterator(group->objects[activityIterator][typeIterator].value(local));
                            while (objectIterator.hasNext()) {
                                objectIterator.next();
                                NxObject *object = objectIterator.value();

                                //Draw the object
                                object->paint();
                            }
                        }
                    }
                }
            }
        }

        if(timePerfRefresh >= 1) {
            emit(setPerfOpenGL(QString().setNum((quint16)qRound(timePerfCounter/timePerfRefresh))));
            timePerfRefresh = 0;
            timePerfCounter = 0;
        }
#ifdef KINECT_INSTALLED
        factory->kinect->paint();
#endif
    }
}


//Draw background
void UiRender::paintBackground() {
    if(renderOptions->paintBackground) {
        Texture texture = renderOptions->textures.value("background");
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture.texture);
        glBegin(GL_QUADS);
        glColor4f(renderOptions->colors.value("background").redF(), renderOptions->colors.value("background").greenF(), renderOptions->colors.value("background").blueF(), renderOptions->colors.value("background").alphaF());
        glLineWidth(1);
        glTexCoord2d(0, 0); glVertex3f(texture.mapping.left() , texture.mapping.bottom(), 0);
        glTexCoord2d(1, 0); glVertex3f(texture.mapping.right(), texture.mapping.bottom(), 0);
        glTexCoord2d(1, 1); glVertex3f(texture.mapping.right(), texture.mapping.top(), 0);
        glTexCoord2d(0, 1); glVertex3f(texture.mapping.left() , texture.mapping.top(), 0);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    }
}


//Draw grid axis
void UiRender::paintAxisGrid() {
    if(renderOptions->paintAxisGrid) {
        //Draw axis
        renderOptions->axisArea.translate(-renderOptions->axisCenter);

        for(qreal x = 0 ; x < ceil(renderOptions->axisArea.right()) ; x += renderOptions->axisGrid) {
            if((x == 0) && (renderOptions->paintAxisMain)) {
                glColor4f(renderOptions->colors.value("axis").redF(), renderOptions->colors.value("axis").greenF(), renderOptions->colors.value("axis").blueF(), renderOptions->colors.value("axis").alphaF());
                glLineWidth(2);
            }
            else {
                glColor4f(renderOptions->colors.value("grid").redF(), renderOptions->colors.value("grid").greenF(), renderOptions->colors.value("grid").blueF(), renderOptions->colors.value("grid").alphaF());
                glLineWidth(1);
            }
            glBegin(GL_LINES);
            glVertex3f(x, renderOptions->axisArea.bottom(), 0);
            glVertex3f(x, renderOptions->axisArea.top(), 0);
            glEnd();
        }
        for(qreal x = 0 ; x > floor(renderOptions->axisArea.left()) ; x -= renderOptions->axisGrid) {
            if((x == 0) && (renderOptions->paintAxisMain)) {
                glColor4f(renderOptions->colors.value("axis").redF(), renderOptions->colors.value("axis").greenF(), renderOptions->colors.value("axis").blueF(), renderOptions->colors.value("axis").alphaF());
                glLineWidth(2);
            }
            else {
                glColor4f(renderOptions->colors.value("grid").redF(), renderOptions->colors.value("grid").greenF(), renderOptions->colors.value("grid").blueF(), renderOptions->colors.value("grid").alphaF());
                glLineWidth(1);
            }
            glBegin(GL_LINES);
            glVertex3f(x, renderOptions->axisArea.bottom(), 0);
            glVertex3f(x, renderOptions->axisArea.top(), 0);
            glEnd();
        }
        for(qreal y = 0 ; y < ceil(renderOptions->axisArea.top()) ; y += renderOptions->axisGrid) {
            if((y == 0) && (renderOptions->paintAxisMain)) {
                glColor4f(renderOptions->colors.value("axis").redF(), renderOptions->colors.value("axis").greenF(), renderOptions->colors.value("axis").blueF(), renderOptions->colors.value("axis").alphaF());
                glLineWidth(2);
            }
            else {
                glColor4f(renderOptions->colors.value("grid").redF(), renderOptions->colors.value("grid").greenF(), renderOptions->colors.value("grid").blueF(), renderOptions->colors.value("grid").alphaF());
                glLineWidth(1);
            }
            glBegin(GL_LINES);
            glVertex3f(renderOptions->axisArea.left(), y, 0);
            glVertex3f(renderOptions->axisArea.right(), y, 0);
            glEnd();
        }
        for(qreal y = 0 ; y > floor(renderOptions->axisArea.bottom()) ; y -= renderOptions->axisGrid) {
            if((y == 0) && (renderOptions->paintAxisMain)) {
                glColor4f(renderOptions->colors.value("axis").redF(), renderOptions->colors.value("axis").greenF(), renderOptions->colors.value("axis").blueF(), renderOptions->colors.value("axis").alphaF());
                glLineWidth(2);
            }
            else {
                glColor4f(renderOptions->colors.value("grid").redF(), renderOptions->colors.value("grid").greenF(), renderOptions->colors.value("grid").blueF(), renderOptions->colors.value("grid").alphaF());
                glLineWidth(1);
            }
            glBegin(GL_LINES);
            glVertex3f(renderOptions->axisArea.left(), y, 0);
            glVertex3f(renderOptions->axisArea.right(), y, 0);
            glEnd();
        }

        renderOptions->axisArea.translate(renderOptions->axisCenter);
    }
}

//Draw selection
void UiRender::paintSelection() {
    //Axis color
    glColor4f(renderOptions->colors.value("selection").redF(), renderOptions->colors.value("selection").greenF(), renderOptions->colors.value("selection").blueF(), 0.7);
    glLineWidth(1);

    //Draw axis
    glBegin(GL_QUADS);
    glVertex3f(renderOptions->selectionArea.left(), renderOptions->selectionArea.top(), 0);
    glVertex3f(renderOptions->selectionArea.right(), renderOptions->selectionArea.top(), 0);
    glVertex3f(renderOptions->selectionArea.right(), renderOptions->selectionArea.bottom(), 0);
    glVertex3f(renderOptions->selectionArea.left(), renderOptions->selectionArea.bottom(), 0);
    glVertex3f(renderOptions->selectionArea.left(), renderOptions->selectionArea.top(), 0);
    glEnd();
}


void UiRender::wheelEvent(QWheelEvent *event) {
    bool mouse3D = event->modifiers() & Qt::AltModifier;

    //Zoom calculation
    if(mouse3D) {
        scale = qMax((qreal)0, scale - (qreal)event->delta() / 150.0F);
        refresh();
    }
    else if(event->modifiers() & Qt::ShiftModifier)
        zoom(renderOptions->zoomValue - (qreal)event->delta() / 3.0F);
    else
        zoom(renderOptions->zoomValue - (qreal)event->delta() / 15.0F);
}
void UiRender::mousePressEvent(QMouseEvent *event) {
    setFocus();

    //Save state when pressed
    mousePressedRawPos = NxPoint(event->posF().x(), event->posF().y());
    //Mouse position
    mousePressedAreaPosNoCenter = NxPoint((event->posF().x() - (qreal)size().width()/2) / (qreal)size().width() * renderOptions->axisArea.width(), (event->posF().y() - (qreal)size().height()/2) / (qreal)size().height() * renderOptions->axisArea.height());
    mousePressedAreaPos = mousePressedAreaPosNoCenter - renderOptions->axisCenter;
    if(mouseSnap) {
        mousePressedAreaPosNoCenter = NxPoint(qRound(mousePressedAreaPosNoCenter.x() / renderOptions->axisGrid) * renderOptions->axisGrid, qRound(mousePressedAreaPosNoCenter.y() / renderOptions->axisGrid) * renderOptions->axisGrid);
        mousePressedAreaPos = NxPoint(qRound(mousePressedAreaPos.x() / renderOptions->axisGrid) * renderOptions->axisGrid, qRound(mousePressedAreaPos.y() / renderOptions->axisGrid) * renderOptions->axisGrid);
    }

    mousePressed = true;
    mousePressedAxisCenter = renderOptions->axisCenter;
    mouseCommand = (event->modifiers() & Qt::ControlModifier);
    mouseShift = (event->modifiers() & Qt::ShiftModifier);
    rotationDrag = rotation;
    translationDrag = translation;

    if(cursor().shape() == Qt::BlankCursor)
        return;

    //Start area selection
    if((editing) && (renderOptions->allowSelection)) {
        foreach(NxObject *selected, selection)   ///CG/// Adding an object should clear the selection and select the added object
            selected->setSelected(false);
        selection.clear();
        //selectedHover->setSelected(true);   /// select this one
        //selectionAdd(selectedHover);
        if(editingMode == EditingModeFree) {
            //emit(editingStart(mousePressedAreaPos));
            if(editingFirstPoint)
                emit(editingStart(mousePressedAreaPos));
            else
                emit(editingMove(mousePressedAreaPos, true));
        }
        else if(editingMode == EditingModePoint) {
            if(editingFirstPoint)
                emit(editingStart(mousePressedAreaPos));
            else
                emit(editingMove(mousePressedAreaPos, true));
        }
        else if(editingMode == EditingModeTriggers) {
            emit(editingStart(mousePressedAreaPos));
        }
        else if(editingMode == EditingModeCircle) {
            emit(editingStart(mousePressedAreaPos));
        }
        editingFirstPoint = false;
    }
    else {
        if(mouseShift) {
            renderOptions->selectionArea.setTopLeft(mousePressedAreaPos);
            renderOptions->selectionArea.setBottomRight(mousePressedAreaPos);
            if(cursor().shape() != Qt::BlankCursor)
                setCursor(Qt::CrossCursor);
        } else if (selectedHover) {
            if (!selectedHover->getSelected()) {  ///CG/// Started dragging an unselected object
                foreach(NxObject *selected, selection)
                    selected->setSelected(false);
                selection.clear();  //deselect any others that may be selected
                selectedHover->setSelected(true);   /// select this one
                selectionAdd(selectedHover);
            }
        }
        else if((!selectedHover) && (cursor().shape() != Qt::BlankCursor))
            setCursor(Qt::ClosedHandCursor);
    }
}
void UiRender::mouseReleaseEvent(QMouseEvent *event) {
    //Edition
    if((editing) && (renderOptions->allowSelection) && (cursor().shape() != Qt::BlankCursor)) {
        /*
        if(editingMode == EditingModeFree) {
            emit(editingStop());
            editing = false;
        }
        */
    }
    else if((cursor().shape() != Qt::BlankCursor) && (renderOptions->allowSelection)) {
        //Copy area selection
        foreach(NxObject *selected, selectionRect)
            selectionAdd(selected);
        selectionRect.clear();

        //End of drag
        foreach(NxObject* object, selection)
            object->dragStop();
        if(selectedHover)
            selectedHover->dragStop();

        if (mouseObjectDrag) {   ///CG///

        }
        emit(selectionChanged());

        //Simple clic
        if(mousePressedRawPos == NxPoint(event->posF().x(), event->posF().y())) {
            if(selectedHover) {
                //Click on an object
                if(!mouseShift)
                    selectionClear();

                //Invert selection
                if(selectedHover->getSelected()) {
                    selectedHover->setSelected(false);
                    selection.removeOne(selectedHover);
                }
                else
                    selectionAdd(selectedHover);
            }
            else if((!selectedHover) && (!mouseShift))
                //Click on the workspace
                selectionClear();
        }
    }

    //Release states
    mousePressed = false;
    mouseShift = false;
    mouseObjectDrag = false;
    renderOptions->selectionArea.setSize(NxSize(0, 0));
    if((cursor().shape() != Qt::BlankCursor) && !((editing) && ((editingMode == EditingModeFree) || (editingMode == EditingModePoint) || (editingMode == EditingModeTriggers) || (editingMode == EditingModeCircle)))) {
        if(mouseShift)
            setCursor(Qt::CrossCursor);
        else
            setCursor(Qt::OpenHandCursor);
    }
}
void UiRender::mouseMoveEvent(QMouseEvent *event) {
    //Mouse position
    bool mouse3D = event->modifiers() & Qt::AltModifier;
    NxPoint mousePosNoCenter = NxPoint((event->posF().x() - (qreal)size().width()/2) / (qreal)size().width() * renderOptions->axisArea.width(), (event->posF().y() - (qreal)size().height()/2) / (qreal)size().height() * renderOptions->axisArea.height());
    NxPoint mousePos = mousePosNoCenter - renderOptions->axisCenter, mousePosBackup = mousePos;
    NxPoint deltaMouseRaw = NxPoint(event->posF().x(), event->posF().y()) - mousePressedRawPos;

    if(mouseSnap)
        mousePos = NxPoint(qRound(mousePos.x() / renderOptions->axisGrid) * renderOptions->axisGrid, qRound(mousePos.y() / renderOptions->axisGrid) * renderOptions->axisGrid);
    bool noSelection = true;
    emit(mousePosChanged(mousePos));

    if((editing) && (cursor().shape() != Qt::BlankCursor) && (renderOptions->allowSelection)) {
        if((mousePressed) && (editingMode == EditingModeFree))
            emit(editingMove(mousePos, true));
        else if((!editingFirstPoint) && ((editingMode == EditingModeFree) || (editingMode == EditingModePoint))){
            emit(editingMove(mousePos, false));
        }
    }
    else {
        //Cursor
        if((cursor().shape() != Qt::BlankCursor) && (renderOptions->allowSelection)) {
            if((mouseShift) && (mousePressed))
                setCursor(Qt::CrossCursor);
            else if((selectedHover) && (!mousePressed))
                setCursor(Qt::PointingHandCursor);
            else if((!selectedHover) && (mousePressed))
                setCursor(Qt::ClosedHandCursor);
            else
                setCursor(Qt::OpenHandCursor);
        }

        //Mouse pressed
        if(mousePressed) {
            if(mouse3D) {
                if(mouseShift)
                    translation = translationDrag + 10 * NxPoint(deltaMouseRaw.x() / (qreal)size().width(), deltaMouseRaw.y() / (qreal)size().height());
                else
                    rotation = rotationDrag + 360 * NxPoint(0, deltaMouseRaw.y() / (qreal)size().height(), deltaMouseRaw.x() / (qreal)size().width());
                refresh();
            }
            else if((mouseShift) && (renderOptions->allowSelection) && (cursor().shape() != Qt::BlankCursor)) {
                //Selection rect
                renderOptions->selectionArea.setBottomRight(mousePos);
            }
            else if((isCanObjectDrag()) && (renderOptions->allowSelection) && ((selection.contains(selectedHover)) || (selectedHover) || (mouseObjectDrag)) && (!mouseCommand) && (cursor().shape() != Qt::BlankCursor)) {
                if(!mouseObjectDrag) {
                    mouseObjectDrag = true;
                    factory->pushSnapshot();

                    ///CG/// If current object is not selected the selection should be cleared and only this object should be dragged
                    //if (!selectedHover->getSelected()) {
                    //    foreach(NxObject* object, selection)
                    //        object->setSelected(false);
                    //    selectedHover->       setSelected(true);
                    //}

                    foreach(NxObject* object, selection)
                        object->dragStart();
                    if(selectedHover)
                        selectedHover->dragStart();
                }
                foreach(NxObject* object, selection)
                    object->drag(mousePos - mousePressedAreaPos);
                if(selectedHover)
                    selectedHover->drag(mousePos - mousePressedAreaPos);
            }
            else if(!mouseObjectDrag) {
                //New center
                renderOptions->axisCenter = mousePressedAxisCenter + NxPoint((mousePosNoCenter - mousePressedAreaPosNoCenter).x(), (mousePosNoCenter - mousePressedAreaPosNoCenter).y());

                //Update
                refresh();
            }
        }

        mousePos = mousePosBackup;
        if((factory) && (document) && (cursor().shape() != Qt::BlankCursor) && (renderOptions->allowSelection)) {
            QList<NxObject*> eligibleSelection;

            //Browse documents
            QMapIterator<QString, NxGroup*> groupIterator(document->groups);
            while (groupIterator.hasNext()) {
                groupIterator.next();
                NxGroup *group = groupIterator.value();

                //Is groups visible ?
                if(group->checkState(0) == Qt::Checked) {

                    //Browse active/inactive objects
                    for(quint16 activityIterator = 0 ; activityIterator < ObjectsActivityLenght ; activityIterator++) {

                        //Browse all types of objects
                        for(quint16 typeIterator = 0 ; typeIterator < ObjectsTypeLength ; typeIterator++) {

                            //Are objects visible ?
                            if(((typeIterator == ObjectsTypeCursor) && (renderOptions->paintCursors)) || ((typeIterator == ObjectsTypeCurve) && (renderOptions->paintCurves)) || ((typeIterator == ObjectsTypeTrigger) && (renderOptions->paintTriggers))) {

                                //Browse locals cursors
                                QHashIterator< QRect, QHash<quint16, NxObject*> > localIterator(group->objects[activityIterator][typeIterator]);
                                while (localIterator.hasNext()) {
                                    localIterator.next();
                                    QRect local = localIterator.key();

                                    //Is object in viewport ?
                                    if((typeIterator != ObjectsTypeTrigger) || (renderOptions->axisAreaSearch.contains(local.x(), local.y()))) {

                                        //Browse objects
                                        QHashIterator<quint16, NxObject*> objectIterator(group->objects[activityIterator][typeIterator].value(local));
                                        while (objectIterator.hasNext()) {
                                            objectIterator.next();
                                            NxObject *object = objectIterator.value();

                                            //Is Z visible ?
                                            if((renderOptions->paintZStart <= object->getPos().z()) && (object->getPos().z() <= renderOptions->paintZEnd)) {
                                                //Check selection
                                                if(object->isMouseHover(mousePos)) {
                                                    if(object->getType() == ObjectsTypeCurve)
                                                        eligibleSelection.prepend(object);
                                                    else
                                                        eligibleSelection.append(object);
                                                }

                                                //Add the object to selection if click+shift
                                                if(mouseShift) {
                                                    NxRect objectBoundingRect = object->getBoundingRect();
                                                    if(objectBoundingRect.width() == 0)  objectBoundingRect.setWidth(0.001);
                                                    if(objectBoundingRect.height() == 0) objectBoundingRect.setHeight(0.001);
                                                    if(renderOptions->selectionArea.intersects(objectBoundingRect)) {
                                                        selectionRect.append(object);
                                                        object->setSelected(true);
                                                    }
                                                    else {
                                                        selectionRect.removeOne(object);
                                                        if(!selection.contains(object))
                                                            object->setSelected(false);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            foreach(NxObject *object, eligibleSelection) {
                bool selected = false;
                if(object->getType() == ObjectsTypeCurve) {
                    NxCurve *curve = (NxCurve*)object;
                    if(curve->isMouseHover(mousePos)) {
                        selected = true;
                        if(curve->getSelected())
                            curve->isOnPathPoint(NxRect(mousePos - NxPoint(renderOptions->objectSize/2, renderOptions->objectSize/2) - curve->getPos(), mousePos + NxPoint(renderOptions->objectSize/2, renderOptions->objectSize/2) - curve->getPos()));
                    }

                }
                else if(object->isMouseHover(mousePos))
                    selected = true;

                //Add the object to selection
                if((selected) && (!mousePressed)) {
                    if((selectedHover) && (selectedHover != object))
                        selectedHover->setSelectedHover(false);
                    selectedHover = object;
                    selectedHover->setSelectedHover(true);
                    emit(selectionChanged());
                    noSelection = false;
                }
            }
            if((noSelection) && (selectedHover) && ((!mousePressed))) {
                selectedHover->setSelectedHover(false);
                selectedHover = 0;
            }
        }
    }
}

void UiRender::mouseDoubleClickEvent(QMouseEvent *event) {
    if(cursor().shape() == Qt::BlankCursor)
        return;

    bool mouse3D = event->modifiers() & Qt::AltModifier;
    bool mouseControl = event->modifiers() & Qt::ControlModifier;

    if(mouse3D) {
        scale = 3;
        rotation = NxPoint();
        translation = NxPoint();
        refresh();
    }
    else if((editing) && (renderOptions->allowSelection)) {
        emit(editingStop());
        editing = false;
    }
    else if((selectedHover) && (renderOptions->allowSelection) && (selectedHover->getType() == ObjectsTypeCurve)) {
        NxCurve *curve = (NxCurve*)selectedHover;
        curve->addMousePointAt(mousePressedAreaPos, mouseControl);
    }
    else {
        factory->askNxObject();
    }

}

void UiRender::keyPressEvent(QKeyEvent *event) {
    qreal translationUnit = 0.1;

    if(event->modifiers() & Qt::ShiftModifier) {
        translationUnit = 1;
        //setCursor(Qt::CrossCursor);
    }

    NxPoint translation;
    if(event->key() == Qt::Key_Left)
        translation = NxPoint(translationUnit, 0);
    else if(event->key() == Qt::Key_Right)
        translation = NxPoint(-translationUnit, 0);
    else if(event->key() == Qt::Key_Up)
        translation = NxPoint(0, -translationUnit);
    else if(event->key() == Qt::Key_Down)
        translation = NxPoint(0, translationUnit);
    else if(event->key() == Qt::Key_Escape) {
        emit(escFullscreen());
        emit(editingStop());
    }
    else if(event->key() == Qt::Key_S) {
        QImage snapshot = grabFrameBuffer(true);
        snapshot.save(QDesktopServices::storageLocation(QDesktopServices::DesktopLocation) + QString(QDir::separator()) + "IanniX_Capture_" + QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss") + ".png");
    }
    else if(event->key() == Qt::Key_Escape) {
        emit(editingStop());
        editing = false;
    }

    if((selection.count() > 0) && ((translation.x() != 0) || (translation.y() != 0))) {
        factory->pushSnapshot();
        foreach(NxObject* object, selection)
            object->setPos(object->getPos() - translation);
        emit(selectionChanged());
    }
    else {
        renderOptions->axisCenter += translation;
        zoom();
    }
}
bool UiRender::event(QEvent *event) {
    switch (event->type()) {
    case QEvent::Gesture: {
        QGestureEvent *gestureEvent = (QGestureEvent*)event;
        QList<QGesture*> gestures = gestureEvent->activeGestures();
        foreach(const QGesture *gesture, gestures) {
            if(gesture->gestureType() == Qt::PinchGesture) {
                QPinchGesture *pinch = (QPinchGesture*)gesture;
                if(pinch->state() == Qt::GestureStarted)
                    pinchValue = renderOptions->zoomValue;
                zoom(pinchValue * 1.0F / pinch->scaleFactor());
            }
        }
        return true;
    }
        break;
    default:
        break;
    }
    return QGLWidget::event(event);
}


void UiRender::selectionClear(bool hoverAussi) {
    //Clear selection
    foreach(NxObject *selected, selection)
        selected->setSelected(false);
    foreach(NxObject *selected, selectionRect)
        selected->setSelected(false);
    selection.clear();
    if(hoverAussi) {
        if(selectedHover)
            selectedHover->setSelectedHover(false);
        selectedHover = 0;
    }
    emit(selectionChanged());
}
void UiRender::selectionAdd(NxObject *object) {
    if(object) {
        object->setSelected(true);
        if(!selection.contains(object))
            selection.append(object);
        emit(selectionChanged());
    }
}

void UiRender::zoom() {
    renderOptions->zoomLinear = qMax((qreal)0.05, renderOptions->zoomValue / 100.F);
    /*
    qreal zoomSensibility = -5.0;
    renderOptions->zoomLinear = zoomSensibility / renderOptions->zoomLinear;
    renderOptions->zoomLinear = 1.0F / renderOptions->zoomLinear;
    if(renderOptions->zoomLinear < 1.0)
        renderOptions->zoomLinear = (exp(zoomSensibility * renderOptions->zoomLinear - zoomSensibility) - exp(-zoomSensibility)) / (1 - exp(-zoomSensibility));
    */
    renderOptions->objectSize = 0.15;
    if(triggerAutosize)
        renderOptions->objectSize *= renderOptions->zoomLinear;
    emit(mouseZoomChanged(100.0F / renderOptions->zoomLinear));
    refresh();
}
void UiRender::zoom(qreal axisZoom) {
    renderOptions->zoomValue = qMax((qreal)0, axisZoom);
    zoom();
}
void UiRender::zoomIn() {
    zoom(renderOptions->zoomValue - 5);
}
void UiRender::zoomOut() {
    zoom(renderOptions->zoomValue + 5);
}
void UiRender::zoomInitial() {
    zoom(100);
}



void UiRender::actionCopy() {
    QString copy = "";
    foreach(NxObject *object, selection)
        if(object->getType() != ObjectsTypeCursor)
            copy += object->serialize() + COMMAND_END;

    QApplication::clipboard()->setText(copy);
}
void UiRender::actionPaste() {
    factory->execute("pushSnapthot");
    if(factory) {
        selectionClear();
        QStringList paste = QApplication::clipboard()->text().split(COMMAND_END, QString::SkipEmptyParts);
        foreach(const QString & command, paste)
            factory->execute(command, true);
    }
}
void UiRender::actionDuplicate() {
    factory->execute("pushSnapthot");
    actionCopy();
    actionPaste();
}

void UiRender::actionCut() {
    factory->execute("pushSnapthot");
    actionCopy();
    QStringList commands;
    foreach(NxObject *object, selection)
        commands << COMMAND_REMOVE + " " + QString().setNum(object->getId()) + COMMAND_END;
    foreach(const QString & command, commands)
        factory->execute(command);
}

void UiRender::actionSelect_all() {
    if((factory) && (document)) {
        selectionClear();

        //Browse documents
        QMapIterator<QString, NxGroup*> groupIterator(document->groups);
        while (groupIterator.hasNext()) {
            groupIterator.next();
            NxGroup *group = groupIterator.value();

            //Is groups visible ?
            if(group->checkState(0) == Qt::Checked) {

                //Browse active/inactive objects
                for(quint16 activityIterator = 0 ; activityIterator < ObjectsActivityLenght ; activityIterator++) {

                    //Browse all types of objects
                    for(quint16 typeIterator = 0 ; typeIterator < ObjectsTypeLength ; typeIterator++) {

                        //Are objects visible ?
                        if(((typeIterator == ObjectsTypeCursor) && (renderOptions->paintCursors)) || ((typeIterator == ObjectsTypeCurve) && (renderOptions->paintCurves)) || ((typeIterator == ObjectsTypeTrigger) && (renderOptions->paintTriggers))) {

                            //Browse locals cursors
                            QHashIterator< QRect, QHash<quint16, NxObject*> > localIterator(group->objects[activityIterator][typeIterator]);
                            while (localIterator.hasNext()) {
                                localIterator.next();
                                QRect local = localIterator.key();

                                //Is object in viewport ?
                                if((typeIterator != ObjectsTypeTrigger) || (renderOptions->axisAreaSearch.contains(local.x(), local.y()))) {

                                    //Browse objects
                                    QHashIterator<quint16, NxObject*> objectIterator(group->objects[activityIterator][typeIterator].value(local));
                                    while (objectIterator.hasNext()) {
                                        objectIterator.next();
                                        NxObject *object = objectIterator.value();

                                        //Is Z visible ?
                                        if((renderOptions->paintZStart <= object->getPos().z()) && (object->getPos().z() <= renderOptions->paintZEnd)) {

                                            //Add the object to selection
                                            selectionAdd(object);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
void UiRender::actionDelete() {
    factory->pushSnapshot();
    QStringList commands;
    foreach(NxObject *object, selection)
        commands << COMMAND_REMOVE + " " + QString().setNum(object->getId());
    selectionClear();
    selectedHover = 0;
    foreach(const QString & command, commands)
        factory->execute(command);
}

void UiRender::actionSnapGrid() {
    mouseSnap = !mouseSnap;
}