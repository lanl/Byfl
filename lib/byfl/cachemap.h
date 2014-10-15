/*
 * Helper library for computing bytes:flops ratios
 * (cached unordered map class definition)
 *
 * By Scott Pakin <pakin@lanl.gov>
 *    Rob Aulwes <rta@lanl.gov>
 */

#ifndef _CACHEMAP_H_
#define _CACHEMAP_H_

#include "byfl.h"

using namespace std;

// Wrap unordered_map with a simple cache.  We expect to have many
// hits to the same key.
template<class Key,
         class T,
         class Hash = std::hash<Key>,
         class KeyEqual = std::equal_to<Key>,
         class Allocator = std::allocator< std::pair<const Key, T> > >
class CachedUnorderedMap {
private:
  typedef unordered_map<Key, T, Hash, KeyEqual, Allocator> umap_type;
  static const size_t cache_size = 1;  // Number of previous entries to cache
  Key prev_key[cache_size];            // Keys previously searched for
  typename umap_type::iterator prev_iter[cache_size];   // Iterators previously returned
  KeyEqual compare_keys;               // Functor for comparing two keys for equality
  umap_type* the_map;                  // The underlying unordered_map

public:
  // The constructor sets prev_key and prev_value to (hopefully) bogus values.
  CachedUnorderedMap() {
    the_map = new umap_type();
    for (size_t i=0; i<cache_size; i++)
      memset((void *)&prev_key[i], 0, sizeof(Key));
  }

  // All iterator types and methods get delegated to unordered_map.
  typedef typename umap_type::iterator iterator;
  typedef typename umap_type::const_iterator const_iterator;
  iterator begin() { return the_map->begin(); }
  const_iterator begin() const { return the_map->begin(); }
  iterator end() { return the_map->end(); }
  const_iterator end() const { return the_map->end(); }

  // Ditto for the size() method.
  size_t size() const { return the_map->size(); }

  iterator contains( const T & value )
  {
      for ( auto iter = this->begin(); iter != this->end(); iter++ )
      {
          if ( iter->second == value )
          {
              return iter;
          }
      }
      return this->end();
  }

  // The find() method first checks the cache then falls back to the
  // unordered_map.
  iterator find (const Key& key) {
    // Linear-search the cache.
    for (size_t i=0; i<cache_size; i++) {
      if (compare_keys(key, prev_key[i])) {
        // Hit -- bubble up.
        typename umap_type::iterator found_iter;
        if (i > 0) {
          Key temp_key = prev_key[i-1];
          typename umap_type::iterator temp_iter = prev_iter[i-1];
          prev_key[i-1] = prev_key[i];
          prev_iter[i-1] = prev_iter[i];
          prev_key[i] = temp_key;
          prev_iter[i] = temp_iter;
          found_iter = prev_iter[i-1];
        }
        else
          found_iter = prev_iter[i];
        return found_iter;
      }
    }

    // The entry wasn't found in the cache -- search the unordered map.
    iterator iter = the_map->find(key);
    if (iter != end()) {
      // Cache only successful searches.
      //memcpy((void *)&prev_key[cache_size-1], &key, sizeof(Key));
      prev_key[cache_size-1] = key;
      prev_iter[cache_size-1] = iter;
    }
    return iter;
  }

  // The erase() method erases the key:value pair from both the cache
  // and the unordered_map.
  size_t erase (const Key& key) {
    // Linear-search the cache.
    for (size_t i=0; i<cache_size; i++) {
      if (compare_keys(key, prev_key[i])) {
        // Hit -- bubble up.
        if (i > 0) {
          Key temp_key = prev_key[i-1];
          typename umap_type::iterator temp_iter = prev_iter[i-1];
          prev_key[i-1] = prev_key[i];
          prev_iter[i-1] = prev_iter[i];
          prev_key[i] = temp_key;
          prev_iter[i] = temp_iter;
        }
        break;
      }
    }

    // Remove the key:value pair from the unordered map.
    return the_map->erase(key);
  }

  // operator[] uses find() to find or create a key:value pair.
  T& operator[] (const Key& key) {
    iterator iter = find(key);
    if (iter == end()) {
      // Not found -- create a new value then try again.
      (void) (*the_map)[key];
      return (*this)[key];
    }
    else
      // Found -- find() has already cached the key:value pair so just
      // return the value.
      return iter->second;
  }

  // sorted_keys() returns a pointer to a vector of keys in sorted order.
  template <typename SortFn = std::less<Key> >
  vector<Key>* sorted_keys (SortFn compare = SortFn() ) {
    vector<Key>* keys = new vector<Key>();
    for (const_iterator iter = begin(); iter != end(); iter++)
      keys->push_back(iter->first);
    sort(keys->begin(), keys->end(), compare);
    return keys;
  }

};

#endif
