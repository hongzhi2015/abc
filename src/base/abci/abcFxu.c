/**CFile****************************************************************

  FileName    [abcFxu.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Interface with the fast extract package.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: abcFxu.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "abc.h"
#include "fxu.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////
 
static bool Abc_NtkFxuCheck( Abc_Ntk_t * pNtk );
static void Abc_NtkFxuCollectInfo( Abc_Ntk_t * pNtk, Fxu_Data_t * p );
static void Abc_NtkFxuReconstruct( Abc_Ntk_t * pNtk, Fxu_Data_t * p );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////


/**Function*************************************************************

  Synopsis    [Performs fast_extract on the current network.]

  Description [Takes the network and the maximum number of nodes to extract.
  Uses the concurrent double-cube and single cube divisor extraction procedure.
  Modifies the network in the end, after extracting all nodes. Note that 
  Ntk_NetworkSweep() may increase the performance of this procedure because 
  the single-literal nodes will not be created in the sparse matrix. Returns 1 
  if the network has been changed.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
bool Abc_NtkFastExtract( Abc_Ntk_t * pNtk, Fxu_Data_t * p )
{
    assert( Abc_NtkIsLogic(pNtk) );
    // if the network is already in the SOP form, it may come from BLIF file
    // and it may not be SCC-ABC_FREE, in which case FXU will not work correctly
    if ( Abc_NtkIsSopLogic(pNtk) )
    { // to make sure the SOPs are SCC-ABC_FREE
//        Abc_NtkSopToBdd(pNtk);
//        Abc_NtkBddToSop(pNtk);
    }
    // get the network in the SOP form
    if ( !Abc_NtkToSop(pNtk, 0) )
    {
        printf( "Abc_NtkFastExtract(): Converting to SOPs has failed.\n" );
        return 0;
    }
    // check if the network meets the requirements
    if ( !Abc_NtkFxuCheck(pNtk) )
    {
        printf( "Abc_NtkFastExtract: Nodes have duplicated or complemented fanins. FXU is not performed.\n" );
        return 0;
    }
    // sweep removes useless nodes
    Abc_NtkCleanup( pNtk, 0 );
    // collect information about the covers
    Abc_NtkFxuCollectInfo( pNtk, p );
    // call the fast extract procedure
    if ( Fxu_FastExtract(p) > 0 )
    {
        // update the network
        Abc_NtkFxuReconstruct( pNtk, p );
        // make sure everything is okay
        if ( !Abc_NtkCheck( pNtk ) )
            printf( "Abc_NtkFastExtract: The network check has failed.\n" );
        return 1;
    }
    else
        printf( "Warning: The network has not been changed by \"fx\".\n" );
    return 0;
}


/**Function*************************************************************

  Synopsis    [Makes sure the nodes do not have complemented and duplicated fanins.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
bool Abc_NtkFxuCheck( Abc_Ntk_t * pNtk )
{
    Abc_Obj_t * pNode, * pFanin1, * pFanin2;
    int n, i, k;
    Abc_NtkForEachNode( pNtk, pNode, n )
    {
        Abc_ObjForEachFanin( pNode, pFanin1, i )
        {
            if ( i < 2 && Abc_ObjFaninC(pNode, i) )
                return 0;
            Abc_ObjForEachFanin( pNode, pFanin2, k )
            {
                if ( i == k )
                    continue;
                if ( pFanin1 == pFanin2 )
                    return 0;
            }
        }
    }
    return 1;
}

/**Function*************************************************************

  Synopsis    [Collect information about the network for fast_extract.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NtkFxuCollectInfo( Abc_Ntk_t * pNtk, Fxu_Data_t * p )
{
    Abc_Obj_t * pNode;
    int i;
    // add information to the manager
    p->pManSop    = pNtk->pManFunc;
    p->vSops      = Vec_PtrAlloc(0);
    p->vFanins    = Vec_PtrAlloc(0);
    p->vSopsNew   = Vec_PtrAlloc(0);
    p->vFaninsNew = Vec_PtrAlloc(0);
    Vec_PtrFill( p->vSops,      Abc_NtkObjNumMax(pNtk), NULL );
    Vec_PtrFill( p->vFanins,    Abc_NtkObjNumMax(pNtk), NULL );
    Vec_PtrFill( p->vSopsNew,   Abc_NtkObjNumMax(pNtk) + p->nNodesExt, NULL );
    Vec_PtrFill( p->vFaninsNew, Abc_NtkObjNumMax(pNtk) + p->nNodesExt, NULL );
    // add SOPs and fanin array
    Abc_NtkForEachNode( pNtk, pNode, i )
    {
        if ( Abc_SopGetVarNum(pNode->pData) < 2 )
            continue;
        if ( Abc_SopGetCubeNum(pNode->pData) < 1 )
            continue;
        p->vSops->pArray[i]   = pNode->pData;
        p->vFanins->pArray[i] = &pNode->vFanins;
    }
    p->nNodesOld = Abc_NtkObjNumMax(pNtk);
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NtkFxuFreeInfo( Fxu_Data_t * p )
{
    int i;
    // ABC_FREE the arrays of new fanins
    if ( p->vFaninsNew )
        for ( i = 0; i < p->vFaninsNew->nSize; i++ )
            if ( p->vFaninsNew->pArray[i] )
                Vec_IntFree( p->vFaninsNew->pArray[i] );
    // ABC_FREE the arrays
    if ( p->vSops      ) Vec_PtrFree( p->vSops      );
    if ( p->vSopsNew   ) Vec_PtrFree( p->vSopsNew   );
    if ( p->vFanins    ) Vec_PtrFree( p->vFanins    );
    if ( p->vFaninsNew ) Vec_PtrFree( p->vFaninsNew );
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    [Reconstructs the network after FX.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NtkFxuReconstruct( Abc_Ntk_t * pNtk, Fxu_Data_t * p )
{
    Vec_Int_t * vFanins;
    Abc_Obj_t * pNode, * pFanin;
    int i, k;

    assert( p->vFanins->nSize < p->vFaninsNew->nSize );
    // create the new nodes
    for ( i = p->vFanins->nSize; i < p->vFanins->nSize + p->nNodesNew; i++ )
    {
        // start the node
        pNode = Abc_NtkCreateNode( pNtk );
        assert( i == (int)pNode->Id );
    }
    // update the old nodes
    for ( i = 0; i < p->vFanins->nSize; i++ )
    {
        // the new array of fanins
        vFanins = p->vFaninsNew->pArray[i];
        if ( vFanins == NULL )
            continue;
        // remove old fanins
        pNode = Abc_NtkObj( pNtk, i );
        Abc_ObjRemoveFanins( pNode );
        // add new fanins
        vFanins = p->vFaninsNew->pArray[i];
        for ( k = 0; k < vFanins->nSize; k++ )
        {
            pFanin = Abc_NtkObj( pNtk, vFanins->pArray[k] );
            Abc_ObjAddFanin( pNode, pFanin );
        }
        pNode->pData = p->vSopsNew->pArray[i];
        assert( pNode->pData != NULL );
    }
    // set up the new nodes
    for ( i = p->vFanins->nSize; i < p->vFanins->nSize + p->nNodesNew; i++ )
    {
        // get the new node
        pNode = Abc_NtkObj( pNtk, i );
        // add the fanins
        vFanins = p->vFaninsNew->pArray[i];
        for ( k = 0; k < vFanins->nSize; k++ )
        {
            pFanin = Abc_NtkObj( pNtk, vFanins->pArray[k] );
            Abc_ObjAddFanin( pNode, pFanin );
        }
        pNode->pData = p->vSopsNew->pArray[i];
        assert( pNode->pData != NULL );
    }
}


////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


