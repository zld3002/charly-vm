/*
 * This file is part of the Charly Virtual Machine (https://github.com/KCreate/charly-vm)
 *
 * MIT License
 *
 * Copyright (c) 2017 Leonard Schütz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <unordered_map>
#include <vector>

#include "defines.h"

#pragma once

namespace Charly {
// Different masks for the flags field in the Basic struct
static const VALUE kFlagType        = 0b00011111;
static const VALUE kFlagMark        = 0b00100000;
static const VALUE kFlagShortString = 0b01000000;

// Type constants
enum kValueTypes : uint8_t {

  // Type identifiers
  kTypeDead,
  kTypeInteger,
  kTypeFloat,
  kTypeString,
  kTypeNumeric,
  kTypeBoolean,
  kTypeNull,
  kTypeObject,
  kTypeArray,
  kTypeFunction,
  kTypeCFunction,
  kTypeClass,
  kTypeSymbol,

  // Machine internal types
  kTypeFrame,
  kTypeCatchTable
};

// String representation of types
// clang-format off
static std::string kValueTypeString[] = {
  "dead",
  "integer",
  "float",
  "string",
  "numeric",
  "boolean",
  "null",
  "object",
  "array",
  "function",
  "cfunction",
  "class",
  "symbol",
  "frame",
  "catchtable"
};
// clang-format on

// Basic fields every data type in Charly has
// This is inspired by the way Ruby stores it's values
struct Basic {
  uint8_t flags;
  Basic() : flags(kTypeDead) {
  }

  // Getters for different flag fields
  inline uint8_t type() {
    return this->flags & kFlagType;
  }
  inline bool mark() {
    return (this->flags & kFlagMark) != 0;
  }
  inline bool short_string() {
    return (this->flags & kFlagShortString) != 0;
  }

  // Setters for different flag fields
  inline void set_type(uint8_t val) {
    this->flags = ((this->flags & ~kFlagType) | (kFlagType & val));
  }
  inline void set_mark(bool val) {
    this->flags ^= (-val ^ this->flags) & kFlagMark;
  }
  inline void set_short_string(bool val) {
    this->flags ^= (-val ^ this->flags) & kFlagShortString;
  }
};

// Memory that is allocated via the GC will be aligned to 8 bytes
// This means that if VALUE is a pointer, the last 3 bits will be set to 0.
// We can use this to our advantage to store some additional information
// in there.
static const uint8_t kPointerMask = 0b00111;
static const uint8_t kPointerFlag = 0b00000;
static const uint8_t kIntegerMask = 0b00001;
static const uint8_t kIntegerFlag = 0b00001;
static const uint8_t kFloatMask = 0b00011;
static const uint8_t kFloatFlag = 0b00010;
static const uint8_t kSymbolMask = 0b01111;
static const uint8_t kSymbolFlag = 0b01100;
static const uint8_t kFalse = 0b00000;
static const uint8_t kTrue = 0b10100;
static const uint8_t kNull = 0b01000;

inline bool is_boolean(VALUE value) {
  return value == kFalse || value == kTrue;
}
inline bool is_integer(VALUE value) {
  return (value & kIntegerMask) == kIntegerFlag;
}
inline bool is_ifloat(VALUE value) {
  return (value & kFloatMask) == kFloatFlag;
}
inline bool is_symbol(VALUE value) {
  return (value & kSymbolMask) == kSymbolFlag;
}
inline bool is_false(VALUE value) {
  return value == kFalse;
}
inline bool is_true(VALUE value) {
  return value == kTrue;
}
inline bool is_null(VALUE value) {
  return value == kNull;
}
inline bool is_pointer(VALUE value) {
  return (!is_null(value) && !is_false(value) && ((value & kPointerMask) == kPointerFlag));
}
inline bool is_special(VALUE value) {
  return !is_pointer(value);
}
inline Basic* basics(VALUE value) {
  return (Basic*)value;
}
inline bool is_numeric(VALUE value) {
  if (is_integer(value))
    return true;
  if (is_ifloat(value))
    return true;
  if (!is_special(value)) {
    if (basics(value)->type() == kTypeFloat)
      return true;
  }

  return false;
}

// Describes an object type
//
// It contains an unordered map which holds the objects values
// The klass field is a VALUE containing the class the object was constructed from
struct Object {
  Basic basic;
  VALUE klass;
  std::unordered_map<VALUE, VALUE>* container;

  void inline clean() {
    delete this->container;
  }
};

// Array type
struct Array {
  Basic basic;
  std::vector<VALUE>* data;

  void inline clean() {
    delete this->data;
  }
};

// String type
//
// A field inside the Basic structure tells the VM which representation is currently active
// +- Basic field
// |
// v
// 0x00 0x00 0x00 0x00 0x00
//      ^                 ^
//      |                 |
//      +-----------------+- Long string length
//      |
//      +- Short string length
static const uint32_t kShortStringMaxSize = 62;
struct String {
  Basic basic;

  union {
    struct {
      uint32_t length;
      char* data;
    } lbuf;
    struct {
      uint8_t length;
      char data[kShortStringMaxSize];
    } sbuf;
  };

  inline char* data() {
    return basic.short_string() ? sbuf.data : lbuf.data;
  }
  inline uint32_t length() {
    return basic.short_string() ? sbuf.length : lbuf.length;
  }
  inline void clean() {
    if (!basic.short_string()) {
      std::free(lbuf.data);
    }
  }
};

// Heap-allocated float type
//
// Used when a floating-point value won't fit into the immediate-encoded format
struct Float {
  Basic basic;
  double float_value;
};

// Frames introduce new environments
struct Frame {
  Basic basic;
  Frame* parent;
  Frame* parent_environment_frame;
  CatchTable* last_active_catchtable;
  Function* function;
  std::vector<VALUE>* environment;
  VALUE self;
  uint8_t* return_address;
  bool halt_after_return;
};

// Catchtable used for exception handling
struct CatchTable {
  Basic basic;
  uint8_t* address;
  size_t stacksize;
  Frame* frame;
  CatchTable* parent;
};

// Normal functions defined inside the virtual machine.
struct Function {
  Basic basic;
  VALUE name;
  uint32_t argc;
  uint32_t lvarcount;
  Frame* context;
  uint8_t* body_address;
  bool anonymous;
  bool bound_self_set;
  VALUE bound_self;
  std::unordered_map<VALUE, VALUE>* container;

  void inline clean() {
    delete this->container;
  }

  // TODO: Bound argumentlist
};

// Function type used for including external functions from C-Land into the virtual machine
// These are basically just a function pointer with some metadata associated to them
struct CFunction {
  Basic basic;
  VALUE name;
  uintptr_t pointer;
  uint32_t argc;
  bool bound_self_set;
  VALUE bound_self;
  std::unordered_map<VALUE, VALUE>* container;

  void inline clean() {
    delete this->container;
  }

  // TODO: Bound argumentlist
};

// Classes defined inside the virtual machine
struct Class {
  Basic basic;
  VALUE name;
  VALUE constructor;
  std::vector<VALUE>* member_properties;
  VALUE prototype;
  VALUE parent_class;
  std::unordered_map<VALUE, VALUE>* container;

  void inline clean() {
    delete this->member_properties;
    delete this->container;
  }
};

// Rotate a given value to the left n times
constexpr VALUE BIT_ROTL(VALUE v, VALUE n) {
  return (((v) << (n)) | ((v) >> ((sizeof(v) * 8) - n)));
}

// Rotate a given value to the right n times
constexpr VALUE BIT_ROTR(VALUE v, VALUE n) {
  return (((v) >> (n)) | ((v) << ((sizeof(v) * 8) - n)));
}

// Encode an integer as a Charly integer value
constexpr VALUE VALUE_ENCODE_INTEGER(int64_t value) {
  return (static_cast<VALUE>(value) << 1) | kIntegerFlag;
}

// Decode a Charly integer value into an integer
constexpr int64_t VALUE_DECODE_INTEGER(VALUE value) {
  return static_cast<int64_t>(value) >> 1;
}
}  // namespace Charly
