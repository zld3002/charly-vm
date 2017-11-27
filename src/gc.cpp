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

#include <iostream>
#include <cassert>

#include "gc.h"
#include "vm.h"

namespace Charly {
  MemoryManager::MemoryManager() {
    this->free_cell = nullptr;
    size_t heap_initial_count = kGCInitialHeapCount;
    this->heaps.reserve(heap_initial_count);
    while (heap_initial_count--) this->add_heap();
  }

  MemoryManager::~MemoryManager() {
    for (MemoryCell* heap : this->heaps) {
      for (size_t i = 0; i < kGCHeapCellCount; i++) {
        free(heap + i);
      }
    }
  }

  void MemoryManager::add_heap() {
    MemoryCell* heap = (MemoryCell*)calloc(kGCHeapCellCount, sizeof(MemoryCell));
    this->heaps.push_back(heap);

    // Add the newly allocated cells to the free list
    MemoryCell* last_cell = this->free_cell;
    for (size_t i = 0; i < kGCHeapCellCount; i++) {
      heap[i].as.free.next = last_cell;
      last_cell = heap + i;
    }

    this->free_cell = last_cell;
  }

  void MemoryManager::grow_heap() {
    size_t heap_count = this->heaps.size();
    size_t heaps_to_add = (heap_count * kGCHeapCountGrowthFactor) - heap_count;
    while (heaps_to_add--) this->add_heap();
  }

  void MemoryManager::register_temporary(VALUE value) {
    this->temporaries.insert(value);
  }

  void MemoryManager::unregister_temporary(VALUE value) {
    auto tmp = this->temporaries.find(value);
    assert(tmp != this->temporaries.end());
    if (tmp != this->temporaries.end()) {
      this->temporaries.erase(tmp);
    }
  }

  void MemoryManager::mark(VALUE value) {
    if (!is_pointer(value)) return;
    if (basics(value)->mark()) return;

    basics(value)->set_mark(true);
    switch (basics(value)->type()) {
      case kTypeObject: {
        auto obj = (Object *)value;
        this->mark(obj->klass);
        for (auto& obj_entry : *obj->container) this->mark(obj_entry.second);
        break;
      }

      case kTypeArray: {
        auto arr = (Array *)value;
        for (auto& arr_entry : *arr->data) this->mark(arr_entry);
        break;
      }

      case kTypeFunction: {
        auto func = (Function *)value;
        this->mark((VALUE)func->context);
        this->mark((VALUE)func->block);
        if (func->bound_self_set) this->mark(func->bound_self);
        break;
      }

      case kTypeCFunction: {
        auto cfunc = (CFunction *)value;
        if (cfunc->bound_self_set) this->mark(cfunc->bound_self);
        break;
      }

      case kTypeFrame: {
        auto frame = (Frame *)value;
        this->mark((VALUE)frame->parent);
        this->mark((VALUE)frame->parent_environment_frame);
        this->mark((VALUE)frame->function);
        this->mark(frame->self);
        for (auto& lvar : *frame->environment) this->mark(lvar.value);
        break;
      }

      case kTypeCatchTable: {
        auto table = (CatchTable *)value;
        this->mark((VALUE)table->frame);
        this->mark((VALUE)table->parent);
        break;
      }

      case kTypeInstructionBlock: {
        auto block = (InstructionBlock *)value;
        for (auto& child_block : block->child_blocks) {
          this->mark((VALUE)child_block);
        }
        break;
      }
    }
  }

  void MemoryManager::collect(VM* vm) {
    std::cout << "#-- GC: Pause --#" << std::endl;

    // Mark Phase
    for (auto& stack_item : vm->stack) this->mark(stack_item);
    for (auto& temp_item : this->temporaries) this->mark(temp_item);
    this->mark((VALUE)vm->frames);
    this->mark((VALUE)vm->catchstack);

    // Sweep Phase
    int freed_cells_count = 0;
    for (MemoryCell* heap : this->heaps) {
      for (size_t i = 0; i < kGCHeapCellCount; i++) {
        MemoryCell* cell = heap + i;
        if (basics((VALUE)cell)->mark()) {
          basics((VALUE)cell)->set_mark(false);
        } else {

          // This cell might already be on the free list
          // Make sure we don't double free cells
          if (basics((VALUE)cell)->type() != kTypeDead) {
            freed_cells_count++;
            this->free(cell);
          }
        }
      }
    }

    std::cout << "#-- GC: Freed a total of " << freed_cells_count << " cells --#" << std::endl;
    std::cout << "#-- GC: Finished --#" << std::endl;
  }

  MemoryCell* MemoryManager::allocate(VM* vm) {
    MemoryCell* cell = this->free_cell;

    if (cell) {
      this->free_cell = this->free_cell->as.free.next;

      // If we've just allocated the last available cell,
      // we do a collect in order to make sure we never get a failing
      // allocation in the future
      if (!this->free_cell) {
        this->collect(vm);

        // If a collection didn't yield new available space,
        // allocate more heaps
        if (!this->free_cell) {
          this->grow_heap();

          if (!this->free_cell) {
            std::cout << "Failed to expand heap, the next allocation will cause a segfault." << std::endl;
          }
        }
      }
    }

    return cell;
  }

  void MemoryManager::free(MemoryCell* cell) {

    // This cell might be inside the temporaries list,
    // check if it's inside and remove it if it is
    if (this->temporaries.count((VALUE)cell) == 1) {
      this->unregister_temporary((VALUE)cell);
    }

    // We don't actually free the cells that were allocated, we just
    // deallocat the properties inside these cells and memset(0)'em
    switch (basics((VALUE)cell)->type()) {
      case kTypeObject: {
        cell->as.object.clean();
        break;
      }

      case kTypeArray: {
        cell->as.array.clean();
        break;
      }

      case kTypeString: {
        cell->as.string.clean();
        break;
      }

      case kTypeFrame: {
        cell->as.frame.clean();
        break;
      }

      case kTypeInstructionBlock: {
        cell->as.instructionblock.clean();
        break;
      }

      default: {
        break;
      }
    }

    memset(cell, 0, sizeof(MemoryCell));
    cell->as.free.next = this->free_cell;
    this->free_cell = cell;
  }
}
