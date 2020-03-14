#pragma once

//
// Lock macros
//
#define fb_init_lock(a) \
  do {                  \
    init_completion(a); \
    complete(a);        \
  } while (0)

#define fb_lock(a)          \
  do {                      \
    wait_for_completion(a); \
    reinit_completion(a);   \
  } while (0)

#define fb_unlock(a) \
  do {               \
    complete(a);     \
  } while (0)
