// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2022, Benoit BLANCHON
// MIT License

#pragma once

#include <ArduinoJson/Document/BasicJsonDocument.hpp>

#include <stdlib.h>  // malloc, free

#include "vmsys.h"
#include "vmio.h"

#define malloc vm_malloc
#define free vm_free
#define calloc(x,y) vm_calloc(x*y)
#define realloc vm_realloc

namespace ARDUINOJSON_NAMESPACE {

struct DefaultAllocator {
  void* allocate(size_t size) {
    return malloc(size);
  }

  void deallocate(void* ptr) {
    free(ptr);
  }

  void* reallocate(void* ptr, size_t new_size) {
    return realloc(ptr, new_size);
  }
};

typedef BasicJsonDocument<DefaultAllocator> DynamicJsonDocument;

}  // namespace ARDUINOJSON_NAMESPACE
