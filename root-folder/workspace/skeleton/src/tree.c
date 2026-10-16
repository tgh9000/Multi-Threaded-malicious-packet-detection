#include "globals.h"
#include "tree.h"
#include <stdlib.h>
#include <stdio.h>


// this is the tree required for storing the IP addresses for the SYN attacks


// function that compares two ip addresses
int compare_ip(struct in_addr a, struct in_addr b) 
{
    if (a.s_addr < b.s_addr) return -1;
    if (a.s_addr > b.s_addr) return  1;
    return 0;
}

/*
int contains_ip(TreeNode* root, struct in_addr addr) 
{
    if (root == NULL) return 0;

    int cmp = compare_ip(addr, root->ip);
    if (cmp == 0) return 1;
    if (cmp < 0) return contains_ip(root->left, addr);
    return contains_ip(root->right, addr);
}
*/

// inserts a new IP address to our binary tree-set and if an element is added then it will increase the size of the tree within the global variables. 
TreeNode* insert_unique(TreeNode* root, struct in_addr addr) {
    if (root == NULL) 
    {
        TreeNode* node = malloc(sizeof(TreeNode));
        node->ip = addr;
        node->left = node->right = NULL;
        treeSize = treeSize + 1;
        // increase the size of the tree
        return node;

    }

    // checks the relation ship between current node IP and the one to add . if they are equal then the ip is already in the tree and therefore does not need to be added so nothing is done
    int cmp = compare_ip(addr, root->ip);

    if (cmp < 0) 
    {
        root->left = insert_unique(root->left, addr);
    }
    
    else if (cmp > 0) 
    {
        root->right = insert_unique(root->right, addr);
    }
    // If cmp == 0: value already exists → DO NOTHING so recursively return the root and do not change the size of the tree.
    // Unique constraint maintained!

    return root;
}


