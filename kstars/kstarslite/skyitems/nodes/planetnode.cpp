/** *************************************************************************
                          planetnode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 05/05/2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QSGSimpleTextureNode>
#include <QImage>

#include <QQuickWindow>
#include "skymaplite.h"
#include "ksplanetbase.h"

#include "planetnode.h"


PlanetNode::PlanetNode(KSPlanetBase* p, PlanetItemNode* parentNode)
    :m_planetPic(new QSGSimpleTextureNode), m_planet(p), m_planetOpacity(new QSGOpacityNode)
{
    // Draw them as bright stars of appropriate color instead of images
    char spType;
    //FIXME: do these need i18n?
    if( m_planet->name() == i18n("Mars") ) {
        spType = 'K';
    } else if( m_planet->name() == i18n("Jupiter") || m_planet->name() == i18n("Mercury") || m_planet->name() == i18n("Saturn") ) {
        spType = 'F';
    } else {
        spType = 'B';
    }

    m_point = new PointNode(spType, parentNode);
    appendChildNode(m_point);

    appendChildNode(m_planetOpacity);
    //Add planet to opacity node so that we could hide the planet
    m_planetOpacity->appendChildNode(m_planetPic);
    m_planetPic->setTexture(SkyMapLite::Instance()->window()->createTextureFromImage(
                              m_planet->image(), QQuickWindow::TextureCanUseAtlas));
}

void PlanetNode::setPointSize(float size) {
    m_point->setSize(size);
}

void PlanetNode::setPlanetPicSize(float size) {
    m_planetPic->setRect(QRect(0,0,size,size));
    markDirty(QSGNode::DirtyGeometry);
}

void PlanetNode::showPoint() {
    m_planetOpacity->setOpacity(0);
    m_point->setOpacity(1);
}

void PlanetNode::showPlanetPic() {
    m_point->setOpacity(0);
    m_planetOpacity->setOpacity(1);
}

void PlanetNode::hide() {
    m_point->setOpacity(0);
    m_planetOpacity->setOpacity(0);
}

void PlanetNode::changePos(QPointF pos) {
    //TODO: port it to transform matrix
    /*QMatrix4x4 m (1,0,0,pos.x(),
                  0,1,0,pos.y(),
                  0,0,0,1,
                  0,0,0,1);
    setMatrix(m);
    markDirty(QSGNode::DirtyMatrix);*/
    m_planetPic->setRect(QRect(pos.x(),pos.y(),m_planetPic->rect().size().width(),m_planetPic->rect().size().width()));
    m_point->changePos(pos);

}