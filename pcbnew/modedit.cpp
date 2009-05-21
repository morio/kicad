/****************/
/* modedit.cpp  */
/****************/

#ifdef __GNUG__
#pragma implementation
#endif

#include "fctsys.h"
#include "appl_wxstruct.h"
#include "class_drawpanel.h"
#include "confirm.h"
#include "gestfich.h"
#include "appl_wxstruct.h"
#include "bitmaps.h"
#include "id.h"
#include "trigo.h"

#include "3d_viewer.h"
#include "pcbnew.h"
#include "protos.h"

#include "collectors.h"

/****************************************************************************/
BOARD_ITEM* WinEDA_ModuleEditFrame::ModeditLocateAndDisplay( int aHotKeyCode )
/****************************************************************************/
{
    BOARD_ITEM* item = GetCurItem();

    if( GetBoard()->m_Modules == NULL )
        return NULL;

    GENERAL_COLLECTORS_GUIDE guide = GetCollectorsGuide();

    // Assign to scanList the proper item types desired based on tool type
    // or hotkey that is in play.

    const KICAD_T* scanList = NULL;

    if( aHotKeyCode )
    {
        // @todo: add switch here and add calls to PcbGeneralLocateAndDisplay( int aHotKeyCode )
        // when searching is needed from a hotkey handler
    }
    else
    {
        scanList = GENERAL_COLLECTOR::ModulesAndTheirItems;
    }

    m_Collector->Collect( GetBoard(), scanList, GetScreen()->RefPos( true ), guide );

    /* Remove redundancies: when an item is found, we can remove the
     * module from list
     */
    if( m_Collector->GetCount() > 1 )
    {
        for( int ii = 0;  ii < m_Collector->GetCount(); ii++ )
        {
            item = (*m_Collector)[ii];
            if( item->Type() != TYPE_MODULE )
                continue;
            m_Collector->Remove( ii );
            ii--;
        }
    }

    if( m_Collector->GetCount() <= 1 )
    {
        item = (*m_Collector)[0];
        SetCurItem( item );
    }
    else    // we can't figure out which item user wants, do popup menu so user can choose
    {
        wxMenu      itemMenu;

        /* Give a title to the selection menu. This is also a cancel menu item */
        wxMenuItem* item_title = new wxMenuItem( &itemMenu, -1, _( "Selection Clarification" ) );
#ifdef __WINDOWS__
        wxFont      bold_font( *wxNORMAL_FONT );
        bold_font.SetWeight( wxFONTWEIGHT_BOLD );
        bold_font.SetStyle( wxFONTSTYLE_ITALIC );
        item_title->SetFont( bold_font );
#endif
        itemMenu.Append( item_title );
        itemMenu.AppendSeparator();

        int limit = MIN( MAX_ITEMS_IN_PICKER, m_Collector->GetCount() );

        for( int ii = 0;  ii<limit;  ++ii )
        {
            wxString     text;
            const char** xpm;

            item = (*m_Collector)[ii];

            text = item->MenuText( GetBoard() );
            xpm  = item->MenuIcon();

            ADD_MENUITEM( &itemMenu, ID_POPUP_PCB_ITEM_SELECTION_START + ii, text, xpm );
        }

        // this menu's handler is void WinEDA_BasePcbFrame::ProcessItemSelection()
        // and it calls SetCurItem() which in turn calls DisplayInfo() on the item.
        DrawPanel->m_AbortRequest = true;   // changed in false if an item
        PopupMenu( &itemMenu );             // m_AbortRequest = false if an item is selected

        DrawPanel->MouseToCursorSchema();

        DrawPanel->m_IgnoreMouseEvents = FALSE;

        // The function ProcessItemSelection() has set the current item, return it.
        item = GetCurItem();
    }

    if( item )
    {
        item->DisplayInfo( this );
    }

    return item;
}


void WinEDA_ModuleEditFrame::LoadModuleFromBoard( wxCommandEvent& event )
{
    GetScreen()->ClearUndoRedoList();
    Load_Module_Module_From_BOARD( NULL );
    GetScreen()->ClrModify();

    if( m_Draw3DFrame )
        m_Draw3DFrame->NewDisplay();
}


/****************************************************************************/
void WinEDA_ModuleEditFrame::Process_Special_Functions( wxCommandEvent& event )
/****************************************************************************/

/* Traite les selections d'outils et les commandes appelees du menu POPUP
 */
{
    int        id = event.GetId();
    wxPoint    pos;
    bool redraw = false;
    wxClientDC dc( DrawPanel );

    DrawPanel->CursorOff( &dc );
    DrawPanel->PrepareGraphicContext( &dc );

    wxGetMousePosition( &pos.x, &pos.y );

    pos.y += 20;

    switch( id )   // Arret eventuel de la commande de d�placement en cours
    {
    case wxID_CUT:
    case wxID_COPY:
    case ID_TOOLBARH_PCB_SELECT_LAYER:
    case ID_MODEDIT_PAD_SETTINGS:
    case ID_PCB_USER_GRID_SETUP:
    case ID_POPUP_PCB_ROTATE_TEXTEPCB:
    case ID_POPUP_PCB_EDIT_TEXTEPCB:
    case ID_POPUP_PCB_ROTATE_TEXTMODULE:
    case ID_POPUP_PCB_ROTATE_MODULE_CLOCKWISE:
    case ID_POPUP_PCB_ROTATE_MODULE_COUNTERCLOCKWISE:
    case ID_POPUP_PCB_EDIT_TEXTMODULE:
    case ID_POPUP_PCB_IMPORT_PAD_SETTINGS:
    case ID_POPUP_PCB_EXPORT_PAD_SETTINGS:
    case ID_POPUP_PCB_GLOBAL_IMPORT_PAD_SETTINGS:
    case ID_POPUP_PCB_STOP_CURRENT_DRAWING:
    case ID_POPUP_PCB_EDIT_EDGE:
    case ID_POPUP_PCB_EDIT_WIDTH_CURRENT_EDGE:
    case ID_POPUP_PCB_EDIT_WIDTH_ALL_EDGE:
    case ID_POPUP_PCB_EDIT_LAYER_CURRENT_EDGE:
    case ID_POPUP_PCB_EDIT_LAYER_ALL_EDGE:
    case ID_POPUP_PCB_ENTER_EDGE_WIDTH:
    case ID_POPUP_DELETE_BLOCK:
    case ID_POPUP_PLACE_BLOCK:
    case ID_POPUP_ZOOM_BLOCK:
    case ID_POPUP_MIRROR_Y_BLOCK:
    case ID_POPUP_MIRROR_X_BLOCK:
    case ID_POPUP_ROTATE_BLOCK:
    case ID_POPUP_COPY_BLOCK:
        break;

    case ID_POPUP_CANCEL_CURRENT_COMMAND:
        if( DrawPanel->ManageCurseur
            && DrawPanel->ForceCloseManageCurseur )
        {
            DrawPanel->ForceCloseManageCurseur( DrawPanel, &dc );
        }
        break;

    default:        // Arret dea commande de d�placement en cours
        if( DrawPanel->ManageCurseur
            && DrawPanel->ForceCloseManageCurseur )
        {
            DrawPanel->ForceCloseManageCurseur( DrawPanel, &dc );
        }
        SetToolID( 0, wxCURSOR_ARROW, wxEmptyString );
        break;
    }

    switch( id )   // Traitement des commandes
    {
    case ID_EXIT:
        Close( true );
        break;

    case ID_LIBEDIT_SELECT_CURRENT_LIB:
        Select_Active_Library();
        break;

    case ID_LIBEDIT_DELETE_PART:
    {
        wxFileName fn = wxFileName( wxEmptyString, m_CurrentLib,
                                    ModuleFileExtension );
        wxString   full_libraryfilename = wxGetApp().FindLibraryPath( fn );
        if( wxFileName::FileExists( full_libraryfilename ) )
            Delete_Module_In_Library( full_libraryfilename );
        break;
    }

    case ID_MODEDIT_NEW_MODULE:
    {
        Clear_Pcb( true );
        GetScreen()->ClearUndoRedoList();
        SetCurItem( NULL );
        GetScreen()->m_Curseur = wxPoint( 0, 0 );

        MODULE* module = Create_1_Module( &dc, wxEmptyString );
        if( module )        // i.e. if create module command not aborted
        {
            module->SetPosition( wxPoint( 0, 0 ) );
            if( GetBoard()->m_Modules )
                GetBoard()->m_Modules->m_Flags = 0;
            Zoom_Automatique( true );
        }
        break;
    }

    case ID_MODEDIT_SAVE_LIBMODULE:
    {
        wxFileName fn;
        fn = wxFileName( wxEmptyString, m_CurrentLib, ModuleFileExtension );
        wxString full_filename = wxGetApp().FindLibraryPath( fn );
        Save_Module_In_Library( full_filename, GetBoard()->m_Modules,
                                true, true, true );
        GetScreen()->ClrModify();
        break;
    }

    case ID_MODEDIT_INSERT_MODULE_IN_BOARD:
    case ID_MODEDIT_UPDATE_MODULE_IN_BOARD:
    {
        // update module in the current board,
        // not just add it to the board with total disregard for the netlist...
        WinEDA_PcbFrame* pcbframe = (WinEDA_PcbFrame*) GetParent();
        BOARD*           mainpcb  = pcbframe->GetBoard();
        MODULE*          source_module  = NULL;
        MODULE*          module_in_edit = GetBoard()->m_Modules;

        // Search the old module (source) if exists
        // Because this source could be deleted when editing the main board...
        if( module_in_edit->m_Link )        // this is not a new module ...
        {
            source_module = mainpcb->m_Modules;
            for( ; source_module != NULL; source_module = (MODULE*) source_module->Next() )
            {
                if( module_in_edit->m_Link == source_module->m_TimeStamp )
                    break;
            }
        }
        if( (source_module == NULL) && id == (ID_MODEDIT_UPDATE_MODULE_IN_BOARD) )      // source not found
        {
            wxString msg;
            msg.Printf( _( "Unable to find the footprint source on the main board" ) );
            msg << _( "\nCannot update the footprint" );
            DisplayError( this, msg );
            break;
        }

        if( (source_module != NULL) && id == (ID_MODEDIT_INSERT_MODULE_IN_BOARD) )      // source not found
        {
            wxString msg;
            msg.Printf( _( "A footprint source was found on the main board" ) );
            msg << _( "\nCannot insert this footprint" );
            DisplayError( this, msg );
            break;
        }

        // Create the "new" module
        MODULE* newmodule = new MODULE( mainpcb );
        newmodule->Copy( module_in_edit );
        newmodule->m_Link = 0;

        // Put the footprint in the main pcb linked list.
        mainpcb->Add( newmodule );

        if( source_module )         // this is an update command
        {
            // The new module replace the old module (pos, orient, ref, value and connexions are kept)
            // and the source_module (old module) is deleted
            newmodule = pcbframe->Exchange_Module( this, source_module, newmodule );
            newmodule->m_TimeStamp = module_in_edit->m_Link;
        }
        else        // This is an insert command
        {
            wxPoint cursor_pos = pcbframe->GetScreen()->m_Curseur;
            pcbframe->GetScreen()->m_Curseur = wxPoint( 0, 0 );
            pcbframe->Place_Module( newmodule, NULL );
            pcbframe->GetScreen()->m_Curseur = cursor_pos;
            newmodule->m_TimeStamp = GetTimeStamp();
        }

        newmodule->m_Flags = 0;
        GetScreen()->ClrModify();
        pcbframe->SetCurItem( NULL );
        mainpcb->m_Status_Pcb = 0;
    }
    break;

    case ID_MODEDIT_IMPORT_PART:
        GetScreen()->ClearUndoRedoList();
        SetCurItem( NULL );
        Clear_Pcb( true );
        GetScreen()->m_Curseur = wxPoint( 0, 0 );
        Import_Module( &dc );
        if( GetBoard()->m_Modules )
            GetBoard()->m_Modules->m_Flags = 0;
        GetScreen()->ClrModify();
        Zoom_Automatique( true );
        if( m_Draw3DFrame )
            m_Draw3DFrame->NewDisplay();
        break;

    case ID_MODEDIT_EXPORT_PART:
        if( GetBoard()->m_Modules )
            Export_Module( GetBoard()->m_Modules, FALSE );
        break;

    case ID_MODEDIT_CREATE_NEW_LIB_AND_SAVE_CURRENT_PART:
        if( GetBoard()->m_Modules )
            Export_Module( GetBoard()->m_Modules, true );
        break;

    case ID_MODEDIT_SHEET_SET:
        break;

    case ID_MODEDIT_LOAD_MODULE:
    {
        wxString full_libraryfilename;
        if( !m_CurrentLib.IsEmpty() )
        {
            wxFileName fn = wxFileName( wxEmptyString, m_CurrentLib,
                                        ModuleFileExtension );
            full_libraryfilename = wxGetApp().FindLibraryPath( fn );
        }

        GetScreen()->ClearUndoRedoList();
        SetCurItem( NULL );
        Clear_Pcb( true );
        GetScreen()->m_Curseur = wxPoint( 0, 0 );
        Load_Module_From_Library( full_libraryfilename, NULL );
        redraw = true;
    }
        if( GetBoard()->m_Modules )
            GetBoard()->m_Modules->m_Flags = 0;

        //if either m_Reference or m_Value are gone, reinstate them -
        //otherwise it becomes hard to see what you are working with in the layout!
        if( GetBoard() && GetBoard()->m_Modules )
        {
            TEXTE_MODULE* ref = GetBoard()->m_Modules->m_Reference;
            TEXTE_MODULE* val = GetBoard()->m_Modules->m_Value;
            if( val && ref )
            {
                ref->m_Type = TEXT_is_REFERENCE;    // just in case ...
                if( ref->m_Text.Length() == 0 )
                    ref->m_Text = L"Ref**";
                val->m_Type = TEXT_is_VALUE;        // just in case ...
                if( val->m_Text.Length() == 0 )
                    val->m_Text = L"Val**";
            }
        }
        GetScreen()->ClrModify();
        Zoom_Automatique( true );
        if( m_Draw3DFrame )
            m_Draw3DFrame->NewDisplay();
        break;

    case ID_MODEDIT_PAD_SETTINGS:
        InstallPadOptionsFrame( NULL, NULL, wxPoint( -1, -1 ) );
        break;

    case ID_MODEDIT_CHECK:
        break;

    case ID_MODEDIT_EDIT_MODULE_PROPERTIES:
        if( GetBoard()->m_Modules )
        {
            SetCurItem( GetBoard()->m_Modules );
            InstallModuleOptionsFrame( (MODULE*) GetScreen()->GetCurItem(),
                                      &dc, pos );
            GetScreen()->GetCurItem()->m_Flags = 0;
        }
        break;

    case ID_MODEDIT_ADD_PAD:
        if( GetBoard()->m_Modules )
            SetToolID( id, wxCURSOR_PENCIL, _( "Add Pad" ) );
        else
        {
            SetToolID( id, wxCURSOR_ARROW, _( "Pad Settings" ) );
            InstallPadOptionsFrame( NULL, NULL, wxPoint( -1, -1 ) );
            SetToolID( 0, wxCURSOR_ARROW, wxEmptyString );
        }
        break;

    case ID_LINE_COMMENT_BUTT:
    case ID_PCB_ARC_BUTT:
    case ID_PCB_CIRCLE_BUTT:
    case ID_TEXT_COMMENT_BUTT:
        SetToolID( id, wxCURSOR_PENCIL, _( "Add Drawing" ) );
        break;

    case ID_MODEDIT_PLACE_ANCHOR:
        SetToolID( id, wxCURSOR_PENCIL, _( "Place anchor" ) );
        break;

    case ID_NO_SELECT_BUTT:
        SetToolID( 0, wxCURSOR_ARROW, wxEmptyString );
        break;

    case ID_POPUP_CLOSE_CURRENT_TOOL:
        break;

    case ID_POPUP_CANCEL_CURRENT_COMMAND:
        break;

    case ID_MODEDIT_DELETE_ITEM_BUTT:
        SetToolID( id, wxCURSOR_BULLSEYE, _( "Delete item" ) );
        break;

    case ID_POPUP_PCB_ROTATE_MODULE_COUNTERCLOCKWISE:
        DrawPanel->MouseToCursorSchema();
        Rotate_Module( NULL, (MODULE*) GetScreen()->GetCurItem(), 900, true );
        break;

    case ID_POPUP_PCB_ROTATE_MODULE_CLOCKWISE:
        DrawPanel->MouseToCursorSchema();
        Rotate_Module( &dc, (MODULE*) GetScreen()->GetCurItem(), -900, true );
        break;

    case ID_POPUP_PCB_EDIT_MODULE:
        InstallModuleOptionsFrame( (MODULE*) GetScreen()->GetCurItem(),
                                  &dc, pos );
        GetScreen()->GetCurItem()->m_Flags = 0;
        DrawPanel->MouseToCursorSchema();
        break;

    case ID_POPUP_PCB_MOVE_PAD_REQUEST:
        DrawPanel->MouseToCursorSchema();
        StartMovePad( (D_PAD*) GetScreen()->GetCurItem(), &dc );
        break;

    case ID_POPUP_PCB_EDIT_PAD:
        InstallPadOptionsFrame( (D_PAD*) GetScreen()->GetCurItem(),
                               &dc, pos );
        DrawPanel->MouseToCursorSchema();
        break;

    case ID_POPUP_PCB_DELETE_PAD:
        SaveCopyInUndoList( GetBoard()->m_Modules );
        DeletePad( (D_PAD*) GetScreen()->GetCurItem() );
        SetCurItem( NULL );
        DrawPanel->MouseToCursorSchema();
        break;

    case ID_POPUP_PCB_IMPORT_PAD_SETTINGS:
        SaveCopyInUndoList( GetBoard()->m_Modules );
        DrawPanel->MouseToCursorSchema();
        Import_Pad_Settings( (D_PAD*) GetScreen()->GetCurItem(), true );
        break;

    case ID_POPUP_PCB_GLOBAL_IMPORT_PAD_SETTINGS:
        SaveCopyInUndoList( GetBoard()->m_Modules );
        Global_Import_Pad_Settings( (D_PAD*) GetScreen()->GetCurItem(), true );
        DrawPanel->MouseToCursorSchema();
        break;

    case ID_POPUP_PCB_EXPORT_PAD_SETTINGS:
        DrawPanel->MouseToCursorSchema();
        Export_Pad_Settings( (D_PAD*) GetScreen()->GetCurItem() );
        break;

    case ID_POPUP_PCB_EDIT_TEXTMODULE:
        InstallTextModOptionsFrame( (TEXTE_MODULE*) GetScreen()->GetCurItem(),
                                   &dc, pos );
        DrawPanel->MouseToCursorSchema();
        break;

    case ID_POPUP_PCB_MOVE_TEXTMODULE_REQUEST:
        DrawPanel->MouseToCursorSchema();
        StartMoveTexteModule( (TEXTE_MODULE*) GetScreen()->GetCurItem(),
                             &dc );
        break;

    case ID_POPUP_PCB_ROTATE_TEXTMODULE:
        RotateTextModule( (TEXTE_MODULE*) GetScreen()->GetCurItem(),
                         &dc );
        DrawPanel->MouseToCursorSchema();
        break;

    case ID_POPUP_PCB_DELETE_TEXTMODULE:
        SaveCopyInUndoList( GetBoard()->m_Modules );
        DeleteTextModule( (TEXTE_MODULE*) GetScreen()->GetCurItem(),
                         &dc );
        SetCurItem( NULL );
        DrawPanel->MouseToCursorSchema();
        break;

    case ID_POPUP_PCB_MOVE_EDGE:
        Start_Move_EdgeMod( (EDGE_MODULE*) GetScreen()->GetCurItem(), &dc );
        DrawPanel->MouseToCursorSchema();
        break;

    case ID_POPUP_PCB_STOP_CURRENT_DRAWING:
        DrawPanel->MouseToCursorSchema();
        if( (GetScreen()->GetCurItem()->m_Flags & IS_NEW) )
        {
            End_Edge_Module( (EDGE_MODULE*) GetScreen()->GetCurItem(), &dc );
            SetCurItem( NULL );
        }
        break;

    case ID_POPUP_PCB_ENTER_EDGE_WIDTH:
    {
        EDGE_MODULE* edge = NULL;
        if( GetScreen()->GetCurItem()
           && ( GetScreen()->GetCurItem()->m_Flags & IS_NEW)
           && (GetScreen()->GetCurItem()->Type() == TYPE_EDGE_MODULE) )
        {
            edge = (EDGE_MODULE*) GetScreen()->GetCurItem();
        }
        Enter_Edge_Width( edge, &dc );
        DrawPanel->MouseToCursorSchema();
    }
    break;

    case ID_POPUP_PCB_EDIT_WIDTH_CURRENT_EDGE:
        DrawPanel->MouseToCursorSchema();
        Edit_Edge_Width( (EDGE_MODULE*) GetScreen()->GetCurItem(), &dc );
        break;

    case ID_POPUP_PCB_EDIT_WIDTH_ALL_EDGE:
        DrawPanel->MouseToCursorSchema();
        Edit_Edge_Width( NULL, &dc );
        break;

    case ID_POPUP_PCB_EDIT_LAYER_CURRENT_EDGE:
        DrawPanel->MouseToCursorSchema();
        Edit_Edge_Layer( (EDGE_MODULE*) GetScreen()->GetCurItem(), &dc );
        break;

    case ID_POPUP_PCB_EDIT_LAYER_ALL_EDGE:
        DrawPanel->MouseToCursorSchema();
        Edit_Edge_Layer( NULL, &dc );
        break;

    case ID_POPUP_PCB_DELETE_EDGE:
        SaveCopyInUndoList( GetBoard()->m_Modules );
        DrawPanel->MouseToCursorSchema();
        RemoveStruct( GetScreen()->GetCurItem(), &dc );
        SetCurItem( NULL );
        break;

    case ID_MODEDIT_MODULE_ROTATE:
    case ID_MODEDIT_MODULE_MIRROR:
    case ID_MODEDIT_MODULE_SCALE:
    case ID_MODEDIT_MODULE_SCALEX:
    case ID_MODEDIT_MODULE_SCALEY:
        SaveCopyInUndoList( GetBoard()->m_Modules );
        Transform( (MODULE*) GetScreen()->GetCurItem(), &dc, id );
        break;

    case ID_PCB_DRAWINGS_WIDTHS_SETUP:
        InstallOptionsFrame( pos );
        break;

    case ID_PCB_PAD_SETUP:
    {
        BOARD_ITEM* item = GetCurItem();
        if( item )
        {
            if( item->Type() != TYPE_PAD )
                item = NULL;
        }
        InstallPadOptionsFrame( (D_PAD*) item, &dc, pos );
    }
    break;

    case ID_PCB_USER_GRID_SETUP:
        InstallGridFrame( pos );
        break;

    case ID_MODEDIT_UNDO:
        GetComponentFromUndoList();
        DrawPanel->Refresh( true );
        break;

    case ID_MODEDIT_REDO:
        GetComponentFromRedoList();
        DrawPanel->Refresh( true );
        break;

    case ID_POPUP_PLACE_BLOCK:
        GetScreen()->BlockLocate.m_Command = BLOCK_MOVE;
        DrawPanel->m_AutoPAN_Request = FALSE;
        HandleBlockPlace( &dc );
        break;

    case ID_POPUP_COPY_BLOCK:
        GetScreen()->BlockLocate.m_Command = BLOCK_COPY;
        GetScreen()->BlockLocate.SetMessageBlock( this );
        DrawPanel->m_AutoPAN_Request = FALSE;
        HandleBlockPlace( &dc );
        break;

    case ID_POPUP_ZOOM_BLOCK:
        GetScreen()->BlockLocate.m_Command = BLOCK_ZOOM;
        GetScreen()->BlockLocate.SetMessageBlock( this );
        GetScreen()->BlockLocate.SetMessageBlock( this );
        HandleBlockEnd( &dc );
        break;

    case ID_POPUP_DELETE_BLOCK:
        GetScreen()->BlockLocate.m_Command = BLOCK_DELETE;
        GetScreen()->BlockLocate.SetMessageBlock( this );
        HandleBlockEnd( &dc );
        break;

    case ID_POPUP_ROTATE_BLOCK:
        GetScreen()->BlockLocate.m_Command = BLOCK_ROTATE;
        GetScreen()->BlockLocate.SetMessageBlock( this );
        HandleBlockEnd( &dc );
        break;

    case ID_POPUP_MIRROR_X_BLOCK:
    case ID_POPUP_MIRROR_Y_BLOCK:
    case ID_POPUP_INVERT_BLOCK:
        GetScreen()->BlockLocate.m_Command = BLOCK_INVERT;
        GetScreen()->BlockLocate.SetMessageBlock( this );
        HandleBlockEnd( &dc );
        break;

    default:
        DisplayError( this,
                     wxT( "WinEDA_ModuleEditFrame::Process_Special_Functions error" ) );
        break;
    }

    SetToolbars();
    DrawPanel->CursorOn( &dc );
    if ( redraw )
        DrawPanel->Refresh();
}


/******************************************************************************/
void WinEDA_ModuleEditFrame::Transform( MODULE* module, wxDC* DC, int transform )
/******************************************************************************/

/* Execute les transformations de la repr�sentation des modules.
 *  le module, apres transformation est toujours en position de reference:
 *  position 0,0
 *  orientation 0, cot� composant.
 */
{
    D_PAD*          pad = module->m_Pads;
    EDA_BaseStruct* PtStruct = module->m_Drawings;
    TEXTE_MODULE*   textmod;
    EDGE_MODULE*    edgemod;
    int             angle = 900; // NECESSAIREMENT +- 900 (+- 90 degres) )

    switch( transform )
    {
    case ID_MODEDIT_MODULE_ROTATE:
        module->SetOrientation( angle );

        for( ; pad != NULL; pad = (D_PAD*) pad->Next() )
        {
            pad->m_Pos0    = pad->m_Pos;
            pad->m_Orient -= angle;
            RotatePoint( &pad->m_Offset.x, &pad->m_Offset.y, angle );
            EXCHG( pad->m_Size.x, pad->m_Size.y );
            RotatePoint( &pad->m_DeltaSize.x, &pad->m_DeltaSize.y, -angle );
        }

        module->m_Reference->m_Pos0    = module->m_Reference->m_Pos;
        module->m_Reference->m_Orient += angle;
        if( module->m_Reference->m_Orient >= 1800 )
            module->m_Reference->m_Orient -= 1800;
        module->m_Value->m_Pos0    = module->m_Value->m_Pos;
        module->m_Value->m_Orient += angle;
        if( module->m_Value->m_Orient >= 1800 )
            module->m_Value->m_Orient -= 1800;

        /* Rectification des contours et textes de l'empreinte : */
        for( ; PtStruct != NULL; PtStruct = PtStruct->Next() )
        {
            if( PtStruct->Type() == TYPE_EDGE_MODULE )
            {
                edgemod = (EDGE_MODULE*) PtStruct;
                edgemod->m_Start0 = edgemod->m_Start;
                edgemod->m_End0   = edgemod->m_End;
            }
            if( PtStruct->Type() == TYPE_TEXTE_MODULE )
            {
                /* deplacement des inscriptions : */
                textmod = (TEXTE_MODULE*) PtStruct;
                textmod->m_Pos0 = textmod->m_Pos;
            }
        }

        module->m_Orient = 0;
        break;

    case ID_MODEDIT_MODULE_MIRROR:
        for( ; pad != NULL; pad = (D_PAD*) pad->Next() )
        {
            pad->m_Pos.y       = -pad->m_Pos.y;
            pad->m_Pos0.y      = -pad->m_Pos0.y;
            pad->m_Offset.y    = -pad->m_Offset.y;
            pad->m_DeltaSize.y = -pad->m_DeltaSize.y;
            if( pad->m_Orient )
                pad->m_Orient = 3600 - pad->m_Orient;
        }

        /* Inversion miroir de la Reference */
        textmod = module->m_Reference;
        textmod->m_Pos.y  = -textmod->m_Pos.y;
        textmod->m_Pos0.y = textmod->m_Pos0.y;
        if( textmod->m_Orient )
            textmod->m_Orient = 3600 - textmod->m_Orient;

        /* Inversion miroir de la Valeur  */
        textmod = module->m_Value;
        textmod->m_Pos.y  = -textmod->m_Pos.y;
        textmod->m_Pos0.y = textmod->m_Pos0.y;
        if( textmod->m_Orient )
            textmod->m_Orient = 3600 - textmod->m_Orient;

        /* Inversion miroir des dessins de l'empreinte : */
        PtStruct = module->m_Drawings;
        for( ; PtStruct != NULL; PtStruct = PtStruct->Next() )
        {
            switch( PtStruct->Type() )
            {
            case TYPE_EDGE_MODULE:
                edgemod = (EDGE_MODULE*) PtStruct;
                edgemod->m_Start.y = -edgemod->m_Start.y;
                edgemod->m_End.y   = -edgemod->m_End.y;
                /* inversion des coords locales */
                edgemod->m_Start0.y = -edgemod->m_Start0.y;
                edgemod->m_End0.y   = -edgemod->m_End0.y;
                break;

            case TYPE_TEXTE_MODULE:
                /* Inversion miroir de la position et mise en miroir : */
                textmod = (TEXTE_MODULE*) PtStruct;
                textmod->m_Pos.y  = -textmod->m_Pos.y;
                textmod->m_Pos0.y = textmod->m_Pos0.y;
                if( textmod->m_Orient )
                    textmod->m_Orient = 3600 - textmod->m_Orient;
                break;

            default:
                DisplayError( this, wxT( "Type Draw Indefini" ) );
                break;
            }
        }

        break;

    case ID_MODEDIT_MODULE_SCALE:
    case ID_MODEDIT_MODULE_SCALEX:
    case ID_MODEDIT_MODULE_SCALEY:
        DisplayInfoMessage( this, wxT( "Not availlable" ) );
        break;
    }

    module->Set_Rectangle_Encadrement();
    DrawPanel->ReDraw( DC );
}
