/***************************************************************************
                          planetviewer.cpp  -  Display overhead view of the solar system
                             -------------------
    begin                : Sun May 25 2003
    copyright            : (C) 2003 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "planetviewer.h"

#include <stdlib.h> //needed for abs() on some platforms

#include <QFile>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QTextStream>
#include <QKeyEvent>
#include <QPaintEvent>

#include <kdebug.h>
#include <klocale.h>
#include <kglobal.h>
#include <kicon.h>
#include <KPlotAxis>
#include <KPlotObject>
#include <KPlotPoint>

#include "ui_planetviewer.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "ksnumbers.h"
#include "ksfilereader.h"
#include "skyobjects/ksplanetbase.h"
#include "skyobjects/ksplanet.h"
#include "skyobjects/kspluto.h"
#include "dms.h"
#include "widgets/timestepbox.h"

PlanetViewerUI::PlanetViewerUI( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

PlanetViewer::PlanetViewer(QWidget *parent)
        : KDialog( parent ), scale(1.0), isClockRunning(false), tmr(this)
{
    pw = new PlanetViewerUI( this );
    setMainWidget( pw );
    setCaption( i18n("Solar System Viewer") );
    setButtons( KDialog::Close );
    setModal( false );

    pw->map->setLimits( -48.0, 48.0, -48.0, 48.0 );
    pw->map->axis( KPlotWidget::BottomAxis )->setLabel( i18nc( "axis label for x-coordinate of solar system viewer.  AU means astronomical unit.", "X-position (AU)" ) );
    pw->map->axis( KPlotWidget::LeftAxis )->setLabel( i18nc( "axis label for y-coordinate of solar system viewer.  AU means astronomical unit.", "Y-position (AU)" ) );

    pw->TimeStep->setDaysOnly( true );
    pw->TimeStep->tsbox()->setValue( 1 ); //start with 1-day timestep

    pw->RunButton->setIcon( KIcon("arrow-right") );
    pw->ZoomInButton->setIcon( KIcon("zoom-in") );
    pw->ZoomOutButton->setIcon( KIcon("zoom-out") );
    pw->DateBox->setDate( ((KStars*)parent)->data()->lt().date() );

    resize( 500, 500 );
    pw->map->QWidget::setFocus(); //give keyboard focus to the plot widget for key and mouse events

    setCenterPlanet(QString());

    KStarsData *data = KStarsData::Instance();
    PlanetList.append( KSPlanetBase::createPlanet( KSPlanetBase::MERCURY ) );
    PlanetList.append( KSPlanetBase::createPlanet( KSPlanetBase::VENUS ) );
    PlanetList.append( new KSPlanet( data, "Earth" ) );
    PlanetList.append( KSPlanetBase::createPlanet( KSPlanetBase::MARS ) );
    PlanetList.append( KSPlanetBase::createPlanet( KSPlanetBase::JUPITER ) );
    PlanetList.append( KSPlanetBase::createPlanet( KSPlanetBase::SATURN ) );
    PlanetList.append( KSPlanetBase::createPlanet( KSPlanetBase::URANUS ) );
    PlanetList.append( KSPlanetBase::createPlanet( KSPlanetBase::NEPTUNE ) );
    PlanetList.append( KSPlanetBase::createPlanet( KSPlanetBase::PLUTO ) );

    ut = data->ut();
    KSNumbers num( ut.djd() );

    for ( uint i=0; i<9; ++i ) {
        PlanetList[i]->findPosition( &num, 0, 0 ); //NULL args: don't need geocent. coords.
        LastUpdate[i] = int( ut.date().toJulianDay() );
    }

    //The planets' update intervals are 0.25% of one period:
    UpdateInterval[0] = 0;
    UpdateInterval[1] = 0;
    UpdateInterval[2] = 0;
    UpdateInterval[3] = 1;
    UpdateInterval[4] = 5;
    UpdateInterval[5] = 13;
    UpdateInterval[6] = 38;
    UpdateInterval[7] = 75;
    UpdateInterval[8] = 113;

    QTimer::singleShot( 0, this, SLOT( initPlotObjects() ) );

    connect( &tmr, SIGNAL( timeout() ), SLOT( tick() ) );
    connect( pw->TimeStep, SIGNAL( scaleChanged(float) ), SLOT( setTimeScale(float) ) );
    connect( pw->RunButton, SIGNAL( clicked() ), SLOT( slotRunClock() ) );
    connect( pw->ZoomInButton, SIGNAL( clicked() ), SLOT( slotMapZoomIn() ) );
    connect( pw->ZoomOutButton, SIGNAL( clicked() ), SLOT( slotMapZoomOut() ) );
    connect( pw->DateBox, SIGNAL( valueChanged( const QDate & ) ), SLOT( slotChangeDate( const QDate & ) ) );
    connect( pw->TodayButton, SIGNAL( clicked() ), SLOT( slotToday() ) );
    connect( this, SIGNAL( closeClicked() ), SLOT( slotCloseWindow() ) );
}

PlanetViewer::~PlanetViewer()
{
}

QString PlanetViewer::planetName(uint i) const {
    return PlanetList[i]->name();
}

void PlanetViewer::tick() {
    //Update the time/date
    ut.setDJD( ut.djd() + scale*0.1 );
    pw->DateBox->setDate( ut.date() );

    updatePlanets();
}

void PlanetViewer::setTimeScale(float f) {
    scale = f/86400.; //convert seconds to days
}

void PlanetViewer::slotRunClock() {
    isClockRunning = !isClockRunning;

    if ( isClockRunning ) {
        pw->RunButton->setIcon( KIcon("media-playback-pause") );
        tmr.start( 100 );
        //		pw->DateBox->setEnabled( false );
    } else {
        pw->RunButton->setIcon( KIcon("arrow-right") );
        tmr.stop();
        //		pw->DateBox->setEnabled( true );
    }
}

void PlanetViewer::slotChangeDate( const QDate & ) {
    ut.setDate( pw->DateBox->date() );
    updatePlanets();
}

void PlanetViewer::slotCloseWindow() {
    //Stop the clock if it's running
    if ( isClockRunning ) {
        tmr.stop();
        isClockRunning = false;
    }
}

void PlanetViewer::updatePlanets() {
    KSNumbers num( ut.djd() );
    bool changed(false);

    //Check each planet to see if it needs to be updated
    for ( unsigned int i=0; i<9; ++i ) {
        if ( abs( int(ut.date().toJulianDay()) - LastUpdate[i] ) > UpdateInterval[i] ) {
            KSPlanetBase *p = PlanetList[i];
            p->findPosition( &num );

            double s, c, s2, c2;
            p->helEcLong()->SinCos( s, c );
            p->helEcLat()->SinCos( s2, c2 );
            QList<KPlotPoint*> points = planet[i]->points();
            points.at(0)->setX( p->rsun()*c*c2 );
            points.at(0)->setY( p->rsun()*s*c2 );

            if ( centerPlanet() == p->name() ) {
                QRectF dataRect = pw->map->dataRect();
                double xc = (dataRect.right() + dataRect.left())*0.5;
                double yc = (dataRect.bottom() + dataRect.top())*0.5;
                double dx = points.at(0)->x() - xc;
                double dy = points.at(0)->y() - yc;
                pw->map->setLimits( dataRect.x() + dx, dataRect.right() + dx,
                                    dataRect.y() + dy, dataRect.bottom() + dy );
            }

            LastUpdate[i] = int(ut.date().toJulianDay());
            changed = true;
        }
    }

    if ( changed ) pw->map->update();
}

void PlanetViewer::slotToday() {
    KStars *ks = (KStars*)parent();
    pw->DateBox->setDate( ks->data()->lt().date() );
}

void PlanetViewer::paintEvent( QPaintEvent* ) {
    pw->map->update();
}

void PlanetViewer::initPlotObjects() {
    // Planets
    ksun = new KPlotObject( Qt::yellow, KPlotObject::Points, 12, KPlotObject::Circle );
    ksun->addPoint( 0.0, 0.0 );
    pw->map->addPlotObject( ksun );

    //Read in the orbit curves
    KPlotObject *orbit[9];
    for ( unsigned int i=0; i<9; ++i ) {
        KSPlanetBase *p = PlanetList[i];
        orbit[i] = new KPlotObject( Qt::white, KPlotObject::Lines, 1.0 );

        QFile orbitFile;
        QString orbitFileName = ( p->isMajorPlanet() ? ((KSPlanet *)p)->untranslatedName().toLower() : p->name().toLower() ) + ".orbit";
        if ( KSUtils::openDataFile( orbitFile, orbitFileName ) ) {
            KSFileReader fileReader( orbitFile ); // close file is included
            double x,y;
            while ( fileReader.hasMoreLines() ) {
                QString line = fileReader.readLine();
                QStringList fields = line.split( ' ', QString::SkipEmptyParts );
                if ( fields.size() == 3 ) {
                    x = fields[0].toDouble();
                    y = fields[1].toDouble();
                    orbit[i]->addPoint( x, y );
                }
            }
        }

        pw->map->addPlotObject( orbit[i] );
    }

    for ( unsigned int i=0; i<9; ++i ) {
        KSPlanetBase *p = PlanetList[i];
        planet[i] = new KPlotObject( p->color(), KPlotObject::Points, 6, KPlotObject::Circle );

        double s, c;
        p->helEcLong()->SinCos( s, c );

        planet[i]->addPoint( p->rsun()*c, p->rsun()*s, p->translatedName() );
        pw->map->addPlotObject( planet[i] );
    }

    update();
}

void PlanetViewer::keyPressEvent( QKeyEvent *e ) {
    switch ( e->key() ) {
    case Qt::Key_Escape:
        close();
        break;
    default:
        e->ignore();
        break;
    }
}

void PlanetViewer::slotMapZoomIn() {
    pw->map->slotZoomIn();
}

void PlanetViewer::slotMapZoomOut() {
    pw->map->slotZoomOut();
}

#include "planetviewer.moc"
