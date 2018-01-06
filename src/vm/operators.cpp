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

#include <cmath>

#include "operators.h"

namespace Charly {
namespace Operators {
VALUE add(VM& vm, VALUE left, VALUE right) {
  if (VM::type(left) == kTypeNumeric && VM::type(right) == kTypeNumeric) {
    double nleft = VM::numeric_value(left);
    double nright = VM::numeric_value(right);
    return vm.create_float(nleft + nright);
  }

  return vm.create_float(NAN);
}

bool truthyness(VALUE value) {
  if (is_numeric(value)) {
    return VM::numeric_value(value) != 0;
  }

  if (is_null(value)) {
    return false;
  }

  if (is_false(value)) {
    return false;
  }

  return true;
}
}  // namespace Operators
}  // namespace Charly
