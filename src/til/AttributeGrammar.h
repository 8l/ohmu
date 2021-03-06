//===- AttributeGrammar.h --------------------------------------*- C++ --*-===//
// Copyright 2014  Google
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
//
// AttributeGrammar.h defines several helper classes that extend the TIL
// traversal system to support attribute-grammar style computations.
//
//===----------------------------------------------------------------------===//

#ifndef OHMU_TIL_ATTRIBUTEGRAMMAR_H
#define OHMU_TIL_ATTRIBUTEGRAMMAR_H

#include "TIL.h"
#include "TILTraverse.h"

namespace ohmu {
namespace til {


/// ContextReducerBase is a base class for handling the current context.
/// In an attribute grammar, the context holds inherited attributes,
/// which propagate from parent to child during the traversal.
/// Examples include:
///   * The typing context, e.g. the names and types of local variables
///   * The current continuation, for the CPS transform.
///
/// When doing rewriting, there are actually two contexts.
/// The source context holds information about the term that is being
/// traversed, and the destination context holds information about the
/// term that is being produced.
template <class Context>
class ContextReducerBase {
public:
  Context* context() { return Ctx; }

  ContextReducerBase() : Ctx(nullptr) { }
  ContextReducerBase(Context* C) : Ctx(C) { }

protected:
  Context* Ctx;
};



/// This class is a base class for handling synthesized attributes in
/// attribute grammars.  When used with AGTraversal, which is depth-first,
/// it maintains a stack of attributes that correspond to the call stack.
/// The reduceX methods can use attr() and resultAttr() to read and set
/// synthesized attributes.
///
/// Attr is a type that holds synthesized attributes.
template <class Attr>
class AGReducerBase {
public:
  /// Get the attribute for the i^th argument.
  /// Should only be called from reduceX().
  Attr& attr(unsigned i) { return Attrs[AttrFrame + i]; }

  /// Get the attribute for the result.
  /// Should only be called from reduceX().
  Attr& resultAttr() { return Attrs[AttrFrame - 1]; }

  /// Push a new attribute onto the attribute stack, and set it to be
  /// the current result attribute.
  unsigned pushResultAttr() {
    unsigned N = AttrFrame;
    Attrs.push_back(Attr());
    AttrFrame = Attrs.size();
    return N;
  }

  /// Pop any extra attributes off the stack, leaving only the current result.
  /// Then restore the previous result attribute.
  /// N should be the value returned from the prior pushResultAttr().
  void restoreResultAttr(unsigned N) {
    while (Attrs.size() > AttrFrame)
      Attrs.pop_back();
    AttrFrame = N;
  }

  AGReducerBase() : AttrFrame(0) { }

protected:
  std::vector<Attr> Attrs;
  unsigned          AttrFrame;
};



/// AGTraversal is a mixin traversal class for use with AGReducerBase.
/// Self must inherit from both AGReducerBase and ContextReducerBase.
template <class Self, class SuperTv>
class AGTraversal : public SuperTv {
public:
  Self *self() { return static_cast<Self*>(this); }

  template<class T>
  T* traverse(T* E, TraversalKind K) {
    unsigned Af = self()->pushResultAttr();
    auto Cstate = self()->Ctx->enterSubExpr(K);

    T* Res = SuperTv::traverse(E, K);

    self()->Ctx->exitSubExpr(K, Cstate);
    self()->restoreResultAttr(Af);

    return Res;
  }
};


}  // end namespace til
}  // end namespace ohmu


#endif  // OHMU_TIL_ATTRIBUTEGRAMMAR_H
