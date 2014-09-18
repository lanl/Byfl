/*
 * Helper library for computing bytes:flops ratios
 * (binning reuse distance)
 *
 * By Scott Pakin <pakin@lanl.gov>
 *    Rob Aulwes <rta@lanl.gov>
 */

#include "byfl.h"

namespace bytesflops {}
using namespace bytesflops;
using namespace std;


namespace bytesflops {

typedef CachedUnorderedMap<uint64_t, uint64_t> addr_to_time_t;
addr_to_time_t last_access;   // Last access time of a given address

// An RDnode is one node in a reuse-distance tree.
class RDnode {
private:
  RDnode* left;         // Left child
  RDnode* right;        // Right child

  // Fix the node's weight (subtree size).
  void fix_node_weight();

  // Fix the weight of all nodes along the path to a given time.
  void fix_path_weights(uint64_t time);

  // Splay a value to the top of the tree, returning the new tree.
  RDnode* splay(uint64_t target);

public:
  uint64_t address;     // Address from trace
  uint64_t time;        // Time of the address's last access
  uint64_t weight;      // Number of items in this subtree (self included)

  // Initialize a new RDnode with a given address and timestamp
  // (defaulting to dummy values).
  RDnode();
  RDnode(uint64_t address, uint64_t time);

  // Reinitialize an existing RDnode with a given address and timestamp.
  void initialize(uint64_t address, uint64_t time);

  // Insert a node into the tree and return the new tree.
  RDnode* insert(RDnode* new_node);

  // Remove a timestamp from the tree and return the new tree and the
  // node that was deleted.
  RDnode* remove(uint64_t timestamp, RDnode** removed_node);

  // Remove all timestamps less than a given value from the tree and
  // from a given histogram, and return the new tree.
  RDnode* prune_tree(uint64_t timestamp, addr_to_time_t* histogram);

  // Return the number of nodes in a splay tree whose timestamp is
  // larger than a given value.
  uint64_t tree_dist(uint64_t timestamp);

  // Ensure that all nodes have a valid weight.
  void validate_weights();
};

// fix_node_weight() sets the weight of a given node to the sum of its
// immediate children's weight plus one.
void RDnode::fix_node_weight()
{
  uint64_t new_weight = 1;
  if (left != NULL)
    new_weight += left->weight;
  if (right != NULL)
    new_weight += right->weight;
  weight = new_weight;
}


// fix_path_weights() fixes node weights along the path to a given time.
void RDnode::fix_path_weights(uint64_t target)
{
  // Do an ordinary binary tree search for target -- which we expect
  // not to find -- but change child pointers to parent pointers as we
  // go (instead of requiring extra memory to maintain our path back
  // to the root).
  RDnode* parent = NULL;
  RDnode* node = this;
  while (node != NULL) {
    RDnode* child;
    if (target < node->time) {
      child = node->left;
      node->left = parent;
    }
    else {
      child = node->right;
      node->right = parent;
    }
    parent = node;
    node = child;
  }

  // Walk back up the tree, fixing weights and child pointers as we go.
  while (parent != NULL) {
    RDnode* prev_node = node;
    node = parent;
    if (target < node->time) {
      // We borrowed our left child's pointer.
      parent = node->left;
      node->left = prev_node;
    }
    else {
      // We borrowed our right child's pointer.
      parent = node->right;
      node->right = prev_node;
    }
    node->fix_node_weight();
  }
}


// splay() splays a value (or a nearby value if the value doesn't
// appear in the tree) to the top of the tree, returning the new tree.
RDnode* RDnode::splay(uint64_t target)
{
  RDnode* node = this;
  RDnode new_node;
  new_node.left = NULL;
  new_node.right = NULL;
  RDnode* left = &new_node;
  RDnode* right = &new_node;

  while (true) {
    if (target < node->time) {
      if (node->left == NULL)
        break;
      if (target < node->left->time) {
        // Rotate right
        RDnode* parent = node->left;
        node->left = parent->right;
        parent->right = node;
        node = parent;

        // Fix weights.
        node->right->fix_node_weight();
        node->fix_node_weight();
        if (node->left == NULL)
          break;
      }

      // Link right
      right->left = node;
      right = node;
      node = node->left;
    }
    else
      if (target > node->time) {
        if (node->right == NULL)
          break;
        if (target > node->right->time) {
          // Rotate left
          RDnode* parent = node->right;
          node->right = parent->left;
          parent->left = node;
          node = parent;

          // Fix weights.
          node->left->fix_node_weight();
          node->fix_node_weight();
          if (node->right == NULL)
            break;
        }

        // Link left
        left->right = node;
        left = node;
        node = node->right;
      }
      else
        break;
  }

  // Assemble the final tree.
  left->right = node->left;
  right->left = node->right;
  node->left = new_node.right;
  node->right = new_node.left;

  // Fix weights up to the node from its previous position.
  node->left->fix_path_weights(node->time);
  node->right->fix_path_weights(node->time);
  return node;
}

// insert() inserts a new node into a splay tree and returns the new
// tree.  Duplicates insertions produce undefined behavior.
// Insertions into NULL trees produce undefined behavior.  (The caller
// should check for the first-insertion and allocate memory
// accordingly.)
RDnode* RDnode::insert(RDnode* new_node)
{
  // Handle some simple cases.
  RDnode* node = this;
  node = node->splay(new_node->time);
  if (new_node->time == node->time)
    // The timestamp is already in the tree.  This should never happen
    // when the tree is used for reuse-distance calculations.
    abort();

  // Handle the normal cases.
  if (new_node->time > node->time) {
    new_node->right = node->right;
    new_node->left = node;
    node->right = NULL;
  }
  else {
    new_node->left = node->left;
    new_node->right = node;
    node->left = NULL;
  }
  node->fix_node_weight();
  new_node->fix_node_weight();
  return new_node;
}


// remove() deletes a timestamp from the tree and returns the new tree
// and the deleted node.  Missing timestamps produce undefined
// behavior.
RDnode* RDnode::remove(uint64_t target, RDnode** removed_node)
{
  RDnode* node = this;
  node = node->splay(target);
  if (node->time != target)
    // Not found
    abort();
  RDnode* new_root;
  if (node->left == NULL)
    // Smallest value in the tree
    new_root = node->right;
  else {
    // Any other value
    new_root = node->left->splay(target);
    if (new_root != NULL) {
      new_root->right = node->right;
      if (new_root->right != NULL)
        new_root->right->fix_node_weight();
      new_root->fix_node_weight();
    }
  }
  *removed_node = node;
  return new_root;
}


// Remove all timestamps less than a given value from the tree and
// from a given histogram, and return the new tree and new set of
// symbols.
RDnode* RDnode::prune_tree(uint64_t timestamp, addr_to_time_t* histogram)
{
  RDnode* new_tree = splay(0);
  while (new_tree && new_tree->time < timestamp) {
    RDnode* dead_node = new_tree;
    new_tree = new_tree->right;
    if (new_tree->left)
      new_tree = new_tree->splay(0);
    histogram->erase(dead_node->address);
    delete dead_node;
  }
  return new_tree;
}


// tree_dist() returns the number of nodes in a splay tree whose
// timestamp is larger than a given value.
uint64_t RDnode::tree_dist(uint64_t timestamp)
{
  RDnode* node = this;
  uint64_t num_larger = 0;
  while (true) {
    if (timestamp > node->time) {
      node = node->right;
    }
    else
      if (timestamp < node->time) {
        num_larger++;
        if (node->right != NULL)
          num_larger += node->right->weight;
        node = node->left;
      }
      else {
        if (node->right != NULL)
          num_larger += node->right->weight;
        return num_larger;
      }
  }
}


// For debugging purposes, ensure that every node of a tree contains
// correct weights.
void RDnode::validate_weights()
{
  uint64_t true_weight = 1;
  if (left != NULL) {
    left->validate_weights();
    true_weight += left->weight;
  }
  if (right != NULL) {
    right->validate_weights();
    true_weight += right->weight;
  }
  if (weight != true_weight) {
    cerr << "*** Internal error: Node " << this << " has weight "
         << weight << " but expected weight " << true_weight << " ***\n";
    abort();
  }
}

// Reinitialize an existing RDnode with a given address and timestamp.
void RDnode::initialize(uint64_t new_address, uint64_t new_time)
{
  address = new_address;
  time = new_time;
  weight = 1;
  left = NULL;
  right = NULL;
}


// Initialize a new RDnode with a dummy address and timestamp.
RDnode::RDnode()
{
  initialize(0, 0);
}


// Initialize a new RDnode with a given address and timestamp.
RDnode::RDnode(uint64_t address, uint64_t time)
{
  initialize(address, time);
}


// Define infinite distance.
const uint64_t infinite_distance = ~(uint64_t)0;


// A ReuseDistance encapsulates all the state needed for a
// reuse-distance calculation.
class ReuseDistance {
private:
  uint64_t clock;           // Current time
  vector<uint64_t> hist;    // Histogram of the number of times each reuse distance was observed
  uint64_t unique_entries;  // Number of unique addresses (infinite reuse distance)
  RDnode* dist_tree;            // Tree of reuse distances

public:
  // Initialize our various fields.
  ReuseDistance() {
    clock = 0;
    unique_entries = 0;
    dist_tree = NULL;
  }

  // Incorporate a new address into the reuse-distance histogram.
  void process_address(uint64_t address);

  // Return a pointer to the reuse-distance histogram.
  vector<uint64_t>* get_histogram() { return &hist; }

  // Return the number of unique addresses.
  uint64_t get_unique_addrs() { return unique_entries; }

  // Compute the median reuse distance.
  void compute_median(uint64_t* median_value, uint64_t* mad_value);
};


// Incorporate a new address into the reuse-distance histogram.
void ReuseDistance::process_address(uint64_t address)
{
  // Update the histogram.
  uint64_t distance = infinite_distance;
  addr_to_time_t::iterator prev_time_iter = last_access.find(address);
  RDnode* new_node = NULL;
  if (prev_time_iter != last_access.end()) {
    // We've previously seen this address.
    uint64_t prev_time = prev_time_iter->second;
    distance = dist_tree->tree_dist(prev_time);
    dist_tree = dist_tree->remove(prev_time, &new_node);
  }
  uint64_t hist_len = hist.size();
  if (distance < hist_len)
    // We've previously seen both this symbol and this reuse distance.
    hist[distance]++;
  else {
    if (distance == infinite_distance)
      // This is the first time we've seen this symbol.
      unique_entries++;
    else {
      // We've previously seen this symbol but not this reuse distance.
      hist.resize(distance+1, 0);
      hist[distance]++;
    }
  }

  // Update the tree and the map.
  if (new_node == NULL)
    new_node = new RDnode(address, clock);
  else
    new_node->initialize(address, clock);
  if (__builtin_expect(dist_tree == NULL, 0))
    // First insertion into the tree.
    dist_tree = new_node;
  else
    // All other tree insertions.
    dist_tree = dist_tree->insert(new_node);
  last_access[address] = clock;
  clock++;

  // If the tree and the map have grown too large, prune old addresses
  // from them.
  if (last_access.size() > bf_max_reuse_distance)
    dist_tree = dist_tree->prune_tree(clock - bf_max_reuse_distance, &last_access);
}


// Compute the median reuse distance and the median absolute
// deviation of that.
void ReuseDistance::compute_median(uint64_t* median_value, uint64_t* mad_value) {
  // Find the total tally.
  uint64_t hist_len = hist.size();   // Entries in the histogram
  uint64_t total_tally;              // Total number of accesses including one-time accesses
  total_tally = unique_entries - hist_len;
  for (size_t dist = 0; dist < hist_len; dist++)
    total_tally += hist[dist];

  // Find the distance that lies at half the total tally.
  uint64_t median_distance = infinite_distance;
  uint64_t median_tally = 0;
  for (size_t dist = 0; dist < hist_len; dist++) {
    median_distance = dist;
    median_tally += hist[dist];
    if (median_tally > total_tally/2)
      break;
  }

  // Tally the absolute deviations.
  vector<uint64_t> absdev(hist_len, 0);
  for (size_t dist = 0; dist < hist_len; dist++) {
    uint64_t tally = hist[dist];
    uint64_t deviation;
    if (dist > median_distance)
      deviation = dist - median_distance;
    else
      deviation = median_distance - dist;
    absdev[deviation] += tally;
  }

  // Find the deviation that lies at half the total tally.
  uint64_t mad = 0;
  uint64_t absdev_tally = 0;
  uint64_t absdev_len = absdev.size();
  for (size_t dev = 0; dev < absdev_len; dev++) {
    mad = dev;
    absdev_tally += absdev[dev];
    if (absdev_tally > total_tally/2)
      break;
  }

  // Return the results.
  *median_value = median_distance;
  *mad_value = mad;
}


// Keep track of the reuse distance of the program as a whole.
static ReuseDistance* global_reuse_dist = NULL;


// Initialize some of our variables at first use.
void initialize_reuse (void)
{
  global_reuse_dist = new ReuseDistance();
}


// Process the reuse distance of a set of addresses relative to the
// program as a whole.
extern "C"
void bf_reuse_dist_addrs_prog (uint64_t baseaddr, uint64_t numaddrs)
{
  for (uint64_t ofs = 0; ofs < numaddrs; ofs++)
    global_reuse_dist->process_address(baseaddr + ofs);
}


// Return the reuse distance histogram and count of unique bytes for
// the program as a whole.
void bf_get_reuse_distance (vector<uint64_t>** hist, uint64_t* unique_addrs)
{
  *hist = global_reuse_dist->get_histogram();
  *unique_addrs = global_reuse_dist->get_unique_addrs();
}


// Compute the median reuse distance for the program as a whole.
void bf_get_median_reuse_distance (uint64_t* median_value, uint64_t* mad_value)
{
  global_reuse_dist->compute_median(median_value, mad_value);
}

}
