/** @file
  A red-black tree library interface.

  The data structure is useful when an associative container is needed. Worst
  case time complexity is O(log n) for search, insert and delete, where "n" is
  the number of elements in the tree.

  The data structure is also useful as a priority queue.

  Copyright (C) 2014, Red Hat, Inc.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License that accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef __RB_TREE_LIB__
#define __RB_TREE_LIB__

#include <Base.h>
#include <Uefi/UefiBaseType.h>

//
// Opaque structure for tree nodes.
//
// Tree nodes do not take ownership of the associated user structures, they
// only link them. This makes it easy to link the same user structure into
// several trees. If reference counting is required, the caller is responsible
// for implementing it, as part of the user structure.
//
// A pointer-to-RB_TREE_NODE is considered an "iterator". Multiple,
// simultaneous iterations are supported.
//
typedef struct RB_TREE_NODE RB_TREE_NODE;

//
// Altering the key field of an in-tree user structure (ie. the portion of the
// user structure that RB_TREE_USER_COMPARE and RB_TREE_KEY_COMPARE, below,
// read) is not allowed in-place. The caller is responsible for bracketing the
// key change with the deletion and the reinsertion of the user structure, so
// that the changed key value is reflected in the tree.
//

/**
  Comparator function type for two user structures.

  @param[in] UserStruct1  Pointer to the first user structure.

  @param[in] UserStruct2  Pointer to the second user structure.

  @retval <0  If UserStruct1 compares less than UserStruct2.

  @retval  0  If UserStruct1 compares equal to UserStruct2.

  @retval >0  If UserStruct1 compares greater than UserStruct2.
**/
typedef
INTN
(EFIAPI *RB_TREE_USER_COMPARE)(
  IN CONST VOID *UserStruct1,
  IN CONST VOID *UserStruct2
  );

/**
  Compare a standalone key against a user structure containing an embedded key.

  @param[in] StandaloneKey  Pointer to the bare key.

  @param[in] UserStruct     Pointer to the user structure with the embedded
                            key.

  @retval <0  If StandaloneKey compares less than UserStruct's key.

  @retval  0  If StandaloneKey compares equal to UserStruct's key.

  @retval >0  If StandaloneKey compares greater than UserStruct's key.
**/
typedef
INTN
(EFIAPI *RB_TREE_KEY_COMPARE)(
  IN CONST VOID *StandaloneKey,
  IN CONST VOID *UserStruct
  );


typedef struct {
  RB_TREE_NODE          *Root;
  RB_TREE_USER_COMPARE  UserStructCompare;
  RB_TREE_KEY_COMPARE   KeyCompare;
} RB_TREE;

//
// Some functions below are read-only, while others are read-write. If any
// write operation is expected to run concurrently with any other operation on
// the same tree, then the caller is responsible for implementing locking for
// the whole tree.
//

/**
  Retrieve the user structure linked by the specified tree node.

  Read-only operation.

  @param[in] Node  Pointer to the tree node whose associated user structure we
                   want to retrieve. The caller is responsible for passing a
                   non-NULL argument.

  @return  Pointer to user structure linked by Node.
**/
VOID *
EFIAPI
RbTreeUserStruct (
  IN CONST RB_TREE_NODE *Node
  );


/**
  Initialize the caller-allocated RB_TREE structure.

  Write-only operation.

  @param[out] Tree               The tree to initialize.

  @param[in]  UserStructCompare  This caller-provided function will be used to
                                 order two user structures linked into the
                                 tree, during the insertion procedure.

  @param[in]  KeyCompare         This caller-provided function will be used to
                                 order the standalone search key against user
                                 structures linked into the tree, during the
                                 lookup procedure.
**/
VOID
EFIAPI
RbTreeInit (
  OUT RB_TREE             *Tree,
  IN RB_TREE_USER_COMPARE UserStructCompare,
  IN RB_TREE_KEY_COMPARE  KeyCompare
  );


/**
  Check whether the tree is empty (has no nodes).

  Read-only operation.

  @param[in] Tree  The tree to check for emptiness.

  @retval TRUE   The tree is empty.

  @retval FALSE  The tree is not empty.
**/
BOOLEAN
EFIAPI
RbTreeIsEmpty (
  IN CONST RB_TREE *Tree
  );


/**
  Uninitialize an empty RB_TREE structure.

  Read-write operation.

  It is the caller's responsibility to delete all nodes from the tree before
  calling this function.

  @param[in] Tree  The empty tree to uninitialize. Note that Tree itself is not
                   released.
**/
VOID
EFIAPI
RbTreeUninit (
  IN RB_TREE *Tree
  );


/**
  Look up the tree node that links the user structure that matches the
  specified standalone key.

  Read-only operation.

  @param[in] Tree           The tree to search for StandaloneKey.

  @param[in] StandaloneKey  The key to locate among the user structures linked
                            into Tree. StandaloneKey will be passed to
                            Tree->KeyCompare().

  @retval NULL  StandaloneKey could not be found.

  @return       The tree node that links to the user structure matching
                StandaloneKey, otherwise.
**/
RB_TREE_NODE *
EFIAPI
RbTreeFind (
  IN CONST RB_TREE *Tree,
  IN CONST VOID    *StandaloneKey
  );


/**
  Find the tree node of the minimum user structure stored in the tree.

  Read-only operation.

  @param[in] Tree  The tree to return the minimum node of. The user structure
                   linked by the minimum node compares less than all other user
                   structures in the tree.

  @retval NULL  If Tree is empty.

  @return       The tree node that links the minimum user structure, otherwise.
**/
RB_TREE_NODE *
EFIAPI
RbTreeMin (
  IN CONST RB_TREE *Tree
  );


/**
  Find the tree node of the maximum user structure stored in the tree.

  Read-only operation.

  @param[in] Tree  The tree to return the maximum node of. The user structure
                   linked by the maximum node compares greater than all other
                   user structures in the tree.

  @retval NULL  If Tree is empty.

  @return       The tree node that links the maximum user structure, otherwise.
**/
RB_TREE_NODE *
EFIAPI
RbTreeMax (
  IN CONST RB_TREE *Tree
  );


/**
  Get the tree node of the least user structure that is greater than the one
  linked by Node.

  Read-only operation.

  @param[in] Node  The node to get the successor node of.

  @retval NULL  If Node is NULL, or Node is the maximum node of its containing
                tree (ie. Node has no successor node).

  @return       The tree node linking the least user structure that is greater
                than the one linked by Node, otherwise.
**/
RB_TREE_NODE *
EFIAPI
RbTreeNext (
  IN CONST RB_TREE_NODE *Node
  );


/**
  Get the tree node of the greatest user structure that is less than the one
  linked by Node.

  Read-only operation.

  @param[in] Node  The node to get the predecessor node of.

  @retval NULL  If Node is NULL, or Node is the minimum node of its containing
                tree (ie. Node has no predecessor node).

  @return       The tree node linking the greatest user structure that is less
                than the one linked by Node, otherwise.
**/
RB_TREE_NODE *
EFIAPI
RbTreePrev (
  IN CONST RB_TREE_NODE *Node
  );


/**
  Insert (link) a user structure into the tree.

  Read-write operation.

  This function allocates the new tree node with MemoryAllocationLib's
  AllocatePool() function.

  @param[in,out] Tree        The tree to insert UserStruct into.

  @param[out]    Node        The meaning of this optional, output-only
                             parameter depends on the return value of the
                             function.

                             When insertion is successful (EFI_SUCCESS), Node
                             is set on output to the new tree node that now
                             links UserStruct.

                             When insertion fails due to lack of memory
                             (EFI_OUT_OF_RESOURCES), Node is not changed.

                             When insertion fails due to key collision (ie.
                             another user structure is already in the tree that
                             compares equal to UserStruct), with return value
                             EFI_ALREADY_STARTED, then Node is set on output to
                             the node that links the colliding user structure.
                             This enables "find-or-insert" in one function
                             call, or helps with later removal of the colliding
                             element.

  @param[in]     UserStruct  The user structure to link into the tree.
                             UserStruct is ordered against in-tree user
                             structures with the Tree->UserStructCompare()
                             function.

  @retval EFI_SUCCESS           Insertion successful. A new tree node has been
                                allocated, linking UserStruct. The new tree
                                node is reported back in Node (if the caller
                                requested it).

                                Existing RB_TREE_NODE pointers into Tree remain
                                valid. For example, on-going iterations in the
                                caller can continue with RbTreeNext() /
                                RbTreePrev(), and they will return the new node
                                at some point if user structure order dictates
                                it.

  @retval EFI_OUT_OF_RESOURCES  AllocatePool() failed to allocate memory for
                                the new tree node. The tree has not been
                                changed. Existing RB_TREE_NODE pointers into
                                Tree remain valid.

  @retval EFI_ALREADY_STARTED   A user structure has been found in the tree
                                that compares equal to UserStruct. The node
                                linking the colliding user structure is
                                reported back in Node (if the caller requested
                                it). The tree has not been changed. Existing
                                RB_TREE_NODE pointers into Tree remain valid.
**/
EFI_STATUS
EFIAPI
RbTreeInsert (
  IN OUT RB_TREE      *Tree,
  OUT    RB_TREE_NODE **Node      OPTIONAL,
  IN     VOID         *UserStruct
  );


/**
  Delete a node from the tree, unlinking the associated user structure.

  Read-write operation.

  @param[in,out] Tree        The tree to delete Node from.

  @param[in]     Node        The tree node to delete from Tree. The caller is
                             responsible for ensuring that Node belongs to
                             Tree, and that Node is non-NULL and valid. Node is
                             typically an earlier return value, or output
                             parameter, of:

                             - RbTreeFind(), for deleting a node by user
                               structure key,

                             - RbTreeMin() / RbTreeMax(), for deleting the
                               minimum / maximum node,

                             - RbTreeNext() / RbTreePrev(), for deleting a node
                               found during an iteration,

                             - RbTreeInsert() with return value
                               EFI_ALREADY_STARTED, for deleting a node whose
                               linked user structure caused collision during
                               insertion.

                             Given a non-empty Tree, Tree->Root is also a valid
                             Node argument (typically used for simplicity in
                             loops that empty the tree completely).

                             Node is released with MemoryAllocationLib's
                             FreePool() function.

                             Existing RB_TREE_NODE pointers (ie. iterators)
                             *different* from Node remain valid. For example:

                             - RbTreeNext() / RbTreePrev() iterations in the
                               caller can be continued from Node, if
                               RbTreeNext() or RbTreePrev() is called on Node
                               *before* RbTreeDelete() is. That is, fetch the
                               successor / predecessor node first, then delete
                               Node.

                             - On-going iterations in the caller that would
                               have otherwise returned Node at some point, as
                               dictated by user structure order, will correctly
                               reflect the absence of Node after RbTreeDelete()
                               is called mid-iteration.

  @param[out]    UserStruct  If the caller provides this optional output-only
                             parameter, then on output it is set to the user
                             structure originally linked by Node (which is now
                             freed).

                             This is a convenience that may save the caller a
                             RbTreeUserStruct() invocation before calling
                             RbTreeDelete(), in order to retrieve the user
                             structure being unlinked.
**/
VOID
EFIAPI
RbTreeDelete (
  IN OUT RB_TREE      *Tree,
  IN     RB_TREE_NODE *Node,
  OUT    VOID         **UserStruct OPTIONAL
  );


/**
  A slow function that asserts that the tree is a valid red-black tree, and
  that it orders user structures correctly.

  Read-only operation.

  This function uses the stack for recursion and is not recommended for
  "production use".

  @param[in] Tree  The tree to validate.
**/
VOID
EFIAPI
RbTreeValidate (
  IN CONST RB_TREE *Tree
  );

#endif
