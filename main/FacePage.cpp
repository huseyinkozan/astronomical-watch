#include "FacePage.h"

#include "Panel.h"

void FacePage::build( lv_obj_t *tile )
{
    m_root = lv_obj_create( tile );
    lv_obj_remove_style_all( m_root );
    lv_obj_set_size( m_root, Panel::WIDTH, Panel::HEIGHT );
    lv_obj_set_clickable( m_root, false );
    lv_obj_set_scrollable( m_root, false );
}

void FacePage::show( const FaceConfig::Page &page, int direction )
{
    ( void ) direction;
    m_page = page;
}

void FacePage::update( const FaceText::Inputs &inputs )
{
    ( void ) inputs;
}

void FacePage::setMoon( int percent, bool waxing )
{
    m_moonPercent = percent;
    m_waxing = waxing;
}
