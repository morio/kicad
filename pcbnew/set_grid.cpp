	/***************************************************/
	/* set_grid.cpp - Gestion de la grille utilisateur */
	/***************************************************/
/*
 Affichage et modifications des parametres de travail de PcbNew
 Parametres = dimensions de la grille utilisateur
*/

#include "fctsys.h"
#include "common.h"
#include "class_drawpanel.h"

#include "pcbnew.h"

#include "protos.h"
#include <wx/spinctrl.h>

#include "id.h"

#include "set_grid.h"

/****************************************************************/
void WinEDA_PcbGridFrame::AcceptPcbOptions(wxCommandEvent& event)
/****************************************************************/
{
    double dtmp = 0;

	g_UserGrid_Unit = m_UnitGrid->GetSelection();
	m_OptGridSizeX->GetValue().ToDouble( &dtmp );
    g_UserGrid.x = dtmp;
	m_OptGridSizeY->GetValue().ToDouble( &dtmp );
    g_UserGrid.y = dtmp;

	m_Parent->GetScreen()->AddGrid( g_UserGrid, g_UserGrid_Unit, ID_POPUP_GRID_USER );

	EndModal(1);
    
    // If the user grid is the current selection , ensure grid size value = new user grid value
    int ii = m_Parent->m_SelGridBox->GetSelection();
    if ( ii == m_Parent->m_SelGridBox->GetCount() - 1 )
        m_Parent->GetScreen()->SetGrid( ID_POPUP_GRID_USER );

	m_Parent->DrawPanel->Refresh( TRUE );
}



/************************************************************/
void WinEDA_BasePcbFrame::InstallGridFrame(const wxPoint & pos)
/************************************************************/
{
	WinEDA_PcbGridFrame * GridFrame =
				new WinEDA_PcbGridFrame( this, pos );
	GridFrame->ShowModal(); GridFrame->Destroy();
}


/*!
 * WinEDA_PcbGridFrame type definition
 */

IMPLEMENT_DYNAMIC_CLASS( WinEDA_PcbGridFrame, wxDialog )

/*!
 * WinEDA_PcbGridFrame event table definition
 */

BEGIN_EVENT_TABLE( WinEDA_PcbGridFrame, wxDialog )

////@begin WinEDA_PcbGridFrame event table entries
    EVT_BUTTON( wxID_OK, WinEDA_PcbGridFrame::OnOkClick )

    EVT_BUTTON( wxID_CANCEL, WinEDA_PcbGridFrame::OnCancelClick )

////@end WinEDA_PcbGridFrame event table entries

END_EVENT_TABLE()

/*!
 * WinEDA_PcbGridFrame constructors
 */

WinEDA_PcbGridFrame::WinEDA_PcbGridFrame( )
{
}

WinEDA_PcbGridFrame::WinEDA_PcbGridFrame( WinEDA_BasePcbFrame* parent,
                                          const wxPoint& pos,
                                          wxWindowID id,
                                          const wxString& caption,
                                          const wxSize& size, long style )
{
    wxString msg;
    PCB_SCREEN * screen;

	m_Parent = parent;
	screen = (PCB_SCREEN*)(m_Parent->GetScreen());

    Create(parent, id, caption, pos, size, style);

	if ( g_UserGrid_Unit != INCHES )
		m_UnitGrid->SetSelection(1);

	msg.Printf( wxT("%.4f"), g_UserGrid.x );
	m_OptGridSizeX->SetValue(msg);
	msg.Printf( wxT("%.4f"), g_UserGrid.y );
	m_OptGridSizeY->SetValue(msg);
}

/*!
 * WinEDA_PcbGridFrame creator
 */

bool WinEDA_PcbGridFrame::Create( wxWindow* parent, wxWindowID id, const wxString& caption, const wxPoint& pos, const wxSize& size, long style )
{
////@begin WinEDA_PcbGridFrame member initialisation
    m_UnitGrid = NULL;
    m_OptGridSizeX = NULL;
    m_OptGridSizeY = NULL;
////@end WinEDA_PcbGridFrame member initialisation

////@begin WinEDA_PcbGridFrame creation
    SetExtraStyle(GetExtraStyle()|wxWS_EX_BLOCK_EVENTS);
    wxDialog::Create( parent, id, caption, pos, size, style );

    CreateControls();
    if (GetSizer())
    {
        GetSizer()->SetSizeHints(this);
    }
////@end WinEDA_PcbGridFrame creation
    return true;
}

/*!
 * Control creation for WinEDA_PcbGridFrame
 */

void WinEDA_PcbGridFrame::CreateControls()
{
	SetFont(*g_DialogFont);
////@begin WinEDA_PcbGridFrame content construction
    // Generated by DialogBlocks, 17/08/2006 09:27:19 (unregistered)

    WinEDA_PcbGridFrame* itemDialog1 = this;

    wxBoxSizer* itemBoxSizer2 = new wxBoxSizer(wxHORIZONTAL);
    itemDialog1->SetSizer(itemBoxSizer2);

    wxBoxSizer* itemBoxSizer3 = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer2->Add(itemBoxSizer3, 0, wxGROW|wxALL, 5);

    wxString m_UnitGridStrings[] = {
        _("Inches"),
        _("mm")
    };
    m_UnitGrid = new wxRadioBox( itemDialog1, ID_RADIOBOX, _("Grid Size Units"), wxDefaultPosition, wxDefaultSize, 2, m_UnitGridStrings, 1, wxRA_SPECIFY_COLS );
    m_UnitGrid->SetSelection(0);
    itemBoxSizer3->Add(m_UnitGrid, 0, wxGROW|wxALL, 5);

    itemBoxSizer3->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    wxStaticText* itemStaticText6 = new wxStaticText( itemDialog1, wxID_STATIC, _("User Grid Size X"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer3->Add(itemStaticText6, 0, wxGROW|wxLEFT|wxRIGHT|wxTOP|wxADJUST_MINSIZE, 5);

    m_OptGridSizeX = new wxTextCtrl( itemDialog1, ID_TEXTCTRL, _T(""), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer3->Add(m_OptGridSizeX, 0, wxGROW|wxLEFT|wxRIGHT|wxBOTTOM, 5);

    wxStaticText* itemStaticText8 = new wxStaticText( itemDialog1, wxID_STATIC, _("User Grid Size Y"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer3->Add(itemStaticText8, 0, wxGROW|wxLEFT|wxRIGHT|wxTOP|wxADJUST_MINSIZE, 5);

    m_OptGridSizeY = new wxTextCtrl( itemDialog1, ID_TEXTCTRL1, _T(""), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer3->Add(m_OptGridSizeY, 0, wxGROW|wxLEFT|wxRIGHT|wxBOTTOM, 5);

    wxBoxSizer* itemBoxSizer10 = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer2->Add(itemBoxSizer10, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxButton* itemButton11 = new wxButton( itemDialog1, wxID_OK, _("&OK"), wxDefaultPosition, wxDefaultSize, 0 );
    itemButton11->SetDefault();
    itemButton11->SetForegroundColour(wxColour(202, 0, 0));
    itemBoxSizer10->Add(itemButton11, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    wxButton* itemButton12 = new wxButton( itemDialog1, wxID_CANCEL, _("&Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
    itemButton12->SetForegroundColour(wxColour(0, 0, 255));
    itemBoxSizer10->Add(itemButton12, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

////@end WinEDA_PcbGridFrame content construction
}

/*!
 * Should we show tooltips?
 */

bool WinEDA_PcbGridFrame::ShowToolTips()
{
    return true;
}

/*!
 * Get bitmap resources
 */

wxBitmap WinEDA_PcbGridFrame::GetBitmapResource( const wxString& name )
{
    // Bitmap retrieval
////@begin WinEDA_PcbGridFrame bitmap retrieval
    wxUnusedVar(name);
    return wxNullBitmap;
////@end WinEDA_PcbGridFrame bitmap retrieval
}

/*!
 * Get icon resources
 */

wxIcon WinEDA_PcbGridFrame::GetIconResource( const wxString& name )
{
    // Icon retrieval
////@begin WinEDA_PcbGridFrame icon retrieval
    wxUnusedVar(name);
    return wxNullIcon;
////@end WinEDA_PcbGridFrame icon retrieval
}
/*!
 * wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_OK
 */

void WinEDA_PcbGridFrame::OnOkClick( wxCommandEvent& event )
{
	AcceptPcbOptions(event);
}

/*!
 * wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_CANCEL
 */

void WinEDA_PcbGridFrame::OnCancelClick( wxCommandEvent& event )
{
////@begin wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_CANCEL in WinEDA_PcbGridFrame.
    // Before editing this code, remove the block markers.
    event.Skip();
////@end wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_CANCEL in WinEDA_PcbGridFrame.
}


