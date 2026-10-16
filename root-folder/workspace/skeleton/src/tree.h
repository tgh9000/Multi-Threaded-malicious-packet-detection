
#ifndef TREE_H
#define TREE_H

#include <netinet/in.h> 
typedef struct TreeNode {
    struct in_addr ip;
    struct TreeNode* left;
    struct TreeNode* right;
} TreeNode;


TreeNode* insert_unique(TreeNode* root, struct in_addr addr);
#endif