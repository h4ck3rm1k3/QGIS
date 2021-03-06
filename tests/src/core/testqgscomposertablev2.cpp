/***************************************************************************
                         testqgscomposertablev2.cpp
                         ----------------------
    begin                : September 2014
    copyright            : (C) 2014 by Nyall Dawson
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsapplication.h"
#include "qgscomposition.h"
#include "qgscomposermap.h"
#include "qgscomposerattributetablev2.h"
#include "qgscomposerframe.h"
#include "qgsmapsettings.h"
#include "qgsvectorlayer.h"
#include "qgsvectordataprovider.h"
#include "qgsfeature.h"
#include "qgscompositionchecker.h"
#include "qgsfontutils.h"

#include <QObject>
#include <QtTest>

class TestQgsComposerTableV2: public QObject
{
    Q_OBJECT;
  private slots:
    void initTestCase();// will be called before the first testfunction is executed.
    void cleanupTestCase();// will be called after the last testfunction was executed.
    void init();// will be called before each testfunction is executed.
    void cleanup();// will be called after every testfunction.

    void attributeTableHeadings(); //test retrieving attribute table headers
    void attributeTableRows(); //test retrieving attribute table rows
    void attributeTableFilterFeatures(); //test filtering attribute table rows
    void attributeTableSetAttributes(); //test subset of attributes in table
    void attributeTableVisibleOnly(); //test displaying only visible attributes
    void attributeTableRender(); //test rendering attribute table

    void attributeTableExtend();
    void attributeTableRepeat();

  private:
    QgsComposition* mComposition;
    QgsComposerMap* mComposerMap;
    QgsMapSettings mMapSettings;
    QgsVectorLayer* mVectorLayer;
    QgsComposerAttributeTableV2* mComposerAttributeTable;
    QgsComposerFrame* mFrame1;
    QgsComposerFrame* mFrame2;
    QString mReport;

    //compares rows in mComposerAttributeTable to expected rows
    void compareTable( QList<QStringList> &expectedRows );
};

void TestQgsComposerTableV2::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();

  //create maplayers from testdata and add to layer registry
  QFileInfo vectorFileInfo( QString( TEST_DATA_DIR ) + QDir::separator() +  "points.shp" );
  mVectorLayer = new QgsVectorLayer( vectorFileInfo.filePath(),
                                     vectorFileInfo.completeBaseName(),
                                     "ogr" );

  //create composition with composer map
  mMapSettings.setLayers( QStringList() << mVectorLayer->id() );
  mMapSettings.setCrsTransformEnabled( false );
  mComposition = new QgsComposition( mMapSettings );
  mComposition->setPaperSize( 297, 210 ); //A4 portrait

  mComposerAttributeTable = new QgsComposerAttributeTableV2( mComposition, false );

  mFrame1 = new QgsComposerFrame( mComposition, mComposerAttributeTable, 5, 5, 100, 30 );
  mFrame2 = new QgsComposerFrame( mComposition, mComposerAttributeTable, 5, 40, 100, 30 );

  mFrame1->setFrameEnabled( true );
  mFrame2->setFrameEnabled( true );
  mComposerAttributeTable->addFrame( mFrame1 );
  mComposerAttributeTable->addFrame( mFrame2 );

  mComposition->addComposerTableFrame( mComposerAttributeTable, mFrame1 );
  mComposition->addComposerTableFrame( mComposerAttributeTable, mFrame2 );
  mComposerAttributeTable->setVectorLayer( mVectorLayer );
  mComposerAttributeTable->setDisplayOnlyVisibleFeatures( false );
  mComposerAttributeTable->setMaximumNumberOfFeatures( 10 );
  mComposerAttributeTable->setContentFont( QgsFontUtils::getStandardTestFont() );
  mComposerAttributeTable->setHeaderFont( QgsFontUtils::getStandardTestFont() );

  mReport = "<h1>Composer TableV2 Tests</h1>\n";
}

void TestQgsComposerTableV2::cleanupTestCase()
{
  delete mComposition;
  delete mVectorLayer;

  QString myReportFile = QDir::tempPath() + QDir::separator() + "qgistest.html";
  QFile myFile( myReportFile );
  if ( myFile.open( QIODevice::WriteOnly | QIODevice::Append ) )
  {
    QTextStream myQTextStream( &myFile );
    myQTextStream << mReport;
    myFile.close();
  }
}

void TestQgsComposerTableV2::init()
{
}

void TestQgsComposerTableV2::cleanup()
{
}

void TestQgsComposerTableV2::attributeTableHeadings()
{
  //test retrieving attribute table headers
  QStringList expectedHeaders;
  expectedHeaders << "Class" << "Heading" << "Importance" << "Pilots" << "Cabin Crew" << "Staff";

  //get header labels and compare
  QMap<int, QString> headerMap = mComposerAttributeTable->headerLabels();
  QMap<int, QString>::const_iterator headerIt = headerMap.constBegin();
  QString expected;
  QString evaluated;
  for ( ; headerIt != headerMap.constEnd(); ++headerIt )
  {
    evaluated = headerIt.value();
    expected = expectedHeaders.at( headerIt.key() );
    QCOMPARE( evaluated, expected );
  }
}

void TestQgsComposerTableV2::compareTable( QList<QStringList> &expectedRows )
{
  //retrieve rows and check
  QgsComposerTableContents tableContents;
  bool result = mComposerAttributeTable->getTableContents( tableContents );
  QCOMPARE( result, true );

  QgsComposerTableContents::const_iterator resultIt = tableContents.constBegin();
  int rowNumber = 0;
  int colNumber = 0;

  //check that number of rows matches expected
  QCOMPARE( tableContents.count(), expectedRows.count() );

  for ( ; resultIt != tableContents.constEnd(); ++resultIt )
  {
    colNumber = 0;
    QgsComposerTableRow::const_iterator cellIt = ( *resultIt ).constBegin();
    for ( ; cellIt != ( *resultIt ).constEnd(); ++cellIt )
    {
      QCOMPARE(( *cellIt ).toString(), expectedRows.at( rowNumber ).at( colNumber ) );
      colNumber++;
    }
    //also check that number of columns matches expected
    QCOMPARE(( *resultIt ).count(), expectedRows.at( rowNumber ).count() );

    rowNumber++;
  }
}

void TestQgsComposerTableV2::attributeTableRows()
{
  //test retrieving attribute table rows

  QList<QStringList> expectedRows;
  QStringList row;
  row << "Jet" << "90" << "3" << "2" << "0" << "2";
  expectedRows.append( row );
  row.clear();
  row << "Biplane" << "0" << "1" << "3" << "3" << "6";
  expectedRows.append( row );
  row.clear();
  row << "Jet" << "85" << "3" << "1" << "1" << "2";
  expectedRows.append( row );

  //retrieve rows and check
  mComposerAttributeTable->setMaximumNumberOfFeatures( 3 );
  compareTable( expectedRows );
}

void TestQgsComposerTableV2::attributeTableFilterFeatures()
{
  //test filtering attribute table rows
  mComposerAttributeTable->setMaximumNumberOfFeatures( 10 );
  mComposerAttributeTable->setFeatureFilter( QString( "\"Class\"='B52'" ) );
  mComposerAttributeTable->setFilterFeatures( true );

  QList<QStringList> expectedRows;
  QStringList row;
  row << "B52" << "0" << "10" << "2" << "1" << "3";
  expectedRows.append( row );
  row.clear();
  row << "B52" << "12" << "10" << "1" << "1" << "2";
  expectedRows.append( row );
  row.clear();
  row << "B52" << "34" << "10" << "2" << "1" << "3";
  expectedRows.append( row );
  row.clear();
  row << "B52" << "80" << "10" << "2" << "1" << "3";
  expectedRows.append( row );

  //retrieve rows and check
  compareTable( expectedRows );

  mComposerAttributeTable->setFilterFeatures( false );
}

void TestQgsComposerTableV2::attributeTableSetAttributes()
{
  //test subset of attributes in table
  QSet<int> attributes;
  attributes << 0 << 3 << 4;
  mComposerAttributeTable->setDisplayAttributes( attributes );
  mComposerAttributeTable->setMaximumNumberOfFeatures( 3 );

  //check headers
  QStringList expectedHeaders;
  expectedHeaders << "Class" << "Pilots" << "Cabin Crew";

  //get header labels and compare
  QMap<int, QString> headerMap = mComposerAttributeTable->headerLabels();
  QMap<int, QString>::const_iterator headerIt = headerMap.constBegin();
  QString expected;
  QString evaluated;
  for ( ; headerIt != headerMap.constEnd(); ++headerIt )
  {
    evaluated = headerIt.value();
    expected = expectedHeaders.at( headerIt.key() );
    QCOMPARE( evaluated, expected );
  }

  QList<QStringList> expectedRows;
  QStringList row;
  row << "Jet" << "2" << "0";
  expectedRows.append( row );
  row.clear();
  row << "Biplane" << "3" << "3";
  expectedRows.append( row );
  row.clear();
  row << "Jet" << "1" << "1";
  expectedRows.append( row );

  //retrieve rows and check
  compareTable( expectedRows );

  attributes.clear();
  mComposerAttributeTable->setDisplayAttributes( attributes );
}

void TestQgsComposerTableV2::attributeTableVisibleOnly()
{
  //test displaying only visible attributes

  mComposerMap = new QgsComposerMap( mComposition, 20, 20, 200, 100 );
  mComposerMap->setFrameEnabled( true );
  mComposition->addComposerMap( mComposerMap );
  mComposerMap->setNewExtent( QgsRectangle( -131.767, 30.558, -110.743, 41.070 ) );

  mComposerAttributeTable->setComposerMap( mComposerMap );
  mComposerAttributeTable->setDisplayOnlyVisibleFeatures( true );

  QList<QStringList> expectedRows;
  QStringList row;
  row << "Jet" << "90" << "3" << "2" << "0" << "2";
  expectedRows.append( row );
  row.clear();
  row << "Biplane" << "240" << "1" << "3" << "2" << "5";
  expectedRows.append( row );
  row.clear();
  row << "Jet" << "180" << "3" << "1" << "0" << "1";
  expectedRows.append( row );

  //retrieve rows and check
  compareTable( expectedRows );

  mComposerAttributeTable->setDisplayOnlyVisibleFeatures( false );
  mComposerAttributeTable->setComposerMap( 0 );
  mComposition->removeItem( mComposerMap );
}

void TestQgsComposerTableV2::attributeTableRender()
{
  mComposerAttributeTable->setMaximumNumberOfFeatures( 20 );
  QgsCompositionChecker checker( "composerattributetable_render", mComposition );
  bool result = checker.testComposition( mReport, 0 );
  QVERIFY( result );
}

void TestQgsComposerTableV2::attributeTableExtend()
{
  //test that adding and removing frames automatically does not result in a crash
  mComposerAttributeTable->removeFrame( 1 );

  //force auto creation of some new frames
  mComposerAttributeTable->setResizeMode( QgsComposerMultiFrame::ExtendToNextPage );

  mComposition->setSelectedItem( mComposerAttributeTable->frame( 1 ) );

  //now auto remove extra created frames
  mComposerAttributeTable->setMaximumNumberOfFeatures( 1 );
}

void TestQgsComposerTableV2::attributeTableRepeat()
{
  mComposerAttributeTable->setResizeMode( QgsComposerMultiFrame::UseExistingFrames );
  //remove extra frames
  for ( int idx = mComposerAttributeTable->frameCount(); idx > 0; --idx )
  {
    mComposerAttributeTable->removeFrame( idx - 1 );
  }

  mComposerAttributeTable->setMaximumNumberOfFeatures( 1 );

  //force auto creation of some new frames
  mComposerAttributeTable->setResizeMode( QgsComposerMultiFrame::RepeatUntilFinished );

  for ( int features = 0; features < 50; ++features )
  {
    mComposerAttributeTable->setMaximumNumberOfFeatures( features );
  }

  //and then the reverse....
  for ( int features = 50; features > 1; --features )
  {
    mComposerAttributeTable->setMaximumNumberOfFeatures( features );
  }
}

QTEST_MAIN( TestQgsComposerTableV2 )
#include "moc_testqgscomposertablev2.cxx"

