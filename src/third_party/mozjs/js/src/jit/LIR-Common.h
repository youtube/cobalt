/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_LIR_Common_h
#define jit_LIR_Common_h

#include "jit/shared/Assembler-shared.h"

// This file declares LIR instructions that are common to every platform.

namespace js {
namespace jit {

template <size_t Temps, size_t ExtraUses = 0>
class LBinaryMath : public LInstructionHelper<1, 2 + ExtraUses, Temps>
{
  public:
    const LAllocation *lhs() {
        return this->getOperand(0);
    }
    const LAllocation *rhs() {
        return this->getOperand(1);
    }
};

// Used for jumps from other blocks. Also simplifies register allocation since
// the first instruction of a block is guaranteed to have no uses.
class LLabel : public LInstructionHelper<0, 0, 0>
{
    Label label_;

  public:
    LIR_HEADER(Label)

    Label *label() {
        return &label_;
    }
};

class LNop : public LInstructionHelper<0, 0, 0>
{
  public:
    LIR_HEADER(Nop)
};

class LMop : public LInstructionHelper<0, 0, 0>
{
  public:
    LIR_HEADER(Mop)
};

// An LOsiPoint captures a snapshot after a call and ensures enough space to
// patch in a call to the invalidation mechanism.
//
// Note: LSafepoints are 1:1 with LOsiPoints, so it holds a reference to the
// corresponding LSafepoint to inform it of the LOsiPoint's masm offset when it
// gets CG'd.
class LOsiPoint : public LInstructionHelper<0, 0, 0>
{
    LSafepoint *safepoint_;

  public:
    LOsiPoint(LSafepoint *safepoint, LSnapshot *snapshot)
      : safepoint_(safepoint)
    {
        JS_ASSERT(safepoint && snapshot);
        assignSnapshot(snapshot);
    }

    LSafepoint *associatedSafepoint() {
        return safepoint_;
    }

    LIR_HEADER(OsiPoint)
};

class LMove
{
    LAllocation *from_;
    LAllocation *to_;

  public:
    LMove(LAllocation *from, LAllocation *to)
      : from_(from),
        to_(to)
    { }

    LAllocation *from() {
        return from_;
    }
    const LAllocation *from() const {
        return from_;
    }
    LAllocation *to() {
        return to_;
    }
    const LAllocation *to() const {
        return to_;
    }
};

class LMoveGroup : public LInstructionHelper<0, 0, 0>
{
    js::Vector<LMove, 2, IonAllocPolicy> moves_;

  public:
    LIR_HEADER(MoveGroup)

    void printOperands(FILE *fp);

    // Add a move which takes place simultaneously with all others in the group.
    bool add(LAllocation *from, LAllocation *to);

    // Add a move which takes place after existing moves in the group.
    bool addAfter(LAllocation *from, LAllocation *to);

    size_t numMoves() const {
        return moves_.length();
    }
    const LMove &getMove(size_t i) const {
        return moves_[i];
    }
};

// Constant 32-bit integer.
class LInteger : public LInstructionHelper<1, 0, 0>
{
    int32_t i32_;

  public:
    LIR_HEADER(Integer)

    LInteger(int32_t i32)
      : i32_(i32)
    { }

    int32_t getValue() const {
        return i32_;
    }
};

// Constant pointer.
class LPointer : public LInstructionHelper<1, 0, 0>
{
  public:
    enum Kind {
        GC_THING,
        NON_GC_THING
    };

  private:
    void *ptr_;
    Kind kind_;

  public:
    LIR_HEADER(Pointer)

    LPointer(gc::Cell *ptr)
      : ptr_(ptr), kind_(GC_THING)
    { }

    LPointer(void *ptr, Kind kind)
      : ptr_(ptr), kind_(kind)
    { }

    void *ptr() const {
        return ptr_;
    }
    Kind kind() const {
        return kind_;
    }

    gc::Cell *gcptr() const {
        JS_ASSERT(kind() == GC_THING);
        return (gc::Cell *) ptr_;
    }
};

// Constant double.
class LDouble : public LInstructionHelper<1, 0, 0>
{
    double d_;
  public:
    LIR_HEADER(Double);

    LDouble(double d) : d_(d)
    { }
    double getDouble() const {
        return d_;
    }
};

// A constant Value.
class LValue : public LInstructionHelper<BOX_PIECES, 0, 0>
{
    Value v_;

  public:
    LIR_HEADER(Value)

    LValue(const Value &v)
      : v_(v)
    { }

    Value value() const {
        return v_;
    }
};

// Formal argument for a function, returning a box. Formal arguments are
// initially read from the stack.
class LParameter : public LInstructionHelper<BOX_PIECES, 0, 0>
{
  public:
    LIR_HEADER(Parameter)
};

// Stack offset for a word-sized immutable input value to a frame.
class LCallee : public LInstructionHelper<1, 0, 0>
{
  public:
    LIR_HEADER(Callee)
};

// Base class for control instructions (goto, branch, etc.)
template <size_t Succs, size_t Operands, size_t Temps>
class LControlInstructionHelper : public LInstructionHelper<0, Operands, Temps> {

    MBasicBlock *successors_[Succs];

  public:
    size_t numSuccessors() const { return Succs; }

    MBasicBlock *getSuccessor(size_t i) const { return successors_[i]; }

    void setSuccessor(size_t i, MBasicBlock *successor) {
        successors_[i] = successor;
    }
};

// Jumps to the start of a basic block.
class LGoto : public LControlInstructionHelper<1, 0, 0>
{
  public:
    LIR_HEADER(Goto)

    LGoto(MBasicBlock *block)
    {
         setSuccessor(0, block);
    }

    MBasicBlock *target() const {
        return getSuccessor(0);
    }
};

class LNewSlots : public LCallInstructionHelper<1, 0, 3>
{
  public:
    LIR_HEADER(NewSlots)

    LNewSlots(const LDefinition &temp1, const LDefinition &temp2, const LDefinition &temp3) {
        setTemp(0, temp1);
        setTemp(1, temp2);
        setTemp(2, temp3);
    }

    const LDefinition *temp1() {
        return getTemp(0);
    }
    const LDefinition *temp2() {
        return getTemp(1);
    }
    const LDefinition *temp3() {
        return getTemp(2);
    }

    MNewSlots *mir() const {
        return mir_->toNewSlots();
    }
};

class LNewParallelArray : public LInstructionHelper<1, 0, 0>
{
  public:
    LIR_HEADER(NewParallelArray);

    MNewParallelArray *mir() const {
        return mir_->toNewParallelArray();
    }
};

class LNewArray : public LInstructionHelper<1, 0, 0>
{
  public:
    LIR_HEADER(NewArray)

    const char *extraName() const {
        return mir()->shouldUseVM() ? "VMCall" : NULL;
    }

    MNewArray *mir() const {
        return mir_->toNewArray();
    }
};

class LNewObject : public LInstructionHelper<1, 0, 0>
{
  public:
    LIR_HEADER(NewObject)

    const char *extraName() const {
        return mir()->shouldUseVM() ? "VMCall" : NULL;
    }

    MNewObject *mir() const {
        return mir_->toNewObject();
    }
};

class LParNew : public LInstructionHelper<1, 1, 2>
{
  public:
    LIR_HEADER(ParNew);

    LParNew(const LAllocation &parSlice,
            const LDefinition &temp1,
            const LDefinition &temp2)
    {
        setOperand(0, parSlice);
        setTemp(0, temp1);
        setTemp(1, temp2);
    }

    MParNew *mir() const {
        return mir_->toParNew();
    }

    const LAllocation *parSlice() {
        return getOperand(0);
    }

    const LAllocation *getTemp0() {
        return getTemp(0)->output();
    }

    const LAllocation *getTemp1() {
        return getTemp(1)->output();
    }
};

class LParNewDenseArray : public LCallInstructionHelper<1, 2, 3>
{
  public:
    LIR_HEADER(ParNewDenseArray);

    LParNewDenseArray(const LAllocation &parSlice,
                      const LAllocation &length,
                      const LDefinition &temp1,
                      const LDefinition &temp2,
                      const LDefinition &temp3) {
        setOperand(0, parSlice);
        setOperand(1, length);
        setTemp(0, temp1);
        setTemp(1, temp2);
        setTemp(2, temp3);
    }

    MParNewDenseArray *mir() const {
        return mir_->toParNewDenseArray();
    }

    const LAllocation *parSlice() {
        return getOperand(0);
    }

    const LAllocation *length() {
        return getOperand(1);
    }

    const LAllocation *getTemp0() {
        return getTemp(0)->output();
    }

    const LAllocation *getTemp1() {
        return getTemp(1)->output();
    }

    const LAllocation *getTemp2() {
        return getTemp(2)->output();
    }
};

// Allocates a new DeclEnvObject.
//
// This instruction generates two possible instruction sets:
//   (1) An inline allocation of the call object is attempted.
//   (2) Otherwise, a callVM create a new object.
//
class LNewDeclEnvObject : public LInstructionHelper<1, 0, 0>
{
  public:
    LIR_HEADER(NewDeclEnvObject);

    MNewDeclEnvObject *mir() const {
        return mir_->toNewDeclEnvObject();
    }
};

// Allocates a new CallObject. The inputs are:
//      slots: either a reg representing a HeapSlot *, or a placeholder
//             meaning that no slots pointer is needed.
//
// This instruction generates two possible instruction sets:
//   (1) If the call object is extensible, this is a callVM to create the
//       call object.
//   (2) Otherwise, an inline allocation of the call object is attempted.
//
class LNewCallObject : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(NewCallObject)

    LNewCallObject(const LAllocation &slots) {
        setOperand(0, slots);
    }

    const LAllocation *slots() {
        return getOperand(0);
    }
    MNewCallObject *mir() const {
        return mir_->toNewCallObject();
    }
};

class LParNewCallObject : public LInstructionHelper<1, 2, 2>
{
    LParNewCallObject(const LAllocation &parSlice,
                      const LAllocation &slots,
                      const LDefinition &temp1,
                      const LDefinition &temp2) {
        setOperand(0, parSlice);
        setOperand(1, slots);
        setTemp(0, temp1);
        setTemp(1, temp2);
    }

public:
    LIR_HEADER(ParNewCallObject);

    static LParNewCallObject *NewWithSlots(const LAllocation &parSlice,
                                           const LAllocation &slots,
                                           const LDefinition &temp1,
                                           const LDefinition &temp2) {
        return new LParNewCallObject(parSlice, slots, temp1, temp2);
    }

    static LParNewCallObject *NewSansSlots(const LAllocation &parSlice,
                                           const LDefinition &temp1,
                                           const LDefinition &temp2) {
        LAllocation slots = LConstantIndex::Bogus();
        return new LParNewCallObject(parSlice, slots, temp1, temp2);
    }

    const LAllocation *parSlice() {
        return getOperand(0);
    }

    const LAllocation *slots() {
        return getOperand(1);
    }

    const bool hasDynamicSlots() {
        // TO INVESTIGATE: Felix tried using isRegister() method here,
        // but for useFixed(_, CallTempN), isRegister() is false (and
        // isUse() is true).  So for now ignore that and try to match
        // the LConstantIndex::Bogus() generated above instead.
        return slots() && ! slots()->isConstant();
    }

    const MParNewCallObject *mir() const {
        return mir_->toParNewCallObject();
    }

    const LAllocation *getTemp0() {
        return getTemp(0)->output();
    }

    const LAllocation *getTemp1() {
        return getTemp(1)->output();
    }
};

class LNewStringObject : public LInstructionHelper<1, 1, 1>
{
  public:
    LIR_HEADER(NewStringObject)

    LNewStringObject(const LAllocation &input, const LDefinition &temp) {
        setOperand(0, input);
        setTemp(0, temp);
    }

    const LAllocation *input() {
        return getOperand(0);
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
    MNewStringObject *mir() const {
        return mir_->toNewStringObject();
    }
};

class LParBailout : public LInstructionHelper<0, 0, 0>
{
  public:
    LIR_HEADER(ParBailout);
};

class LInitElem : public LCallInstructionHelper<0, 1 + 2*BOX_PIECES, 0>
{
  public:
    LIR_HEADER(InitElem)

    LInitElem(const LAllocation &object) {
        setOperand(0, object);
    }

    static const size_t IdIndex = 1;
    static const size_t ValueIndex = 1 + BOX_PIECES;

    const LAllocation *getObject() {
        return getOperand(0);
    }
    MInitElem *mir() const {
        return mir_->toInitElem();
    }
};

// Takes in an Object and a Value.
class LInitProp : public LCallInstructionHelper<0, 1 + BOX_PIECES, 0>
{
  public:
    LIR_HEADER(InitProp)

    LInitProp(const LAllocation &object) {
        setOperand(0, object);
    }

    static const size_t ValueIndex = 1;

    const LAllocation *getObject() {
        return getOperand(0);
    }
    const LAllocation *getValue() {
        return getOperand(1);
    }

    MInitProp *mir() const {
        return mir_->toInitProp();
    }
};

class LCheckOverRecursed : public LInstructionHelper<0, 0, 0>
{
  public:
    LIR_HEADER(CheckOverRecursed)

    LCheckOverRecursed()
    { }
};

class LParCheckOverRecursed : public LInstructionHelper<0, 1, 1>
{
  public:
    LIR_HEADER(ParCheckOverRecursed);

    LParCheckOverRecursed(const LAllocation &parSlice,
                          const LDefinition &tempReg)
    {
        setOperand(0, parSlice);
        setTemp(0, tempReg);
    }

    const LAllocation *parSlice() {
        return getOperand(0);
    }

    const LDefinition *getTempReg() {
        return getTemp(0);
    }
};

class LParCheckInterrupt : public LInstructionHelper<0, 1, 1>
{
  public:
    LIR_HEADER(ParCheckInterrupt);

    LParCheckInterrupt(const LAllocation &parSlice,
                       const LDefinition &tempReg)
    {
        setOperand(0, parSlice);
        setTemp(0, tempReg);
    }

    const LAllocation *parSlice() {
        return getOperand(0);
    }

    const LDefinition *getTempReg() {
        return getTemp(0);
    }
};

class LDefVar : public LCallInstructionHelper<0, 1, 0>
{
  public:
    LIR_HEADER(DefVar)

    LDefVar(const LAllocation &scopeChain)
    {
        setOperand(0, scopeChain);
    }

    const LAllocation *scopeChain() {
        return getOperand(0);
    }
    MDefVar *mir() const {
        return mir_->toDefVar();
    }
};

class LDefFun : public LCallInstructionHelper<0, 1, 0>
{
  public:
    LIR_HEADER(DefFun)

    LDefFun(const LAllocation &scopeChain)
    {
        setOperand(0, scopeChain);
    }

    const LAllocation *scopeChain() {
        return getOperand(0);
    }
    MDefFun *mir() const {
        return mir_->toDefFun();
    }
};

class LTypeOfV : public LInstructionHelper<1, BOX_PIECES, 0>
{
  public:
    LIR_HEADER(TypeOfV)

    static const size_t Input = 0;

    MTypeOf *mir() const {
        return mir_->toTypeOf();
    }
};

class LToIdV : public LInstructionHelper<BOX_PIECES, 2 * BOX_PIECES, 1>
{
  public:
    LIR_HEADER(ToIdV)
    BOX_OUTPUT_ACCESSORS()

    LToIdV(const LDefinition &temp)
    {
        setTemp(0, temp);
    }

    static const size_t Object = 0;
    static const size_t Index = BOX_PIECES;

    MToId *mir() const {
        return mir_->toToId();
    }

    const LDefinition *tempFloat() {
        return getTemp(0);
    }
};

// Allocate an object for |new| on the caller-side,
// when there is no templateObject or prototype known
class LCreateThis : public LCallInstructionHelper<BOX_PIECES, 1, 0>
{
  public:
    LIR_HEADER(CreateThis)

    LCreateThis(const LAllocation &callee)
    {
        setOperand(0, callee);
    }

    const LAllocation *getCallee() {
        return getOperand(0);
    }

    MCreateThis *mir() const {
        return mir_->toCreateThis();
    }
};

// Allocate an object for |new| on the caller-side,
// when the prototype is known.
class LCreateThisWithProto : public LCallInstructionHelper<1, 2, 0>
{
  public:
    LIR_HEADER(CreateThisWithProto)

    LCreateThisWithProto(const LAllocation &callee, const LAllocation &prototype)
    {
        setOperand(0, callee);
        setOperand(1, prototype);
    }

    const LAllocation *getCallee() {
        return getOperand(0);
    }
    const LAllocation *getPrototype() {
        return getOperand(1);
    }

    MCreateThis *mir() const {
        return mir_->toCreateThis();
    }
};

// Allocate an object for |new| on the caller-side.
// Always performs object initialization with a fast path.
class LCreateThisWithTemplate : public LInstructionHelper<1, 0, 0>
{
  public:
    LIR_HEADER(CreateThisWithTemplate)

    LCreateThisWithTemplate()
    { }

    MCreateThisWithTemplate *mir() const {
        return mir_->toCreateThisWithTemplate();
    }
};

// Allocate a new arguments object for the frame.
class LCreateArgumentsObject : public LCallInstructionHelper<1, 1, 1>
{
  public:
    LIR_HEADER(CreateArgumentsObject)

    LCreateArgumentsObject(const LAllocation &callObj, const LDefinition &temp)
    {
        setOperand(0, callObj);
        setTemp(0, temp);
    }

    const LAllocation *getCallObject() {
        return getOperand(0);
    }

    MCreateArgumentsObject *mir() const {
        return mir_->toCreateArgumentsObject();
    }
};

// Get argument from arguments object.
class LGetArgumentsObjectArg : public LInstructionHelper<BOX_PIECES, 1, 1>
{
  public:
    LIR_HEADER(GetArgumentsObjectArg)

    LGetArgumentsObjectArg(const LAllocation &argsObj, const LDefinition &temp)
    {
        setOperand(0, argsObj);
        setTemp(0, temp);
    }

    const LAllocation *getArgsObject() {
        return getOperand(0);
    }

    MGetArgumentsObjectArg *mir() const {
        return mir_->toGetArgumentsObjectArg();
    }
};

// Set argument on arguments object.
class LSetArgumentsObjectArg : public LInstructionHelper<0, 1 + BOX_PIECES, 1>
{
  public:
    LIR_HEADER(SetArgumentsObjectArg)

    LSetArgumentsObjectArg(const LAllocation &argsObj, const LDefinition &temp)
    {
        setOperand(0, argsObj);
        setTemp(0, temp);
    }

    const LAllocation *getArgsObject() {
        return getOperand(0);
    }

    MSetArgumentsObjectArg *mir() const {
        return mir_->toSetArgumentsObjectArg();
    }

    static const size_t ValueIndex = 1;
};

// If the Value is an Object, return unbox(Value).
// Otherwise, return the other Object.
class LReturnFromCtor : public LInstructionHelper<1, BOX_PIECES + 1, 0>
{
  public:
    LIR_HEADER(ReturnFromCtor)

    LReturnFromCtor(const LAllocation &object)
    {
        // Value set by useBox() during lowering.
        setOperand(LReturnFromCtor::ObjectIndex, object);
    }

    const LAllocation *getObject() {
        return getOperand(LReturnFromCtor::ObjectIndex);
    }

    static const size_t ValueIndex = 0;
    static const size_t ObjectIndex = BOX_PIECES;
};

// Writes a typed argument for a function call to the frame's argument vector.
class LStackArgT : public LInstructionHelper<0, 1, 0>
{
    uint32_t argslot_; // Index into frame-scope argument vector.

  public:
    LIR_HEADER(StackArgT)

    LStackArgT(uint32_t argslot, const LAllocation &arg)
      : argslot_(argslot)
    {
        setOperand(0, arg);
    }

    MPassArg *mir() const {
        return this->mir_->toPassArg();
    }
    uint32_t argslot() const {
        return argslot_;
    }
    const LAllocation *getArgument() {
        return getOperand(0);
    }
};

// Writes an untyped argument for a function call to the frame's argument vector.
class LStackArgV : public LInstructionHelper<0, BOX_PIECES, 0>
{
    uint32_t argslot_; // Index into frame-scope argument vector.

  public:
    LIR_HEADER(StackArgV)

    LStackArgV(uint32_t argslot)
      : argslot_(argslot)
    { }

    uint32_t argslot() const {
        return argslot_;
    }
};

// Common code for LIR descended from MCall.
template <size_t Defs, size_t Operands, size_t Temps>
class LJSCallInstructionHelper : public LCallInstructionHelper<Defs, Operands, Temps>
{
    // Slot below which %esp should be adjusted to make the call.
    // Zero for a function without arguments.
    uint32_t argslot_;

  public:
    LJSCallInstructionHelper(uint32_t argslot)
      : argslot_(argslot)
    { }

    uint32_t argslot() const {
        return argslot_;
    }
    MCall *mir() const {
        return this->mir_->toCall();
    }

    bool hasSingleTarget() const {
        return getSingleTarget() != NULL;
    }
    JSFunction *getSingleTarget() const {
        return mir()->getSingleTarget();
    }

    // The number of stack arguments is the max between the number of formal
    // arguments and the number of actual arguments. The number of stack
    // argument includes the |undefined| padding added in case of underflow.
    // Does not include |this|.
    uint32_t numStackArgs() const {
        JS_ASSERT(mir()->numStackArgs() >= 1);
        return mir()->numStackArgs() - 1; // |this| is not a formal argument.
    }
    // Does not include |this|.
    uint32_t numActualArgs() const {
        return mir()->numActualArgs();
    }

    typedef LJSCallInstructionHelper<Defs, Operands, Temps> JSCallHelper;
};

// Generates a polymorphic callsite, wherein the function being called is
// unknown and anticipated to vary.
class LCallGeneric : public LJSCallInstructionHelper<BOX_PIECES, 1, 2>
{
  public:
    LIR_HEADER(CallGeneric)

    LCallGeneric(const LAllocation &func, uint32_t argslot,
                 const LDefinition &nargsreg, const LDefinition &tmpobjreg)
      : JSCallHelper(argslot)
    {
        setOperand(0, func);
        setTemp(0, nargsreg);
        setTemp(1, tmpobjreg);
    }

    const LAllocation *getFunction() {
        return getOperand(0);
    }
    const LAllocation *getNargsReg() {
        return getTemp(0)->output();
    }
    const LAllocation *getTempObject() {
        return getTemp(1)->output();
    }
};

// Generates a hardcoded callsite for a known, non-native target.
class LCallKnown : public LJSCallInstructionHelper<BOX_PIECES, 1, 1>
{
  public:
    LIR_HEADER(CallKnown)

    LCallKnown(const LAllocation &func, uint32_t argslot, const LDefinition &tmpobjreg)
      : JSCallHelper(argslot)
    {
        setOperand(0, func);
        setTemp(0, tmpobjreg);
    }

    const LAllocation *getFunction() {
        return getOperand(0);
    }
    const LAllocation *getTempObject() {
        return getTemp(0)->output();
    }
};

// Generates a hardcoded callsite for a known, native target.
class LCallNative : public LJSCallInstructionHelper<BOX_PIECES, 0, 4>
{
  public:
    LIR_HEADER(CallNative)

    LCallNative(uint32_t argslot,
                const LDefinition &argJSContext, const LDefinition &argUintN,
                const LDefinition &argVp, const LDefinition &tmpreg)
      : JSCallHelper(argslot)
    {
        // Registers used for callWithABI().
        setTemp(0, argJSContext);
        setTemp(1, argUintN);
        setTemp(2, argVp);

        // Temporary registers.
        setTemp(3, tmpreg);
    }

    const LAllocation *getArgJSContextReg() {
        return getTemp(0)->output();
    }
    const LAllocation *getArgUintNReg() {
        return getTemp(1)->output();
    }
    const LAllocation *getArgVpReg() {
        return getTemp(2)->output();
    }
    const LAllocation *getTempReg() {
        return getTemp(3)->output();
    }
};

// Generates a hardcoded callsite for a known, DOM-native target.
class LCallDOMNative : public LJSCallInstructionHelper<BOX_PIECES, 0, 4>
{
  public:
    LIR_HEADER(CallDOMNative)

    LCallDOMNative(uint32_t argslot,
                   const LDefinition &argJSContext, const LDefinition &argObj,
                   const LDefinition &argPrivate, const LDefinition &argArgs)
      : JSCallHelper(argslot)
    {
        setTemp(0, argJSContext);
        setTemp(1, argObj);
        setTemp(2, argPrivate);
        setTemp(3, argArgs);
    }

    const LAllocation *getArgJSContext() {
        return getTemp(0)->output();
    }
    const LAllocation *getArgObj() {
        return getTemp(1)->output();
    }
    const LAllocation *getArgPrivate() {
        return getTemp(2)->output();
    }
    const LAllocation *getArgArgs() {
        return getTemp(3)->output();
    }
};

template <size_t defs, size_t ops>
class LDOMPropertyInstructionHelper : public LCallInstructionHelper<defs, 1 + ops, 3>
{
  protected:
    LDOMPropertyInstructionHelper(const LDefinition &JSContextReg, const LAllocation &ObjectReg,
                                  const LDefinition &PrivReg, const LDefinition &ValueReg)
    {
        this->setOperand(0, ObjectReg);
        this->setTemp(0, JSContextReg);
        this->setTemp(1, PrivReg);
        this->setTemp(2, ValueReg);
    }

  public:
    const LAllocation *getJSContextReg() {
        return this->getTemp(0)->output();
    }
    const LAllocation *getObjectReg() {
        return this->getOperand(0);
    }
    const LAllocation *getPrivReg() {
        return this->getTemp(1)->output();
    }
    const LAllocation *getValueReg() {
        return this->getTemp(2)->output();
    }
};


class LGetDOMProperty : public LDOMPropertyInstructionHelper<BOX_PIECES, 0>
{
  public:
    LIR_HEADER(GetDOMProperty)

    LGetDOMProperty(const LDefinition &JSContextReg, const LAllocation &ObjectReg,
                    const LDefinition &PrivReg, const LDefinition &ValueReg)
      : LDOMPropertyInstructionHelper<BOX_PIECES, 0>(JSContextReg, ObjectReg,
                                                     PrivReg, ValueReg)
    { }

    MGetDOMProperty *mir() const {
        return mir_->toGetDOMProperty();
    }
};

class LSetDOMProperty : public LDOMPropertyInstructionHelper<0, BOX_PIECES>
{
  public:
    LIR_HEADER(SetDOMProperty)

    LSetDOMProperty(const LDefinition &JSContextReg, const LAllocation &ObjectReg,
                    const LDefinition &PrivReg, const LDefinition &ValueReg)
      : LDOMPropertyInstructionHelper<0, BOX_PIECES>(JSContextReg, ObjectReg,
                                                     PrivReg, ValueReg)
    { }

    static const size_t Value = 1;

    MSetDOMProperty *mir() const {
        return mir_->toSetDOMProperty();
    }
};

// Generates a polymorphic callsite, wherein the function being called is
// unknown and anticipated to vary.
class LApplyArgsGeneric : public LCallInstructionHelper<BOX_PIECES, BOX_PIECES + 2, 2>
{
  public:
    LIR_HEADER(ApplyArgsGeneric)

    LApplyArgsGeneric(const LAllocation &func, const LAllocation &argc,
                      const LDefinition &tmpobjreg, const LDefinition &tmpcopy)
    {
        setOperand(0, func);
        setOperand(1, argc);
        setTemp(0, tmpobjreg);
        setTemp(1, tmpcopy);
    }

    MApplyArgs *mir() const {
        return mir_->toApplyArgs();
    }

    bool hasSingleTarget() const {
        return getSingleTarget() != NULL;
    }
    JSFunction *getSingleTarget() const {
        return mir()->getSingleTarget();
    }

    const LAllocation *getFunction() {
        return getOperand(0);
    }
    const LAllocation *getArgc() {
        return getOperand(1);
    }
    static const size_t ThisIndex = 2;

    const LAllocation *getTempObject() {
        return getTemp(0)->output();
    }
    const LAllocation *getTempCopy() {
        return getTemp(1)->output();
    }
};

class LGetDynamicName : public LCallInstructionHelper<BOX_PIECES, 2, 3>
{
  public:
    LIR_HEADER(GetDynamicName)

    LGetDynamicName(const LAllocation &scopeChain, const LAllocation &name,
                    const LDefinition &temp1, const LDefinition &temp2, const LDefinition &temp3)
    {
        setOperand(0, scopeChain);
        setOperand(1, name);
        setTemp(0, temp1);
        setTemp(1, temp2);
        setTemp(2, temp3);
    }

    MGetDynamicName *mir() const {
        return mir_->toGetDynamicName();
    }

    const LAllocation *getScopeChain() {
        return getOperand(0);
    }
    const LAllocation *getName() {
        return getOperand(1);
    }

    const LDefinition *temp1() {
        return getTemp(0);
    }
    const LDefinition *temp2() {
        return getTemp(1);
    }
    const LDefinition *temp3() {
        return getTemp(2);
    }
};

class LFilterArguments : public LCallInstructionHelper<0, 1, 2>
{
  public:
    LIR_HEADER(FilterArguments)

    LFilterArguments(const LAllocation &string, const LDefinition &temp1, const LDefinition &temp2)
    {
        setOperand(0, string);
        setTemp(0, temp1);
        setTemp(1, temp2);
    }

    MFilterArguments *mir() const {
        return mir_->toFilterArguments();
    }

    const LAllocation *getString() {
        return getOperand(0);
    }
    const LDefinition *temp1() {
        return getTemp(0);
    }
    const LDefinition *temp2() {
        return getTemp(1);
    }
};

class LCallDirectEval : public LCallInstructionHelper<BOX_PIECES, 2 + BOX_PIECES, 0>
{
  public:
    LIR_HEADER(CallDirectEval)

    LCallDirectEval(const LAllocation &scopeChain, const LAllocation &string)
    {
        setOperand(0, scopeChain);
        setOperand(1, string);
    }

    static const size_t ThisValueInput = 2;

    MCallDirectEval *mir() const {
        return mir_->toCallDirectEval();
    }

    const LAllocation *getScopeChain() {
        return getOperand(0);
    }
    const LAllocation *getString() {
        return getOperand(1);
    }
};

// Takes in either an integer or boolean input and tests it for truthiness.
class LTestIAndBranch : public LControlInstructionHelper<2, 1, 0>
{
  public:
    LIR_HEADER(TestIAndBranch)

    LTestIAndBranch(const LAllocation &in, MBasicBlock *ifTrue, MBasicBlock *ifFalse)
    {
        setOperand(0, in);
        setSuccessor(0, ifTrue);
        setSuccessor(1, ifFalse);
    }

    MBasicBlock *ifTrue() const {
        return getSuccessor(0);
    }
    MBasicBlock *ifFalse() const {
        return getSuccessor(1);
    }
};

// Takes in either an integer or boolean input and tests it for truthiness.
class LTestDAndBranch : public LControlInstructionHelper<2, 1, 0>
{
  public:
    LIR_HEADER(TestDAndBranch)

    LTestDAndBranch(const LAllocation &in, MBasicBlock *ifTrue, MBasicBlock *ifFalse)
    {
        setOperand(0, in);
        setSuccessor(0, ifTrue);
        setSuccessor(1, ifFalse);
    }

    MBasicBlock *ifTrue() const {
        return getSuccessor(0);
    }
    MBasicBlock *ifFalse() const {
        return getSuccessor(1);
    }
};

// Takes an object and tests it for truthiness.  An object is falsy iff it
// emulates |undefined|; see js::EmulatesUndefined.
class LTestOAndBranch : public LControlInstructionHelper<2, 1, 1>
{
  public:
    LIR_HEADER(TestOAndBranch)

    LTestOAndBranch(const LAllocation &input, MBasicBlock *ifTruthy, MBasicBlock *ifFalsy,
                    const LDefinition &temp)
    {
        setOperand(0, input);
        setSuccessor(0, ifTruthy);
        setSuccessor(1, ifFalsy);
        setTemp(0, temp);
    }

    const LDefinition *temp() {
        return getTemp(0);
    }

    Label *ifTruthy() {
        return getSuccessor(0)->lir()->label();
    }
    Label *ifFalsy() {
        return getSuccessor(1)->lir()->label();
    }

    MTest *mir() {
        return mir_->toTest();
    }
};

// Takes in a boxed value and tests it for truthiness.
class LTestVAndBranch : public LControlInstructionHelper<2, BOX_PIECES, 3>
{
  public:
    LIR_HEADER(TestVAndBranch)

    LTestVAndBranch(MBasicBlock *ifTruthy, MBasicBlock *ifFalsy, const LDefinition &temp0,
                    const LDefinition &temp1, const LDefinition &temp2)
    {
        setSuccessor(0, ifTruthy);
        setSuccessor(1, ifFalsy);
        setTemp(0, temp0);
        setTemp(1, temp1);
        setTemp(2, temp2);
    }

    const char *extraName() const {
        return mir()->operandMightEmulateUndefined() ? "MightEmulateUndefined" : NULL;
    }

    static const size_t Input = 0;

    const LAllocation *tempFloat() {
        return getTemp(0)->output();
    }

    const LDefinition *temp1() {
        return getTemp(1);
    }

    const LDefinition *temp2() {
        return getTemp(2);
    }

    Label *ifTruthy() {
        return getSuccessor(0)->lir()->label();
    }
    Label *ifFalsy() {
        return getSuccessor(1)->lir()->label();
    }

    MTest *mir() const {
        return mir_->toTest();
    }
};

// Dispatches control flow to a successor based on incoming JSFunction*.
// Used to implemenent polymorphic inlining.
class LFunctionDispatch : public LInstructionHelper<0, 1, 0>
{
    // Dispatch is performed based on a function -> block map
    // stored in the MIR.

  public:
    LIR_HEADER(FunctionDispatch);

    LFunctionDispatch(const LAllocation &in) {
        setOperand(0, in);
    }

    MFunctionDispatch *mir() {
        return mir_->toFunctionDispatch();
    }
};

class LTypeObjectDispatch : public LInstructionHelper<0, 1, 1>
{
    // Dispatch is performed based on a TypeObject -> block
    // map inferred by the MIR.

  public:
    LIR_HEADER(TypeObjectDispatch);

    LTypeObjectDispatch(const LAllocation &in, const LDefinition &temp) {
        setOperand(0, in);
        setTemp(0, temp);
    }

    const LDefinition *temp() {
        return getTemp(0);
    }

    MTypeObjectDispatch *mir() {
        return mir_->toTypeObjectDispatch();
    }
};

class LPolyInlineDispatch : public LInstructionHelper<0, 1, 1>
{
  // Accesses function/block table from MIR instruction.
  public:
    LIR_HEADER(PolyInlineDispatch)

    LPolyInlineDispatch(const LAllocation &in, const LDefinition &temp) {
        setOperand(0, in);
        setTemp(0, temp);
    }
 
    const LDefinition *temp() {
        return getTemp(0);
    }

    MPolyInlineDispatch *mir() {
        return mir_->toPolyInlineDispatch();
    }
};

// Compares two integral values of the same JS type, either integer or object.
// For objects, both operands are in registers.
class LCompare : public LInstructionHelper<1, 2, 0>
{
    JSOp jsop_;

  public:
    LIR_HEADER(Compare)
    LCompare(JSOp jsop, const LAllocation &left, const LAllocation &right)
      : jsop_(jsop)
    {
        setOperand(0, left);
        setOperand(1, right);
    }

    JSOp jsop() const {
        return jsop_;
    }
    const LAllocation *left() {
        return getOperand(0);
    }
    const LAllocation *right() {
        return getOperand(1);
    }
    MCompare *mir() {
        return mir_->toCompare();
    }
};

// Compares two integral values of the same JS type, either integer or object.
// For objects, both operands are in registers.
class LCompareAndBranch : public LControlInstructionHelper<2, 2, 0>
{
    JSOp jsop_;

  public:
    LIR_HEADER(CompareAndBranch)
    LCompareAndBranch(JSOp jsop, const LAllocation &left, const LAllocation &right,
                      MBasicBlock *ifTrue, MBasicBlock *ifFalse)
      : jsop_(jsop)
    {
        setOperand(0, left);
        setOperand(1, right);
        setSuccessor(0, ifTrue);
        setSuccessor(1, ifFalse);
    }

    JSOp jsop() const {
        return jsop_;
    }
    MBasicBlock *ifTrue() const {
        return getSuccessor(0);
    }
    MBasicBlock *ifFalse() const {
        return getSuccessor(1);
    }
    const LAllocation *left() {
        return getOperand(0);
    }
    const LAllocation *right() {
        return getOperand(1);
    }
    MCompare *mir() {
        return mir_->toCompare();
    }
};

class LCompareD : public LInstructionHelper<1, 2, 0>
{
  public:
    LIR_HEADER(CompareD)
    LCompareD(const LAllocation &left, const LAllocation &right) {
        setOperand(0, left);
        setOperand(1, right);
    }

    const LAllocation *left() {
        return getOperand(0);
    }
    const LAllocation *right() {
        return getOperand(1);
    }
    MCompare *mir() {
        return mir_->toCompare();
    }
};

#if defined(JS_CPU_MIPS)
class LCompareDAndBranch : public LControlInstructionHelper<2, 2, 0>
{
    MCompare *cmpMir_;

  public:
    LIR_HEADER(CompareDAndBranch)
    LCompareDAndBranch(MCompare *cmpMir, const LAllocation &left, const LAllocation &right,
                       MBasicBlock *ifTrue, MBasicBlock *ifFalse)
      : cmpMir_(cmpMir)
    {
        setOperand(0, left);
        setOperand(1, right);
        setSuccessor(0, ifTrue);
        setSuccessor(1, ifFalse);
    }

    MBasicBlock *ifTrue() const {
        return getSuccessor(0);
    }
    MBasicBlock *ifFalse() const {
        return getSuccessor(1);
    }
    const LAllocation *left() {
        return getOperand(0);
    }
    const LAllocation *right() {
        return getOperand(1);
    }
    MTest *mir() const {
        return mir_->toTest();
    }
    MCompare *cmpMir() const {
        return cmpMir_;
    }
};
#else  // defined(JS_CPU_MIPS)
class LCompareDAndBranch : public LControlInstructionHelper<2, 2, 0>
{
  public:
    LIR_HEADER(CompareDAndBranch)
    LCompareDAndBranch(const LAllocation &left, const LAllocation &right,
                       MBasicBlock *ifTrue, MBasicBlock *ifFalse)
    {
        setOperand(0, left);
        setOperand(1, right);
        setSuccessor(0, ifTrue);
        setSuccessor(1, ifFalse);
    }

    MBasicBlock *ifTrue() const {
        return getSuccessor(0);
    }
    MBasicBlock *ifFalse() const {
        return getSuccessor(1);
    }
    const LAllocation *left() {
        return getOperand(0);
    }
    const LAllocation *right() {
        return getOperand(1);
    }
    MCompare *mir() {
        return mir_->toCompare();
    }
};
#endif  // defined(JS_CPU_MIPS)

class LCompareS : public LInstructionHelper<1, 2, 1>
{
  public:
    LIR_HEADER(CompareS)
    LCompareS(const LAllocation &left, const LAllocation &right,
              const LDefinition &temp) {
        setOperand(0, left);
        setOperand(1, right);
        setTemp(0, temp);
    }

    const LAllocation *left() {
        return getOperand(0);
    }
    const LAllocation *right() {
        return getOperand(1);
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
    MCompare *mir() {
        return mir_->toCompare();
    }
};

// strict-equality between value and string.
class LCompareStrictS : public LInstructionHelper<1, BOX_PIECES + 1, 2>
{
  public:
    LIR_HEADER(CompareStrictS)
    LCompareStrictS(const LAllocation &rhs, const LDefinition &temp0,
                    const LDefinition &temp1) {
        setOperand(BOX_PIECES, rhs);
        setTemp(0, temp0);
        setTemp(1, temp1);
    }

    static const size_t Lhs = 0;

    const LAllocation *right() {
        return getOperand(BOX_PIECES);
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
    const LDefinition *tempToUnbox() {
        return getTemp(1);
    }
    MCompare *mir() {
        return mir_->toCompare();
    }
};

// Used for strict-equality comparisons where one side is a boolean
// and the other is a value. Note that CompareI is used to compare
// two booleans.
class LCompareB : public LInstructionHelper<1, BOX_PIECES + 1, 0>
{
  public:
    LIR_HEADER(CompareB)

    LCompareB(const LAllocation &rhs) {
        setOperand(BOX_PIECES, rhs);
    }

    static const size_t Lhs = 0;

    const LAllocation *rhs() {
        return getOperand(BOX_PIECES);
    }

    MCompare *mir() {
        return mir_->toCompare();
    }
};

class LCompareBAndBranch : public LControlInstructionHelper<2, BOX_PIECES + 1, 0>
{
  public:
    LIR_HEADER(CompareBAndBranch)

    LCompareBAndBranch(const LAllocation &rhs, MBasicBlock *ifTrue, MBasicBlock *ifFalse)
    {
        setOperand(BOX_PIECES, rhs);
        setSuccessor(0, ifTrue);
        setSuccessor(1, ifFalse);
    }

    static const size_t Lhs = 0;

    const LAllocation *rhs() {
        return getOperand(BOX_PIECES);
    }

    MBasicBlock *ifTrue() const {
        return getSuccessor(0);
    }
    MBasicBlock *ifFalse() const {
        return getSuccessor(1);
    }
    MCompare *mir() {
        return mir_->toCompare();
    }
};

class LCompareV : public LInstructionHelper<1, 2 * BOX_PIECES, 0>
{
  public:
    LIR_HEADER(CompareV)

    static const size_t LhsInput = 0;
    static const size_t RhsInput = BOX_PIECES;

    MCompare *mir() const {
        return mir_->toCompare();
    }
};

class LCompareVAndBranch : public LControlInstructionHelper<2, 2 * BOX_PIECES, 0>
{
  public:
    LIR_HEADER(CompareVAndBranch)

    static const size_t LhsInput = 0;
    static const size_t RhsInput = BOX_PIECES;

    LCompareVAndBranch(MBasicBlock *ifTrue, MBasicBlock *ifFalse)
    {
        setSuccessor(0, ifTrue);
        setSuccessor(1, ifFalse);
    }

    MBasicBlock *ifTrue() const {
        return getSuccessor(0);
    }
    MBasicBlock *ifFalse() const {
        return getSuccessor(1);
    }
    MCompare *mir() {
        return mir_->toCompare();
    }
};

class LCompareVM : public LCallInstructionHelper<1, 2 * BOX_PIECES, 0>
{
  public:
    LIR_HEADER(CompareVM)

    static const size_t LhsInput = 0;
    static const size_t RhsInput = BOX_PIECES;

    MCompare *mir() const {
        return mir_->toCompare();
    }
};

class LIsNullOrLikeUndefined : public LInstructionHelper<1, BOX_PIECES, 2>
{
  public:
    LIR_HEADER(IsNullOrLikeUndefined)

    LIsNullOrLikeUndefined(const LDefinition &temp, const LDefinition &tempToUnbox)
    {
        setTemp(0, temp);
        setTemp(1, tempToUnbox);
    }

    static const size_t Value = 0;

    MCompare *mir() {
        return mir_->toCompare();
    }

    const LDefinition *temp() {
        return getTemp(0);
    }

    const LDefinition *tempToUnbox() {
        return getTemp(1);
    }
};

class LIsNullOrLikeUndefinedAndBranch : public LControlInstructionHelper<2, BOX_PIECES, 2>
{
  public:
    LIR_HEADER(IsNullOrLikeUndefinedAndBranch)

    LIsNullOrLikeUndefinedAndBranch(MBasicBlock *ifTrue, MBasicBlock *ifFalse, const LDefinition &temp, const LDefinition &tempToUnbox)
    {
        setSuccessor(0, ifTrue);
        setSuccessor(1, ifFalse);
        setTemp(0, temp);
        setTemp(1, tempToUnbox);
    }

    static const size_t Value = 0;

    MBasicBlock *ifTrue() const {
        return getSuccessor(0);
    }
    MBasicBlock *ifFalse() const {
        return getSuccessor(1);
    }
    MCompare *mir() {
        return mir_->toCompare();
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
    const LDefinition *tempToUnbox() {
        return getTemp(1);
    }
};

// Takes an object and tests whether it emulates |undefined|, as determined by
// the JSCLASS_EMULATES_UNDEFINED class flag on unwrapped objects.  See also
// js::EmulatesUndefined.
class LEmulatesUndefined : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(EmulatesUndefined)

    LEmulatesUndefined(const LAllocation &input)
    {
        setOperand(0, input);
    }

    MCompare *mir() {
        return mir_->toCompare();
    }
};

class LEmulatesUndefinedAndBranch : public LControlInstructionHelper<2, 1, 1>
{
  public:
    LIR_HEADER(EmulatesUndefinedAndBranch)

    LEmulatesUndefinedAndBranch(const LAllocation &input, MBasicBlock *ifTrue, MBasicBlock *ifFalse, const LDefinition &temp)
    {
        setOperand(0, input);
        setSuccessor(0, ifTrue);
        setSuccessor(1, ifFalse);
        setTemp(0, temp);
    }

    MBasicBlock *ifTrue() const {
        return getSuccessor(0);
    }
    MBasicBlock *ifFalse() const {
        return getSuccessor(1);
    }
    MCompare *mir() {
        return mir_->toCompare();
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
};

// Not operation on an integer.
class LNotI : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(NotI)

    LNotI(const LAllocation &input) {
        setOperand(0, input);
    }
};

// Not operation on a double.
class LNotD : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(NotD)

    LNotD(const LAllocation &input) {
        setOperand(0, input);
    }
};

// Boolean complement operation on an object.
class LNotO : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(NotO)

    LNotO(const LAllocation &input)
    {
        setOperand(0, input);
    }

    MNot *mir() {
        return mir_->toNot();
    }
};

// Boolean complement operation on a value.
class LNotV : public LInstructionHelper<1, BOX_PIECES, 3>
{
  public:
    LIR_HEADER(NotV)

    static const size_t Input = 0;
    LNotV(const LDefinition &temp0, const LDefinition &temp1, const LDefinition &temp2)
    {
        setTemp(0, temp0);
        setTemp(1, temp1);
        setTemp(2, temp2);
    }

    const LAllocation *tempFloat() {
        return getTemp(0)->output();
    }

    const LDefinition *temp1() {
        return getTemp(1);
    }

    const LDefinition *temp2() {
        return getTemp(2);
    }

    MNot *mir() {
        return mir_->toNot();
    }
};

// Bitwise not operation, takes a 32-bit integer as input and returning
// a 32-bit integer result as an output.
class LBitNotI : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(BitNotI)
};

// Call a VM function to perform a BITNOT operation.
class LBitNotV : public LCallInstructionHelper<1, BOX_PIECES, 0>
{
  public:
    LIR_HEADER(BitNotV)

    static const size_t Input = 0;
};

// Binary bitwise operation, taking two 32-bit integers as inputs and returning
// a 32-bit integer result as an output.
class LBitOpI : public LInstructionHelper<1, 2, 0>
{
    JSOp op_;

  public:
    LIR_HEADER(BitOpI)

    LBitOpI(JSOp op)
      : op_(op)
    { }

    const char *extraName() const {
        if (bitop() == JSOP_URSH && mir_->toUrsh()->canOverflow())
            return "UrshCanOverflow";
        return NULL;
    }

    JSOp bitop() const {
        return op_;
    }
};

// Call a VM function to perform a bitwise operation.
class LBitOpV : public LCallInstructionHelper<1, 2 * BOX_PIECES, 0>
{
    JSOp jsop_;

  public:
    LIR_HEADER(BitOpV)

    LBitOpV(JSOp jsop)
      : jsop_(jsop)
    { }

    JSOp jsop() const {
        return jsop_;
    }

    static const size_t LhsInput = 0;
    static const size_t RhsInput = BOX_PIECES;
};

// Shift operation, taking two 32-bit integers as inputs and returning
// a 32-bit integer result as an output.
class LShiftI : public LBinaryMath<0>
{
    JSOp op_;

  public:
    LIR_HEADER(ShiftI)

    LShiftI(JSOp op)
      : op_(op)
    { }

    JSOp bitop() {
        return op_;
    }

    MInstruction *mir() {
        return mir_->toInstruction();
    }
};

class LUrshD : public LBinaryMath<1>
{
  public:
    LIR_HEADER(UrshD)

    LUrshD(const LAllocation &lhs, const LAllocation &rhs, const LDefinition &temp) {
        setOperand(0, lhs);
        setOperand(1, rhs);
        setTemp(0, temp);
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
};

// Returns from the function being compiled (not used in inlined frames). The
// input must be a box.
class LReturn : public LInstructionHelper<0, BOX_PIECES, 0>
{
  public:
    LIR_HEADER(Return)
};

class LThrow : public LCallInstructionHelper<0, BOX_PIECES, 0>
{
  public:
    LIR_HEADER(Throw)

    static const size_t Value = 0;
};

class LMinMaxI : public LInstructionHelper<1, 2, 0>
{
  public:
    LIR_HEADER(MinMaxI)
    LMinMaxI(const LAllocation &first, const LAllocation &second)
    {
        setOperand(0, first);
        setOperand(1, second);
    }

    const LAllocation *first() {
        return this->getOperand(0);
    }
    const LAllocation *second() {
        return this->getOperand(1);
    }
    const LDefinition *output() {
        return this->getDef(0);
    }
    MMinMax *mir() const {
        return mir_->toMinMax();
    }
};

class LMinMaxD : public LInstructionHelper<1, 2, 0>
{
  public:
    LIR_HEADER(MinMaxD)
    LMinMaxD(const LAllocation &first, const LAllocation &second) 
    {
        setOperand(0, first);
        setOperand(1, second);
    }

    const LAllocation *first() {
        return this->getOperand(0);
    }
    const LAllocation *second() {
        return this->getOperand(1);
    }
    const LDefinition *output() {
        return this->getDef(0);
    }
    MMinMax *mir() const {
        return mir_->toMinMax();
    }
};

// Negative of an integer
class LNegI : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(NegI);
    LNegI(const LAllocation &num) {
        setOperand(0, num);
    }
};

// Negative of a double.
class LNegD : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(NegD)
    LNegD(const LAllocation &num) {
        setOperand(0, num);
    }
};

// Absolute value of an integer.
class LAbsI : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(AbsI)
    LAbsI(const LAllocation &num) {
        setOperand(0, num);
    }
};

// Absolute value of a double.
class LAbsD : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(AbsD)
    LAbsD(const LAllocation &num) {
        setOperand(0, num);
    }
};

// Square root of a double.
class LSqrtD : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(SqrtD)
    LSqrtD(const LAllocation &num) {
        setOperand(0, num);
    }
};

class LAtan2D : public LCallInstructionHelper<1, 2, 1>
{
  public:
    LIR_HEADER(Atan2D)
    LAtan2D(const LAllocation &y, const LAllocation &x, const LDefinition &temp) {
        setOperand(0, y);
        setOperand(1, x);
        setTemp(0, temp);
    }

    const LAllocation *y() {
        return getOperand(0);
    }

    const LAllocation *x() {
        return getOperand(1);
    }

    const LDefinition *temp() {
        return getTemp(0);
    }

    const LDefinition *output() {
        return getDef(0);
    }
};

// Double raised to an integer power.
class LPowI : public LCallInstructionHelper<1, 2, 1>
{
  public:
    LIR_HEADER(PowI)
    LPowI(const LAllocation &value, const LAllocation &power, const LDefinition &temp) {
        setOperand(0, value);
        setOperand(1, power);
        setTemp(0, temp);
    }

    const LAllocation *value() {
        return getOperand(0);
    }
    const LAllocation *power() {
        return getOperand(1);
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
};

// Double raised to a double power.
class LPowD : public LCallInstructionHelper<1, 2, 1>
{
  public:
    LIR_HEADER(PowD)
    LPowD(const LAllocation &value, const LAllocation &power, const LDefinition &temp) {
        setOperand(0, value);
        setOperand(1, power);
        setTemp(0, temp);
    }

    const LAllocation *value() {
        return getOperand(0);
    }
    const LAllocation *power() {
        return getOperand(1);
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
};

// Math.random().
class LRandom : public LCallInstructionHelper<1, 0, 2>
{
  public:
    LIR_HEADER(Random)
    LRandom(const LDefinition &temp, const LDefinition &temp2) {
        setTemp(0, temp);
        setTemp(1, temp2);
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
    const LDefinition *temp2() {
        return getTemp(1);
    }
};

class LMathFunctionD : public LCallInstructionHelper<1, 1, 1>
{
  public:
    LIR_HEADER(MathFunctionD)
    LMathFunctionD(const LAllocation &input, const LDefinition &temp) {
        setOperand(0, input);
        setTemp(0, temp);
    }

    const LDefinition *temp() {
        return getTemp(0);
    }
    MMathFunction *mir() const {
        return mir_->toMathFunction();
    }
};

// Adds two integers, returning an integer value.
class LAddI : public LBinaryMath<0>
{
    bool recoversInput_;

  public:
    LIR_HEADER(AddI)

    LAddI()
      : recoversInput_(false)
    { }

    const char *extraName() const {
        return snapshot() ? "OverflowCheck" : NULL;
    }

    virtual bool recoversInput() const {
        return recoversInput_;
    }
    void setRecoversInput() {
        recoversInput_ = true;
    }
};

// Subtracts two integers, returning an integer value.
class LSubI : public LBinaryMath<0>
{
    bool recoversInput_;

  public:
    LIR_HEADER(SubI)

    LSubI()
      : recoversInput_(false)
    { }

    const char *extraName() const {
        return snapshot() ? "OverflowCheck" : NULL;
    }

    virtual bool recoversInput() const {
        return recoversInput_;
    }
    void setRecoversInput() {
        recoversInput_ = true;
    }
};

// Performs an add, sub, mul, or div on two double values.
class LMathD : public LBinaryMath<0>
{
    JSOp jsop_;

  public:
    LIR_HEADER(MathD)

    LMathD(JSOp jsop)
      : jsop_(jsop)
    { }

    JSOp jsop() const {
        return jsop_;
    }
};

class LModD : public LBinaryMath<1>
{
  public:
    LIR_HEADER(ModD)

    LModD(const LAllocation &lhs, const LAllocation &rhs, const LDefinition &temp) {
        setOperand(0, lhs);
        setOperand(1, rhs);
        setTemp(0, temp);
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
    bool isCall() const {
        return true;
    }
};

// Call a VM function to perform a binary operation.
class LBinaryV : public LCallInstructionHelper<BOX_PIECES, 2 * BOX_PIECES, 0>
{
    JSOp jsop_;

  public:
    LIR_HEADER(BinaryV)
    BOX_OUTPUT_ACCESSORS()

    LBinaryV(JSOp jsop)
      : jsop_(jsop)
    { }

    JSOp jsop() const {
        return jsop_;
    }

    static const size_t LhsInput = 0;
    static const size_t RhsInput = BOX_PIECES;
};

// Adds two string, returning a string.
class LConcat : public LInstructionHelper<1, 2, 4>
{
  public:
    LIR_HEADER(Concat)

    LConcat(const LAllocation &lhs, const LAllocation &rhs, const LDefinition &temp1,
            const LDefinition &temp2, const LDefinition &temp3, const LDefinition &temp4) {
        setOperand(0, lhs);
        setOperand(1, rhs);
        setTemp(0, temp1);
        setTemp(1, temp2);
        setTemp(2, temp3);
        setTemp(3, temp4);
    }

    const LAllocation *lhs() {
        return this->getOperand(0);
    }
    const LAllocation *rhs() {
        return this->getOperand(1);
    }
    const LDefinition *temp1() {
        return this->getTemp(0);
    }
    const LDefinition *temp2() {
        return this->getTemp(1);
    }
    const LDefinition *temp3() {
        return this->getTemp(2);
    }
    const LDefinition *temp4() {
        return this->getTemp(3);
    }
};

// Get uint16 character code from a string.
class LCharCodeAt : public LInstructionHelper<1, 2, 0>
{
  public:
    LIR_HEADER(CharCodeAt)

    LCharCodeAt(const LAllocation &str, const LAllocation &index) {
        setOperand(0, str);
        setOperand(1, index);
    }

    const LAllocation *str() {
        return this->getOperand(0);
    }
    const LAllocation *index() {
        return this->getOperand(1);
    }
};

// Convert uint16 character code to a string.
class LFromCharCode : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(FromCharCode)

    LFromCharCode(const LAllocation &code) {
        setOperand(0, code);
    }

    const LAllocation *code() {
        return this->getOperand(0);
    }
};

// Convert a 32-bit integer to a double.
class LInt32ToDouble : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(Int32ToDouble)

    LInt32ToDouble(const LAllocation &input) {
        setOperand(0, input);
    }
};

// Convert a value to a double.
class LValueToDouble : public LInstructionHelper<1, BOX_PIECES, 0>
{
  public:
    LIR_HEADER(ValueToDouble)
    static const size_t Input = 0;

    MToDouble *mir() {
        return mir_->toToDouble();
    }
};

// Convert a value to an int32.
//   Input: components of a Value
//   Output: 32-bit integer
//   Bailout: undefined, string, object, or non-int32 double
//   Temps: one float register
//
// This instruction requires a temporary float register.
class LValueToInt32 : public LInstructionHelper<1, BOX_PIECES, 1>
{
  public:
    enum Mode {
        NORMAL,
        TRUNCATE
    };

  private:
    Mode mode_;

  public:
    LIR_HEADER(ValueToInt32)

    LValueToInt32(const LDefinition &temp, Mode mode)
      : mode_(mode)
    {
        setTemp(0, temp);
    }

    const char *extraName() const {
        return mode() == NORMAL ? "Normal" : "Truncate";
    }

    static const size_t Input = 0;

    Mode mode() const {
        return mode_;
    }
    const LDefinition *tempFloat() {
        return getTemp(0);
    }
    MToInt32 *mir() const {
        return mir_->toToInt32();
    }
};

// Convert a double to an int32.
//   Input: floating-point register
//   Output: 32-bit integer
//   Bailout: if the double cannot be converted to an integer.
class LDoubleToInt32 : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(DoubleToInt32)

    LDoubleToInt32(const LAllocation &in) {
        setOperand(0, in);
    }

    MToInt32 *mir() const {
        return mir_->toToInt32();
    }
};


// Convert a double to a truncated int32.
//   Input: floating-point register
//   Output: 32-bit integer
class LTruncateDToInt32 : public LInstructionHelper<1, 1, 1>
{
  public:
    LIR_HEADER(TruncateDToInt32)

    LTruncateDToInt32(const LAllocation &in, const LDefinition &temp) {
        setOperand(0, in);
        setTemp(0, temp);
    }

    const LDefinition *tempFloat() {
        return getTemp(0);
    }
};

// Convert a any input type hosted on one definition to a string with a function
// call.
class LIntToString : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(IntToString)

    LIntToString(const LAllocation &input) {
        setOperand(0, input);
    }

    const MToString *mir() {
        return mir_->toToString();
    }
};

// No-op instruction that is used to hold the entry snapshot. This simplifies
// register allocation as it doesn't need to sniff the snapshot out of the
// LIRGraph.
class LStart : public LInstructionHelper<0, 0, 0>
{
  public:
    LIR_HEADER(Start)
};

// Passed the StackFrame address in the OsrFrameReg by SideCannon().
// Forwards this object to the LOsrValues for Value materialization.
class LOsrEntry : public LInstructionHelper<1, 0, 0>
{
  protected:
    Label label_;
    uint32_t frameDepth_;

  public:
    LIR_HEADER(OsrEntry)

    LOsrEntry()
      : frameDepth_(0)
    { }

    void setFrameDepth(uint32_t depth) {
        frameDepth_ = depth;
    }
    uint32_t getFrameDepth() {
        return frameDepth_;
    }
    Label *label() {
        return &label_;
    }

};

// Materialize a Value stored in an interpreter frame for OSR.
class LOsrValue : public LInstructionHelper<BOX_PIECES, 1, 0>
{
  public:
    LIR_HEADER(OsrValue)

    LOsrValue(const LAllocation &entry)
    {
        setOperand(0, entry);
    }

    const MOsrValue *mir() {
        return mir_->toOsrValue();
    }
};

// Materialize a JSObject scope chain stored in an interpreter frame for OSR.
class LOsrScopeChain : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(OsrScopeChain)

    LOsrScopeChain(const LAllocation &entry)
    {
        setOperand(0, entry);
    }

    const MOsrScopeChain *mir() {
        return mir_->toOsrScopeChain();
    }
};

class LRegExp : public LCallInstructionHelper<1, 0, 0>
{
  public:
    LIR_HEADER(RegExp)

    const MRegExp *mir() const {
        return mir_->toRegExp();
    }
};

class LRegExpTest : public LCallInstructionHelper<1, 2, 0>
{
  public:
    LIR_HEADER(RegExpTest)

    LRegExpTest(const LAllocation &regexp, const LAllocation &string)
    {
        setOperand(0, regexp);
        setOperand(1, string);
    }

    const LAllocation *regexp() {
        return getOperand(0);
    }
    const LAllocation *string() {
        return getOperand(1);
    }

    const MRegExpTest *mir() const {
        return mir_->toRegExpTest();
    }
};

class LLambdaForSingleton : public LCallInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(LambdaForSingleton)

    LLambdaForSingleton(const LAllocation &scopeChain)
    {
        setOperand(0, scopeChain);
    }
    const LAllocation *scopeChain() {
        return getOperand(0);
    }
    const MLambda *mir() const {
        return mir_->toLambda();
    }
};

class LLambda : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(Lambda)

    LLambda(const LAllocation &scopeChain) {
        setOperand(0, scopeChain);
    }
    const LAllocation *scopeChain() {
        return getOperand(0);
    }
    const MLambda *mir() const {
        return mir_->toLambda();
    }
};

class LParLambda : public LInstructionHelper<1, 2, 2>
{
  public:
    LIR_HEADER(ParLambda);

    LParLambda(const LAllocation &parSlice,
               const LAllocation &scopeChain,
               const LDefinition &temp1,
               const LDefinition &temp2) {
        setOperand(0, parSlice);
        setOperand(1, scopeChain);
        setTemp(0, temp1);
        setTemp(1, temp2);
    }
    const LAllocation *parSlice() {
        return getOperand(0);
    }
    const LAllocation *scopeChain() {
        return getOperand(1);
    }
    const MParLambda *mir() const {
        return mir_->toParLambda();
    }
    const LAllocation *getTemp0() {
        return getTemp(0)->output();
    }
    const LAllocation *getTemp1() {
        return getTemp(1)->output();
    }
};

// Determines the implicit |this| value for function calls.
class LImplicitThis : public LInstructionHelper<BOX_PIECES, 1, 0>
{
  public:
    LIR_HEADER(ImplicitThis)
    BOX_OUTPUT_ACCESSORS()

    LImplicitThis(const LAllocation &callee) {
        setOperand(0, callee);
    }

    const MImplicitThis *mir() const {
        return mir_->toImplicitThis();
    }
    const LAllocation *callee() {
        return getOperand(0);
    }
};

// Load the "slots" member out of a JSObject.
//   Input: JSObject pointer
//   Output: slots pointer
class LSlots : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(Slots)

    LSlots(const LAllocation &object) {
        setOperand(0, object);
    }

    const LAllocation *object() {
        return getOperand(0);
    }
};

// Load the "elements" member out of a JSObject.
//   Input: JSObject pointer
//   Output: elements pointer
class LElements : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(Elements)

    LElements(const LAllocation &object) {
        setOperand(0, object);
    }

    const LAllocation *object() {
        return getOperand(0);
    }
};

// If necessary, convert any int32 elements in a vector into doubles.
class LConvertElementsToDoubles : public LInstructionHelper<0, 1, 0>
{
  public:
    LIR_HEADER(ConvertElementsToDoubles)

    LConvertElementsToDoubles(const LAllocation &elements) {
        setOperand(0, elements);
    }

    const LAllocation *elements() {
        return getOperand(0);
    }
};

// Load a dense array's initialized length from an elements vector.
class LInitializedLength : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(InitializedLength)

    LInitializedLength(const LAllocation &elements) {
        setOperand(0, elements);
    }

    const LAllocation *elements() {
        return getOperand(0);
    }
};

// Set a dense array's initialized length to an elements vector.
class LSetInitializedLength : public LInstructionHelper<0, 2, 0>
{
  public:
    LIR_HEADER(SetInitializedLength)

    LSetInitializedLength(const LAllocation &elements, const LAllocation &index) {
        setOperand(0, elements);
        setOperand(1, index);
    }

    const LAllocation *elements() {
        return getOperand(0);
    }
    const LAllocation *index() {
        return getOperand(1);
    }
};

// Read length field of an object element.
class LArrayLength : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(ArrayLength)

    LArrayLength(const LAllocation &elements) {
        setOperand(0, elements);
    }

    const LAllocation *elements() {
        return getOperand(0);
    }
};

// Read the length of a typed array.
class LTypedArrayLength : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(TypedArrayLength)

    LTypedArrayLength(const LAllocation &obj) {
        setOperand(0, obj);
    }

    const LAllocation *object() {
        return getOperand(0);
    }
};

// Load a typed array's elements vector.
class LTypedArrayElements : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(TypedArrayElements)

    LTypedArrayElements(const LAllocation &object) {
        setOperand(0, object);
    }
    const LAllocation *object() {
        return getOperand(0);
    }
};

// Bailout if index >= length.
class LBoundsCheck : public LInstructionHelper<0, 2, 0>
{
  public:
    LIR_HEADER(BoundsCheck)

    LBoundsCheck(const LAllocation &index, const LAllocation &length) {
        setOperand(0, index);
        setOperand(1, length);
    }
    const MBoundsCheck *mir() const {
        return mir_->toBoundsCheck();
    }
    const LAllocation *index() {
        return getOperand(0);
    }
    const LAllocation *length() {
        return getOperand(1);
    }
};

// Bailout if index + minimum < 0 or index + maximum >= length.
class LBoundsCheckRange : public LInstructionHelper<0, 2, 1>
{
  public:
    LIR_HEADER(BoundsCheckRange)

    LBoundsCheckRange(const LAllocation &index, const LAllocation &length,
                      const LDefinition &temp)
    {
        setOperand(0, index);
        setOperand(1, length);
        setTemp(0, temp);
    }
    const MBoundsCheck *mir() const {
        return mir_->toBoundsCheck();
    }
    const LAllocation *index() {
        return getOperand(0);
    }
    const LAllocation *length() {
        return getOperand(1);
    }
};

// Bailout if index < minimum.
class LBoundsCheckLower : public LInstructionHelper<0, 1, 0>
{
  public:
    LIR_HEADER(BoundsCheckLower)

    LBoundsCheckLower(const LAllocation &index)
    {
        setOperand(0, index);
    }
    MBoundsCheckLower *mir() const {
        return mir_->toBoundsCheckLower();
    }
    const LAllocation *index() {
        return getOperand(0);
    }
};

// Load a value from a dense array's elements vector. Bail out if it's the hole value.
class LLoadElementV : public LInstructionHelper<BOX_PIECES, 2, 0>
{
  public:
    LIR_HEADER(LoadElementV)
    BOX_OUTPUT_ACCESSORS()

    LLoadElementV(const LAllocation &elements, const LAllocation &index) {
        setOperand(0, elements);
        setOperand(1, index);
    }

    const char *extraName() const {
        return mir()->needsHoleCheck() ? "HoleCheck" : NULL;
    }

    const MLoadElement *mir() const {
        return mir_->toLoadElement();
    }
    const LAllocation *elements() {
        return getOperand(0);
    }
    const LAllocation *index() {
        return getOperand(1);
    }
};

class LInArray : public LInstructionHelper<1, 4, 0>
{
  public:
    LIR_HEADER(InArray)

    LInArray(const LAllocation &elements, const LAllocation &index,
             const LAllocation &initLength, const LAllocation &object)
    {
        setOperand(0, elements);
        setOperand(1, index);
        setOperand(2, initLength);
        setOperand(3, object);
    }
    const MInArray *mir() const {
        return mir_->toInArray();
    }
    const LAllocation *elements() {
        return getOperand(0);
    }
    const LAllocation *index() {
        return getOperand(1);
    }
    const LAllocation *initLength() {
        return getOperand(2);
    }
    const LAllocation *object() {
        return getOperand(3);
    }
};


// Load a value from a dense array's elements vector. Bail out if it's the hole value.
class LLoadElementHole : public LInstructionHelper<BOX_PIECES, 3, 0>
{
  public:
    LIR_HEADER(LoadElementHole)
    BOX_OUTPUT_ACCESSORS()

    LLoadElementHole(const LAllocation &elements, const LAllocation &index, const LAllocation &initLength) {
        setOperand(0, elements);
        setOperand(1, index);
        setOperand(2, initLength);
    }

    const char *extraName() const {
        return mir()->needsHoleCheck() ? "HoleCheck" : NULL;
    }

    const MLoadElementHole *mir() const {
        return mir_->toLoadElementHole();
    }
    const LAllocation *elements() {
        return getOperand(0);
    }
    const LAllocation *index() {
        return getOperand(1);
    }
    const LAllocation *initLength() {
        return getOperand(2);
    }
};

// Load a typed value from a dense array's elements vector. The array must be
// known to be packed, so that we don't have to check for the hole value.
// This instruction does not load the type tag and can directly load into a
// FP register.
class LLoadElementT : public LInstructionHelper<1, 2, 0>
{
  public:
    LIR_HEADER(LoadElementT)

    LLoadElementT(const LAllocation &elements, const LAllocation &index) {
        setOperand(0, elements);
        setOperand(1, index);
    }

    const char *extraName() const {
        return mir()->needsHoleCheck() ? "HoleCheck" : (mir()->loadDoubles() ? "Doubles" : NULL);
    }

    const MLoadElement *mir() const {
        return mir_->toLoadElement();
    }
    const LAllocation *elements() {
        return getOperand(0);
    }
    const LAllocation *index() {
        return getOperand(1);
    }
};

// Store a boxed value to a dense array's element vector.
class LStoreElementV : public LInstructionHelper<0, 2 + BOX_PIECES, 0>
{
  public:
    LIR_HEADER(StoreElementV)

    LStoreElementV(const LAllocation &elements, const LAllocation &index) {
        setOperand(0, elements);
        setOperand(1, index);
    }

    const char *extraName() const {
        return mir()->needsHoleCheck() ? "HoleCheck" : NULL;
    }

    static const size_t Value = 2;

    const MStoreElement *mir() const {
        return mir_->toStoreElement();
    }
    const LAllocation *elements() {
        return getOperand(0);
    }
    const LAllocation *index() {
        return getOperand(1);
    }
};

// Store a typed value to a dense array's elements vector. Compared to
// LStoreElementV, this instruction can store doubles and constants directly,
// and does not store the type tag if the array is monomorphic and known to
// be packed.
class LStoreElementT : public LInstructionHelper<0, 3, 0>
{
  public:
    LIR_HEADER(StoreElementT)

    LStoreElementT(const LAllocation &elements, const LAllocation &index, const LAllocation &value) {
        setOperand(0, elements);
        setOperand(1, index);
        setOperand(2, value);
    }

    const char *extraName() const {
        return mir()->needsHoleCheck() ? "HoleCheck" : NULL;
    }

    const MStoreElement *mir() const {
        return mir_->toStoreElement();
    }
    const LAllocation *elements() {
        return getOperand(0);
    }
    const LAllocation *index() {
        return getOperand(1);
    }
    const LAllocation *value() {
        return getOperand(2);
    }
};

// Like LStoreElementV, but supports indexes >= initialized length.
class LStoreElementHoleV : public LInstructionHelper<0, 3 + BOX_PIECES, 0>
{
  public:
    LIR_HEADER(StoreElementHoleV)

    LStoreElementHoleV(const LAllocation &object, const LAllocation &elements,
                       const LAllocation &index) {
        setOperand(0, object);
        setOperand(1, elements);
        setOperand(2, index);
    }

    static const size_t Value = 3;

    const MStoreElementHole *mir() const {
        return mir_->toStoreElementHole();
    }
    const LAllocation *object() {
        return getOperand(0);
    }
    const LAllocation *elements() {
        return getOperand(1);
    }
    const LAllocation *index() {
        return getOperand(2);
    }
};

// Like LStoreElementT, but supports indexes >= initialized length.
class LStoreElementHoleT : public LInstructionHelper<0, 4, 0>
{
  public:
    LIR_HEADER(StoreElementHoleT)

    LStoreElementHoleT(const LAllocation &object, const LAllocation &elements,
                       const LAllocation &index, const LAllocation &value) {
        setOperand(0, object);
        setOperand(1, elements);
        setOperand(2, index);
        setOperand(3, value);
    }

    const MStoreElementHole *mir() const {
        return mir_->toStoreElementHole();
    }
    const LAllocation *object() {
        return getOperand(0);
    }
    const LAllocation *elements() {
        return getOperand(1);
    }
    const LAllocation *index() {
        return getOperand(2);
    }
    const LAllocation *value() {
        return getOperand(3);
    }
};

class LArrayPopShiftV : public LInstructionHelper<BOX_PIECES, 1, 2>
{
  public:
    LIR_HEADER(ArrayPopShiftV)

    LArrayPopShiftV(const LAllocation &object, const LDefinition &temp0, const LDefinition &temp1) {
        setOperand(0, object);
        setTemp(0, temp0);
        setTemp(1, temp1);
    }

    const char *extraName() const {
        return mir()->mode() == MArrayPopShift::Pop ? "Pop" : "Shift";
    }

    const MArrayPopShift *mir() const {
        return mir_->toArrayPopShift();
    }
    const LAllocation *object() {
        return getOperand(0);
    }
    const LDefinition *temp0() {
        return getTemp(0);
    }
    const LDefinition *temp1() {
        return getTemp(1);
    }
};

class LArrayPopShiftT : public LInstructionHelper<1, 1, 2>
{
  public:
    LIR_HEADER(ArrayPopShiftT)

    LArrayPopShiftT(const LAllocation &object, const LDefinition &temp0, const LDefinition &temp1) {
        setOperand(0, object);
        setTemp(0, temp0);
        setTemp(1, temp1);
    }

    const char *extraName() const {
        return mir()->mode() == MArrayPopShift::Pop ? "Pop" : "Shift";
    }

    const MArrayPopShift *mir() const {
        return mir_->toArrayPopShift();
    }
    const LAllocation *object() {
        return getOperand(0);
    }
    const LDefinition *temp0() {
        return getTemp(0);
    }
    const LDefinition *temp1() {
        return getTemp(1);
    }
};

class LArrayPushV : public LInstructionHelper<1, 1 + BOX_PIECES, 1>
{
  public:
    LIR_HEADER(ArrayPushV)

    LArrayPushV(const LAllocation &object, const LDefinition &temp) {
        setOperand(0, object);
        setTemp(0, temp);
    }

    static const size_t Value = 1;

    const MArrayPush *mir() const {
        return mir_->toArrayPush();
    }
    const LAllocation *object() {
        return getOperand(0);
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
};

class LArrayPushT : public LInstructionHelper<1, 2, 1>
{
  public:
    LIR_HEADER(ArrayPushT)

    LArrayPushT(const LAllocation &object, const LAllocation &value, const LDefinition &temp) {
        setOperand(0, object);
        setOperand(1, value);
        setTemp(0, temp);
    }

    const MArrayPush *mir() const {
        return mir_->toArrayPush();
    }
    const LAllocation *object() {
        return getOperand(0);
    }
    const LAllocation *value() {
        return getOperand(1);
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
};

class LArrayConcat : public LCallInstructionHelper<1, 2, 2>
{
  public:
    LIR_HEADER(ArrayConcat)

    LArrayConcat(const LAllocation &lhs, const LAllocation &rhs,
                 const LDefinition &temp1, const LDefinition &temp2) {
        setOperand(0, lhs);
        setOperand(1, rhs);
        setTemp(0, temp1);
        setTemp(1, temp2);
    }
    const MArrayConcat *mir() const {
        return mir_->toArrayConcat();
    }
    const LAllocation *lhs() {
        return getOperand(0);
    }
    const LAllocation *rhs() {
        return getOperand(1);
    }
    const LDefinition *temp1() {
        return getTemp(0);
    }
    const LDefinition *temp2() {
        return getTemp(1);
    }
};

// Load a typed value from a typed array's elements vector.
class LLoadTypedArrayElement : public LInstructionHelper<1, 2, 1>
{
  public:
    LIR_HEADER(LoadTypedArrayElement)

    LLoadTypedArrayElement(const LAllocation &elements, const LAllocation &index,
                           const LDefinition &temp) {
        setOperand(0, elements);
        setOperand(1, index);
        setTemp(0, temp);
    }
    const MLoadTypedArrayElement *mir() const {
        return mir_->toLoadTypedArrayElement();
    }
    const LAllocation *elements() {
        return getOperand(0);
    }
    const LAllocation *index() {
        return getOperand(1);
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
};

class LLoadTypedArrayElementHole : public LInstructionHelper<BOX_PIECES, 2, 0>
{
  public:
    LIR_HEADER(LoadTypedArrayElementHole)
    BOX_OUTPUT_ACCESSORS()

    LLoadTypedArrayElementHole(const LAllocation &object, const LAllocation &index) {
        setOperand(0, object);
        setOperand(1, index);
    }
    const MLoadTypedArrayElementHole *mir() const {
        return mir_->toLoadTypedArrayElementHole();
    }
    const LAllocation *object() {
        return getOperand(0);
    }
    const LAllocation *index() {
        return getOperand(1);
    }
};

class LLoadTypedArrayElementStatic : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(LoadTypedArrayElementStatic);
    LLoadTypedArrayElementStatic(const LAllocation &ptr) {
        setOperand(0, ptr);
    }
    MLoadTypedArrayElementStatic *mir() const {
        return mir_->toLoadTypedArrayElementStatic();
    }
    const LAllocation *ptr() {
        return getOperand(0);
    }
};

class LStoreTypedArrayElement : public LInstructionHelper<0, 3, 0>
{
  public:
    LIR_HEADER(StoreTypedArrayElement)

    LStoreTypedArrayElement(const LAllocation &elements, const LAllocation &index,
                            const LAllocation &value) {
        setOperand(0, elements);
        setOperand(1, index);
        setOperand(2, value);
    }

    const MStoreTypedArrayElement *mir() const {
        return mir_->toStoreTypedArrayElement();
    }
    const LAllocation *elements() {
        return getOperand(0);
    }
    const LAllocation *index() {
        return getOperand(1);
    }
    const LAllocation *value() {
        return getOperand(2);
    }
};

class LStoreTypedArrayElementHole : public LInstructionHelper<0, 4, 0>
{
  public:
    LIR_HEADER(StoreTypedArrayElementHole)

    LStoreTypedArrayElementHole(const LAllocation &elements, const LAllocation &length,
                                const LAllocation &index, const LAllocation &value)
    {
        setOperand(0, elements);
        setOperand(1, length);
        setOperand(2, index);
        setOperand(3, value);
    }

    const MStoreTypedArrayElementHole *mir() const {
        return mir_->toStoreTypedArrayElementHole();
    }
    const LAllocation *elements() {
        return getOperand(0);
    }
    const LAllocation *length() {
        return getOperand(1);
    }
    const LAllocation *index() {
        return getOperand(2);
    }
    const LAllocation *value() {
        return getOperand(3);
    }
};

class LStoreTypedArrayElementStatic : public LInstructionHelper<0, 2, 0>
{
  public:
    LIR_HEADER(StoreTypedArrayElementStatic);
    LStoreTypedArrayElementStatic(const LAllocation &ptr, const LAllocation &value) {
        setOperand(0, ptr);
        setOperand(1, value);
    }
    MStoreTypedArrayElementStatic *mir() const {
        return mir_->toStoreTypedArrayElementStatic();
    }
    const LAllocation *ptr() {
        return getOperand(0);
    }
    const LAllocation *value() {
        return getOperand(1);
    }
};

class LEffectiveAddress : public LInstructionHelper<1, 2, 0>
{
  public:
    LIR_HEADER(EffectiveAddress);

    LEffectiveAddress(const LAllocation &base, const LAllocation &index) {
        setOperand(0, base);
        setOperand(1, index);
    }
    const MEffectiveAddress *mir() const {
        return mir_->toEffectiveAddress();
    }
    const LAllocation *base() {
        return getOperand(0);
    }
    const LAllocation *index() {
        return getOperand(1);
    }
};

class LClampIToUint8 : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(ClampIToUint8)

    LClampIToUint8(const LAllocation &in) {
        setOperand(0, in);
    }
};

class LClampDToUint8 : public LInstructionHelper<1, 1, 1>
{
  public:
    LIR_HEADER(ClampDToUint8)

    LClampDToUint8(const LAllocation &in, const LDefinition &temp) {
        setOperand(0, in);
        setTemp(0, temp);
    }
};

class LClampVToUint8 : public LInstructionHelper<1, BOX_PIECES, 1>
{
  public:
    LIR_HEADER(ClampVToUint8)

    LClampVToUint8(const LDefinition &tempFloat) {
        setTemp(0, tempFloat);
    }

    static const size_t Input = 0;

    const LDefinition *tempFloat() {
        return getTemp(0);
    }
};

// Load a boxed value from an object's fixed slot.
class LLoadFixedSlotV : public LInstructionHelper<BOX_PIECES, 1, 0>
{
  public:
    LIR_HEADER(LoadFixedSlotV)
    BOX_OUTPUT_ACCESSORS()

    LLoadFixedSlotV(const LAllocation &object) {
        setOperand(0, object);
    }
    const MLoadFixedSlot *mir() const {
        return mir_->toLoadFixedSlot();
    }
};

// Load a typed value from an object's fixed slot.
class LLoadFixedSlotT : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(LoadFixedSlotT)

    LLoadFixedSlotT(const LAllocation &object) {
        setOperand(0, object);
    }
    const MLoadFixedSlot *mir() const {
        return mir_->toLoadFixedSlot();
    }
};

// Store a boxed value to an object's fixed slot.
class LStoreFixedSlotV : public LInstructionHelper<0, 1 + BOX_PIECES, 0>
{
  public:
    LIR_HEADER(StoreFixedSlotV)

    LStoreFixedSlotV(const LAllocation &obj) {
        setOperand(0, obj);
    }

    static const size_t Value = 1;

    const MStoreFixedSlot *mir() const {
        return mir_->toStoreFixedSlot();
    }
    const LAllocation *obj() {
        return getOperand(0);
    }
};

// Store a typed value to an object's fixed slot.
class LStoreFixedSlotT : public LInstructionHelper<0, 2, 0>
{
  public:
    LIR_HEADER(StoreFixedSlotT)

    LStoreFixedSlotT(const LAllocation &obj, const LAllocation &value)
    {
        setOperand(0, obj);
        setOperand(1, value);
    }
    const MStoreFixedSlot *mir() const {
        return mir_->toStoreFixedSlot();
    }
    const LAllocation *obj() {
        return getOperand(0);
    }
    const LAllocation *value() {
        return getOperand(1);
    }
};

// Note, Name ICs always return a Value. There are no V/T variants.
class LGetNameCache : public LInstructionHelper<BOX_PIECES, 1, 0>
{
  public:
    LIR_HEADER(GetNameCache)
    BOX_OUTPUT_ACCESSORS()

    LGetNameCache(const LAllocation &scopeObj) {
        setOperand(0, scopeObj);
    }
    const LAllocation *scopeObj() {
        return getOperand(0);
    }
    const MGetNameCache *mir() const {
        return mir_->toGetNameCache();
    }
};

class LCallGetIntrinsicValue : public LCallInstructionHelper<BOX_PIECES, 0, 0>
{
  public:
    LIR_HEADER(CallGetIntrinsicValue)
    BOX_OUTPUT_ACCESSORS()

    const MCallGetIntrinsicValue *mir() const {
        return mir_->toCallGetIntrinsicValue();
    }
};

class LCallsiteCloneCache : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(CallsiteCloneCache);

    LCallsiteCloneCache(const LAllocation &callee) {
        setOperand(0, callee);
    }
    const LAllocation *callee() {
        return getOperand(0);
    }
    const MCallsiteCloneCache *mir() const {
        return mir_->toCallsiteCloneCache();
    }
};

// Patchable jump to stubs generated for a GetProperty cache, which loads a
// boxed value.
class LGetPropertyCacheV : public LInstructionHelper<BOX_PIECES, 1, 0>
{
  public:
    LIR_HEADER(GetPropertyCacheV)
    BOX_OUTPUT_ACCESSORS()

    LGetPropertyCacheV(const LAllocation &object) {
        setOperand(0, object);
    }
    const MGetPropertyCache *mir() const {
        return mir_->toGetPropertyCache();
    }
};

// Patchable jump to stubs generated for a GetProperty cache, which loads a
// value of a known type, possibly into an FP register.
class LGetPropertyCacheT : public LInstructionHelper<1, 1, 1>
{
  public:
    LIR_HEADER(GetPropertyCacheT)

    LGetPropertyCacheT(const LAllocation &object, const LDefinition &temp) {
        setOperand(0, object);
        setTemp(0, temp);
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
    const MGetPropertyCache *mir() const {
        return mir_->toGetPropertyCache();
    }
};

// Emit code to load a boxed value from an object's slots if its shape matches
// one of the shapes observed by the baseline IC, else bails out.
class LGetPropertyPolymorphicV : public LInstructionHelper<BOX_PIECES, 1, 0>
{
  public:
    LIR_HEADER(GetPropertyPolymorphicV)
    BOX_OUTPUT_ACCESSORS()

    LGetPropertyPolymorphicV(const LAllocation &obj) {
        setOperand(0, obj);
    }
    const LAllocation *obj() {
        return getOperand(0);
    }
    const MGetPropertyPolymorphic *mir() const {
        return mir_->toGetPropertyPolymorphic();
    }
};

// Emit code to load a typed value from an object's slots if its shape matches
// one of the shapes observed by the baseline IC, else bails out.
class LGetPropertyPolymorphicT : public LInstructionHelper<1, 1, 1>
{
  public:
    LIR_HEADER(GetPropertyPolymorphicT)

    LGetPropertyPolymorphicT(const LAllocation &obj, const LDefinition &temp) {
        setOperand(0, obj);
        setTemp(0, temp);
    }
    const LAllocation *obj() {
        return getOperand(0);
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
    const MGetPropertyPolymorphic *mir() const {
        return mir_->toGetPropertyPolymorphic();
    }
};

// Emit code to store a boxed value to an object's slots if its shape matches
// one of the shapes observed by the baseline IC, else bails out.
class LSetPropertyPolymorphicV : public LInstructionHelper<0, 1 + BOX_PIECES, 1>
{
  public:
    LIR_HEADER(SetPropertyPolymorphicV)

    LSetPropertyPolymorphicV(const LAllocation &obj, const LDefinition &temp) {
        setOperand(0, obj);
        setTemp(0, temp);
    }

    static const size_t Value = 1;

    const LAllocation *obj() {
        return getOperand(0);
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
    const MSetPropertyPolymorphic *mir() const {
        return mir_->toSetPropertyPolymorphic();
    }
};

// Emit code to store a typed value to an object's slots if its shape matches
// one of the shapes observed by the baseline IC, else bails out.
class LSetPropertyPolymorphicT : public LInstructionHelper<0, 2, 1>
{
    MIRType valueType_;

  public:
    LIR_HEADER(SetPropertyPolymorphicT)

    LSetPropertyPolymorphicT(const LAllocation &obj, const LAllocation &value, MIRType valueType,
                             const LDefinition &temp)
      : valueType_(valueType)
    {
        setOperand(0, obj);
        setOperand(1, value);
        setTemp(0, temp);
    }

    const LAllocation *obj() {
        return getOperand(0);
    }
    const LAllocation *value() {
        return getOperand(1);
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
    MIRType valueType() const {
        return valueType_;
    }
    const MSetPropertyPolymorphic *mir() const {
        return mir_->toSetPropertyPolymorphic();
    }
};

class LGetElementCacheV : public LInstructionHelper<BOX_PIECES, 1 + BOX_PIECES, 0>
{
  public:
    LIR_HEADER(GetElementCacheV)
    BOX_OUTPUT_ACCESSORS()

    static const size_t Index = 1;

    LGetElementCacheV(const LAllocation &object) {
        setOperand(0, object);
    }
    const LAllocation *object() {
        return getOperand(0);
    }
    const MGetElementCache *mir() const {
        return mir_->toGetElementCache();
    }
};

class LGetElementCacheT : public LInstructionHelper<1, 2, 0>
{
  public:
    LIR_HEADER(GetElementCacheT)

    LGetElementCacheT(const LAllocation &object, const LAllocation &index) {
        setOperand(0, object);
        setOperand(1, index);
    }
    const LAllocation *object() {
        return getOperand(0);
    }
    const LAllocation *index() {
        return getOperand(1);
    }
    const LDefinition *output() {
        return getDef(0);
    }
    const MGetElementCache *mir() const {
        return mir_->toGetElementCache();
    }
};

class LBindNameCache : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(BindNameCache)

    LBindNameCache(const LAllocation &scopeChain) {
        setOperand(0, scopeChain);
    }
    const LAllocation *scopeChain() {
        return getOperand(0);
    }
    const MBindNameCache *mir() const {
        return mir_->toBindNameCache();
    }
};

// Load a value from an object's dslots or a slots vector.
class LLoadSlotV : public LInstructionHelper<BOX_PIECES, 1, 0>
{
  public:
    LIR_HEADER(LoadSlotV)
    BOX_OUTPUT_ACCESSORS()

    LLoadSlotV(const LAllocation &in) {
        setOperand(0, in);
    }
    const MLoadSlot *mir() const {
        return mir_->toLoadSlot();
    }
};

// Load a typed value from an object's dslots or a slots vector. Unlike
// LLoadSlotV, this can bypass extracting a type tag, directly retrieving a
// pointer, integer, or double.
class LLoadSlotT : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(LoadSlotT)

    LLoadSlotT(const LAllocation &in) {
        setOperand(0, in);
    }
    const MLoadSlot *mir() const {
        return mir_->toLoadSlot();
    }
};

// Store a value to an object's dslots or a slots vector.
class LStoreSlotV : public LInstructionHelper<0, 1 + BOX_PIECES, 0>
{
  public:
    LIR_HEADER(StoreSlotV)

    LStoreSlotV(const LAllocation &slots) {
        setOperand(0, slots);
    }

    static const size_t Value = 1;

    const MStoreSlot *mir() const {
        return mir_->toStoreSlot();
    }
    const LAllocation *slots() {
        return getOperand(0);
    }
};

// Store a typed value to an object's dslots or a slots vector. This has a
// few advantages over LStoreSlotV:
// 1) We can bypass storing the type tag if the slot has the same type as
//    the value.
// 2) Better register allocation: we can store constants and FP regs directly
//    without requiring a second register for the value.
class LStoreSlotT : public LInstructionHelper<0, 2, 0>
{
  public:
    LIR_HEADER(StoreSlotT)

    LStoreSlotT(const LAllocation &slots, const LAllocation &value) {
        setOperand(0, slots);
        setOperand(1, value);
    }
    const MStoreSlot *mir() const {
        return mir_->toStoreSlot();
    }
    const LAllocation *slots() {
        return getOperand(0);
    }
    const LAllocation *value() {
        return getOperand(1);
    }
};

// Read length field of a JSString*.
class LStringLength : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(StringLength)

    LStringLength(const LAllocation &string) {
        setOperand(0, string);
    }

    const LAllocation *string() {
        return getOperand(0);
    }
};

// Take the floor of a number. Implements Math.floor().
class LFloor : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(Floor)

    LFloor(const LAllocation &num) {
        setOperand(0, num);
    }

    MRound *mir() const {
        return mir_->toRound();
    }
};

// Round a number. Implements Math.round().
class LRound : public LInstructionHelper<1, 1, 1>
{
  public:
    LIR_HEADER(Round)

    LRound(const LAllocation &num, const LDefinition &temp) {
        setOperand(0, num);
        setTemp(0, temp);
    }

    const LDefinition *temp() {
        return getTemp(0);
    }
    MRound *mir() const {
        return mir_->toRound();
    }
};

// Load a function's call environment.
class LFunctionEnvironment : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(FunctionEnvironment)

    LFunctionEnvironment(const LAllocation &function) {
        setOperand(0, function);
    }
    const LAllocation *function() {
        return getOperand(0);
    }
};

class LParSlice : public LCallInstructionHelper<1, 0, 1>
{
  public:
    LIR_HEADER(ParSlice);

    LParSlice(const LDefinition &temp1) {
        setTemp(0, temp1);
    }

    const LAllocation *getTempReg() {
        return getTemp(0)->output();
    }
};

class LCallGetProperty : public LCallInstructionHelper<BOX_PIECES, BOX_PIECES, 0>
{
  public:
    LIR_HEADER(CallGetProperty)

    static const size_t Value = 0;

    MCallGetProperty *mir() const {
        return mir_->toCallGetProperty();
    }
};

// Call js::GetElement.
class LCallGetElement : public LCallInstructionHelper<BOX_PIECES, 2 * BOX_PIECES, 0>
{
  public:
    LIR_HEADER(CallGetElement)
    BOX_OUTPUT_ACCESSORS()

    static const size_t LhsInput = 0;
    static const size_t RhsInput = BOX_PIECES;

    MCallGetElement *mir() const {
        return mir_->toCallGetElement();
    }
};

// Call js::SetElement.
class LCallSetElement : public LCallInstructionHelper<0, 1 + 2 * BOX_PIECES, 0>
{
  public:
    LIR_HEADER(CallSetElement)
    BOX_OUTPUT_ACCESSORS()

    static const size_t Index = 1;
    static const size_t Value = 1 + BOX_PIECES;
};

// Call js::InitElementArray.
class LCallInitElementArray : public LCallInstructionHelper<0, 1 + BOX_PIECES, 0>
{
public:
    LIR_HEADER(CallInitElementArray)

    static const size_t Value = 1;

    const MCallInitElementArray *mir() const {
        return mir_->toCallInitElementArray();
    }
};

// Call a VM function to perform a property or name assignment of a generic value.
class LCallSetProperty : public LCallInstructionHelper<0, 1 + BOX_PIECES, 0>
{
  public:
    LIR_HEADER(CallSetProperty)

    LCallSetProperty(const LAllocation &obj) {
        setOperand(0, obj);
    }

    static const size_t Value = 1;

    const MCallSetProperty *mir() const {
        return mir_->toCallSetProperty();
    }
};

class LCallDeleteProperty : public LCallInstructionHelper<1, BOX_PIECES, 0>
{
  public:
    LIR_HEADER(CallDeleteProperty)

    static const size_t Value = 0;

    MDeleteProperty *mir() const {
        return mir_->toDeleteProperty();
    }
};

// Patchable jump to stubs generated for a SetProperty cache, which stores a
// boxed value.
class LSetPropertyCacheV : public LInstructionHelper<0, 1 + BOX_PIECES, 1>
{
  public:
    LIR_HEADER(SetPropertyCacheV)

    LSetPropertyCacheV(const LAllocation &object, const LDefinition &slots) {
        setOperand(0, object);
        setTemp(0, slots);
    }

    static const size_t Value = 1;

    const MSetPropertyCache *mir() const {
        return mir_->toSetPropertyCache();
    }
};

// Patchable jump to stubs generated for a SetProperty cache, which stores a
// value of a known type.
class LSetPropertyCacheT : public LInstructionHelper<0, 2, 1>
{
    MIRType valueType_;

  public:
    LIR_HEADER(SetPropertyCacheT)

    LSetPropertyCacheT(const LAllocation &object, const LDefinition &slots,
                       const LAllocation &value, MIRType valueType)
        : valueType_(valueType)
    {
        setOperand(0, object);
        setOperand(1, value);
        setTemp(0, slots);
    }

    const MSetPropertyCache *mir() const {
        return mir_->toSetPropertyCache();
    }
    MIRType valueType() {
        return valueType_;
    }
};

class LSetElementCacheV : public LInstructionHelper<0, 1 + 2 * BOX_PIECES, 2>
{
  public:
    LIR_HEADER(SetElementCacheV);

    static const size_t Index = 1;
    static const size_t Value = 1 + BOX_PIECES;

    LSetElementCacheV(const LAllocation &object, const LDefinition &tempToUnboxIndex,
                      const LDefinition &temp)
    {
        setOperand(0, object);
        setTemp(0, tempToUnboxIndex);
        setTemp(1, temp);
    }
    const MSetElementCache *mir() const {
        return mir_->toSetElementCache();
    }

    const LAllocation *object() {
        return getOperand(0);
    }
    const LDefinition *tempToUnboxIndex() {
        return getTemp(0);
    }
    const LDefinition *temp() {
        return getTemp(1);
    }
};

class LSetElementCacheT : public LInstructionHelper<0, 2 + BOX_PIECES, 2>
{
  public:
    LIR_HEADER(SetElementCacheT);

    static const size_t Index = 2;

    LSetElementCacheT(const LAllocation &object, const LAllocation &value,
                      const LDefinition &tempToUnboxIndex, const LDefinition &temp) {
        setOperand(0, object);
        setOperand(1, value);
        setTemp(0, tempToUnboxIndex);
        setTemp(1, temp);
    }
    const MSetElementCache *mir() const {
        return mir_->toSetElementCache();
    }

    const LAllocation *object() {
        return getOperand(0);
    }
    const LAllocation *value() {
        return getOperand(1);
    }
    const LDefinition *tempToUnboxIndex() {
        return getTemp(0);
    }
    const LDefinition *temp() {
        return getTemp(1);
    }
};

class LCallIteratorStart : public LCallInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(CallIteratorStart)

    LCallIteratorStart(const LAllocation &object) {
        setOperand(0, object);
    }
    const LAllocation *object() {
        return getOperand(0);
    }
    MIteratorStart *mir() const {
        return mir_->toIteratorStart();
    }
};

class LIteratorStart : public LInstructionHelper<1, 1, 3>
{
  public:
    LIR_HEADER(IteratorStart)

    LIteratorStart(const LAllocation &object, const LDefinition &temp1,
                   const LDefinition &temp2, const LDefinition &temp3) {
        setOperand(0, object);
        setTemp(0, temp1);
        setTemp(1, temp2);
        setTemp(2, temp3);
    }
    const LAllocation *object() {
        return getOperand(0);
    }
    const LDefinition *temp1() {
        return getTemp(0);
    }
    const LDefinition *temp2() {
        return getTemp(1);
    }
    const LDefinition *temp3() {
        return getTemp(2);
    }
    MIteratorStart *mir() const {
        return mir_->toIteratorStart();
    }
};

class LIteratorNext : public LInstructionHelper<BOX_PIECES, 1, 1>
{
  public:
    LIR_HEADER(IteratorNext)
    BOX_OUTPUT_ACCESSORS()

    LIteratorNext(const LAllocation &iterator, const LDefinition &temp) {
        setOperand(0, iterator);
        setTemp(0, temp);
    }
    const LAllocation *object() {
        return getOperand(0);
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
    MIteratorNext *mir() const {
        return mir_->toIteratorNext();
    }
};

class LIteratorMore : public LInstructionHelper<1, 1, 1>
{
  public:
    LIR_HEADER(IteratorMore)

    LIteratorMore(const LAllocation &iterator, const LDefinition &temp) {
        setOperand(0, iterator);
        setTemp(0, temp);
    }
    const LAllocation *object() {
        return getOperand(0);
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
    MIteratorMore *mir() const {
        return mir_->toIteratorMore();
    }
};

class LIteratorEnd : public LInstructionHelper<0, 1, 3>
{
  public:
    LIR_HEADER(IteratorEnd)

    LIteratorEnd(const LAllocation &iterator, const LDefinition &temp1,
                 const LDefinition &temp2, const LDefinition &temp3) {
        setOperand(0, iterator);
        setTemp(0, temp1);
        setTemp(1, temp2);
        setTemp(2, temp3);
    }
    const LAllocation *object() {
        return getOperand(0);
    }
    const LDefinition *temp1() {
        return getTemp(0);
    }
    const LDefinition *temp2() {
        return getTemp(1);
    }
    const LDefinition *temp3() {
        return getTemp(2);
    }
    MIteratorEnd *mir() const {
        return mir_->toIteratorEnd();
    }
};

// Read the number of actual arguments.
class LArgumentsLength : public LInstructionHelper<1, 0, 0>
{
  public:
    LIR_HEADER(ArgumentsLength)
};

// Load a value from the actual arguments.
class LGetArgument : public LInstructionHelper<BOX_PIECES, 1, 0>
{
  public:
    LIR_HEADER(GetArgument)
    BOX_OUTPUT_ACCESSORS()

    LGetArgument(const LAllocation &index) {
        setOperand(0, index);
    }
    const LAllocation *index() {
        return getOperand(0);
    }
};

class LRunOncePrologue : public LCallInstructionHelper<0, 0, 0>
{
  public:
    LIR_HEADER(RunOncePrologue)

    MRunOncePrologue *mir() const {
        return mir_->toRunOncePrologue();
    }
};

// Create the rest parameter.
class LRest : public LCallInstructionHelper<1, 1, 3>
{
  public:
    LIR_HEADER(Rest)

    LRest(const LAllocation &numActuals, const LDefinition &temp1, const LDefinition &temp2,
          const LDefinition &temp3) {
        setOperand(0, numActuals);
        setTemp(0, temp1);
        setTemp(1, temp2);
        setTemp(2, temp3);
    }
    const LAllocation *numActuals() {
        return getOperand(0);
    }
    MRest *mir() const {
        return mir_->toRest();
    }
};

class LParRest : public LCallInstructionHelper<1, 2, 3>
{
  public:
    LIR_HEADER(ParRest);

    LParRest(const LAllocation &parSlice, const LAllocation &numActuals,
             const LDefinition &temp1, const LDefinition &temp2, const LDefinition &temp3) {
        setOperand(0, parSlice);
        setOperand(1, numActuals);
        setTemp(0, temp1);
        setTemp(1, temp2);
        setTemp(2, temp3);
    }
    const LAllocation *parSlice() {
        return getOperand(0);
    }
    const LAllocation *numActuals() {
        return getOperand(1);
    }
    MRest *mir() const {
        return mir_->toRest();
    }
};

class LParWriteGuard : public LCallInstructionHelper<0, 2, 1>
{
  public:
    LIR_HEADER(ParWriteGuard);

    LParWriteGuard(const LAllocation &parSlice,
                   const LAllocation &object,
                   const LDefinition &temp1) {
        setOperand(0, parSlice);
        setOperand(1, object);
        setTemp(0, temp1);
    }

    bool isCall() const {
        return true;
    }

    const LAllocation *parSlice() {
        return getOperand(0);
    }

    const LAllocation *object() {
        return getOperand(1);
    }

    const LAllocation *getTempReg() {
        return getTemp(0)->output();
    }
};

class LParDump : public LCallInstructionHelper<0, BOX_PIECES, 0>
{
  public:
    LIR_HEADER(ParDump);

    static const size_t Value = 0;

    const LAllocation *value() {
        return getOperand(0);
    }
};

// Guard that a value is in a TypeSet.
class LTypeBarrier : public LInstructionHelper<0, BOX_PIECES, 1>
{
  public:
    LIR_HEADER(TypeBarrier)

    LTypeBarrier(const LDefinition &temp) {
        setTemp(0, temp);
    }

    static const size_t Input = 0;

    const MTypeBarrier *mir() const {
        return mir_->toTypeBarrier();
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
};

// Guard that a value is in a TypeSet.
class LMonitorTypes : public LInstructionHelper<0, BOX_PIECES, 1>
{
  public:
    LIR_HEADER(MonitorTypes)

    LMonitorTypes(const LDefinition &temp) {
        setTemp(0, temp);
    }

    static const size_t Input = 0;

    const MMonitorTypes *mir() const {
        return mir_->toMonitorTypes();
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
};

// Generational write barrier used when writing an object to another object.
class LPostWriteBarrierO : public LInstructionHelper<0, 2, 0>
{
  public:
    LIR_HEADER(PostWriteBarrierO)

    LPostWriteBarrierO(const LAllocation &obj, const LAllocation &value) {
        setOperand(0, obj);
        setOperand(1, value);
    }

    const MPostWriteBarrier *mir() const {
        return mir_->toPostWriteBarrier();
    }
    const LAllocation *object() {
        return getOperand(0);
    }
    const LAllocation *value() {
        return getOperand(1);
    }
};

// Generational write barrier used when writing a value to another object.
class LPostWriteBarrierV : public LInstructionHelper<0, 1 + BOX_PIECES, 1>
{
  public:
    LIR_HEADER(PostWriteBarrierV)

    LPostWriteBarrierV(const LAllocation &obj, const LDefinition &temp) {
        setOperand(0, obj);
        setTemp(0, temp);
    }

    static const size_t Input = 1;

    const MPostWriteBarrier *mir() const {
        return mir_->toPostWriteBarrier();
    }
    const LAllocation *object() {
        return getOperand(0);
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
};

// Guard against an object's class.
class LGuardClass : public LInstructionHelper<0, 1, 1>
{
  public:
    LIR_HEADER(GuardClass)

    LGuardClass(const LAllocation &in, const LDefinition &temp) {
        setOperand(0, in);
        setTemp(0, temp);
    }
    const MGuardClass *mir() const {
        return mir_->toGuardClass();
    }
    const LAllocation *tempInt() {
        return getTemp(0)->output();
    }
};

class MPhi;

// Phi is a pseudo-instruction that emits no code, and is an annotation for the
// register allocator. Like its equivalent in MIR, phis are collected at the
// top of blocks and are meant to be executed in parallel, choosing the input
// corresponding to the predecessor taken in the control flow graph.
class LPhi : public LInstruction
{
    uint32_t numInputs_;
    LAllocation *inputs_;
    LDefinition def_;

    bool init(MIRGenerator *gen);

    LPhi(MPhi *mir);

  public:
    LIR_HEADER(Phi)

    static LPhi *New(MIRGenerator *gen, MPhi *phi);

    size_t numDefs() const {
        return 1;
    }
    LDefinition *getDef(size_t index) {
        JS_ASSERT(index == 0);
        return &def_;
    }
    void setDef(size_t index, const LDefinition &def) {
        JS_ASSERT(index == 0);
        def_ = def;
    }
    size_t numOperands() const {
        return numInputs_;
    }
    LAllocation *getOperand(size_t index) {
        JS_ASSERT(index < numOperands());
        return &inputs_[index];
    }
    void setOperand(size_t index, const LAllocation &a) {
        JS_ASSERT(index < numOperands());
        inputs_[index] = a;
    }
    size_t numTemps() const {
        return 0;
    }
    LDefinition *getTemp(size_t index) {
        JS_NOT_REACHED("no temps");
        return NULL;
    }
    void setTemp(size_t index, const LDefinition &temp) {
        JS_NOT_REACHED("no temps");
    }
    size_t numSuccessors() const {
        return 0;
    }
    MBasicBlock *getSuccessor(size_t i) const {
        JS_NOT_REACHED("no successors");
        return NULL;
    }
    void setSuccessor(size_t i, MBasicBlock *) {
        JS_NOT_REACHED("no successors");
    }

    virtual void printInfo(FILE *fp) {
        printOperands(fp);
    }
};

class LIn : public LCallInstructionHelper<1, BOX_PIECES+1, 0>
{
  public:
    LIR_HEADER(In)
    LIn(const LAllocation &rhs) {
        setOperand(RHS, rhs);
    }

    const LAllocation *lhs() {
        return getOperand(LHS);
    }
    const LAllocation *rhs() {
        return getOperand(RHS);
    }

    static const size_t LHS = 0;
    static const size_t RHS = BOX_PIECES;
};

class LInstanceOfO : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(InstanceOfO)
    LInstanceOfO(const LAllocation &lhs) {
        setOperand(0, lhs);
    }

    MInstanceOf *mir() const {
        return mir_->toInstanceOf();
    }

    const LAllocation *lhs() {
        return getOperand(0);
    }
};

class LInstanceOfV : public LInstructionHelper<1, BOX_PIECES, 0>
{
  public:
    LIR_HEADER(InstanceOfV)
    LInstanceOfV() {
    }

    MInstanceOf *mir() const {
        return mir_->toInstanceOf();
    }

    const LAllocation *lhs() {
        return getOperand(LHS);
    }

    static const size_t LHS = 0;
};

class LCallInstanceOf : public LCallInstructionHelper<1, BOX_PIECES+1, 0>
{
  public:
    LIR_HEADER(CallInstanceOf)
    LCallInstanceOf(const LAllocation &rhs) {
        setOperand(RHS, rhs);
    }

    const LDefinition *output() {
        return this->getDef(0);
    }
    const LAllocation *lhs() {
        return getOperand(LHS);
    }
    const LAllocation *rhs() {
        return getOperand(RHS);
    }

    static const size_t LHS = 0;
    static const size_t RHS = BOX_PIECES;
};

class LFunctionBoundary : public LInstructionHelper<0, 0, 1>
{
  public:
    LIR_HEADER(FunctionBoundary)

    LFunctionBoundary(const LDefinition &temp) {
        setTemp(0, temp);
    }

    const LDefinition *temp() {
        return getTemp(0);
    }

    JSScript *script() {
        return mir_->toFunctionBoundary()->script();
    }

    MFunctionBoundary::Type type() {
        return mir_->toFunctionBoundary()->type();
    }

    unsigned inlineLevel() {
        return mir_->toFunctionBoundary()->inlineLevel();
    }
};

class LIsCallable : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(IsCallable);
    LIsCallable(const LAllocation &object) {
        setOperand(0, object);
    }

    const LAllocation *object() {
        return getOperand(0);
    }
    MIsCallable *mir() const {
        return mir_->toIsCallable();
    }
};

class LHaveSameClass : public LInstructionHelper<1, 2, 1>
{
  public:
    LIR_HEADER(HaveSameClass);
    LHaveSameClass(const LAllocation &left, const LAllocation &right,
                   const LDefinition &temp) {
        setOperand(0, left);
        setOperand(1, right);
        setTemp(0, temp);
    }

    const LAllocation *lhs() {
        return getOperand(0);
    }
    const LAllocation *rhs() {
        return getOperand(1);
    }
    MHaveSameClass *mir() const {
        return mir_->toHaveSameClass();
    }
};

class LAsmJSLoadHeap : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(AsmJSLoadHeap);
    LAsmJSLoadHeap(const LAllocation &ptr) {
        setOperand(0, ptr);
    }
    MAsmJSLoadHeap *mir() const {
        return mir_->toAsmJSLoadHeap();
    }
    const LAllocation *ptr() {
        return getOperand(0);
    }
};

class LAsmJSStoreHeap : public LInstructionHelper<0, 2, 0>
{
  public:
    LIR_HEADER(AsmJSStoreHeap);
    LAsmJSStoreHeap(const LAllocation &ptr, const LAllocation &value) {
        setOperand(0, ptr);
        setOperand(1, value);
    }
    MAsmJSStoreHeap *mir() const {
        return mir_->toAsmJSStoreHeap();
    }
    const LAllocation *ptr() {
        return getOperand(0);
    }
    const LAllocation *value() {
        return getOperand(1);
    }
};

class LAsmJSLoadGlobalVar : public LInstructionHelper<1, 0, 0>
{
  public:
    LIR_HEADER(AsmJSLoadGlobalVar);
    MAsmJSLoadGlobalVar *mir() const {
        return mir_->toAsmJSLoadGlobalVar();
    }
};

class LAsmJSStoreGlobalVar : public LInstructionHelper<0, 1, 0>
{
  public:
    LIR_HEADER(AsmJSStoreGlobalVar);
    LAsmJSStoreGlobalVar(const LAllocation &value) {
        setOperand(0, value);
    }
    MAsmJSStoreGlobalVar *mir() const {
        return mir_->toAsmJSStoreGlobalVar();
    }
    const LAllocation *value() {
        return getOperand(0);
    }
};

class LAsmJSLoadFFIFunc : public LInstructionHelper<1, 0, 0>
{
  public:
    LIR_HEADER(AsmJSLoadFFIFunc);
    MAsmJSLoadFFIFunc *mir() const {
        return mir_->toAsmJSLoadFFIFunc();
    }
};

class LAsmJSParameter : public LInstructionHelper<1, 0, 0>
{
  public:
    LIR_HEADER(AsmJSParameter);
};

class LAsmJSReturn : public LInstructionHelper<0, 1, 0>
{
  public:
    LIR_HEADER(AsmJSReturn);
};

class LAsmJSVoidReturn : public LInstructionHelper<0, 0, 0>
{
  public:
    LIR_HEADER(AsmJSVoidReturn);
};

class LAsmJSPassStackArg : public LInstructionHelper<0, 1, 0>
{
  public:
    LIR_HEADER(AsmJSPassStackArg);
    LAsmJSPassStackArg(const LAllocation &arg) {
        setOperand(0, arg);
    }
    MAsmJSPassStackArg *mir() const {
        return mirRaw()->toAsmJSPassStackArg();
    }
    const LAllocation *arg() {
        return getOperand(0);
    }
};

class LAsmJSCall : public LInstruction
{
    LAllocation *operands_;
    uint32_t numOperands_;
    LDefinition def_;

  public:
    LIR_HEADER(AsmJSCall);

    LAsmJSCall(LAllocation *operands, uint32_t numOperands)
      : operands_(operands),
        numOperands_(numOperands),
        def_(LDefinition::BogusTemp())
    {}

    MAsmJSCall *mir() const {
        return mir_->toAsmJSCall();
    }

    bool isCall() const {
        return true;
    };

    // LInstruction interface
    size_t numDefs() const {
        return def_.isBogusTemp() ? 0 : 1;
    }
    LDefinition *getDef(size_t index) {
        JS_ASSERT(numDefs() == 1);
        JS_ASSERT(index == 0);
        return &def_;
    }
    void setDef(size_t index, const LDefinition &def) {
        JS_ASSERT(index == 0);
        def_ = def;
    }
    size_t numOperands() const {
        return numOperands_;
    }
    LAllocation *getOperand(size_t index) {
        JS_ASSERT(index < numOperands_);
        return &operands_[index];
    }
    void setOperand(size_t index, const LAllocation &a) {
        JS_ASSERT(index < numOperands_);
        operands_[index] = a;
    }
    size_t numTemps() const {
        return 0;
    }
    LDefinition *getTemp(size_t index) {
        JS_NOT_REACHED("no temps");
    }
    void setTemp(size_t index, const LDefinition &a) {
        JS_NOT_REACHED("no temps");
    }
    size_t numSuccessors() const {
        return 0;
    }
    MBasicBlock *getSuccessor(size_t i) const {
        JS_NOT_REACHED("no successors");
        return NULL;
    }
    void setSuccessor(size_t i, MBasicBlock *) {
        JS_NOT_REACHED("no successors");
    }
};

class LAsmJSCheckOverRecursed : public LInstructionHelper<0, 0, 0>
{
  public:
    LIR_HEADER(AsmJSCheckOverRecursed)

    MAsmJSCheckOverRecursed *mir() const {
        return mir_->toAsmJSCheckOverRecursed();
    }
};

} // namespace jit
} // namespace js

#endif /* jit_LIR_Common_h */
