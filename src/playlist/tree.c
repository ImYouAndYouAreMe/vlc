/*****************************************************************************
 * tree.c : Playlist tree walking functions
 *****************************************************************************
 * Copyright (C) 1999-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include <vlc/vlc.h>
#include <assert.h>
#include "vlc_playlist.h"
#include "playlist_internal.h"

/************************************************************************
 * Local prototypes
 ************************************************************************/
playlist_item_t *GetNextUncle( playlist_t *p_playlist, playlist_item_t *p_item,
                               playlist_item_t *p_root );
playlist_item_t *GetPrevUncle( playlist_t *p_playlist, playlist_item_t *p_item,
                               playlist_item_t *p_root );

playlist_item_t *GetNextItem( playlist_t *p_playlist,
                              playlist_item_t *p_root,
                              playlist_item_t *p_item );
playlist_item_t *GetPrevItem( playlist_t *p_playlist,
                              playlist_item_t *p_item,
                              playlist_item_t *p_root );

/**
 * Create a playlist node
 *
 * \param p_playlist the playlist
 * \paam psz_name the name of the node
 * \param p_parent the parent node to attach to or NULL if no attach
 * \param p_flags miscellaneous flags
 * \param p_input the input_item to attach to or NULL if it has to be created
 * \return the new node
 */
playlist_item_t * playlist_NodeCreate( playlist_t *p_playlist,
                                       const char *psz_name,
                                       playlist_item_t *p_parent, int i_flags,
                                       input_item_t *p_input )
{
    input_item_t *p_new_input;
    playlist_item_t *p_item;

    if( !psz_name ) psz_name = _("Undefined");

    if( !p_input )
        p_new_input = input_ItemNewWithType( VLC_OBJECT(p_playlist), NULL,
                                        psz_name, 0, NULL, -1, ITEM_TYPE_NODE );
    p_item = playlist_ItemNewFromInput( VLC_OBJECT(p_playlist),
                                        p_input ? p_input : p_new_input );

    if( p_item == NULL )  return NULL;
    p_item->i_children = 0;

    ARRAY_APPEND(p_playlist->all_items, p_item);

    if( p_parent != NULL )
        playlist_NodeAppend( p_playlist, p_item, p_parent );
    playlist_SendAddNotify( p_playlist, p_item->i_id,
                            p_parent ? p_parent->i_id : -1,
                            !( i_flags & PLAYLIST_NO_REBUILD ));
    return p_item;
}

/**
 * Remove all the children of a node
 *
 * This function must be entered with the playlist lock
 *
 * \param p_playlist the playlist
 * \param p_root the node
 * \param b_delete_items do we have to delete the children items ?
 * \return VLC_SUCCESS or an error
 */
int playlist_NodeEmpty( playlist_t *p_playlist, playlist_item_t *p_root,
                        vlc_bool_t b_delete_items )
{
    int i;
    if( p_root->i_children == -1 )
    {
        return VLC_EGENERIC;
    }

    /* Delete the children */
    for( i =  p_root->i_children-1 ; i >= 0 ;i-- )
    {
        if( p_root->pp_children[i]->i_children > -1 )
        {
            playlist_NodeDelete( p_playlist, p_root->pp_children[i],
                                 b_delete_items , VLC_FALSE );
        }
        else if( b_delete_items )
        {
            /* Delete the item here */
            playlist_DeleteFromItemId( p_playlist,
                                       p_root->pp_children[i]->i_id );
        }
    }
    return VLC_SUCCESS;
}

/**
 * Remove all the children of a node and removes the node
 *
 * \param p_playlist the playlist
 * \param p_root the node
 * \param b_delete_items do we have to delete the children items ?
 * \return VLC_SUCCESS or an error
 */
int playlist_NodeDelete( playlist_t *p_playlist, playlist_item_t *p_root,
                         vlc_bool_t b_delete_items, vlc_bool_t b_force )
{
    int i;

    if( p_root->i_children == -1 )
    {
        return VLC_EGENERIC;
    }

    /* Delete the children */
    for( i =  p_root->i_children - 1 ; i >= 0; i-- )
    {
        if( p_root->pp_children[i]->i_children > -1 )
        {
            playlist_NodeDelete( p_playlist, p_root->pp_children[i],
                                 b_delete_items , b_force );
        }
        else if( b_delete_items )
        {
            playlist_DeleteFromItemId( p_playlist,
                                       p_root->pp_children[i]->i_id );
        }
    }
    /* Delete the node */
    if( p_root->i_flags & PLAYLIST_RO_FLAG && !b_force )
    {
    }
    else
    {
        int i;
        var_SetInteger( p_playlist, "item-deleted", p_root->i_id );
        ARRAY_BSEARCH( p_playlist->all_items, ->i_id, int,
                       p_root->i_id, i );
        if( i != -1 )
            ARRAY_REMOVE( p_playlist->all_items, i );

        /* Remove the item from its parent */
        if( p_root->p_parent )
            playlist_NodeRemoveItem( p_playlist, p_root, p_root->p_parent );

        playlist_ItemDelete( p_root );
    }
    return VLC_SUCCESS;
}


/**
 * Adds an item to the children of a node
 *
 * \param p_playlist the playlist
 * \param p_item the item to append
 * \param p_parent the parent node
 * \return VLC_SUCCESS or an error
 */
int playlist_NodeAppend( playlist_t *p_playlist,
                         playlist_item_t *p_item,
                         playlist_item_t *p_parent )
{
    return playlist_NodeInsert( p_playlist, p_item, p_parent, -1 );
}

int playlist_NodeInsert( playlist_t *p_playlist,
                         playlist_item_t *p_item,
                         playlist_item_t *p_parent,
                         int i_position )
{
   (void)p_playlist;
   assert( p_parent && p_parent->i_children != -1 );
   if( i_position == -1 ) i_position = p_parent->i_children ;

   INSERT_ELEM( p_parent->pp_children,
                p_parent->i_children,
                i_position,
                p_item );
   p_item->p_parent = p_parent;
   return VLC_SUCCESS;
}

/**
 * Deletes an item from the children of a node
 *
 * \param p_playlist the playlist
 * \param p_item the item to remove
 * \param p_parent the parent node
 * \return VLC_SUCCESS or an error
 */
int playlist_NodeRemoveItem( playlist_t *p_playlist,
                        playlist_item_t *p_item,
                        playlist_item_t *p_parent )
{
   (void)p_playlist;

   for(int i= 0; i< p_parent->i_children ; i++ )
   {
       if( p_parent->pp_children[i] == p_item )
       {
           REMOVE_ELEM( p_parent->pp_children, p_parent->i_children, i );
       }
   }

   return VLC_SUCCESS;
}


/**
 * Count the children of a node
 *
 * \param p_playlist the playlist
 * \param p_node the node
 * \return the number of children
 */
int playlist_NodeChildrenCount( playlist_t *p_playlist, playlist_item_t*p_node)
{
    int i;
    int i_nb = 0;

    if( p_node->i_children == -1 )
        return 0;

    i_nb = p_node->i_children;
    for( i=0 ; i< p_node->i_children;i++ )
    {
        if( p_node->pp_children[i]->i_children == -1 )
            break;
        else
            i_nb += playlist_NodeChildrenCount( p_playlist,
                                                p_node->pp_children[i] );
    }
    return i_nb;
}

/**
 * Search a child of a node by its name
 *
 * \param p_node the node
 * \param psz_search the name of the child to search
 * \return the child item or NULL if not found or error
 */
playlist_item_t *playlist_ChildSearchName( playlist_item_t *p_node,
                                           const char *psz_search )
{
    int i;

    if( p_node->i_children < 0 )
    {
         return NULL;
    }
    for( i = 0 ; i< p_node->i_children; i++ )
    {
        if( !strcmp( p_node->pp_children[i]->p_input->psz_name, psz_search ) )
        {
            return p_node->pp_children[i];
        }
    }
    return NULL;
}

/**
 * Create a pair of nodes in the category and onelevel trees.
 * They share the same input item.
 * \param p_playlist the playlist
 * \param psz_name the name of the nodes
 * \param pp_node_cat pointer to return the node in category tree
 * \param pp_node_one pointer to return the node in onelevel tree
 * \param b_for_sd For Services Discovery ? (make node read-only and unskipping)
 */
void playlist_NodesPairCreate( playlist_t *p_playlist, const char *psz_name,
                               playlist_item_t **pp_node_cat,
                               playlist_item_t **pp_node_one,
                               vlc_bool_t b_for_sd )
{
    *pp_node_cat = playlist_NodeCreate( p_playlist, psz_name,
                                        p_playlist->p_root_category, 0, NULL );
    *pp_node_one = playlist_NodeCreate( p_playlist, psz_name,
                                        p_playlist->p_root_onelevel, 0,
                                        (*pp_node_cat)->p_input );
    if( b_for_sd )
    {
        (*pp_node_cat)->i_flags |= PLAYLIST_RO_FLAG;
        (*pp_node_cat)->i_flags |= PLAYLIST_SKIP_FLAG;
        (*pp_node_one)->i_flags |= PLAYLIST_RO_FLAG;
        (*pp_node_one)->i_flags |= PLAYLIST_SKIP_FLAG;
    }
}

/**
 * Get the node in the preferred tree from a node in one of category
 * or onelevel tree.
 * For example, for the SAP node, it will return the node in the category
 * tree if --playlist-tree is not set to never, because the SAP node prefers
 * category
 */
playlist_item_t * playlist_GetPreferredNode( playlist_t *p_playlist,
                                             playlist_item_t *p_node )
{
    int i;
    if( p_node->p_parent == p_playlist->p_root_category )
    {
        if( p_playlist->b_always_tree ||
            p_node->p_input->b_prefers_tree ) return p_node;
        for( i = 0 ; i< p_playlist->p_root_onelevel->i_children; i++ )
        {
            if( p_playlist->p_root_onelevel->pp_children[i]->p_input->i_id ==
                    p_node->p_input->i_id )
                return p_playlist->p_root_onelevel->pp_children[i];
        }
    }
    else if( p_node->p_parent == p_playlist->p_root_onelevel )
    {
        if( p_playlist->b_never_tree || !p_node->p_input->b_prefers_tree )
            return p_node;
        for( i = 0 ; i< p_playlist->p_root_category->i_children; i++ )
        {
            if( p_playlist->p_root_category->pp_children[i]->p_input->i_id ==
                    p_node->p_input->i_id )
                return p_playlist->p_root_category->pp_children[i];
        }
    }
    return NULL;
}

/**********************************************************************
 * Tree walking functions
 **********************************************************************/

playlist_item_t *playlist_GetLastLeaf(playlist_t *p_playlist,
                                      playlist_item_t *p_root )
{
    int i;
    playlist_item_t *p_item;
    for ( i = p_root->i_children - 1; i >= 0; i-- )
    {
        if( p_root->pp_children[i]->i_children == -1 )
            return p_root->pp_children[i];
        else if( p_root->pp_children[i]->i_children > 0)
        {
             p_item = playlist_GetLastLeaf( p_playlist,
                                            p_root->pp_children[i] );
            if ( p_item != NULL )
                return p_item;
        }
        else if( i == 0 )
            return NULL;
    }
    return NULL;
}

int playlist_GetAllEnabledChildren( playlist_t *p_playlist,
                                    playlist_item_t *p_node,
                                    playlist_item_t ***ppp_items )
{
    int i_count = 0;
    playlist_item_t *p_next = NULL;
    while( 1 )
    {
        p_next = playlist_GetNextLeaf( p_playlist, p_node,
                                       p_next, VLC_TRUE, VLC_FALSE );
        if( p_next )
            INSERT_ELEM( *ppp_items, i_count, i_count, p_next );
        else
            break;
    }
    return i_count;
}

/**
 * Finds the next item to play
 *
 * \param p_playlist the playlist
 * \param p_root the root node
 * \param p_item the previous item  (NULL if none )
 * \return the next item to play, or NULL if none found
 */
playlist_item_t *playlist_GetNextLeaf( playlist_t *p_playlist,
                                       playlist_item_t *p_root,
                                       playlist_item_t *p_item,
                                       vlc_bool_t b_ena, vlc_bool_t b_unplayed )
{
    playlist_item_t *p_next;

    assert( p_root && p_root->i_children != -1 );

    PL_DEBUG2( "finding next of %s within %s",
               PLI_NAME( p_item ), PLI_NAME( p_root ) );

    /* Now, walk the tree until we find a suitable next item */
    p_next = p_item;
    while( 1 )
    {
        vlc_bool_t b_ena_ok = VLC_TRUE, b_unplayed_ok = VLC_TRUE;
        p_next = GetNextItem( p_playlist, p_root, p_next );
        if( !p_next || p_next == p_root )
            break;
        if( p_next->i_children == -1 )
        {
            if( b_ena && p_next->i_flags & PLAYLIST_DBL_FLAG )
                b_ena_ok = VLC_FALSE;
            if( b_unplayed && p_next->p_input->i_nb_played != 0 )
                b_unplayed_ok = VLC_FALSE;
            if( b_ena_ok && b_unplayed_ok ) break;
        }
    }
    if( p_next == NULL ) PL_DEBUG2( "at end of node" );
    return p_next;
}

/**
 * Finds the previous item to play
 *
 * \param p_playlist the playlist
 * \param p_root the root node
 * \param p_item the previous item  (NULL if none )
 * \return the next item to play, or NULL if none found
 */
playlist_item_t *playlist_GetPrevLeaf( playlist_t *p_playlist,
                                       playlist_item_t *p_root,
                                       playlist_item_t *p_item,
                                       vlc_bool_t b_ena, vlc_bool_t b_unplayed )
{
    playlist_item_t *p_prev;

    PL_DEBUG2( "finding previous os %s within %s", PLI_NAME( p_item ),
                                                   PLI_NAME( p_root ) );
    assert( p_root && p_root->i_children != -1 );

    /* Now, walk the tree until we find a suitable previous item */
    p_prev = p_item;
    while( 1 )
    {
        vlc_bool_t b_ena_ok = VLC_TRUE, b_unplayed_ok = VLC_TRUE;
        p_prev = GetPrevItem( p_playlist, p_root, p_prev );
        if( !p_prev || p_prev == p_root )
            break;
        if( p_prev->i_children == -1 )
        {
            if( b_ena && p_prev->i_flags & PLAYLIST_DBL_FLAG )
                b_ena_ok = VLC_FALSE;
            if( b_unplayed && p_prev->p_input->i_nb_played != 0 )
                b_unplayed_ok = VLC_FALSE;
            if( b_ena_ok && b_unplayed_ok ) break;
        }
    }
    if( p_prev == NULL ) PL_DEBUG2( "at beginning of node" );
    return p_prev;
}

/************************************************************************
 * Following functions are local
 ***********************************************************************/

/**
 * Get the next item in the tree
 * If p_item is NULL, return the first child of root
 **/
playlist_item_t *GetNextItem( playlist_t *p_playlist,
                              playlist_item_t *p_root,
                              playlist_item_t *p_item )
{
    playlist_item_t *p_parent;
    int i;

    /* Node with children, get the first one */
    if( p_item && p_item->i_children > 0 )
        return p_item->pp_children[0];

    if( p_item != NULL )
        p_parent = p_item->p_parent;
    else
        p_parent = p_root;
    for( i= 0 ; i < p_parent->i_children ; i++ )
    {
        if( p_item == NULL || p_parent->pp_children[i] == p_item )
        {
            if( p_item == NULL )
                i = -1;

            if( i+1 >= p_parent->i_children )
            {
                /* Was already the last sibling. Look for uncles */
                PL_DEBUG2( "Current item is the last of the node,"
                           "looking for uncle from %s",
                            p_parent->p_input->psz_name );

                if( p_parent == p_root )
                {
                    PL_DEBUG2( "already at root" );
                    return NULL;
                }
                return GetNextUncle( p_playlist, p_item, p_root );
            }
            else
            {
                return  p_parent->pp_children[i+1];
            }
        }
    }
    return NULL;
}

playlist_item_t *GetNextUncle( playlist_t *p_playlist, playlist_item_t *p_item,
                               playlist_item_t *p_root )
{
    playlist_item_t *p_parent = p_item->p_parent;
    playlist_item_t *p_grandparent;
    vlc_bool_t b_found = VLC_FALSE;

    (void)p_playlist;

    if( p_parent != NULL )
    {
        p_grandparent = p_parent->p_parent;
        while( p_grandparent )
        {
            int i;
            for( i = 0 ; i< p_grandparent->i_children ; i++ )
            {
                if( p_parent == p_grandparent->pp_children[i] )
                {
                    PL_DEBUG2( "parent %s found as child %i of grandparent %s",
                               p_parent->p_input->psz_name, i,
                               p_grandparent->p_input->psz_name );
                    b_found = VLC_TRUE;
                    break;
                }
            }
            if( b_found && i + 1 < p_grandparent->i_children )
            {
                    return p_grandparent->pp_children[i+1];
            }
            /* Not found at root */
            if( p_grandparent == p_root )
            {
                return NULL;
            }
            else
            {
                p_parent = p_grandparent;
                p_grandparent = p_parent->p_parent;
            }
        }
    }
    /* We reached root */
    return NULL;
}

playlist_item_t *GetPrevUncle( playlist_t *p_playlist, playlist_item_t *p_item,
                               playlist_item_t *p_root )
{
    playlist_item_t *p_parent = p_item->p_parent;
    playlist_item_t *p_grandparent;
    vlc_bool_t b_found = VLC_FALSE;

    (void)p_playlist;

    if( p_parent != NULL )
    {
        p_grandparent = p_parent->p_parent;
        while( 1 )
        {
            int i;
            for( i = p_grandparent->i_children -1 ; i >= 0; i-- )
            {
                if( p_parent == p_grandparent->pp_children[i] )
                {
                    b_found = VLC_TRUE;
                    break;
                }
            }
            if( b_found && i - 1 > 0 )
            {
                return p_grandparent->pp_children[i-1];
            }
            /* Not found at root */
            if( p_grandparent == p_root )
            {
                return NULL;
            }
            else
            {
                p_parent = p_grandparent;
                p_grandparent = p_parent->p_parent;
            }
        }
    }
    /* We reached root */
    return NULL;
}


/* Recursively search the tree for previous item */
playlist_item_t *GetPrevItem( playlist_t *p_playlist,
                              playlist_item_t *p_root,
                              playlist_item_t *p_item )
{
    playlist_item_t *p_parent;
    int i;

    /* Node with children, get the last one */
    if( p_item && p_item->i_children > 0 )
        return p_item->pp_children[p_item->i_children - 1];

    /* Last child of its parent ? */
    if( p_item != NULL )
        p_parent = p_item->p_parent;
    else
    {
        msg_Err( p_playlist, "Get the last one" );
        abort();
    };

    for( i = p_parent->i_children -1 ; i >= 0 ;  i-- )
    {
        if( p_parent->pp_children[i] == p_item )
        {
            if( i-1 < 0 )
            {
               /* Was already the first sibling. Look for uncles */
                PL_DEBUG2( "current item is the first of its node,"
                           "looking for uncle from %s",
                           p_parent->p_input->psz_name );
                if( p_parent == p_root )
                {
                    PL_DEBUG2( "already at root" );
                    return NULL;
                }
                return GetPrevUncle( p_playlist, p_item, p_root );
            }
            else
            {
                return p_parent->pp_children[i-1];
            }
        }
    }
    return NULL;
}

/* Dump the contents of a node */
void playlist_NodeDump( playlist_t *p_playlist, playlist_item_t *p_item,
                        int i_level )
{
    char str[512];
    int i;

    if( i_level == 1 )
    {
        msg_Dbg( p_playlist, "%s (%i)",
                        p_item->p_input->psz_name, p_item->i_children );
    }

    if( p_item->i_children == -1 )
    {
        return;
    }

    for( i = 0; i< p_item->i_children; i++ )
    {
        memset( str, 32, 512 );
        sprintf( str + 2 * i_level , "%s (%i)",
                                p_item->pp_children[i]->p_input->psz_name,
                                p_item->pp_children[i]->i_children );
        msg_Dbg( p_playlist, "%s",str );
        if( p_item->pp_children[i]->i_children >= 0 )
        {
            playlist_NodeDump( p_playlist, p_item->pp_children[i],
                              i_level + 1 );
        }
    }
    return;
}
