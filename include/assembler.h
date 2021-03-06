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

#include <functional>
#include <list>
#include <unordered_map>

#include "instructionblock.h"
#include "opcode.h"

#pragma once

namespace Charly::Compilation {

// A label for a given position in the generated instruction stream of the assembler
typedef uint32_t Label;

// The offset field of an instruction that needs to be resolved
struct UnresolvedReference {
  Label id;
  uint32_t target_offset;
  uint32_t instruction_base;
};

// Handles label resolution and compile-time offset calculations
class Assembler : public InstructionBlock {
public:
  inline void reset() {
    this->labels.clear();
    this->unresolved_label_references.clear();
    this->next_label_id = 0;
  }

  // Label handling
  Label reserve_label();
  Label place_label();
  Label place_label(Label label);

  // Wrappers for instructions which branch by some byte offset
  void write_branch_to_label(Label label);
  void write_branchif_to_label(Label label);
  void write_branchunless_to_label(Label label);
  void write_branchlt_to_label(Label label);
  void write_branchgt_to_label(Label label);
  void write_branchle_to_label(Label label);
  void write_branchge_to_label(Label label);
  void write_brancheq_to_label(Label label);
  void write_branchneq_to_label(Label label);
  void write_registercatchtable_to_label(Label label);
  void write_putfunction_to_label(VALUE symbol,
                                  Label label,
                                  bool anonymous,
                                  bool needs_arguments,
                                  uint32_t argc,
                                  uint32_t lvarcount);
  void write_putgenerator_to_label(VALUE symbol, Label label);

  // Unresolved reference handling
  void resolve_unresolved_label_references();
  inline bool has_unresolved_label_references() {
    return this->unresolved_label_references.size() != 0;
  }

private:
  std::unordered_map<Label, uint32_t> labels;
  std::list<UnresolvedReference> unresolved_label_references;
  uint32_t next_label_id = 0;
};
}  // namespace Charly::Compilation
