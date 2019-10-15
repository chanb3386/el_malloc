// el_malloc.c: implementation of explicit list malloc functions.

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "el_malloc.h"

////////////////////////////////////////////////////////////////////////////////
// Global control functions

// Global control variable for the allocator. Must be initialized in
// el_init().
el_ctl_t el_ctl = {};

// Create an initial block of memory for the heap using
// malloc(). Initialize the el_ctl data structure to point at this
// block. Initialize the lists in el_ctl to contain a single large
// block of available memory and no used blocks of memory.
int el_init(int max_bytes){
  void *heap = malloc(max_bytes);
  if(heap == NULL){
    fprintf(stderr,"el_init: malloc() failed in setup\n");
    exit(1);
  }

  el_ctl.heap_bytes = max_bytes; // make the heap as big as possible to begin with
  el_ctl.heap_start = heap;      // set addresses of start and end of heap
  el_ctl.heap_end   = PTR_PLUS_BYTES(heap,max_bytes);

  if(el_ctl.heap_bytes < EL_BLOCK_OVERHEAD){
    fprintf(stderr,"el_init: heap size %ld to small for a block overhead %ld\n",
            el_ctl.heap_bytes,EL_BLOCK_OVERHEAD);
    return 1;
  }

  el_init_blocklist(&el_ctl.avail_actual);
  el_init_blocklist(&el_ctl.used_actual);
  el_ctl.avail = &el_ctl.avail_actual;
  el_ctl.used  = &el_ctl.used_actual;

  // establish the first available block by filling in size in
  // block/foot and null links in head
  size_t size = el_ctl.heap_bytes - EL_BLOCK_OVERHEAD;
  el_blockhead_t *ablock = el_ctl.heap_start;
  ablock->size = size;
  ablock->state = EL_AVAILABLE;
  el_blockfoot_t *afoot = el_get_footer(ablock);
  afoot->size = size;
  el_add_block_front(el_ctl.avail, ablock);
  return 0;
}

// Clean up the heap area associated with the system which simply
// calls free() on the malloc'd block used as the heap.
void el_cleanup(){
  free(el_ctl.heap_start);
  el_ctl.heap_start = NULL;
  el_ctl.heap_end   = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Pointer arithmetic functions to access adjacent headers/footers

// Compute the address of the foot for the given head which is at a
// higher address than the head.
el_blockfoot_t *el_get_footer(el_blockhead_t *head){
  size_t size = head->size;
  el_blockfoot_t *foot = PTR_PLUS_BYTES(head, sizeof(el_blockhead_t) + size);
  return foot;
}

// Compute the address of the head for the given foot which is at a
// lower address than the foot.
el_blockhead_t *el_get_header(el_blockfoot_t *foot){
  // size stores the size of the block between the header and foot
  size_t size = foot->size;

  // Subtracts the size of the footer, the size of the block, and the size of
  // the head to point to the front of the header
  el_blockhead_t *header = PTR_MINUS_BYTES(foot, size + sizeof(el_blockhead_t));
  return header;

}

// Return a pointer to the block that is one block higher in memory
// from the given block.  This should be the size of the block plus
// the EL_BLOCK_OVERHEAD which is the space occupied by the header and
// footer. Returns NULL if the block above would be off the heap.
// DOES NOT follow next pointer, looks in adjacent memory.
el_blockhead_t *el_block_above(el_blockhead_t *block){
  el_blockhead_t *higher =
    PTR_PLUS_BYTES(block, block->size + EL_BLOCK_OVERHEAD);
  if((void *) higher >= (void*) el_ctl.heap_end){
    return NULL;
  }
  else{
    return higher;
  }
}

// Return a pointer to the block that is one block lower in memory
// from the given block.  Uses the size of the preceding block found
// in its foot. DOES NOT follow next pointer, looks in adjacent
// memory. Returns NULL if the block below would be outside the heap.
el_blockhead_t *el_block_below(el_blockhead_t *block){
  el_blockfoot_t *prev_foot = PTR_MINUS_BYTES(block, sizeof(el_blockfoot_t));
  // first checks if the block lays outside the heap
  if((void *) prev_foot < (void*) el_ctl.heap_start) {
    return NULL;
  } else {
    // if the block is in the heap then get the head and return
    el_blockhead_t *lower = el_get_header(prev_foot);
    return lower;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Block list operations

// Print an entire blocklist. The format appears as follows.
//
// blocklist{length:      5  bytes:    566}
//   [  0] head @    618 {state: u  size:    200}  foot @    850 {size:    200}
//   [  1] head @    256 {state: u  size:     32}  foot @    320 {size:     32}
//   [  2] head @    514 {state: u  size:     64}  foot @    610 {size:     64}
//   [  3] head @    452 {state: u  size:     22}  foot @    506 {size:     22}
//   [  4] head @    168 {state: u  size:     48}  foot @    248 {size:     48}
//   index        offset        a/u                       offset
//
// Note that the '@ offset' column is given from the starting heap
// address (el_ctl->heap_start) so it should be run-independent.
void el_print_blocklist(el_blocklist_t *list){
  printf("blocklist{length: %6lu  bytes: %6lu}\n", list->length,list->bytes);
  el_blockhead_t *block = list->beg;
  for(int i=0; i<list->length; i++){
    printf("  ");
    block = block->next;
    printf("[%3d] head @ %6lu ", i,PTR_MINUS_PTR(block,el_ctl.heap_start));
    printf("{state: %c  size: %6lu}", block->state,block->size);
    el_blockfoot_t *foot = el_get_footer(block);
    printf("  foot @ %6lu ", PTR_MINUS_PTR(foot,el_ctl.heap_start));
    printf("{size: %6lu}", foot->size);
    printf("\n");
  }
}

// Print out basic heap statistics. This shows total heap info along
// with the Available and Used Lists. The output format resembles the following.
//
// HEAP STATS
// Heap bytes: 1024
// AVAILABLE LIST: blocklist{length:      3  bytes:    458}
//   [  0] head @    858 {state: a  size:    126}  foot @   1016 {size:    126}
//   [  1] head @    328 {state: a  size:     84}  foot @    444 {size:     84}
//   [  2] head @      0 {state: a  size:    128}  foot @    160 {size:    128}
// USED LIST: blocklist{length:      5  bytes:    566}
//   [  0] head @    618 {state: u  size:    200}  foot @    850 {size:    200}
//   [  1] head @    256 {state: u  size:     32}  foot @    320 {size:     32}
//   [  2] head @    514 {state: u  size:     64}  foot @    610 {size:     64}
//   [  3] head @    452 {state: u  size:     22}  foot @    506 {size:     22}
//   [  4] head @    168 {state: u  size:     48}  foot @    248 {size:     48}
void el_print_stats(){
  printf("HEAP STATS\n");
  printf("Heap bytes: %lu\n",el_ctl.heap_bytes);
  printf("AVAILABLE LIST: ");
  el_print_blocklist(el_ctl.avail);
  printf("USED LIST: ");
  el_print_blocklist(el_ctl.used);
}

// Initialize the specified list to be empty. Sets the beg/end
// pointers to the actual space and initializes those data to be the
// ends of the list.  Initializes length and size to 0.
void el_init_blocklist(el_blocklist_t *list){
  list->beg        = &(list->beg_actual);
  list->beg->state = EL_BEGIN_BLOCK;
  list->beg->size  = EL_UNINITIALIZED;
  list->end        = &(list->end_actual);
  list->end->state = EL_END_BLOCK;
  list->end->size  = EL_UNINITIALIZED;
  list->beg->next  = list->end;
  list->beg->prev  = NULL;
  list->end->next  = NULL;
  list->end->prev  = list->beg;
  list->length     = 0;
  list->bytes      = 0;
}

// Add to the front of list; links for block are adjusted as are links
// within list.  Length is incremented and the bytes for the list are
// updated to include the new block's size and its overhead.
void el_add_block_front(el_blocklist_t *list, el_blockhead_t *block){
  // Inserts a new blockhead at the front of the blocklist
  block->prev = list->beg;
  block->next = list->beg->next;
  block->prev->next = block;
  block->next->prev = block;

  // Updating size parameters of list
  list->length += 1;
  list->bytes += (EL_BLOCK_OVERHEAD + block->size);
  return;
}

// Unlink block from the list it is in which should be the list
// parameter.  Updates the length and bytes for that list including
// the EL_BLOCK_OVERHEAD bytes associated with header/footer.
void el_remove_block(el_blocklist_t *list, el_blockhead_t *block){
  // Unlinking the block
  block->prev->next = block->next;
  block->next->prev = block->prev;

  // Updating list parameters
  list->length -= 1;
  list->bytes -= (EL_BLOCK_OVERHEAD + block->size);
  return;
}

////////////////////////////////////////////////////////////////////////////////
// Allocation-related functions

// Find the first block in the available list with block size of at
// least (size+EL_BLOCK_OVERHEAD). Overhead is accounted so this
// routine may be used to find an available block to split: splitting
// requires adding in a new header/footer. Returns a pointer to the
// found block or NULL if no of sufficient size is available.
el_blockhead_t *el_find_first_avail(size_t size){
  // Pointer to available list
  el_blocklist_t *avail = el_ctl.avail;
  size_t total_avail = avail->length;
  el_blockhead_t *start = avail->beg->next;

  // Searching the list for appropriate size
  for(int i = 0; i<total_avail; i++) {
    if(start->size >= (size+EL_BLOCK_OVERHEAD)) {
      return start;
    } else {
      start = start->next;
    }
  }
  return NULL;
}

// Set the pointed to block to the given size and add a footer to
// it. Creates another block above it by creating a new header and
// assigning it the remaining space. Ensures that the new block has a
// footer with the correct size. Returns a pointer to the newly
// created block while the parameter block has its size altered to
// parameter size. Does not do any linking of blocks.  If the
// parameter block does not have sufficient size for a split (at least
// new_size + EL_BLOCK_OVERHEAD for the new header/footer) makes no
// changes and returns NULL.
el_blockhead_t *el_split_block(el_blockhead_t *block, size_t new_size){
  if(block->size < new_size + EL_BLOCK_OVERHEAD || block == NULL) {
    return NULL;
  } else {
    // size is the total size to be split
    size_t size = block->size;

    // finding the footer of the original block
    el_blockfoot_t *split_foot = el_get_footer(block);

    // new_foot is now the foot of the first half
    // new_head is the header of the second half
    el_blockhead_t *new_head = PTR_PLUS_BYTES(block, new_size+sizeof(el_blockfoot_t)+sizeof(el_blockhead_t));
    el_blockfoot_t *new_foot = PTR_PLUS_BYTES(block, new_size+sizeof(el_blockhead_t));

    // Updating sizes of the first half
    block->size = new_size;
    new_foot->size = new_size;

    // Updating sizes of the second half
    new_head->size = size - new_size - EL_BLOCK_OVERHEAD;
    split_foot->size = size - new_size - EL_BLOCK_OVERHEAD;
    return new_head;

    // Doesn't update state or availability list - done in el_malloc
  }
}

// Return pointer to a block of memory with at least the given size
// for use by the user.  The pointer returned is to the usable space,
// not the block header. Makes use of find_first_avail() to find a
// suitable block and el_split_block() to split it.  Returns NULL if
// no space is available.
void *el_malloc(size_t nbytes){
  // Finding a pointer to the first available block of at least nbytes size
  el_blockhead_t *first = el_find_first_avail(nbytes);
  if(first == NULL) {
    return NULL;
  }

  // Splitting the block
  // First is now of size nbytes, and second is now the remainder half
  el_remove_block(el_ctl.avail, first);
  el_blockhead_t *second = el_split_block(first, nbytes);

  // linking
  el_add_block_front(el_ctl.used,first);
  first->state = EL_USED;
  el_add_block_front(el_ctl.avail,second);
  second->state = EL_AVAILABLE;

  // Updating pointer to point to memory, not header
  first = PTR_PLUS_BYTES(first, sizeof(el_blockhead_t));
  return first;
}

////////////////////////////////////////////////////////////////////////////////
// De-allocation/free() related functions

// Attempt to merge the block lower with the next block in
// memory. Does nothing if lower is null or not EL_AVAILABLE and does
// nothing if the next higher block is null (because lower is the last
// block) or not EL_AVAILABLE.  Otherwise, locates the next block with
// el_block_above() and merges these two into a single block. Adjusts
// the fields of lower to incorporate the size of higher block and the
// reclaimed overhead. Adjusts footer of higher to indicate the two
// blocks are merged.  Removes both lower and higher from the
// available list and re-adds lower to the front of the available
// list.
void el_merge_block_with_above(el_blockhead_t *lower){
  el_blockhead_t *above = el_block_above(lower);
  if(lower == NULL || lower->state != EL_AVAILABLE || above == NULL || above->state != EL_AVAILABLE) {
    return;
  } else {
    // calculating size
    size_t size_lower = lower->size;
    size_t size_above = above->size;
    size_t total = size_lower + size_above;

    // assigning the footer of the second above block
    el_blockfoot_t *above_foot = el_get_footer(above);

    // removes both from available
    el_remove_block(el_ctl.avail, above);
    el_remove_block(el_ctl.avail, lower);

    // Updating the size
    lower->size = total + EL_BLOCK_OVERHEAD;
    above_foot->size = total + EL_BLOCK_OVERHEAD;

    // Adds the new block with updated size to the front of the avail list.
    el_add_block_front(el_ctl.avail, lower);
    return;
  }
}

// Free the block pointed to by the give ptr.  The area immediately
// preceding the pointer should contain an el_blockhead_t with information
// on the block size. Attempts to merge the free'd block with adjacent
// blocks using el_merge_block_with_above().
void el_free(void *ptr){
  el_blockhead_t *header_to_free = PTR_MINUS_BYTES(ptr, sizeof(el_blockhead_t));
  if(header_to_free->state == EL_AVAILABLE) {
    return;
  }
  el_blockhead_t *before = el_block_below(header_to_free);

  // Removes block from the used list, set state to avail, add to avail list
  el_remove_block(el_ctl.used, header_to_free);
  header_to_free->state = EL_AVAILABLE;
  el_add_block_front(el_ctl.avail,header_to_free);

  // Attempts to merge with above and below block.
  el_merge_block_with_above(header_to_free);
  // Checks if before is out of bounds - can't merge if so
  if (before != NULL) {
    el_merge_block_with_above(before);
  }
  return;
}
