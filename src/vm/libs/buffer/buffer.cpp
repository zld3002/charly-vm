/*
 * This file is part of the Charly Virtual Machine (https://github.com/KCreate/charly-vm)
 *
 * MIT License
 *
 * Copyright (c) 2017 - 2019 Leonard Schütz
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

#include <utf8/utf8.h>

#include "buffer.h"
#include "vm.h"
#include "managedcontext.h"

using namespace std;

namespace Charly {
namespace Internals {
namespace Buffer {

static void destructor(void* data) {
  uint64_t id = reinterpret_cast<uint64_t>(data);
  UTF8Buffer* buf = buffer_list[id];
  if (buf) {
    delete buf;
  };
}

VALUE create(VM& vm, VALUE size) {
  CHECK(number, size);
  uint64_t id = next_buf_id++;
  UTF8Buffer* buf = new UTF8Buffer();
  buf->grow_to_fit(charly_number_to_uint32(size));
  buffer_list[id] = buf;

  ManagedContext lalloc(vm);
  return lalloc.create_cpointer(reinterpret_cast<void*>(id), reinterpret_cast<void*>(destructor));
}

VALUE reserve(VM& vm, VALUE buf, VALUE size) {
  CHECK(cpointer, buf);
  CHECK(number, size);

  UTF8Buffer* buffer = buffer_list[reinterpret_cast<uint64_t>(charly_as_cpointer(buf)->data)];
  if (!buffer) {
    return kNull;
  }

  buffer->grow_to_fit(charly_number_to_uint32(size));

  return kNull;
}

VALUE get_size(VM& vm, VALUE buf) {
  CHECK(cpointer, buf);

  UTF8Buffer* buffer = buffer_list[reinterpret_cast<uint64_t>(charly_as_cpointer(buf)->data)];
  if (!buffer) {
    return kNull;
  }

  return charly_create_number(buffer->get_capacity());
}

VALUE get_offset(VM& vm, VALUE buf) {
  CHECK(cpointer, buf);

  UTF8Buffer* buffer = buffer_list[reinterpret_cast<uint64_t>(charly_as_cpointer(buf)->data)];
  if (!buffer) {
    return kNull;
  }

  return charly_create_integer(buffer->get_writeoffset());
}

VALUE write(VM& vm, VALUE buf, VALUE src) {
  CHECK(cpointer, buf);
  CHECK(string, src);

  UTF8Buffer* buffer = buffer_list[reinterpret_cast<uint64_t>(charly_as_cpointer(buf)->data)];
  if (!buffer) {
    return kNull;
  }

  buffer->write_block(reinterpret_cast<uint8_t*>(charly_string_data(src)), charly_string_length(src));
  return charly_create_integer(buffer->get_writeoffset());
}

VALUE write_partial(VM& vm, VALUE buf, VALUE src, VALUE off, VALUE cnt) {
  CHECK(cpointer, buf);
  CHECK(string, src);
  CHECK(number, off);
  CHECK(number, cnt);

  uint32_t _off = charly_number_to_uint32(off);
  uint32_t _cnt = charly_number_to_uint32(cnt);

  UTF8Buffer* buffer = buffer_list[reinterpret_cast<uint64_t>(charly_as_cpointer(buf)->data)];
  if (!buffer) {
    return kNull;
  }

  // Calculate the offset of the start pointer and the amount of bytes to copy
  uint8_t* data_ptr = reinterpret_cast<uint8_t*>(charly_string_data(src));
  uint32_t total_bytesize = charly_string_length(src);

  // Calculcate utf8 offsets
  uint8_t* utf8_data = data_ptr;
  while (utf8_data < data_ptr + total_bytesize && _off > 0) {
    utf8::next(utf8_data, data_ptr + total_bytesize);
    _off--;
  }

  // Backup data ptr
  uint8_t* data_begin = utf8_data;

  // Check if we can still copy _cnt characters
  uint32_t bytes_copyable = 0;
  while (utf8_data < data_ptr + total_bytesize && _cnt > 0) {
    uint8_t* _tmp = utf8_data;
    utf8::next(utf8_data, data_ptr + total_bytesize);
    if (utf8_data <= data_ptr + total_bytesize) {
      bytes_copyable += utf8_data - _tmp;
    }
    _cnt--;
  }

  buffer->write_block(data_begin, bytes_copyable);
  return charly_create_integer(buffer->get_writeoffset());
}

VALUE write_bytes(VM&vm, VALUE buf, VALUE bytes) {
  CHECK(cpointer, buf);
  CHECK(array, bytes);
  CHECK(array_of<kTypeNumber>, bytes);

  UTF8Buffer* buffer = buffer_list[reinterpret_cast<uint64_t>(charly_as_cpointer(buf)->data)];
  if (!buffer) {
    return kNull;
  }

  Array* arr = charly_as_array(bytes);
  for (VALUE v : *(arr->data)) {
    buffer->write_u8(charly_number_to_uint8(v));
  }

  return charly_create_integer(buffer->get_writeoffset());
}

VALUE str(VM& vm, VALUE buf) {
  CHECK(cpointer, buf);
  UTF8Buffer* buffer = buffer_list[reinterpret_cast<uint64_t>(charly_as_cpointer(buf)->data)];
  if (!buffer) {
    return kNull;
  }
  ManagedContext lalloc(vm);
  return lalloc.create_string(buffer->get_const_data(), buffer->get_writeoffset());
}

VALUE bytes(VM& vm, VALUE buf) {
  CHECK(cpointer, buf);
  UTF8Buffer* buffer = buffer_list[reinterpret_cast<uint64_t>(charly_as_cpointer(buf)->data)];
  if (!buffer) {
    return kNull;
  }

  uint8_t* data = buffer->get_data();
  uint32_t offset = buffer->get_writeoffset();

  // Allocate the array that will return the bytes
  // offset is the amount of bytes we need to store
  ManagedContext lalloc(vm);
  Array* byte_array = charly_as_array(lalloc.create_array(offset));

  for (uint32_t i = 0; i < offset; i++) {
    byte_array->data->push_back(charly_create_integer(data[i]));
  }

  return charly_create_pointer(byte_array);
}

}  // namespace Buffer
}  // namespace Internals
}  // namespace Charly
