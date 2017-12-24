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

#include "ast.h"
#include "compiler-pass.h"

#pragma once

namespace Charly::Compilation {
class Normalizer : public CompilerPass {
  using CompilerPass::CompilerPass;

public:
  AST::AbstractNode* visit_block(AST::Block* node, VisitContinue cont);
  AST::AbstractNode* visit_if(AST::If* node, VisitContinue cont);
  AST::AbstractNode* visit_ifelse(AST::IfElse* node, VisitContinue cont);
  AST::AbstractNode* visit_unless(AST::Unless* node, VisitContinue cont);
  AST::AbstractNode* visit_unlesselse(AST::UnlessElse* node, VisitContinue cont);
  AST::AbstractNode* visit_guard(AST::Guard* node, VisitContinue cont);
  AST::AbstractNode* visit_while(AST::While* node, VisitContinue cont);
  AST::AbstractNode* visit_until(AST::Until* node, VisitContinue cont);
  AST::AbstractNode* visit_loop(AST::Loop* node, VisitContinue cont);
  AST::AbstractNode* visit_switch(AST::Switch* node, VisitContinue cont);
  AST::AbstractNode* visit_function(AST::Function* node, VisitContinue cont);
  AST::AbstractNode* visit_localinitialisation(AST::LocalInitialisation* node, VisitContinue cont);

private:
  AST::AbstractNode* wrap_in_block(AST::AbstractNode* node, VisitContinue cont);
};
}