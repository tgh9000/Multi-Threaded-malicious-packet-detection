#include "list.h"

typedef struct Queue { //  change the nodes so that they contain the stuff required for threads because if not that is kinda stinky   
    Node* front;
    Node* rear;
    int size;
} Queue;