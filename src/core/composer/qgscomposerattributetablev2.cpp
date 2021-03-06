/***************************************************************************
                         QgsComposerAttributeTableV2.cpp
                         -----------------------------
    begin                : September 2014
    copyright            : (C) 2014 by Marco Hugentobler
    email                : marco at hugis dot net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgscomposerattributetablev2.h"
#include "qgscomposertablecolumn.h"
#include "qgscomposermap.h"
#include "qgscomposerutils.h"
#include "qgsmaplayerregistry.h"
#include "qgsvectorlayer.h"
#include "qgscomposerframe.h"

//QgsComposerAttributeTableCompareV2

QgsComposerAttributeTableCompareV2::QgsComposerAttributeTableCompareV2()
    : mCurrentSortColumn( 0 ), mAscending( true )
{
}


bool QgsComposerAttributeTableCompareV2::operator()( const QgsComposerTableRow& m1, const QgsComposerTableRow& m2 )
{
  QVariant v1 = m1[mCurrentSortColumn];
  QVariant v2 = m2[mCurrentSortColumn];

  bool less = false;

  //sort null values first
  if ( v1.isNull() && v2.isNull() )
  {
    less = false;
  }
  else if ( v1.isNull() )
  {
    less = true;
  }
  else if ( v2.isNull() )
  {
    less = false;
  }
  else
  {
    //otherwise sort by converting to corresponding type and comparing
    switch ( v1.type() )
    {
      case QVariant::Int:
      case QVariant::UInt:
      case QVariant::LongLong:
      case QVariant::ULongLong:
        less = v1.toLongLong() < v2.toLongLong();
        break;

      case QVariant::Double:
        less = v1.toDouble() < v2.toDouble();
        break;

      case QVariant::Date:
        less = v1.toDate() < v2.toDate();
        break;

      case QVariant::DateTime:
        less = v1.toDateTime() < v2.toDateTime();
        break;

      case QVariant::Time:
        less = v1.toTime() < v2.toTime();
        break;

      default:
        //use locale aware compare for strings
        less = v1.toString().localeAwareCompare( v2.toString() ) < 0;
        break;
    }
  }

  return ( mAscending ? less : !less );
}

//
// QgsComposerAttributeTableV2
//

QgsComposerAttributeTableV2::QgsComposerAttributeTableV2( QgsComposition* composition, bool createUndoCommands )
    : QgsComposerTableV2( composition, createUndoCommands )
    , mVectorLayer( 0 )
    , mComposerMap( 0 )
    , mMaximumNumberOfFeatures( 5 )
    , mShowOnlyVisibleFeatures( false )
    , mFilterFeatures( false )
    , mFeatureFilter( "" )
{
  //set first vector layer from layer registry as default one
  QMap<QString, QgsMapLayer*> layerMap =  QgsMapLayerRegistry::instance()->mapLayers();
  QMap<QString, QgsMapLayer*>::const_iterator mapIt = layerMap.constBegin();
  for ( ; mapIt != layerMap.constEnd(); ++mapIt )
  {
    QgsVectorLayer* vl = dynamic_cast<QgsVectorLayer*>( mapIt.value() );
    if ( vl )
    {
      mVectorLayer = vl;
      break;
    }
  }
  if ( mVectorLayer )
  {
    resetColumns();
  }
  connect( QgsMapLayerRegistry::instance(), SIGNAL( layerWillBeRemoved( QString ) ), this, SLOT( removeLayer( const QString& ) ) );

  if ( mComposition )
  {
    //refresh table attributes when composition is refreshed
    connect( mComposition, SIGNAL( refreshItemsTriggered() ), this, SLOT( refreshAttributes() ) );

    //connect to atlas feature changes to update table rows
    connect( &mComposition->atlasComposition(), SIGNAL( featureChanged( QgsFeature* ) ), this, SLOT( refreshAttributes() ) );
  }
  refreshAttributes();
}

QgsComposerAttributeTableV2::~QgsComposerAttributeTableV2()
{
}

void QgsComposerAttributeTableV2::setVectorLayer( QgsVectorLayer* layer )
{
  if ( layer == mVectorLayer )
  {
    //no change
    return;
  }

  if ( mVectorLayer )
  {
    //disconnect from previous layer
    QObject::disconnect( mVectorLayer, SIGNAL( layerModified() ), this, SLOT( refreshAttributes() ) );
  }

  mVectorLayer = layer;

  //rebuild column list to match all columns from layer
  resetColumns();
  refreshAttributes();

  //listen for modifications to layer and refresh table when they occur
  QObject::connect( mVectorLayer, SIGNAL( layerModified() ), this, SLOT( refreshAttributes() ) );

  emit changed();
}

void QgsComposerAttributeTableV2::resetColumns()
{
  if ( !mVectorLayer )
  {
    return;
  }

  //remove existing columns
  qDeleteAll( mColumns );
  mColumns.clear();

  //rebuild columns list from vector layer fields
  const QgsFields& fields = mVectorLayer->pendingFields();
  for ( int idx = 0; idx < fields.count(); ++idx )
  {
    QString currentAlias = mVectorLayer->attributeDisplayName( idx );
    QgsComposerTableColumn* col = new QgsComposerTableColumn;
    col->setAttribute( fields[idx].name() );
    col->setHeading( currentAlias );
    mColumns.append( col );
  }
}

void QgsComposerAttributeTableV2::setComposerMap( const QgsComposerMap* map )
{
  if ( map == mComposerMap )
  {
    //no change
    return;
  }

  if ( mComposerMap )
  {
    //disconnect from previous map
    QObject::disconnect( mComposerMap, SIGNAL( extentChanged() ), this, SLOT( refreshAttributes() ) );
  }
  mComposerMap = map;
  if ( mComposerMap )
  {
    //listen out for extent changes in linked map
    QObject::connect( mComposerMap, SIGNAL( extentChanged() ), this, SLOT( refreshAttributes() ) );
  }
  refreshAttributes();
  emit changed();
}

void QgsComposerAttributeTableV2::setMaximumNumberOfFeatures( int features )
{
  if ( features == mMaximumNumberOfFeatures )
  {
    return;
  }

  mMaximumNumberOfFeatures = features;
  refreshAttributes();
  emit changed();
}

void QgsComposerAttributeTableV2::setDisplayOnlyVisibleFeatures( bool visibleOnly )
{
  if ( visibleOnly == mShowOnlyVisibleFeatures )
  {
    return;
  }

  mShowOnlyVisibleFeatures = visibleOnly;
  refreshAttributes();
  emit changed();
}

void QgsComposerAttributeTableV2::setFilterFeatures( bool filter )
{
  if ( filter == mFilterFeatures )
  {
    return;
  }

  mFilterFeatures = filter;
  refreshAttributes();
  emit changed();
}

void QgsComposerAttributeTableV2::setFeatureFilter( const QString& expression )
{
  if ( expression == mFeatureFilter )
  {
    return;
  }

  mFeatureFilter = expression;
  refreshAttributes();
  emit changed();
}

void QgsComposerAttributeTableV2::setDisplayAttributes( const QSet<int>& attr, bool refresh )
{
  if ( !mVectorLayer )
  {
    return;
  }

  //rebuild columns list, taking only attributes with index in supplied QSet
  qDeleteAll( mColumns );
  mColumns.clear();

  const QgsFields& fields = mVectorLayer->pendingFields();

  if ( !attr.empty() )
  {
    QSet<int>::const_iterator attIt = attr.constBegin();
    for ( ; attIt != attr.constEnd(); ++attIt )
    {
      int attrIdx = ( *attIt );
      if ( !fields.exists( attrIdx ) )
      {
        continue;
      }
      QString currentAlias = mVectorLayer->attributeDisplayName( attrIdx );
      QgsComposerTableColumn* col = new QgsComposerTableColumn;
      col->setAttribute( fields[attrIdx].name() );
      col->setHeading( currentAlias );
      mColumns.append( col );
    }
  }
  else
  {
    //resetting, so add all attributes to columns
    for ( int idx = 0; idx < fields.count(); ++idx )
    {
      QString currentAlias = mVectorLayer->attributeDisplayName( idx );
      QgsComposerTableColumn* col = new QgsComposerTableColumn;
      col->setAttribute( fields[idx].name() );
      col->setHeading( currentAlias );
      mColumns.append( col );
    }
  }

  if ( refresh )
  {
    refreshAttributes();
  }
}

void QgsComposerAttributeTableV2::restoreFieldAliasMap( const QMap<int, QString>& map )
{
  if ( !mVectorLayer )
  {
    return;
  }

  QList<QgsComposerTableColumn*>::const_iterator columnIt = mColumns.constBegin();
  for ( ; columnIt != mColumns.constEnd(); ++columnIt )
  {
    int attrIdx = mVectorLayer->fieldNameIndex(( *columnIt )->attribute() );
    if ( map.contains( attrIdx ) )
    {
      ( *columnIt )->setHeading( map.value( attrIdx ) );
    }
    else
    {
      ( *columnIt )->setHeading( mVectorLayer->attributeDisplayName( attrIdx ) );
    }
  }
}

bool QgsComposerAttributeTableV2::getTableContents( QgsComposerTableContents &contents )
{
  if ( !mVectorLayer )
  {
    return false;
  }

  contents.clear();

  //prepare filter expression
  std::auto_ptr<QgsExpression> filterExpression;
  bool activeFilter = false;
  if ( mFilterFeatures && !mFeatureFilter.isEmpty() )
  {
    filterExpression = std::auto_ptr<QgsExpression>( new QgsExpression( mFeatureFilter ) );
    if ( !filterExpression->hasParserError() )
    {
      activeFilter = true;
    }
  }

  QgsRectangle selectionRect;
  if ( mComposerMap && mShowOnlyVisibleFeatures )
  {
    selectionRect = *mComposerMap->currentMapExtent();
    if ( mVectorLayer && mComposition->mapSettings().hasCrsTransformEnabled() )
    {
      //transform back to layer CRS
      QgsCoordinateTransform coordTransform( mVectorLayer->crs(), mComposition->mapSettings().destinationCrs() );
      try
      {
        selectionRect = coordTransform.transformBoundingBox( selectionRect, QgsCoordinateTransform::ReverseTransform );
      }
      catch ( QgsCsException &cse )
      {
        Q_UNUSED( cse );
        return false;
      }
    }
  }

  QgsFeatureRequest req;
  if ( !selectionRect.isEmpty() )
    req.setFilterRect( selectionRect );

  req.setFlags( mShowOnlyVisibleFeatures ? QgsFeatureRequest::ExactIntersect : QgsFeatureRequest::NoFlags );

  QgsFeature f;
  int counter = 0;
  QgsFeatureIterator fit = mVectorLayer->getFeatures( req );

  while ( fit.nextFeature( f ) && counter < mMaximumNumberOfFeatures )
  {
    //check feature against filter
    if ( activeFilter )
    {
      QVariant result = filterExpression->evaluate( &f, mVectorLayer->pendingFields() );
      // skip this feature if the filter evaluation is false
      if ( !result.toBool() )
      {
        continue;
      }
    }

    QgsComposerTableRow currentRow;

    QList<QgsComposerTableColumn*>::const_iterator columnIt = mColumns.constBegin();
    for ( ; columnIt != mColumns.constEnd(); ++columnIt )
    {
      int idx = mVectorLayer->fieldNameIndex(( *columnIt )->attribute() );
      if ( idx != -1 )
      {
        currentRow << f.attributes()[idx];
      }
      else
      {
        // Lets assume it's an expression
        QgsExpression* expression = new QgsExpression(( *columnIt )->attribute() );
        expression->setCurrentRowNumber( counter + 1 );
        expression->prepare( mVectorLayer->pendingFields() );
        QVariant value = expression->evaluate( f ) ;
        currentRow << value;
      }
    }
    contents << currentRow;
    ++counter;
  }

  //sort the list, starting with the last attribute
  QgsComposerAttributeTableCompareV2 c;
  QList< QPair<int, bool> > sortColumns = sortAttributes();
  for ( int i = sortColumns.size() - 1; i >= 0; --i )
  {
    c.setSortColumn( sortColumns.at( i ).first );
    c.setAscending( sortColumns.at( i ).second );
    qStableSort( contents.begin(), contents.end(), c );
  }

  recalculateTableSize();
  return true;
}

void QgsComposerAttributeTableV2::removeLayer( QString layerId )
{
  if ( mVectorLayer )
  {
    if ( layerId == mVectorLayer->id() )
    {
      mVectorLayer = 0;
      //remove existing columns
      qDeleteAll( mColumns );
      mColumns.clear();
    }
  }
}

static bool columnsBySortRank( QPair<int, QgsComposerTableColumn* > a, QPair<int, QgsComposerTableColumn* > b )
{
  return a.second->sortByRank() < b.second->sortByRank();
}

QList<QPair<int, bool> > QgsComposerAttributeTableV2::sortAttributes() const
{
  //generate list of all sorted columns
  QList< QPair<int, QgsComposerTableColumn* > > sortedColumns;
  QList<QgsComposerTableColumn*>::const_iterator columnIt = mColumns.constBegin();
  int idx = 0;
  for ( ; columnIt != mColumns.constEnd(); ++columnIt )
  {
    if (( *columnIt )->sortByRank() > 0 )
    {
      sortedColumns.append( qMakePair( idx, *columnIt ) );
    }
    idx++;
  }

  //sort columns by rank
  qSort( sortedColumns.begin(), sortedColumns.end(), columnsBySortRank );

  //generate list of column index, bool for sort direction (to match 2.0 api)
  QList<QPair<int, bool> > attributesBySortRank;
  QList< QPair<int, QgsComposerTableColumn* > >::const_iterator sortedColumnIt = sortedColumns.constBegin();
  for ( ; sortedColumnIt != sortedColumns.constEnd(); ++sortedColumnIt )
  {

    attributesBySortRank.append( qMakePair(( *sortedColumnIt ).first,
                                           ( *sortedColumnIt ).second->sortOrder() == Qt::AscendingOrder ) );
  }
  return attributesBySortRank;
}

bool QgsComposerAttributeTableV2::writeXML( QDomElement& elem, QDomDocument & doc, bool ignoreFrames ) const
{
  QDomElement composerTableElem = doc.createElement( "ComposerAttributeTableV2" );
  composerTableElem.setAttribute( "showOnlyVisibleFeatures", mShowOnlyVisibleFeatures );
  composerTableElem.setAttribute( "maxFeatures", mMaximumNumberOfFeatures );
  composerTableElem.setAttribute( "filterFeatures", mFilterFeatures ? "true" : "false" );
  composerTableElem.setAttribute( "featureFilter", mFeatureFilter );

  if ( mComposerMap )
  {
    composerTableElem.setAttribute( "composerMap", mComposerMap->id() );
  }
  else
  {
    composerTableElem.setAttribute( "composerMap", -1 );
  }
  if ( mVectorLayer )
  {
    composerTableElem.setAttribute( "vectorLayer", mVectorLayer->id() );
  }

  bool ok = QgsComposerTableV2::writeXML( composerTableElem, doc, ignoreFrames );

  elem.appendChild( composerTableElem );

  return ok;
}

bool QgsComposerAttributeTableV2::readXML( const QDomElement& itemElem, const QDomDocument& doc, bool ignoreFrames )
{
  if ( itemElem.isNull() )
  {
    return false;
  }

  //read general table properties
  if ( !QgsComposerTableV2::readXML( itemElem, doc, ignoreFrames ) )
  {
    return false;
  }

  mShowOnlyVisibleFeatures = itemElem.attribute( "showOnlyVisibleFeatures", "1" ).toInt();
  mFilterFeatures = itemElem.attribute( "filterFeatures", "false" ) == "true" ? true : false;
  mFeatureFilter = itemElem.attribute( "featureFilter", "" );
  mMaximumNumberOfFeatures = itemElem.attribute( "maxFeatures", "5" ).toInt();

  //composer map
  int composerMapId = itemElem.attribute( "composerMap", "-1" ).toInt();
  if ( composerMapId == -1 )
  {
    mComposerMap = 0;
  }

  if ( composition() )
  {
    mComposerMap = composition()->getComposerMapById( composerMapId );
  }
  else
  {
    mComposerMap = 0;
  }

  if ( mComposerMap )
  {
    //if we have found a valid map item, listen out to extent changes on it and refresh the table
    QObject::connect( mComposerMap, SIGNAL( extentChanged() ), this, SLOT( refreshAttributes() ) );
  }

  //vector layer
  QString layerId = itemElem.attribute( "vectorLayer", "not_existing" );
  if ( layerId == "not_existing" )
  {
    mVectorLayer = 0;
  }
  else
  {
    QgsMapLayer* ml = QgsMapLayerRegistry::instance()->mapLayer( layerId );
    if ( ml )
    {
      mVectorLayer = dynamic_cast<QgsVectorLayer*>( ml );
      if ( mVectorLayer )
      {
        //if we have found a valid vector layer, listen for modifications on it and refresh the table
        QObject::connect( mVectorLayer, SIGNAL( layerModified() ), this, SLOT( refreshAttributes() ) );
      }
    }
  }

  refreshAttributes();

  emit changed();
  return true;
}

void QgsComposerAttributeTableV2::addFrame( QgsComposerFrame *frame, bool recalcFrameSizes )
{
  mFrameItems.push_back( frame );
  connect( frame, SIGNAL( sizeChanged() ), this, SLOT( recalculateFrameSizes() ) );
  if ( mComposition )
  {
    mComposition->addComposerTableFrame( this, frame );
  }

  if ( recalcFrameSizes )
  {

    recalculateFrameSizes();
  }
}
