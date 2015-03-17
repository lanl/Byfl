/*
 * Helper library for computing bytes:flops ratios
 * (cached-map class definitions)
 *
 * By Scott Pakin <pakin@lanl.gov>
 *    Rob Aulwes <rta@lanl.gov>
 */

#ifndef _CACHEMAP_H_
#define _CACHEMAP_H_

#include "byfl.h"

using namespace std;

// Wrap an STL map or unordered_map with a simple cache.  We expect to have
// many hits to the same key.
template<typename map_type,
         class Key,
         class T,
         class KeyEqual = std::equal_to<Key> >
class CachedAnyMap {
private:
  static const size_t cache_size = 1;  // Number of previous entries to cache
  struct CacheElt {
    Key key;                           // Key previously searched for
    typename map_type::iterator iter;  // Iterator previously returned
  };
  CacheElt* cache[cache_size];         // The cache proper
  KeyEqual compare_keys;               // Functor for comparing two keys for equality
  map_type* the_map;                   // The underlying map
  size_t null_entries = cache_size;    // Number of null entries in the cache

public:
  // The constructor initializes all cache entries to null (no entry).
  CachedAnyMap() {
    the_map = new map_type();
    for (size_t i = 0; i < cache_size; i++)
      cache[i] = nullptr;
  }

  // All iterator types and methods get delegated to the underlying map.
  typedef typename map_type::iterator iterator;
  typedef typename map_type::const_iterator const_iterator;
  iterator begin() { return the_map->begin(); }
  const_iterator begin() const { return the_map->begin(); }
  iterator end() { return the_map->end(); }
  const_iterator end() const { return the_map->end(); }

  // Ditto for the size() method.
  size_t size() const { return the_map->size(); }

  // The find() method first checks the cache then falls back to the
  // underlying map.
  iterator find (const Key& key) {
    // Linear-search the cache.
    for (size_t i = 0; i < cache_size; i++) {
      if (cache[i] != nullptr && compare_keys(key, cache[i]->key)) {
        // Hit!
        typename map_type::iterator found_iter;
        if (i > 0) {
          // Hit was not in the first cache entry -- bubble up.
          CacheElt* prev_elt = cache[i - 1];
          cache[i - 1] = cache[i];
          cache[i] = prev_elt;
          found_iter = cache[i - 1]->iter;
        }
        else
          // Hit was in the first cache entry -- don't bubble up.
          found_iter = cache[0]->iter;
        return found_iter;
      }
    }

    // The item wasn't found in the cache -- search the unordered map.
    iterator iter = the_map->find(key);
    if (iter != end()) {
      // Cache only successful searches.
      CacheElt* new_ent = cache[cache_size - 1];
      if (null_entries > 0)
        for (size_t i = 0; i < cache_size; i++)
          if (cache[i] == nullptr) {
            // Insert a new entry into the cache.
            new_ent = cache[i] = new CacheElt;
            null_entries--;
            break;
          }
      new_ent->key = key;
      new_ent->iter = iter;
    }
    return iter;
  }

  // The erase() method erases the key:value pair from both the cache
  // and the underlying map.
  size_t erase (const Key& key) {
    // Linear-search the cache.
    for (size_t i = 0; i < cache_size; i++)
      if (cache[i] != nullptr && compare_keys(key, cache[i]->key)) {
        // Hit -- mark as invalid.
        delete cache[i];
        cache[i] = nullptr;
        null_entries++;
        break;
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

// Specialize CachedAnyMap to an STL unordered_map.
template<class Key,
         class T,
         class Hash = std::hash<Key>,
         class KeyEqual = std::equal_to<Key>,
         class Allocator = std::allocator< std::pair<const Key, T> > >
class CachedUnorderedMap : public CachedAnyMap<
  unordered_map<Key, T, Hash, KeyEqual, Allocator>, Key, T, KeyEqual>
{
};

// Specialize CachedAnyMap to an STL map.
template<class Key,
         class T,
         class Compare = std::less<Key>,
         class Allocator = std::allocator< std::pair<const Key, T> >,
         class KeyEqual = std::equal_to<Key> >
class CachedOrderedMap : public CachedAnyMap<
  map<Key, T, Compare, Allocator>, Key, T, KeyEqual>
{
};

#endif
