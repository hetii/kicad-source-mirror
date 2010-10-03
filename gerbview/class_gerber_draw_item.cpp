/*************************************
*  file class_gerber_draw_item.cpp
*************************************/

/*
 * This program source code file is part of KICAD, a free EDA CAD application.
 *
 * Copyright (C) 1992-2010 <Jean-Pierre Charras>
 * Copyright (C) 1992-2010 Kicad Developers, see change_log.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "fctsys.h"
#include "polygons_defs.h"
#include "gr_basic.h"
#include "common.h"
#include "trigo.h"
#include "class_drawpanel.h"
#include "drawtxt.h"

#include "gerbview.h"
#include "class_board_design_settings.h"
#include "colors_selection.h"
#include "class_gerber_draw_item.h"


/**********************************************************/
GERBER_DRAW_ITEM::GERBER_DRAW_ITEM( BOARD_ITEM* aParent ) :
    BOARD_ITEM( aParent, TYPE_GERBER_DRAW_ITEM )
/**********************************************************/
{
    m_Layer   = 0;
    m_Shape   = GBR_SEGMENT;
    m_Flashed = false;
    m_DCode   = 0;
    m_UnitsMetric = false;
    m_ImageNegative = false;
    m_LayerNegative = false;
}


// Copy constructor
GERBER_DRAW_ITEM::GERBER_DRAW_ITEM( const GERBER_DRAW_ITEM& aSource ) :
    BOARD_ITEM( aSource )
{
    m_Shape = aSource.m_Shape;

    m_Flags     = aSource.m_Flags;
    m_TimeStamp = aSource.m_TimeStamp;

    SetStatus( aSource.ReturnStatus() );
    m_Start       = aSource.m_Start;
    m_End         = aSource.m_End;
    m_Size        = aSource.m_Size;
    m_Layer       = aSource.m_Layer;
    m_Shape       = aSource.m_Shape;
    m_Flashed     = aSource.m_Flashed;
    m_DCode       = aSource.m_DCode;
    m_PolyCorners = aSource.m_PolyCorners;
    m_UnitsMetric = aSource.m_UnitsMetric;
    m_ImageNegative = aSource.m_ImageNegative;
    m_LayerNegative = aSource.m_LayerNegative;

}


GERBER_DRAW_ITEM::~GERBER_DRAW_ITEM()
{
}


GERBER_DRAW_ITEM* GERBER_DRAW_ITEM::Copy() const
{
    return new GERBER_DRAW_ITEM( *this );
}


wxString GERBER_DRAW_ITEM::ShowGBRShape()
{
    switch( m_Shape )
    {
    case GBR_SEGMENT:
        return _( "Line" );

    case GBR_ARC:
        return _( "Arc" );

    case GBR_CIRCLE:
        return _( "Circle" );

    case GBR_SPOT_OVAL:
        return wxT( "spot_oval" );

    case GBR_SPOT_CIRCLE:
        return wxT( "spot_circle" );

    case GBR_SPOT_RECT:
        return wxT( "spot_rect" );

    case GBR_SPOT_POLY:
        return wxT( "spot_poly" );

    case GBR_POLYGON:
        return wxT( "polygon" );

    case GBR_SPOT_MACRO:
        return wxT( "apt_macro" );  // TODO: add aperture macro name

    default:
        return wxT( "??" );
    }
}


/**
 * Function GetDcodeDescr
 * returns the GetDcodeDescr of this object, or NULL.
 * @return D_CODE* - a pointer to the DCode description (for flashed items).
 */
D_CODE* GERBER_DRAW_ITEM::GetDcodeDescr()
{
    if( (m_DCode < FIRST_DCODE) || (m_DCode > LAST_DCODE) )
        return NULL;
    GERBER* gerber = g_GERBER_List[m_Layer];
    if( gerber == NULL )
        return NULL;

    D_CODE* d_code = gerber->GetDCODE( m_DCode, false );

    return d_code;
}


EDA_Rect GERBER_DRAW_ITEM::GetBoundingBox()
{
    // return a rectangle which is (pos,dim) in nature.  therefore the +1
    EDA_Rect bbox( m_Start, wxSize( 1, 1 ) );

    bbox.Inflate( m_Size.x / 2, m_Size.y / 2 );
    return bbox;
}


/**
 * Function Move
 * move this object.
 * @param const wxPoint& aMoveVector - the move vector for this object.
 */
void GERBER_DRAW_ITEM::Move( const wxPoint& aMoveVector )
{
    m_Start     += aMoveVector;
    m_End       += aMoveVector;
    m_ArcCentre += aMoveVector;
    for( unsigned ii = 0; ii < m_PolyCorners.size(); ii++ )
        m_PolyCorners[ii] += aMoveVector;
}


/** function Save.
 * currently: no nothing, but must be defined to meet requirements
 * of the basic class
 */
bool GERBER_DRAW_ITEM::Save( FILE* aFile ) const
{
    return true;
}


/*********************************************************************/
void GERBER_DRAW_ITEM::Draw( WinEDA_DrawPanel* aPanel, wxDC* aDC, int aDrawMode,
                             const wxPoint& aOffset )
/*********************************************************************/
{
    static D_CODE dummyD_CODE( 0 );      // used when a D_CODE is not found. default D_CODE to draw a flashed item
    int           color, alt_color;
    bool          isFilled;
    int           radius;
    int           halfPenWidth;
    static bool   show_err;
    BOARD*        brd = GetBoard();
    D_CODE*       d_codeDescr = GetDcodeDescr();

    if( d_codeDescr == NULL )
        d_codeDescr = &dummyD_CODE;

    if( brd->IsLayerVisible( GetLayer() ) == false )
        return;

    color = brd->GetLayerColor( GetLayer() );

    if( aDrawMode & GR_SURBRILL )
    {
        if( aDrawMode & GR_AND )
            color &= ~HIGHT_LIGHT_FLAG;
        else
            color |= HIGHT_LIGHT_FLAG;
    }
    if( color & HIGHT_LIGHT_FLAG )
        color = ColorRefs[color & MASKCOLOR].m_LightColor;

    alt_color = g_DrawBgColor ;

    if( m_Flags & DRAW_ERASED )   // draw in background color ("negative" color)
    {
        EXCHG(color, alt_color);
    }

    GRSetDrawMode( aDC, aDrawMode );

    isFilled = DisplayOpt.DisplayPcbTrackFill ? true : false;

    switch( m_Shape )
    {
    case GBR_POLYGON:
        isFilled = (g_DisplayPolygonsModeSketch == false);
        if( m_Flags & DRAW_ERASED )
            isFilled = true;
        DrawGbrPoly( &aPanel->m_ClipBox, aDC, color, aOffset, isFilled );
        break;

    case GBR_CIRCLE:
        radius = (int) hypot( (double) ( m_End.x - m_Start.x ),
                             (double) ( m_End.y - m_Start.y ) );

        halfPenWidth = m_Size.x >> 1;

        if( !isFilled )
        {
            // draw the border of the pen's path using two circles, each as narrow as possible
            GRCircle( &aPanel->m_ClipBox, aDC, m_Start.x, m_Start.y,
                      radius - halfPenWidth, 0, color );
            GRCircle( &aPanel->m_ClipBox, aDC, m_Start.x, m_Start.y,
                      radius + halfPenWidth, 0, color );
        }
        else    // Filled mode
        {
            GRCircle( &aPanel->m_ClipBox, aDC, m_Start.x, m_Start.y,
                      radius, m_Size.x, color );
        }
        break;

    case GBR_ARC:
        if( !isFilled )
        {
            GRArc1( &aPanel->m_ClipBox, aDC, m_Start.x, m_Start.y,
                    m_End.x, m_End.y,
                    m_ArcCentre.x, m_ArcCentre.y, 0, color );
        }
        else
        {
            GRArc1( &aPanel->m_ClipBox, aDC, m_Start.x, m_Start.y,
                    m_End.x, m_End.y,
                    m_ArcCentre.x, m_ArcCentre.y,
                    m_Size.x, color );
        }
        break;

    case GBR_SPOT_CIRCLE:
    case GBR_SPOT_RECT:
    case GBR_SPOT_OVAL:
    case GBR_SPOT_POLY:
    case GBR_SPOT_MACRO:
        isFilled = DisplayOpt.DisplayPadFill ? true : false;
        d_codeDescr->DrawFlashedShape( this, &aPanel->m_ClipBox, aDC, color, alt_color,
                                       m_Start, isFilled );
        break;

    case GBR_SEGMENT:
        if( !isFilled )
            GRCSegm( &aPanel->m_ClipBox, aDC, m_Start.x, m_Start.y,
                     m_End.x, m_End.y, m_Size.x, color );
        else
            GRFillCSegm( &aPanel->m_ClipBox, aDC, m_Start.x,
                         m_Start.y, m_End.x, m_End.y, m_Size.x, color );
        break;

    default:
        if( !show_err )
        {
            wxMessageBox( wxT( "Trace_Segment() type error" ) );
            show_err = TRUE;
        }
        break;
    }
}


/** function DrawGbrPoly
 * a helper function used id ::Draw to draw the polygon stored ion m_PolyCorners
 * Draw filled polygons
 */
void GERBER_DRAW_ITEM::DrawGbrPoly( EDA_Rect*      aClipBox,
                                    wxDC*          aDC,
                                    int            aColor,
                                    const wxPoint& aOffset,
                                    bool aFilledShape )
{
    std::vector<wxPoint> points;

    points = m_PolyCorners;
    if( aOffset != wxPoint( 0, 0 ) )
    {
        for( unsigned ii = 0; ii < points.size(); ii++ )
        {
            points[ii] += aOffset;
        }
    }

    GRClosedPoly( aClipBox, aDC, points.size(), &points[0], aFilledShape, aColor, aColor );
}


/** Function DisplayInfoBase
 * has knowledge about the frame and how and where to put status information
 * about this object into the frame's message panel.
 * Display info about the track segment only, and does not calculate the full track length
 * @param frame A WinEDA_DrawFrame in which to print status information.
 */
void GERBER_DRAW_ITEM::DisplayInfo( WinEDA_DrawFrame* frame )
{
    wxString msg;
    BOARD*   board = ( (WinEDA_BasePcbFrame*) frame )->GetBoard();

    frame->ClearMsgPanel();

    msg = ShowGBRShape();
    frame->AppendMsgPanel( _( "Type" ), msg, DARKCYAN );

    /* Display layer */
    msg = board->GetLayerName( m_Layer );
    frame->AppendMsgPanel( _( "Layer" ), msg, BROWN );
}


/**
 * Function HitTest
 * tests if the given wxPoint is within the bounds of this object.
 * @param ref_pos A wxPoint to test
 * @return bool - true if a hit, else false
 */
bool GERBER_DRAW_ITEM::HitTest( const wxPoint& ref_pos )
{
    // TODO: a better analyse od the shape (perhaps create a D_CODE::HitTest for flashed items)
    int     radius = MIN( m_Size.x, m_Size.y) >> 1;

    // delta is a vector from m_Start to m_End (an origin of m_Start)
    wxPoint delta = m_End - m_Start;

    // dist is a vector from m_Start to ref_pos (an origin of m_Start)
    wxPoint dist = ref_pos - m_Start;

    if( m_Flashed )
    {
        return (double) dist.x * dist.x + (double) dist.y * dist.y <=
               (double) radius * radius;
    }
    else
    {
        if( DistanceTest( radius, delta.x, delta.y, dist.x, dist.y ) )
            return true;
    }

    return false;
}


/**
 * Function HitTest (overlayed)
 * tests if the given EDA_Rect intersect this object.
 * For now, an ending point must be inside this rect.
 * @param refArea : the given EDA_Rect
 * @return bool - true if a hit, else false
 */
bool GERBER_DRAW_ITEM::HitTest( EDA_Rect& refArea )
{
    if( refArea.Inside( m_Start ) )
        return true;
    if( refArea.Inside( m_End ) )
        return true;
    return false;
}


#if defined(DEBUG)

/**
 * Function Show
 * is used to output the object tree, currently for debugging only.
 * @param nestLevel An aid to prettier tree indenting, and is the level
 *          of nesting of this object within the overall tree.
 * @param os The ostream& to output to.
 */
void GERBER_DRAW_ITEM::Show( int nestLevel, std::ostream& os )
{
    NestedSpace( nestLevel, os ) << '<' << GetClass().Lower().mb_str() <<

    " shape=\"" << m_Shape << '"' <<
    " addr=\"" << std::hex << this << std::dec << '"' <<
    " layer=\"" << m_Layer << '"' <<
    " size=\"" << m_Size << '"' <<
    " flags=\"" << m_Flags << '"' <<
    " status=\"" << GetState( -1 ) << '"' <<
    "<start" << m_Start << "/>" <<
    "<end" << m_End << "/>";

    os << "</" << GetClass().Lower().mb_str() << ">\n";
}


#endif
