Keeps track of free and allocated data by maintaining a free and allocated list.
Appropriately finds space on the imaginary heap, and returns an error if there is no space to be allocated
all free and allocated blocks are kept track by a starting pointer and ending pointer, and has a header and footer