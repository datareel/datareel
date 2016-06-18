// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxbstree.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/07/1996
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ---------- Include File Description and Details  ---------- // 
// ----------------------------------------------------------- // 
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.
 
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  
USA

Generic binary search tree class. This tree implementation is
designed to work with minimum overhead by eliminating excessive
pointer manipulation and execution cycles associated with tree
rotations and balancing. Searching performance may degenerate
when large sort ordered lists are inserted into the tree.

Changes:
==============================================================
03/24/2002: Added the Release() synchronization function. The 
release function is used to signal to the class destructor not  
to delete the tree when an object is deleted or no longer 
visible.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_BSTREE_HPP__
#define __GX_BSTREE_HPP__

#include "gxdlcode.h"

#include "bstnode.h"

// Binary search tree node
template<class TYPE>
class gxBStreeNode : public gxBStreeNode_t
{
public:
  gxBStreeNode() { right = left = 0; }
  gxBStreeNode(TYPE &X) : data(X) { right = left = 0; }
  ~gxBStreeNode() { }

public: // Type-casting functions
  gxBStreeNode *GetLeft() const { return (gxBStreeNode<TYPE> *)left; }
  gxBStreeNode *GetLeft() { return (gxBStreeNode<TYPE> *)left; }
  gxBStreeNode *GetRight() const { return (gxBStreeNode<TYPE> *)right; }
  gxBStreeNode *GetRight() { return (gxBStreeNode<TYPE> *)right; }
  
public:
  TYPE data; // User defined node data type
};

// Binary search tree class
template<class TYPE>
class gxBStree : public gxBStreeNode<TYPE>
{
public:
  gxBStree() { root = 0; }
  ~gxBStree() { DeleteTree(root); }

private: // Disallow copying and assignment
  gxBStree(const gxBStree<TYPE> &ob) {  }
  void operator=(const gxBStree<TYPE> &ob) { }
  
public:
  int Insert(TYPE &X, int *exists = 0);
  int Delete(TYPE &X);
  int Find(TYPE &X);
  int FindFirst(TYPE &X);
  int FindLast(TYPE &X);
  int ExtractFirst(TYPE &X);
  int ExtractLast(TYPE &X);
  void Clear() { DeleteTree(root);  root = 0; }
  void ClearTree() { DeleteTree(root);  root = 0; }
  gxBStreeNode<TYPE> *GetRoot() { return root; }
  gxBStreeNode<TYPE> *GetRoot() const { return root; }
  int IsEmpty() { return root == 0; }
  int IsEmpty() const { return root == 0; }
  
public: // Synchronization functions
  void Release() {
    // The release function is used to release this object's tree  
    // without clearing the tree or performing any other action. 
    // NOTE: The release function is used to release this tree when  
    // more then one object is referencing this tree. After calling   
    // the release function the object previously bound to this tree 
    // can safely be deleted without affecting any other objects   
    // bound to this tree. This allows the tree data to remain in  
    // memory until all threads accessing this tree have exited or 
    // released it.
    root = 0;
  }

private: // Internal processing functions
  gxBStreeNode<TYPE> *DetachNode(TYPE &X);
  void DeleteTree(gxBStreeNode<TYPE> *tree);

private:
  gxBStreeNode<TYPE> *root; // Pointer to the top of the tree 
};

template<class TYPE>
int gxBStree<TYPE>::FindFirst(TYPE &X)
// Find the left most node in the tree. Returns false if the
// tree is empty.
{
  if(IsEmpty()) return 0;
  gxBStreeNode<TYPE> *node = root;
  while(node->GetLeft()) node = node->GetLeft();
  X = node->data;
  return 1;
}

template<class TYPE>
int gxBStree<TYPE>::FindLast(TYPE &X)
// Find the right most node in the tree. Returns false if the
// tree is empty.
{
  if(IsEmpty()) return 0;
  gxBStreeNode<TYPE> *node = root;
  while(node->GetRight()) node = node->GetRight();
  X = node->data;
  return 1;
}

template<class TYPE>
int gxBStree<TYPE>::ExtractFirst(TYPE &X)
// Queue operation used to extract the first node from the tree
// in sort order. Returns true if the node was extracted.
{
  if(!FindFirst(X)) return 0;
  if(!Delete(X)) return 0;
  return 1;
}

template<class TYPE>
int gxBStree<TYPE>::ExtractLast(TYPE &X)
// Queue operation used to extract the last node from the tree
// in sort order. Returns true if the node was extracted.
{
  if(!FindLast(X)) return 0;
  if(!Delete(X)) return 0;
  return 1;
}

template<class TYPE>
int gxBStree<TYPE>::Find(TYPE &X)
// Search the tree for the specified entry. Return true
// if the entry was found or false if the entry does not
// exist.
{
  gxBStreeNode<TYPE> *parent = 0;
  gxBStreeNode<TYPE> *node = root;
  
  while(node) {
    if(X == node->data) break; // Found the a matching node

    parent = node;
    if(X < node->data) { // Search the left sub-tree
      node = node->GetLeft();
    }
    else { // Serach the right sub-tree
      node = node->GetRight();
    }
  }
  return node != 0;
}

template<class TYPE>
int gxBStree<TYPE>::Insert(TYPE &X, int *exists)
// Unbalanced tree insertion routine used to insert a node into the
// tree. Returns false is an error occurs or the node already exists.
// If initialized by the application, the "exists" variable will be set
// to true if this entry already exists. 
{
  gxBStreeNode<TYPE> *parent = 0;
  gxBStreeNode<TYPE> *node = root;
  int insert_left = 0;
  if(exists) *exists = 0;
  
  // Find the insertion point be seaching for the parent node
  while(node) {
    if(X == node->data) { // This node already exists in the tree
      if(exists) *exists = 1;
      return 0;
    }
    parent = node;
    if(X < node->data) { // Insert the node in the left sub-tree
      node = node->GetLeft();
      insert_left = 1; 
    }
    else { // Insert the node in the right sub-tree
      node = node->GetRight();
      insert_left = 0; 
    }
  }

  if(node == 0) { // The node does not exist in the tree
    node = new gxBStreeNode<TYPE>(X); // Allocate a new node
    if(!node) return 0; // Memory allocation error
    if(parent) {
      if(insert_left) // Update the parent's left and right pointers
	parent->left = node;
      else
	parent->right = node;
    }
    else // This node has no parent so this is the new root
      root = node;
  }

  return node != 0;
}

template<class TYPE>
gxBStreeNode<TYPE> *gxBStree<TYPE>::DetachNode(TYPE &X)
// Internal processing function used to detach the node from
// its current position. Returns a pointer to the detached node
// or a null value if the node was not found.
{
  gxBStreeNode<TYPE> *parent = 0;
  gxBStreeNode<TYPE> *node = root;
  gxBStreeNode<TYPE> *parent_of_successor = 0;
  gxBStreeNode<TYPE> *replacement = 0;
  int insert_left = 0;

  // Search for the node 
  while(node) {
    if(X == node->data) break; // Found the a matching node

    parent = node;
    if(X < node->data) { // Search the left sub-tree
      node = node->GetLeft();
      insert_left = 1;
    }
    else { // Serach the right sub-tree
      node = node->GetRight();
      insert_left = 0;
    }
  }
  
  if(node) {
    // If the left or right child is null use non-null child as the
    // repalcement node.
    if(node->GetLeft() == 0 || node->GetRight() == 0) {
      if(node->GetLeft()) { // Use the left child as the replacement
	replacement = node->GetLeft();
      }
      else { // Use the right child as the replacement
	replacement = node->GetRight();
      }
    }
    else { // Neither child is null so find the parent of the successor node
      // Search the right sub-tree once and then go all the way left
      gxBStreeNode<TYPE> *p = node->GetRight();
      if(p) {
	parent_of_successor = node;
	while(p->GetLeft()) {
	  parent_of_successor = p;
	  p = p->GetLeft();
	}
      }
      
      if(parent_of_successor == node) { // Immediate successor
	replacement = parent_of_successor->GetRight();
      }
      else {
	// Detach the replacement node from its current loaction
	replacement = parent_of_successor->GetLeft();
	parent_of_successor->left = parent_of_successor->left->right;
        replacement->right = node->right;
      }

      replacement->left = node->left;
    }

    if(parent) { // Set the parent node pointers
      if(insert_left)
	parent->left = replacement;
      else
        parent->right = replacement;
    }
    else { // This node has no parent
      root = replacement;
    }
  }
  return node;
}

template<class TYPE>
int gxBStree<TYPE>::Delete(TYPE &X)
// Deletes the specified entry from the tree. 
// Returns false if the entry does not exist.
{
  gxBStreeNode<TYPE> *node = DetachNode(X);
  if(!node) return 0;

  // NOTE: The node data must be destroyed by the TYPE destructor
  // or the memory for the node data will not be released.
  delete node;
  return 1;
}

template<class TYPE>
void gxBStree<TYPE>::DeleteTree(gxBStreeNode<TYPE> *t)
// Recursive function used to clear the tree from the bottom up.
{
  if(t == 0) return;
  DeleteTree(t->GetLeft());
  DeleteTree(t->GetRight());

  // NOTE: The node data must be destroyed by the TYPE destructor
  // or the memory for the node data will not be released.
  delete t;
}

#endif // __GX_BSTREE_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
