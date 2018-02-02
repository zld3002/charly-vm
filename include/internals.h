/*
 * This file is part of the Charly Virtual Machine (https://github.com/KCreate/charly-vm)
 *
 * MIT License
 *
 * Copyright (c) 2017 - 2018 Leonard Schütz
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

#include "defines.h"

#pragma once

namespace Charly {
namespace Internals {

// The signature of an internal method
struct InternalMethodSignature {
  std::string name;
  size_t argc;
  uintptr_t func_pointer;
};

VALUE require(VM& vm, VALUE filename);
VALUE get_method(VM& vm, VALUE argument);
VALUE write(VM& vm, VALUE value);
VALUE getn(VM& vm);
VALUE set_primitive_object(VM& vm, VALUE klass);
VALUE set_primitive_class(VM& vm, VALUE klass);
VALUE set_primitive_array(VM& vm, VALUE klass);
VALUE set_primitive_string(VM& vm, VALUE klass);
VALUE set_primitive_number(VM& vm, VALUE klass);
VALUE set_primitive_function(VM& vm, VALUE klass);
VALUE set_primitive_generator(VM& vm, VALUE klass);
VALUE set_primitive_boolean(VM& vm, VALUE klass);
VALUE set_primitive_null(VM& vm, VALUE klass);
}  // namespace Internals
}  // namespace Charly
