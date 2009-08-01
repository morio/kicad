/***********************/
/**** ratsnest.cpp  ****/
/* Ratsnets functions  */
/***********************/

#include "fctsys.h"
#include "gr_basic.h"
#include "common.h"
#include "class_drawpanel.h"
#include "confirm.h"
#include "pcbnew.h"
#include "autorout.h"

#include "protos.h"


/* local variables */
static std::vector <D_PAD*> s_localPadBuffer;           // for local ratsnest calculations when moving a footprint: buffer of pads to consider

static bool DisplayRastnestInProgress;                  // Enable the display of the ratsnest during the ratsnest computations

/* Note about the ratsnest computation:
 *  Building the general ratsnest:
 *  I used the "lee algoritm".
 *  This is a 2 steps algoritm.
 *  the m_SubRatsnest member of pads handle a "block number" or a "cluster number" or a "subnet number"
 *  initially, m_SubRatsnest = 0 (pad not connected).
 *  Build_Board_Ratsnest( wxDC* DC )  Create this rastnest
 *  for each net:
 *  First:
 *  we create a link (and therefore a logical block) between 2 pad. This is achieved by:
 *  search for a pad without link.
 *  search its nearest pad
 *  link these 2 pads (i.e. create a ratsnest item)
 *  the pads are grouped in a logical block ( a cluster).
 *  until no pad without link found.
 *  Each logical block has a number called block number or "subnet number",
 *  stored in m_SubRatsnest member for each pad of the block.
 *  The first block has its block number = 1, the second is 2 ...
 *  the function to do thas is gen_rats_pad_to_pad()
 *
 *  Secondly:
 *  The first pass created many logical blocks
 *  A block contains 2 or more pads.
 *  we create links between 2 block. This is achieved by:
 *  Test all pads in the first block, and search (for each pad)
 *  a neighboor in other blocks and compute the distance between pads,
 *  We select the pad pair which have the smallest distance.
 *  These 2 pads are linked (i.e. a new ratsnest item is created between thes 2 pads)
 *  and the 2 block are merged.
 *  Therefore the logical block 1 contains the initial block 1 "eats" the pads of the other block
 *  The computation is made until only one block is found.
 *  the function used is gen_rats_block_to_block()
 *
 *
 *  How existing and new tracks are handled:
 *  The complete rastnest (using the pad analysis) is computed.
 *  it is independant of the tracks and handle the "logical connections".
 *  It depends only on the footprints geometry (and the netlist),
 *  and must be computed only after a netlist read or a footprints geometry change.
 *  Each link (ratsnest) can only be INACTIVE (because pads are connected by a track) or ACTIVE (no tracks)
 *
 *  After the complete rastnest is built, or when a track is added or deleted,
 * we run an algorithm derived from the complete rastnest computation.
 * it is much faster because it analyses only the existing rastnest and not all the pads list
 * and determine only if an existing rastnest must be activated
 * (no physical track exists) or not (a physical track exists)
 * if a track is added or deleted only the corresponding net is tested.
 *
 *  the m_SubRatsnest member of pads is set to 0 (no blocks), and alls links are set to INACTIVE (ratsnest not show).
 *  Before running this fast lee algorithm, we create blocks (and their corresponding block number)
 *  by grouping pads connected by tracks.
 *  So, when tracks exists, the fast lee algorithm is started with some blocks already created.
 * because the fast lee algorithm test only the ratsnest and does not search for
 * nearest pads (this search was previously made) the online ratsnest can be done
 * when a track is created without noticeable computing time
 *  First:
 * for all links (in this step, all are inactive):
 * search for a link which have 1 (or 2) pad having the m_SubRatsnest member = 0.
 * if found the link is set to ACTIVE (i.e. the ratsnest will be showed) and the pad is meged with the block
 * or a new block is created ( see tst_rats_pad_to_pad() ).
 * Secondly:
 * blocks are tested:
 * for all links we search if the 2 pads linkeds are in 2 different block.
 * if yes, the link status is set to ACTIVE, and the 2 block are merged
 * until only one block is found
 * ( see tst_rats_block_to_block() )
 *
 *
 */
/******************************************************************************/
void WinEDA_BasePcbFrame::Compile_Ratsnest( wxDC* DC, bool display_status_pcb )
/******************************************************************************/

/** Function Compile_Ratsnest
 *  Create the entire board ratsnesr.
 *  Must be called after a board change (changes for
 *  pads, footprints or a read netlist ).
 *
 *  @param display_status_pcb : if true, display the computation results
 */
{
    wxString msg;

    DisplayRastnestInProgress = TRUE;


    GetBoard()->m_Status_Pcb = 0;                   /* we want a full ratnest computation, from the scratch */
    MsgPanel->EraseMsgBox();

    // Rebuild the full pads and net info list
    RecalculateAllTracksNetcode();

    if( display_status_pcb )
    {
        msg.Printf( wxT( " %d" ), m_Pcb->GetPadsCount() );
        Affiche_1_Parametre( this, 1, wxT( "pads" ), msg, RED );
    }

    if( display_status_pcb )
    {
        msg.Printf( wxT( " %d" ), m_Pcb->m_NetInfo->GetNetsCount() );
        Affiche_1_Parametre( this, 8, wxT( "Nets" ), msg, CYAN );
    }

    /* Compute the full ratsnest
     *  which can be see like all the possible links or logical connections.
     *  some of thems are active (no track connected) and others are inactive (when track connect pads)
     *  This full ratsnest is not modified by track editing.
     *  It changes only when a netlist is read, or footprints are modified
     */
    Build_Board_Ratsnest( DC );

    /* Compute the pad connections due to the existing tracks (physical connections)*/
    test_connexions( DC );

    /* Compute the active ratsnest, i.e. the unconnected links
     *  it is faster than Build_Board_Ratsnest()
     *  because many optimisations and computations are already made
     */
    Tst_Ratsnest( DC, 0 );

    // Redraw the active ratsnest ( if enabled )
    if( g_Show_Ratsnest && DC )
        DrawGeneralRatsnest( DC, 0 );

    if( display_status_pcb )
        m_Pcb->DisplayInfo( this );
}


/*****************************************************************/
static int sortByNetcode( const void* o1, const void* o2 )
/****************************************************************/

/* Sort function used by  QSORT
 *  Sort pads by net code
 */
{
    D_PAD** pt_ref     = (D_PAD**) o1;
    D_PAD** pt_compare = (D_PAD**) o2;

    return (*pt_ref)->GetNet() - (*pt_compare)->GetNet();
}


/********************************************************/
static int sort_by_length( const void* o1, const void* o2 )
/********************************************************/

/* Sort function used by  QSORT
 *  Sort ratsnest by lenght
 */
{
    RATSNEST_ITEM* ref     = (RATSNEST_ITEM*) o1;
    RATSNEST_ITEM* compare = (RATSNEST_ITEM*) o2;

    return ref->m_Lenght - compare->m_Lenght;
}


/*****************************************************************************/
static int gen_rats_block_to_block( std::vector<RATSNEST_ITEM>& aRatsnestBuffer,
                                    std::vector<D_PAD*>&        aPadBuffer,
                                    unsigned                    aPadIdxStart,
                                    unsigned                    aPadIdxMax )
/*****************************************************************************/

/**
 *  Function used by Build_Board_Ratsnest()
 *  This function creates a rastsnet between two blocks ( which fit the same net )
 *  A block is a group of pads already linked (by a previous ratsnest computation, or tracks)
 *  The search is made between the pads in block 1 (the reference block) and other blocks
 *  the block n ( n > 1 ) it connected to block 1 by their 2 nearest pads.
 *  When the block is found, it is merged with the block 1
 *  the D_PAD member m_SubRatsnest handles the block number
 *  @param aRatsnestBuffer = a std::vector<RATSNEST_ITEM> buffer to fill with new ratsnest items
 *  @param aPadBuffer = a std::vector<D_PAD*> that is the list of pads to consider
 *  @param  aPadIdxStart = starting index (within the pad list) for search
 *  @param  aPadIdxMax	  = ending index (within the pad list) for search
 *  @return blocks not connected count
 */
{
    int dist_min, current_dist;
    int current_num_block = 1;
    int padBlock1Idx = -1;              // Index in aPadBuffer for the "better" pad found in block 1
    int padBlockToMergeIdx = -1;        // Index in aPadBuffer for the "better" pad found in block to merge

    dist_min = 0x7FFFFFFF;

    /* Search the nearest pad from block 1 */
    for( unsigned ii = aPadIdxStart; ii < aPadIdxMax; ii++ )
    {
        D_PAD* ref_pad = aPadBuffer[ii];

        /* search a pad which is in the block 1 */
        if( ref_pad->GetSubRatsnest() != 1 )
            continue;

        /* pad is found, search its nearest neighbour in other blocks */
        for( unsigned jj = aPadIdxStart; jj < aPadIdxMax; jj++ )
        {
            D_PAD* curr_pad = aPadBuffer[jj];

            if( curr_pad->GetSubRatsnest() == 1 )  // not in an other block
                continue;

            /* Compare distance between pads ("Manhattan" distance) */
            current_dist = abs( curr_pad->m_Pos.x - ref_pad->m_Pos.x ) +
                           abs( curr_pad->m_Pos.y - ref_pad->m_Pos.y );

            if( dist_min > current_dist )   // we have found a better pad pair
            {
                // The tested block can be a good candidate for merging
                // we memorise the "best" current values for merging
                current_num_block = curr_pad->GetSubRatsnest();
                dist_min = current_dist;

                padBlockToMergeIdx = jj;
                padBlock1Idx = ii;
            }
        }
    }

    /*  The reference block is labelled block 1.
     *  if current_num_block != 1 we have found an other block, and we must merge it
     *  with the reference block
     *  The link is made by the 2 nearest pads
     */
    if( current_num_block > 1 )
    {
        /* The block n (n=current_num_block) is merged with the bloc 1 :
         *  to do that, we set the m_SubRatsnest member to 1 for all pads in block n
         */
        for( unsigned ii = aPadIdxStart; ii < aPadIdxMax; ii++ )
        {
            D_PAD* pad = aPadBuffer[ii];
            if( pad->GetSubRatsnest() == current_num_block )
                pad->SetSubRatsnest( 1 );
        }

        if( padBlock1Idx < 0 )
            DisplayError( NULL, wxT( "gen_rats_block_to_block() internal error" ) );
        else
        {
            /* Create the new ratsnet */
            RATSNEST_ITEM net;
            net.SetNet( aPadBuffer[padBlock1Idx]->GetNet() );
            net.m_Status   = CH_ACTIF | CH_VISIBLE;
            net.m_Lenght   = dist_min;
            net.m_PadStart = aPadBuffer[padBlock1Idx];
            net.m_PadEnd   = aPadBuffer[padBlockToMergeIdx];
            aRatsnestBuffer.push_back( net );
        }
    }
    return current_num_block;
}


/*****************************************************************************/
static int gen_rats_pad_to_pad( vector<RATSNEST_ITEM>& aRatsnestBuffer,
                                std::vector<D_PAD*>&   aPadBuffer,
                                unsigned               aPadIdxStart,
                                unsigned               aPadIdxMax,
                                int                    current_num_block )
/*****************************************************************************/

/**
 *  Function used by Build_Board_Ratsnest()
 *  this is the first pass of the lee algorithm
 *  This function creates the link (ratsnest) between 2 pads ( fitting the same net )
 *  the function search for a first not connected pad
 *  and search its nearest neighboor
 * Its creates a block if the 2 pads are not connected, or merge the unconnected pad to the existing block.
 * These blocks include 2 pads and the 2 pads are linked by a ratsnest.
 *
 * @param aRatsnestBuffer = a std::vector<RATSNEST_ITEM> buffer to fill with new ratsnest items
 * @param aPadBuffer = a std::vector<D_PAD*> that is the list of pads to consider
 * @param  aPadIdxStart = starting index (within the pad list) for search
 * @param  aPadIdxMax	  = ending index (within the pad list) for search
 * @param   current_num_block = Last existing block number de pads
 *      These block are created by the existing tracks analysis
 *
 * @return the last block number used
 */
{
    int    dist_min, current_dist;
    D_PAD* ref_pad, * pad;

    for( unsigned ii = aPadIdxStart; ii < aPadIdxMax; ii++ )
    {
        ref_pad = aPadBuffer[ii];

        if( ref_pad->GetSubRatsnest() )
            continue; // Pad already connected

        dist_min = 0x7FFFFFFF;
        int padBlockToMergeIdx = -1;        // Index in aPadBuffer for the "better" pad found in block to merge
        for( unsigned jj = aPadIdxStart; jj < aPadIdxMax; jj++ )
        {
            if( ii == jj )
                continue;

            pad = aPadBuffer[jj];

            /* Compare distance between pads ("Manhattan" distance) */
            current_dist = abs( pad->m_Pos.x - ref_pad->m_Pos.x ) +
                           abs( pad->m_Pos.y - ref_pad->m_Pos.y );

            if( dist_min > current_dist )
            {
                dist_min = current_dist;
                padBlockToMergeIdx = jj;
            }
        }

        if( padBlockToMergeIdx >= 0 )
        {
            pad = aPadBuffer[padBlockToMergeIdx];

            /* Update the block number
             *  if the 2 pads are not already created : a new block is created
             */
            if( (pad->GetSubRatsnest() == 0) && (ref_pad->GetSubRatsnest() == 0) )
            {
                current_num_block++;        // Creates a new block number (or subratsnest)
                pad->SetSubRatsnest( current_num_block );
                ref_pad->SetSubRatsnest( current_num_block );
            }
            /* If a pad is already connected connected : merge the other pad in the block */
            else
            {
                ref_pad->SetSubRatsnest( pad->GetSubRatsnest() );
            }

            /* Create the new ratsnet item */
            RATSNEST_ITEM rast;
            rast.SetNet( ref_pad->GetNet() );
            rast.m_Status   = CH_ACTIF | CH_VISIBLE;
            rast.m_Lenght   = dist_min;
            rast.m_PadStart = ref_pad;
            rast.m_PadEnd   = pad;
            aRatsnestBuffer.push_back( rast );
        }
    }

    return current_num_block;
}


/***********************************************************/
void WinEDA_BasePcbFrame::Build_Board_Ratsnest( wxDC* DC )
/***********************************************************/

/**  Function to compute the full ratsnest (using the LEE algorithm )
 *  In the functions tracks are not considered
 *  This is only the "basic" ratsnest depending only on pads.
 *
 *  - Create the sorted pad list (if necessary)
 *          The active pads (i.e included in a net ) are called nodes
 *    This pad list is sorted by net codes
 *
 *  - Compute the ratsnest (LEE algorithm ):
 *      a - Create the ratsnest between a not connected pad and its nearest
 *          neighbour. Blocks of pads are created
 *      b - Create the ratsnest between blocks:
 *          Test the pads of the 1st block and create a link (ratsnest)
 *           with the nearest pad found in an other block.
 *          The other block is merged with the first block.
 *           until only one block is left.
 *
 *   A ratnest can be seen as a logical connection.
 *
 * Update :
 *      nb_nodes = Active pads count for the board
 *      nb_links = link count for the board (logical connection count)
 *           (there are n-1 links for an equipotent which have n active pads) .
 *
 */
{
    D_PAD* pad;
    int    noconn;

    m_Pcb->m_NbNoconnect = 0;

    m_Pcb->m_FullRatsnest.clear();

    if( m_Pcb->GetPadsCount() == 0 )
        return;

    /* Created pad list and the net_codes if needed */
    if( (m_Pcb->m_Status_Pcb & NET_CODES_OK) == 0 )
        m_Pcb->m_NetInfo->BuildListOfNets();

    for( unsigned ii = 0;  ii<m_Pcb->GetPadsCount();  ++ii )
    {
        pad = m_Pcb->m_NetInfo->GetPad( ii );
        pad->SetSubRatsnest( 0 );
    }

    if( m_Pcb->GetNodesCount() == 0 )
        return; /* pas de connexions utiles */

    /* Ratsnest computation */
    DisplayRastnestInProgress = TRUE;

    unsigned current_net_code = 1;    // 1er net_code a analyser (net_code = 0 -> no connect)
    noconn = 0;

    for( ; current_net_code < m_Pcb->m_NetInfo->GetNetsCount(); current_net_code++ )
    {
        NETINFO_ITEM* net = m_Pcb->FindNet( current_net_code );
        if( net == NULL )       //Should not occur
        {
            DisplayError( this, wxT( "Build_Board_Ratsnest() error: net not found" ) );
            return;
        }
        net->m_RatsnestStartIdx = m_Pcb->GetRatsnestsCount();

        // Search for the last subratsnest already in use
        int num_block = 0;
        for( unsigned ii = 0; ii < net->m_ListPad.size(); ii++ )
        {
            pad = net->m_ListPad[ii];
            if( num_block < pad->GetSubRatsnest() )
                num_block = pad->GetSubRatsnest();
        }

        /* Compute the ratsnest relative to the current net */

        /* a - first pass : create the blocks from not already in block pads */
        int icnt = gen_rats_pad_to_pad( m_Pcb->m_FullRatsnest,
                                        net->m_ListPad,
                                        0,
                                        net->m_ListPad.size(),
                                        num_block );

        /* b - blocks connection (Iteration) */
        while( icnt > 1 )
        {
            icnt = gen_rats_block_to_block( m_Pcb->m_FullRatsnest, net->m_ListPad, 0,
                                           net->m_ListPad.size() );
            net = m_Pcb->FindNet( current_net_code );
        }

        net->m_RatsnestEndIdx = m_Pcb->GetRatsnestsCount();

        /* sort by lenght */
        net = m_Pcb->FindNet( current_net_code );
        if( (net->m_RatsnestEndIdx - net->m_RatsnestStartIdx) > 1 )
        {
            RATSNEST_ITEM* rats = &m_Pcb->m_FullRatsnest[0];
            qsort( rats + net->m_RatsnestStartIdx,
                   net->m_RatsnestEndIdx - net->m_RatsnestStartIdx,
                   sizeof(RATSNEST_ITEM), sort_by_length );
        }
    }

    m_Pcb->m_NbNoconnect = noconn;
    m_Pcb->m_Status_Pcb |= LISTE_RATSNEST_ITEM_OK;

// erase the ratsnest displayed on screen if needed
    for( unsigned ii = 0; ii < m_Pcb->GetRatsnestsCount(); ii++ )
    {
        if( !g_Show_Ratsnest && DC )  // Clear VISIBLE flag
            m_Pcb->m_FullRatsnest[ii].m_Status &= ~CH_VISIBLE;

        if( DC )
            m_Pcb->m_FullRatsnest[ii].Draw( DrawPanel, DC, GR_XOR, wxPoint( 0, 0 ) );
    }
}


/*********************************************************************/
void WinEDA_BasePcbFrame::DrawGeneralRatsnest( wxDC* DC, int net_code )
/*********************************************************************/

/**
 *  Displays the general ratsnest
 *  Only ratsnets with the status bit CH_VISIBLE is set are displayed
 *  @param netcode if > 0, Display only the ratsnest relative to the correponding net_code
 */
{
    if( (m_Pcb->m_Status_Pcb & LISTE_RATSNEST_ITEM_OK) == 0 )
        return;
    if( (m_Pcb->m_Status_Pcb & DO_NOT_SHOW_GENERAL_RASTNEST) )
        return;
    if( DC == NULL )
        return;

    for( unsigned ii = 0; ii < m_Pcb->GetRatsnestsCount(); ii++ )
    {
        if( ( m_Pcb->m_FullRatsnest[ii].m_Status & (CH_VISIBLE | CH_ACTIF) ) !=
           (CH_VISIBLE | CH_ACTIF) )
            continue;

        if( (net_code <= 0) || ( net_code == m_Pcb->m_FullRatsnest[ii].GetNet() ) )
            m_Pcb->m_FullRatsnest[ii].Draw( DrawPanel, DC, GR_XOR, wxPoint( 0, 0 ) );
    }
}


/**********************************************************************************************/
static int tst_rats_block_to_block( NETINFO_ITEM* net, vector<RATSNEST_ITEM>& aRatsnestBuffer )
/**********************************************************************************************/

/**
 *  Function used by Tst_Ratsnest()
 *  Function like gen_rats_block_to_block(..)
 *  Function testing the ratsnest between 2 blocks ( same net )
 *  The search is made between pads in block 1 and the others blocks
 *  The block n ( n > 1 ) is merged with block 1 by the smallest ratsnest
 *  Difference between gen_rats_block_to_block(..):
 *  The analysis is not made pads to pads but uses the general ratsnest list.
 *  The function activate the smallest ratsnest between block 1 and the block n
 *  (activate a logical connexion)
 *
 *  @param  net = the current NETINFO_ITEM for the current net
 *      output:
 *          .state member of the ratsnests
 *  @return    blocks not connected count
 */
{
    int            current_num_block, min_block;
    RATSNEST_ITEM* rats, * min_rats;

    /* Search a link from a block to an other block */
    min_rats = NULL;
    for( unsigned ii = net->m_RatsnestStartIdx; ii < net->m_RatsnestEndIdx; ii++ )
    {
        rats = &aRatsnestBuffer[ii];
        if( rats->m_PadStart->GetSubRatsnest() == rats->m_PadEnd->GetSubRatsnest() )  // Same block
            continue;

        if( min_rats == NULL )
            min_rats = rats;
        else if( min_rats->m_Lenght > rats->m_Lenght )
            min_rats = rats;
    }

    if( min_rats == NULL )
        return 1;

    /* At this point we have found a link between 2 differents blocks (clusters) :
     * we must set its status to ACTIVE and merge the 2 blocks
     */
    min_rats->m_Status |= CH_ACTIF;
    current_num_block   = min_rats->m_PadStart->GetSubRatsnest();
    min_block = min_rats->m_PadEnd->GetSubRatsnest();

    if( min_block > current_num_block )
        EXCHG( min_block, current_num_block );

    /* Merging the 2 blocks in one cluster */
    for( unsigned ii = 0; ii < net->m_ListPad.size(); ii++ )
    {
        if( net->m_ListPad[ii]->GetSubRatsnest() == current_num_block )
        {
            net->m_ListPad[ii]->SetSubRatsnest( min_block );
        }
    }

    return current_num_block;
}


/*********************************************************************/
static int tst_rats_pad_to_pad( int current_num_block,
                                RATSNEST_ITEM* start_rat_list, RATSNEST_ITEM* end_rat_list )
/**********************************************************************/

/**
 *  Function used by Tst_Ratsnest_general()
 *  The general ratsnest list must exists
 *  Activates the ratsnest between 2 pads ( supposes du meme net )
 *  The function links 1 pad not already connected an other pad and activate
 *  some blocks linked by a ratsnest
 *  Its test only the existing ratsnest and activate some ratsnest (status bit CH_ACTIF set)
 *
 * @param   start_rat_list = starting address for the ratnest list
 * @param   end_rat_list   = ending address for the ratnest list
 * @param   current_num_block =  last block number (computed from the track analysis)
 *
 *      output:
 *          ratsnest list (status member set)
 *          and pad list (m_SubRatsnest set)
 *
 * @return new block number
 */
{
    D_PAD*         pad_start, * pad_end;
    RATSNEST_ITEM* chevelu;

    for( chevelu = start_rat_list; chevelu < end_rat_list; chevelu++ )
    {
        pad_start = chevelu->m_PadStart;
        pad_end   = chevelu->m_PadEnd;

        /* Update the block if the 2 pads are not connected : a new block is created
         */
        if( (pad_start->GetSubRatsnest() == 0) && (pad_end->GetSubRatsnest() == 0) )
        {
            current_num_block++;
            pad_start->SetSubRatsnest( current_num_block );
            pad_end->SetSubRatsnest( current_num_block );
            chevelu->m_Status |= CH_ACTIF;
        }
        /* If a pad is already connected : the other is merged in the current block */
        else if( pad_start->GetSubRatsnest() == 0 )
        {
            pad_start->SetSubRatsnest( pad_end->GetSubRatsnest() );
            chevelu->m_Status |= CH_ACTIF;
        }
        else if( pad_end->GetSubRatsnest() == 0 )
        {
            pad_end->SetSubRatsnest( pad_start->GetSubRatsnest() );
            chevelu->m_Status |= CH_ACTIF;
        }
    }

    return current_num_block;
}


/******************************************************************/
void WinEDA_BasePcbFrame::Tst_Ratsnest( wxDC* DC, int ref_netcode )
/*******************************************************************/

/* Compute the active ratsnest
 *  The general ratsnest list must exists
 *  Compute the ACTIVE ratsnests in the general ratsnest list
 * if ref_netcode == 0, test all nets, else test only ref_netcode
 */
{
    RATSNEST_ITEM* rats;
    D_PAD*         pad;
    NETINFO_ITEM*  net;

    if( m_Pcb->GetPadsCount() == 0 )
        return;
    if( (m_Pcb->m_Status_Pcb & LISTE_RATSNEST_ITEM_OK) == 0 )
        Build_Board_Ratsnest( DC );

    for( int net_code = 1; net_code < (int) m_Pcb->m_NetInfo->GetNetsCount(); net_code++ )
    {
        net = m_Pcb->FindNet( net_code );
        if( net == NULL )       //Should not occur
        {
            DisplayError( this, wxT( "Tst_Ratsnest() error: net not found" ) );
            return;
        }

        if( ref_netcode && (net_code != ref_netcode) )
            continue;

        int num_block = 0;
        for( unsigned ip = 0; ip < net->m_ListPad.size(); ip++ )
        {
            pad = net->m_ListPad[ip];
            int subnet = pad->GetSubNet();
            pad->SetSubRatsnest( subnet );
            num_block = MAX( num_block, subnet );
        }

        for( unsigned ii = net->m_RatsnestStartIdx; ii < net->m_RatsnestEndIdx; ii++ )
        {
            m_Pcb->m_FullRatsnest[ii].m_Status &= ~CH_ACTIF;
        }

        /* a - tst connection between pads */
        rats = &m_Pcb->m_FullRatsnest[0];
        int icnt = tst_rats_pad_to_pad( num_block,
                                        rats + net->m_RatsnestStartIdx,
                                        rats + net->m_RatsnestEndIdx );

        /* b - test connexion between blocks (Iteration) */
        while( icnt > 1 )
        {
            icnt = tst_rats_block_to_block( net, m_Pcb->m_FullRatsnest );
        }
    }

    m_Pcb->m_NbNoconnect = 0;
    for( unsigned ii = 0; ii < m_Pcb->GetRatsnestsCount(); ii++ )
    {
        if( m_Pcb->m_FullRatsnest[ii].m_Status & CH_ACTIF )
            m_Pcb->m_NbNoconnect++;
    }
}


/**************************************************************************/
int WinEDA_BasePcbFrame::Test_1_Net_Ratsnest( wxDC* DC, int ref_netcode )
/**************************************************************************/

/** function Test_1_Net_Ratsnest
 *  Compute the rastnest relative to the net "net_code"
 *  @param ref_netcode = netcode used to compute the rastnest.
 */
{
    DisplayRastnestInProgress = FALSE;
    DrawGeneralRatsnest( DC, ref_netcode );
    Tst_Ratsnest( DC, ref_netcode );
    DrawGeneralRatsnest( DC, ref_netcode );

    return m_Pcb->GetRatsnestsCount();
}


/*****************************************************************************/
void WinEDA_BasePcbFrame::build_ratsnest_module( wxDC* DC, MODULE* Module )
/*****************************************************************************/

/**
 *  Build a rastenest relative to one footprint. This is a simplified computation
 *  used only in move footprint. It is not optimal, but it is fast and sufficient
 *  to guide a footprint placement
 * It shows the connections from a pad to the nearest conected pad
 *  @param Module = module to consider.
 *
 *  The ratsnest has 2 sections:
 *      - An "internal" ratsnet relative to pads of this footprint which are in the same net.
 *          this ratsnest section is computed once.
 *      - An "external" rastnest connecting a pad of this footprint to an other pad (in an other footprint)
 *          The ratsnest section must be computed for each new position
 */
{
    static unsigned pads_module_count;              // node count (node = pad with a net code) for the footprint beeing moved
    static unsigned internalRatsCount;              // number of internal links (links between pads of the module)
    D_PAD**         baseListePad;
    D_PAD*          pad_ref;
    D_PAD*          pad_externe;
    int             current_net_code;
    int             distance;                       // variables de calcul de ratsnest
    wxPoint         pad_pos;                        // True pad position according to the current footprint position


    if( (GetBoard()->m_Status_Pcb & LISTE_PAD_OK) == 0 )
    {
        GetBoard()->m_Status_Pcb = 0;
        GetBoard()->m_NetInfo->BuildListOfNets();
    }

    /* Compute the "local" ratsnest if needed (when this footprint starts move)
     *  and the list of external pads to consider, i.e pads in others footprints which are "connected" to
     *  a pad in the current footprint
     */
    if( (m_Pcb->m_Status_Pcb & RATSNEST_ITEM_LOCAL_OK) != 0 )
        goto CalculateExternalRatsnest;

    /* Compute the "internal" ratsnest, i.e the links between the curent footprint pads */
    s_localPadBuffer.clear();
    m_Pcb->m_LocalRatsnest.clear();

    for( pad_ref = Module->m_Pads; pad_ref != NULL; pad_ref = pad_ref->Next() )
    {
        if( pad_ref->GetNet() == 0 )
            continue;

        s_localPadBuffer.push_back( pad_ref );
        pad_ref->SetSubRatsnest( 0 );
        pad_ref->SetSubNet( 0 );
    }

    pads_module_count = s_localPadBuffer.size();
    if( pads_module_count == 0 )
        return; /* no connection! */

    qsort( &s_localPadBuffer[0], pads_module_count, sizeof(D_PAD*), sortByNetcode );

    /* Build the list of pads linked to the current footprint pads */
    DisplayRastnestInProgress = FALSE;

    current_net_code = 0;
    for( unsigned ii = 0; ii < pads_module_count; ii++ )
    {
        pad_ref = s_localPadBuffer[ii];
        if( pad_ref->GetNet() == current_net_code )
            continue;

        // A new net was found, load all pads of others modules members of this net:
        NETINFO_ITEM* net = m_Pcb->FindNet( pad_ref->GetNet() );
        if( net == NULL )       //Should not occur
        {
            DisplayError( this, wxT( "build_ratsnest_module() error: net not found" ) );
            return;
        }

        for( unsigned jj = 0; jj < net->m_ListPad.size(); jj++ )
        {
            pad_externe = net->m_ListPad[jj];
            if( pad_externe->GetParent() == Module )
                continue;

            pad_externe->SetSubRatsnest( 0 );
            pad_externe->SetSubNet( 0 );

            s_localPadBuffer.push_back( pad_externe );
        }
    }

    /* Sort the pad list by net_code */
    baseListePad = &s_localPadBuffer[0];
    qsort( baseListePad + pads_module_count,
           s_localPadBuffer.size() - pads_module_count,
           sizeof(D_PAD*), sortByNetcode );

    /* Compute the internal rats nest:
     *  this is the same as general ratsnest, but considers only the current footprint pads
     * it is therefore not time consuming, and it is made only once
     */
    current_net_code = s_localPadBuffer[0]->GetNet();

    for( unsigned ii = 0; ii < pads_module_count; ii++ )
    {
        /* Search the end of pad list relative to the current net */
        unsigned jj = ii + 1;
        for( ; jj <= pads_module_count; jj++ )
        {
            if( jj >= pads_module_count )
                break;

            if( s_localPadBuffer[jj]->GetNet() != current_net_code )
                break;
        }

        /* End of list found: */
        /* a - first step of lee algorithm : build the pad to pad link list */
        int icnt = gen_rats_pad_to_pad( m_Pcb->m_LocalRatsnest, s_localPadBuffer, ii, jj, 0 );

        /* b - second step of lee algorithm : build the block to block link list (Iteration) */
        while( icnt > 1 )
        {
            icnt = gen_rats_block_to_block( m_Pcb->m_LocalRatsnest, s_localPadBuffer, ii, jj );
        }

        ii = jj;
        if( ii < s_localPadBuffer.size() )
            current_net_code = s_localPadBuffer[ii]->GetNet();
    }

    internalRatsCount = m_Pcb->m_LocalRatsnest.size();

    /* set the ratsnets status, flag LOCAL_RATSNEST_ITEM */
    for( unsigned ii = 0; ii < m_Pcb->m_LocalRatsnest.size(); ii++ )
    {
        m_Pcb->m_LocalRatsnest[ii].m_Status = LOCAL_RATSNEST_ITEM;
    }

    m_Pcb->m_Status_Pcb |= RATSNEST_ITEM_LOCAL_OK;

    /*
     *  This section computes the "external" ratsnest: must be done when the footprint position changes
     */
CalculateExternalRatsnest:

    /* This section search:
     *  for each current module pad the nearest neighbour external pad (of course for the same net code).
     *  For each current footprint cluster of pad (pads having the same net code),
     *  we search the smaller rats nest.
     *  so, for each net, only one rats nest item is created
     */
    RATSNEST_ITEM local_rats;
    local_rats.m_Lenght = 0x7FFFFFFF;
    local_rats.m_Status = 0;
    bool addRats = false;
    if( internalRatsCount < m_Pcb->m_LocalRatsnest.size() )
        m_Pcb->m_LocalRatsnest.erase( m_Pcb->m_LocalRatsnest.begin() + internalRatsCount,
                                     m_Pcb->m_LocalRatsnest.end() );

    current_net_code = s_localPadBuffer[0]->GetNet();
    for( unsigned ii = 0; ii < pads_module_count; ii++ )
    {
        pad_ref = s_localPadBuffer[ii];
        if( pad_ref->GetNet() != current_net_code )
        {
            /* if needed, creates a new ratsnest for the old net */
            if( addRats )
            {
                m_Pcb->m_LocalRatsnest.push_back( local_rats );
            }
            addRats = false;
            current_net_code    = pad_ref->GetNet();
            local_rats.m_Lenght = 0x7FFFFFFF;
        }

        pad_pos = pad_ref->m_Pos - g_Offset_Module;

        // Search the nearest external pad of this current pad
        for( unsigned jj = pads_module_count; jj < s_localPadBuffer.size(); jj++ )
        {
            pad_externe = s_localPadBuffer[jj];

            /* we search pads having the same net code */
            if( pad_externe->GetNet() < pad_ref->GetNet() )
                continue;

            if( pad_externe->GetNet() > pad_ref->GetNet() ) // remember pads are sorted by net code
                break;

            distance = abs( pad_externe->m_Pos.x - pad_pos.x ) +
                       abs( pad_externe->m_Pos.y - pad_pos.y );

            if( distance < local_rats.m_Lenght )
            {
                local_rats.m_PadStart = pad_ref;
                local_rats.m_PadEnd   = pad_externe;
                local_rats.SetNet( pad_ref->GetNet() );
                local_rats.m_Lenght = distance;
                local_rats.m_Status = 0;

                addRats = true;
            }
        }
    }

    if( addRats ) // Ensure the last created rats nest item is stored in buffer
        m_Pcb->m_LocalRatsnest.push_back( local_rats );
}


/***********************************************************/
void WinEDA_BasePcbFrame::trace_ratsnest_module( wxDC* DC )
/**********************************************************/

/*
 *  Display the rastnest of a moving footprint, computed by build_ratsnest_module()
 */
{
    if( DC == NULL )
        return;
    if( (m_Pcb->m_Status_Pcb & RATSNEST_ITEM_LOCAL_OK) == 0 )
        return;

    int tmpcolor = g_DesignSettings.m_RatsnestColor;
    for( unsigned ii = 0; ii < m_Pcb->m_LocalRatsnest.size(); ii++ )
    {
        RATSNEST_ITEM* rats = &m_Pcb->m_LocalRatsnest[ii];
        if( rats->m_Status & LOCAL_RATSNEST_ITEM )
        {
            g_DesignSettings.m_RatsnestColor = YELLOW;
            rats->Draw( DrawPanel, DC, GR_XOR, g_Offset_Module );
        }
        else
        {
            g_DesignSettings.m_RatsnestColor = tmpcolor;
            wxPoint tmp = rats->m_PadStart->m_Pos;
            rats->m_PadStart->m_Pos -= g_Offset_Module;
            rats->Draw( DrawPanel, DC, GR_XOR, wxPoint( 0, 0 ) );
            rats->m_PadStart->m_Pos = tmp;
        }
    }

    g_DesignSettings.m_RatsnestColor = tmpcolor;
}


/*
 *  construction de la liste en mode de calcul rapide pour affichage
 *  en temps reel du chevelu d'un pad lors des trac�s d'une piste d�marrant
 *  sur ce pad.
 *
 *  parametres d'appel:
 *      pad_ref ( si null : mise a 0 du nombre de chevelus )
 *      ox, oy = coord de l'extremite de la piste en trace
 *      init (flag)
 *          = 0 : mise a jour des chevelu
 *          <> 0:	creation de la liste
 */

/* Buffer to store pads coordinates when creating a track.
 *  these pads are members of the net
 *  and when the mouse is moved, the g_MaxLinksShowed links to neightbors are drawn
 */
static std::vector <wxPoint> s_RatsnestMouseToPads;
static wxPoint s_CursorPos; // Coordinate of the moving point (mouse cursor and end of current track segment)

/* Used by build_ratsnest_pad(): sort function by link lenght (manhattan distance)*/
static bool sort_by_localnetlength( const wxPoint& ref, const wxPoint& compare )
{
    wxPoint deltaref = ref - s_CursorPos;
    wxPoint deltacmp = compare - s_CursorPos;

    // = distance between ref coordinate and pad ref
    int     lengthref = abs( deltaref.x ) + abs( deltaref.y );

    // distance between ref coordinate and the other pad
    int     lengthcmp = abs( deltacmp.x ) + abs( deltacmp.y );

    return lengthref < lengthcmp;
}


/****************************************************************************************/
void WinEDA_BasePcbFrame::build_ratsnest_pad( BOARD_ITEM* ref,
                                              const wxPoint& refpos, bool init )
/****************************************************************************************/
{
    int    current_net_code = 0, conn_number = 0;
    D_PAD* pad_ref = NULL;

    if( ( (m_Pcb->m_Status_Pcb & LISTE_RATSNEST_ITEM_OK) == 0 )
       || ( (m_Pcb->m_Status_Pcb & LISTE_PAD_OK) == 0 )
       || ( (m_Pcb->m_Status_Pcb & NET_CODES_OK) == 0 ) )
    {
        s_RatsnestMouseToPads.clear();
        return;
    }


    s_CursorPos = refpos;
    if( init )
    {
        s_RatsnestMouseToPads.clear();
        if( ref == NULL )
            return;

        switch( ref->Type() )
        {
        case TYPE_PAD:
            pad_ref = (D_PAD*) ref;
            current_net_code = pad_ref->GetNet();
            conn_number = pad_ref->GetSubNet();
            break;

        case TYPE_TRACK:
        case TYPE_VIA:
        {
            TRACK* track_ref = (TRACK*) ref;
            current_net_code = track_ref->GetNet();
            conn_number = track_ref->GetSubNet();
            break;
        }

        default:
            ;
        }

        if( current_net_code <= 0 )
            return;

        NETINFO_ITEM* net = m_Pcb->FindNet( current_net_code );
        if( net == NULL )        //Should not occur
        {
            DisplayError( this, wxT( "build_ratsnest_pad() error: net not found" ) );
            return;
        }

        // Create a list of pads candidates ( pads not already connected to the current track:
        for( unsigned ii = 0; ii < net->m_ListPad.size(); ii++ )
        {
            D_PAD* pad = net->m_ListPad[ii];
            if( pad == pad_ref )
                continue;

            if( !pad->GetSubNet() || (pad->GetSubNet() != conn_number) )
                s_RatsnestMouseToPads.push_back( pad->m_Pos );
        }
    }   /* end if Init */

    if( s_RatsnestMouseToPads.size() > 1 )
        sort( s_RatsnestMouseToPads.begin(), s_RatsnestMouseToPads.end(), sort_by_localnetlength );
}


/*******************************************************/
void WinEDA_BasePcbFrame::trace_ratsnest_pad( wxDC* DC )
/*******************************************************/

/*
 *  Displays a "ratsnest" during track creation
 */
{
    if( DC == NULL )
        return;

    if( s_RatsnestMouseToPads.size() == 0 )
        return;


    GRSetDrawMode( DC, GR_XOR );
    for( int ii = 0; ii < (int) s_RatsnestMouseToPads.size(); ii++ )
    {
        if( ii >= g_MaxLinksShowed )
            break;

        GRLine( &DrawPanel->m_ClipBox, DC, s_CursorPos, s_RatsnestMouseToPads[ii], 0, YELLOW );
    }
}
