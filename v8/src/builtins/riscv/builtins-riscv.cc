// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-arguments.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/frame-constants.h"
#include "src/execution/frames.h"
#include "src/logging/counters.h"
// For interpreter_entry_return_pc_offset. TODO(jkummerow): Drop.
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/register-configuration.h"
#include "src/heap/heap-inl.h"
#include "src/objects/cell.h"
#include "src/objects/foreign.h"
#include "src/objects/heap-number.h"
#include "src/objects/js-generator.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"
#include "src/runtime/runtime.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/baseline/liftoff-assembler-defs.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-objects.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void Builtins::Generate_Adaptor(MacroAssembler* masm, Address address) {
  ASM_CODE_COMMENT(masm);
  __ li(kJavaScriptCallExtraArg1Register, ExternalReference::Create(address));
  __ Jump(BUILTIN_CODE(masm->isolate(), AdaptorWithBuiltinExitFrame),
          RelocInfo::CODE_TARGET);
}

namespace {

enum class ArgumentsElementType {
  kRaw,    // Push arguments as they are.
  kHandle  // Dereference arguments before pushing.
};

void Generate_PushArguments(MacroAssembler* masm, Register array, Register argc,
                            Register scratch, Register scratch2,
                            ArgumentsElementType element_type) {
  DCHECK(!AreAliased(array, argc, scratch));
  Label loop, entry;
  __ SubWord(scratch, argc, Operand(kJSArgcReceiverSlots));
  __ Branch(&entry);
  __ bind(&loop);
  __ CalcScaledAddress(scratch2, array, scratch, kSystemPointerSizeLog2);
  __ LoadWord(scratch2, MemOperand(scratch2));
  if (element_type == ArgumentsElementType::kHandle) {
    __ LoadWord(scratch2, MemOperand(scratch2));
  }
  __ push(scratch2);
  __ bind(&entry);
  __ AddWord(scratch, scratch, Operand(-1));
  __ Branch(&loop, greater_equal, scratch, Operand(zero_reg));
}

void Generate_JSBuiltinsConstructStubHelper(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : number of arguments
  //  -- a1     : constructor function
  //  -- a3     : new target
  //  -- cp     : context
  //  -- ra     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ SmiTag(a0);
    __ Push(cp, a0);
    __ SmiUntag(a0);

    // Set up pointer to first argument (skip receiver).
    __ AddWord(
        t2, fp,
        Operand(StandardFrameConstants::kCallerSPOffset + kSystemPointerSize));
    // t2: Pointer to start of arguments.
    // a0: Number of arguments.
    {
      UseScratchRegisterScope temps(masm);
      temps.Include(t0);
      Generate_PushArguments(masm, t2, a0, temps.Acquire(), temps.Acquire(),
                             ArgumentsElementType::kRaw);
    }
    // The receiver for the builtin/api call.
    __ PushRoot(RootIndex::kTheHoleValue);

    // Call the function.
    // a0: number of arguments (untagged)
    // a1: constructor function
    // a3: new target
    __ InvokeFunctionWithNewTarget(a1, a3, a0, InvokeType::kCall);

    // Restore context from the frame.
    __ LoadWord(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
    // Restore smi-tagged arguments count from the frame.
    __ LoadWord(kScratchReg,
                MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  __ DropArguments(kScratchReg, MacroAssembler::kCountIsSmi,
                   MacroAssembler::kCountIncludesReceiver, kScratchReg);
  __ Ret();
}

}  // namespace

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  --      a0: number of arguments (untagged)
  //  --      a1: constructor function
  //  --      a3: new target
  //  --      cp: context
  //  --      ra: return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  UseScratchRegisterScope temps(masm);
  temps.Include(t0, t1);
  // Enter a construct frame.
  FrameScope scope(masm, StackFrame::MANUAL);
  Label post_instantiation_deopt_entry, not_create_implicit_receiver;
  __ EnterFrame(StackFrame::CONSTRUCT);

  // Preserve the incoming parameters on the stack.
  __ SmiTag(a0);
  __ Push(cp, a0, a1);
  __ PushRoot(RootIndex::kUndefinedValue);
  __ Push(a3);

  // ----------- S t a t e -------------
  //  --        sp[0*kSystemPointerSize]: new target
  //  --        sp[1*kSystemPointerSize]: padding
  //  -- a1 and sp[2*kSystemPointerSize]: constructor function
  //  --        sp[3*kSystemPointerSize]: number of arguments (tagged)
  //  --        sp[4*kSystemPointerSize]: context
  // -----------------------------------
  {
    UseScratchRegisterScope temps(masm);
    Register func_info = temps.Acquire();
    __ LoadTaggedField(
        func_info, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
    __ Load32U(func_info,
               FieldMemOperand(func_info, SharedFunctionInfo::kFlagsOffset));
    __ DecodeField<SharedFunctionInfo::FunctionKindBits>(func_info);
    __ JumpIfIsInRange(
        func_info,
        static_cast<uint32_t>(FunctionKind::kDefaultDerivedConstructor),
        static_cast<uint32_t>(FunctionKind::kDerivedConstructor),
        &not_create_implicit_receiver);
    // If not derived class constructor: Allocate the new receiver object.
    __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject),
            RelocInfo::CODE_TARGET);
    __ BranchShort(&post_instantiation_deopt_entry);

    // Else: use TheHoleValue as receiver for constructor call
    __ bind(&not_create_implicit_receiver);
    __ LoadRoot(a0, RootIndex::kTheHoleValue);
  }
  // ----------- S t a t e -------------
  //  --                          a0: receiver
  //  -- Slot 4 / sp[0*kSystemPointerSize]: new target
  //  -- Slot 3 / sp[1*kSystemPointerSize]: padding
  //  -- Slot 2 / sp[2*kSystemPointerSize]: constructor function
  //  -- Slot 1 / sp[3*kSystemPointerSize]: number of arguments (tagged)
  //  -- Slot 0 / sp[4*kSystemPointerSize]: context
  // -----------------------------------
  // Deoptimizer enters here.
  masm->isolate()->heap()->SetConstructStubCreateDeoptPCOffset(
      masm->pc_offset());
  __ bind(&post_instantiation_deopt_entry);

  // Restore new target.
  __ Pop(a3);

  // Push the allocated receiver to the stack.
  __ Push(a0);

  // We need two copies because we may have to return the original one
  // and the calling conventions dictate that the called function pops the
  // receiver. The second copy is pushed after the arguments, we saved in a6
  // since a0 will store the return value of callRuntime.
  __ Move(a6, a0);

  // Set up pointer to first argument (skip receiver)..
  __ AddWord(
      t2, fp,
      Operand(StandardFrameConstants::kCallerSPOffset + kSystemPointerSize));

  // ----------- S t a t e -------------
  //  --                 a3: new target
  //  -- sp[0*kSystemPointerSize]: implicit receiver
  //  -- sp[1*kSystemPointerSize]: implicit receiver
  //  -- sp[2*kSystemPointerSize]: padding
  //  -- sp[3*kSystemPointerSize]: constructor function
  //  -- sp[4*kSystemPointerSize]: number of arguments (tagged)
  //  -- sp[5*kSystemPointerSize]: context
  // -----------------------------------

  // Restore constructor function and argument count.
  __ LoadWord(a1, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
  __ LoadWord(a0, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
  __ SmiUntag(a0);

  Label stack_overflow;
  {
    UseScratchRegisterScope temps(masm);
    __ StackOverflowCheck(a0, temps.Acquire(), temps.Acquire(),
                          &stack_overflow);
  }
  // TODO(victorgomes): When the arguments adaptor is completely removed, we
  // should get the formal parameter count and copy the arguments in its
  // correct position (including any undefined), instead of delaying this to
  // InvokeFunction.

  // Copy arguments and receiver to the expression stack.
  // t2: Pointer to start of argument.
  // a0: Number of arguments.
  {
    UseScratchRegisterScope temps(masm);
    Generate_PushArguments(masm, t2, a0, temps.Acquire(), temps.Acquire(),
                           ArgumentsElementType::kRaw);
  }
  // We need two copies because we may have to return the original one
  // and the calling conventions dictate that the called function pops the
  // receiver. The second copy is pushed after the arguments,
  __ Push(a6);

  // Call the function.
  __ InvokeFunctionWithNewTarget(a1, a3, a0, InvokeType::kCall);

  // ----------- S t a t e -------------
  //  --                 a0: constructor result
  //  -- sp[0*kSystemPointerSize]: implicit receiver
  //  -- sp[1*kSystemPointerSize]: padding
  //  -- sp[2*kSystemPointerSize]: constructor function
  //  -- sp[3*kSystemPointerSize]: number of arguments
  //  -- sp[4*kSystemPointerSize]: context
  // -----------------------------------

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetConstructStubInvokeDeoptPCOffset(
      masm->pc_offset());

  // Restore the context from the frame.
  __ LoadWord(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));

  // If the result is an object (in the ECMA sense), we should get rid
  // of the receiver and use the result; see ECMA-262 section 13.2.2-7
  // on page 74.
  Label use_receiver, do_throw, leave_and_return, check_receiver;

  // If the result is undefined, we jump out to using the implicit receiver.
  __ JumpIfNotRoot(a0, RootIndex::kUndefinedValue, &check_receiver);

  // Otherwise we do a smi check and fall through to check if the return value
  // is a valid receiver.

  // Throw away the result of the constructor invocation and use the
  // on-stack receiver as the result.
  __ bind(&use_receiver);
  __ LoadWord(a0, MemOperand(sp, 0 * kSystemPointerSize));
  __ JumpIfRoot(a0, RootIndex::kTheHoleValue, &do_throw);

  __ bind(&leave_and_return);
  // Restore smi-tagged arguments count from the frame.
  __ LoadWord(a1, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
  // Leave construct frame.
  __ LeaveFrame(StackFrame::CONSTRUCT);

  // Remove caller arguments from the stack and return.
  __ DropArguments(a1, MacroAssembler::kCountIsSmi,
                   MacroAssembler::kCountIncludesReceiver, a4);
  __ Ret();

  __ bind(&check_receiver);
  __ JumpIfSmi(a0, &use_receiver);

  // If the type of the result (stored in its map) is less than
  // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
  {
    UseScratchRegisterScope temps(masm);
    Register map = temps.Acquire(), type = temps.Acquire();
    __ GetObjectType(a0, map, type);

    static_assert(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ Branch(&leave_and_return, greater_equal, type,
              Operand(FIRST_JS_RECEIVER_TYPE));
    __ Branch(&use_receiver);
  }
  __ bind(&do_throw);
  // Restore the context from the frame.
  __ LoadWord(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
  __ CallRuntime(Runtime::kThrowConstructorReturnedNonObject);
  __ break_(0xCC);

  __ bind(&stack_overflow);
  // Restore the context from the frame.
  __ LoadWord(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
  __ CallRuntime(Runtime::kThrowStackOverflow);
  __ break_(0xCC);
}

void Builtins::Generate_JSBuiltinsConstructStub(MacroAssembler* masm) {
  Generate_JSBuiltinsConstructStubHelper(masm);
}

static void AssertCodeIsBaseline(MacroAssembler* masm, Register code,
                                 Register scratch) {
  DCHECK(!AreAliased(code, scratch));
  // Verify that the code kind is baseline code via the CodeKind.
  __ LoadWord(scratch, FieldMemOperand(code, Code::kFlagsOffset));
  __ DecodeField<Code::KindField>(scratch);
  __ Assert(eq, AbortReason::kExpectedBaselineData, scratch,
            Operand(static_cast<int64_t>(CodeKind::BASELINE)));
}
// TODO(v8:11429): Add a path for "not_compiled" and unify the two uses under
// the more general dispatch.
static void GetSharedFunctionInfoBytecodeOrBaseline(MacroAssembler* masm,
                                                    Register sfi_data,
                                                    Register scratch1,
                                                    Label* is_baseline) {
  ASM_CODE_COMMENT(masm);
  Label done;

  __ GetObjectType(sfi_data, scratch1, scratch1);

#ifndef V8_JITLESS
  if (v8_flags.debug_code) {
    Label not_baseline;
    __ Branch(&not_baseline, ne, scratch1, Operand(CODE_TYPE));
    AssertCodeIsBaseline(masm, sfi_data, scratch1);
    __ Branch(is_baseline);
    __ bind(&not_baseline);
  } else {
    __ Branch(is_baseline, eq, scratch1, Operand(CODE_TYPE));
  }
#endif  // !V8_JITLESS

  __ Branch(&done, ne, scratch1, Operand(INTERPRETER_DATA_TYPE),
            Label::Distance::kNear);
  __ LoadTaggedField(
      sfi_data,
      FieldMemOperand(sfi_data, InterpreterData::kBytecodeArrayOffset));

  __ bind(&done);
}

// static
void Builtins::Generate_ResumeGeneratorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the value to pass to the generator
  //  -- a1 : the JSGeneratorObject to resume
  //  -- ra : return address
  // -----------------------------------

  // Store input value into generator object.
  __ StoreTaggedField(
      a0, FieldMemOperand(a1, JSGeneratorObject::kInputOrDebugPosOffset));
  __ RecordWriteField(a1, JSGeneratorObject::kInputOrDebugPosOffset, a0,
                      kRAHasNotBeenSaved, SaveFPRegsMode::kIgnore);
  // Check that a1 is still valid, RecordWrite might have clobbered it.
  __ AssertGeneratorObject(a1);

  // Load suspended function and context.
  __ LoadTaggedField(a4,
                     FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));
  __ LoadTaggedField(cp, FieldMemOperand(a4, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  __ li(a5, debug_hook);
  __ Lb(a5, MemOperand(a5));
  __ Branch(&prepare_step_in_if_stepping, ne, a5, Operand(zero_reg));

  // Flood function if we need to continue stepping in the suspended generator.
  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());
  __ li(a5, debug_suspended_generator);
  __ LoadWord(a5, MemOperand(a5));
  __ Branch(&prepare_step_in_suspended_generator, eq, a1, Operand(a5));
  __ bind(&stepping_prepared);

  // Check the stack for overflow. We are not trying to catch interruptions
  // (i.e. debug break and preemption) here, so check the "real stack limit".
  Label stack_overflow;
  __ LoadStackLimit(kScratchReg,
                    MacroAssembler::StackLimitKind::kRealStackLimit);
  __ Branch(&stack_overflow, Uless, sp, Operand(kScratchReg));

  // ----------- S t a t e -------------
  //  -- a1    : the JSGeneratorObject to resume
  //  -- a4    : generator function
  //  -- cp    : generator context
  //  -- ra    : return address
  // -----------------------------------

  // Push holes for arguments to generator function. Since the parser forced
  // context allocation for any variables in generators, the actual argument
  // values have already been copied into the context and these dummy values
  // will never be used.
  __ LoadTaggedField(
      a3, FieldMemOperand(a4, JSFunction::kSharedFunctionInfoOffset));
  __ Lhu(a3,
         FieldMemOperand(a3, SharedFunctionInfo::kFormalParameterCountOffset));
  __ SubWord(a3, a3, Operand(kJSArgcReceiverSlots));
  __ LoadTaggedField(
      t1,
      FieldMemOperand(a1, JSGeneratorObject::kParametersAndRegistersOffset));
  {
    Label done_loop, loop;
    __ bind(&loop);
    __ SubWord(a3, a3, Operand(1));
    __ Branch(&done_loop, lt, a3, Operand(zero_reg), Label::Distance::kNear);
    __ CalcScaledAddress(kScratchReg, t1, a3, kTaggedSizeLog2);
    __ LoadTaggedField(kScratchReg,
                       FieldMemOperand(kScratchReg, FixedArray::kHeaderSize));
    __ Push(kScratchReg);
    __ Branch(&loop);
    __ bind(&done_loop);
    // Push receiver.
    __ LoadTaggedField(kScratchReg,
                       FieldMemOperand(a1, JSGeneratorObject::kReceiverOffset));
    __ Push(kScratchReg);
  }

  // Underlying function needs to have bytecode available.
  if (v8_flags.debug_code) {
    Label is_baseline;
    __ LoadTaggedField(
        a3, FieldMemOperand(a4, JSFunction::kSharedFunctionInfoOffset));
    __ LoadTaggedField(
        a3, FieldMemOperand(a3, SharedFunctionInfo::kFunctionDataOffset));
    GetSharedFunctionInfoBytecodeOrBaseline(masm, a3, a0, &is_baseline);
    __ GetObjectType(a3, a3, a3);
    __ Assert(eq, AbortReason::kMissingBytecodeArray, a3,
              Operand(BYTECODE_ARRAY_TYPE));
    __ bind(&is_baseline);
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    __ LoadTaggedField(
        a0, FieldMemOperand(a4, JSFunction::kSharedFunctionInfoOffset));
    __ Lhu(a0, FieldMemOperand(
                   a0, SharedFunctionInfo::kFormalParameterCountOffset));
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ Move(a3, a1);
    __ Move(a1, a4);
    static_assert(kJavaScriptCallCodeStartRegister == a2, "ABI mismatch");
    __ LoadTaggedField(a2, FieldMemOperand(a1, JSFunction::kCodeOffset));
    __ JumpCodeObject(a2);
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1, a4);
    // Push hole as receiver since we do not use it for stepping.
    __ PushRoot(RootIndex::kTheHoleValue);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(a1);
  }
  __ LoadTaggedField(a4,
                     FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));
  __ Branch(&stepping_prepared);

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(a1);
  }
  __ LoadTaggedField(a4,
                     FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));
  __ Branch(&stepping_prepared);

  __ bind(&stack_overflow);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ break_(0xCC);  // This should be unreachable.
  }
}

void Builtins::Generate_ConstructedNonConstructable(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  __ Push(a1);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
}

// Clobbers scratch1 and scratch2; preserves all other registers.
static void Generate_CheckStackOverflow(MacroAssembler* masm, Register argc,
                                        Register scratch1, Register scratch2) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  Label okay;
  __ LoadStackLimit(scratch1, MacroAssembler::StackLimitKind::kRealStackLimit);
  // Make a2 the space we have left. The stack might already be overflowed
  // here which will cause r2 to become negative.
  __ SubWord(scratch1, sp, scratch1);
  // Check if the arguments will overflow the stack.
  __ SllWord(scratch2, argc, kSystemPointerSizeLog2);
  __ Branch(&okay, gt, scratch1, Operand(scratch2),
            Label::Distance::kNear);  // Signed comparison.

  // Out of stack space.
  __ CallRuntime(Runtime::kThrowStackOverflow);

  __ bind(&okay);
}

namespace {

// Called with the native C calling convention. The corresponding function
// signature is either:
//
//   using JSEntryFunction = GeneratedCode<Address(
//       Address root_register_value, Address new_target, Address target,
//       Address receiver, intptr_t argc, Address** args)>;
// or
//   using JSEntryFunction = GeneratedCode<Address(
//       Address root_register_value, MicrotaskQueue* microtask_queue)>;
void Generate_JSEntryVariant(MacroAssembler* masm, StackFrame::Type type,
                             Builtin entry_trampoline) {
  Label invoke, handler_entry, exit;

  {
    NoRootArrayScope no_root_array(masm);

    // TODO(plind): unify the ABI description here.
    // Registers:
    //  either
    //   a0: root register value
    //   a1: entry address
    //   a2: function
    //   a3: receiver
    //   a4: argc
    //   a5: argv
    //  or
    //   a0: root register value
    //   a1: microtask_queue

    // Save callee saved registers on the stack.
    __ MultiPush(kCalleeSaved | ra);

    // Save callee-saved FPU registers.
    __ MultiPushFPU(kCalleeSavedFPU);
    // Set up the reserved register for 0.0.
    __ LoadFPRImmediate(kDoubleRegZero, 0.0);
    __ LoadFPRImmediate(kSingleRegZero, 0.0f);

    // Initialize the root register.
    // C calling convention. The first argument is passed in a0.
    __ Move(kRootRegister, a0);

#ifdef V8_COMPRESS_POINTERS
    // Initialize the pointer cage base register.
    __ LoadRootRelative(kPtrComprCageBaseRegister,
                        IsolateData::cage_base_offset());
#endif
  }

  // a1: entry address
  // a2: function
  // a3: receiver
  // a4: argc
  // a5: argv

  // We build an EntryFrame.
  __ li(s1, Operand(-1));  // Push a bad frame pointer to fail if it is used.
  __ li(s2, Operand(StackFrame::TypeToMarker(type)));
  __ li(s3, Operand(StackFrame::TypeToMarker(type)));
  ExternalReference c_entry_fp = ExternalReference::Create(
      IsolateAddressId::kCEntryFPAddress, masm->isolate());
  __ li(s5, c_entry_fp);
  __ LoadWord(s4, MemOperand(s5));
  __ Push(s1, s2, s3, s4);
  // Clear c_entry_fp, now we've pushed its previous value to the stack.
  // If the c_entry_fp is not already zero and we don't clear it, the
  // StackFrameIteratorForProfiler will assume we are executing C++ and miss the
  // JS frames on top.
  __ StoreWord(zero_reg, MemOperand(s5));
  // Set up frame pointer for the frame to be pushed.
  __ AddWord(fp, sp, -EntryFrameConstants::kNextExitFrameFPOffset);
  // Registers:
  //  either
  //   a1: entry address
  //   a2: function
  //   a3: receiver
  //   a4: argc
  //   a5: argv
  //  or
  //   a1: microtask_queue
  //
  // Stack:
  // caller fp          |
  // function slot      | entry frame
  // context slot       |
  // bad fp (0xFF...F)  |
  // callee saved registers + ra
  // [ O32: 4 args slots]
  // args

  // If this is the outermost JS call, set js_entry_sp value.
  Label non_outermost_js;
  ExternalReference js_entry_sp = ExternalReference::Create(
      IsolateAddressId::kJSEntrySPAddress, masm->isolate());
  __ li(s1, js_entry_sp);
  __ LoadWord(s2, MemOperand(s1));
  __ Branch(&non_outermost_js, ne, s2, Operand(zero_reg),
            Label::Distance::kNear);
  __ StoreWord(fp, MemOperand(s1));
  __ li(s3, Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  Label cont;
  __ Branch(&cont);
  __ bind(&non_outermost_js);
  __ li(s3, Operand(StackFrame::INNER_JSENTRY_FRAME));
  __ bind(&cont);
  __ push(s3);

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ BranchShort(&invoke);
  __ bind(&handler_entry);

  // Store the current pc as the handler offset. It's used later to create the
  // handler table.
  masm->isolate()->builtins()->SetJSEntryHandlerOffset(handler_entry.pos());

  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.  Coming in here the
  // fp will be invalid because the PushStackHandler below sets it to 0 to
  // signal the existence of the JSEntry frame.
  __ li(s1, ExternalReference::Create(
                IsolateAddressId::kPendingExceptionAddress, masm->isolate()));
  __ StoreWord(a0,
               MemOperand(s1));  // We come back from 'invoke'. result is in a0.
  __ LoadRoot(a0, RootIndex::kException);
  __ BranchShort(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  __ PushStackHandler();
  // If an exception not caught by another handler occurs, this handler
  // returns control to the code after the bal(&invoke) above, which
  // restores all kCalleeSaved registers (including cp and fp) to their
  // saved values before returning a failure to C.
  //
  // Registers:
  //  either
  //   a0: root register value
  //   a1: entry address
  //   a2: function
  //   a3: receiver
  //   a4: argc
  //   a5: argv
  //  or
  //   a0: root register value
  //   a1: microtask_queue
  //
  // Stack:
  // handler frame
  // entry frame
  // callee saved registers + ra
  // [ O32: 4 args slots]
  // args
  //
  // Invoke the function by calling through JS entry trampoline builtin and
  // pop the faked function when we return.

  Handle<Code> trampoline_code =
      masm->isolate()->builtins()->code_handle(entry_trampoline);
  __ Call(trampoline_code, RelocInfo::CODE_TARGET);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();

  __ bind(&exit);  // a0 holds result
  // Check if the current stack frame is marked as the outermost JS frame.
  Label non_outermost_js_2;
  __ pop(a5);
  __ Branch(&non_outermost_js_2, ne, a5,
            Operand(StackFrame::OUTERMOST_JSENTRY_FRAME),
            Label::Distance::kNear);
  __ li(a5, js_entry_sp);
  __ StoreWord(zero_reg, MemOperand(a5));
  __ bind(&non_outermost_js_2);

  // Restore the top frame descriptors from the stack.
  __ pop(a5);
  __ li(a4, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                      masm->isolate()));
  __ StoreWord(a5, MemOperand(a4));

  // Reset the stack to the callee saved registers.
  __ AddWord(sp, sp, -EntryFrameConstants::kNextExitFrameFPOffset);

  // Restore callee-saved fpu registers.
  __ MultiPopFPU(kCalleeSavedFPU);

  // Restore callee saved registers from the stack.
  __ MultiPop(kCalleeSaved | ra);
  // Return.
  __ Jump(ra);
}

}  // namespace

void Builtins::Generate_JSEntry(MacroAssembler* masm) {
  Generate_JSEntryVariant(masm, StackFrame::ENTRY, Builtin::kJSEntryTrampoline);
}

void Builtins::Generate_JSConstructEntry(MacroAssembler* masm) {
  Generate_JSEntryVariant(masm, StackFrame::CONSTRUCT_ENTRY,
                          Builtin::kJSConstructEntryTrampoline);
}

void Builtins::Generate_JSRunMicrotasksEntry(MacroAssembler* masm) {
  Generate_JSEntryVariant(masm, StackFrame::ENTRY,
                          Builtin::kRunMicrotasksTrampoline);
}

static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // ----------- S t a t e -------------
  //  -- a1: new.target
  //  -- a2: function
  //  -- a3: receiver_pointer
  //  -- a4: argc
  //  -- a5: argv
  // -----------------------------------

  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address = ExternalReference::Create(
        IsolateAddressId::kContextAddress, masm->isolate());
    __ li(cp, context_address);
    __ LoadWord(cp, MemOperand(cp));

    // Push the function onto the stack.
    __ Push(a2);

    // Check if we have enough stack space to push all arguments.
    __ mv(a6, a4);
    Generate_CheckStackOverflow(masm, a6, a0, s2);

    // Copy arguments to the stack.
    // a4: argc
    // a5: argv, i.e. points to first arg
    {
      UseScratchRegisterScope temps(masm);
      Generate_PushArguments(masm, a5, a4, temps.Acquire(), temps.Acquire(),
                             ArgumentsElementType::kHandle);
    }
    // Push the receive.
    __ Push(a3);

    // a0: argc
    // a1: function
    // a3: new.target
    __ Move(a3, a1);
    __ Move(a1, a2);
    __ Move(a0, a4);

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(a4, RootIndex::kUndefinedValue);
    __ Move(a5, a4);
    __ Move(s1, a4);
    __ Move(s2, a4);
    __ Move(s3, a4);
    __ Move(s4, a4);
    __ Move(s5, a4);
#ifndef V8_COMPRESS_POINTERS
    __ Move(s11, a4);
#endif
    // s6 holds the root address. Do not clobber.
    // s7 is cp. Do not init.

    // Invoke the code.
    Handle<Code> builtin = is_construct
                               ? BUILTIN_CODE(masm->isolate(), Construct)
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

    // Leave internal frame.
  }
  __ Jump(ra);
}

void Builtins::Generate_JSEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, false);
}

void Builtins::Generate_JSConstructEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, true);
}

void Builtins::Generate_RunMicrotasksTrampoline(MacroAssembler* masm) {
  // a1: microtask_queue
  __ Move(RunMicrotasksDescriptor::MicrotaskQueueRegister(), a1);
  __ Jump(BUILTIN_CODE(masm->isolate(), RunMicrotasks), RelocInfo::CODE_TARGET);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch1,
                                  Register scratch2) {
  ASM_CODE_COMMENT(masm);
  Register params_size = scratch1;

  // Get the size of the formal parameters + receiver (in bytes).
  __ LoadWord(params_size,
              MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ Lw(params_size,
        FieldMemOperand(params_size, BytecodeArray::kParameterSizeOffset));

  Register actual_params_size = scratch2;
  Label L1;
  // Compute the size of the actual parameters + receiver (in bytes).
  __ LoadWord(actual_params_size,
              MemOperand(fp, StandardFrameConstants::kArgCOffset));
  __ SllWord(actual_params_size, actual_params_size, kSystemPointerSizeLog2);
  // If actual is bigger than formal, then we should use it to free up the stack
  // arguments.
  __ Branch(&L1, le, actual_params_size, Operand(params_size),
            Label::Distance::kNear);
  __ Move(params_size, actual_params_size);
  __ bind(&L1);

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::INTERPRETED);

  // Drop receiver + arguments.
  __ DropArguments(params_size, MacroAssembler::kCountIsBytes,
                   MacroAssembler::kCountIncludesReceiver);
}

// Advance the current bytecode offset. This simulates what all bytecode
// handlers do upon completion of the underlying operation. Will bail out to a
// label if the bytecode (without prefix) is a return bytecode. Will not advance
// the bytecode offset if the current bytecode is a JumpLoop, instead just
// re-executing the JumpLoop to jump to the correct bytecode.
static void AdvanceBytecodeOffsetOrReturn(MacroAssembler* masm,
                                          Register bytecode_array,
                                          Register bytecode_offset,
                                          Register bytecode, Register scratch1,
                                          Register scratch2, Register scratch3,
                                          Label* if_return) {
  ASM_CODE_COMMENT(masm);
  Register bytecode_size_table = scratch1;

  // The bytecode offset value will be increased by one in wide and extra wide
  // cases. In the case of having a wide or extra wide JumpLoop bytecode, we
  // will restore the original bytecode. In order to simplify the code, we have
  // a backup of it.
  Register original_bytecode_offset = scratch3;
  DCHECK(!AreAliased(bytecode_array, bytecode_offset, bytecode,
                     bytecode_size_table, original_bytecode_offset));
  __ Move(original_bytecode_offset, bytecode_offset);
  __ li(bytecode_size_table, ExternalReference::bytecode_size_table_address());

  // Check if the bytecode is a Wide or ExtraWide prefix bytecode.
  Label process_bytecode, extra_wide;
  static_assert(0 == static_cast<int>(interpreter::Bytecode::kWide));
  static_assert(1 == static_cast<int>(interpreter::Bytecode::kExtraWide));
  static_assert(2 == static_cast<int>(interpreter::Bytecode::kDebugBreakWide));
  static_assert(3 ==
                static_cast<int>(interpreter::Bytecode::kDebugBreakExtraWide));
  __ Branch(&process_bytecode, Ugreater, bytecode, Operand(3),
            Label::Distance::kNear);
  __ And(scratch2, bytecode, Operand(1));
  __ Branch(&extra_wide, ne, scratch2, Operand(zero_reg),
            Label::Distance::kNear);

  // Load the next bytecode and update table to the wide scaled table.
  __ AddWord(bytecode_offset, bytecode_offset, Operand(1));
  __ AddWord(scratch2, bytecode_array, bytecode_offset);
  __ Lbu(bytecode, MemOperand(scratch2));
  __ AddWord(bytecode_size_table, bytecode_size_table,
             Operand(kByteSize * interpreter::Bytecodes::kBytecodeCount));
  __ BranchShort(&process_bytecode);

  __ bind(&extra_wide);
  // Load the next bytecode and update table to the extra wide scaled table.
  __ AddWord(bytecode_offset, bytecode_offset, Operand(1));
  __ AddWord(scratch2, bytecode_array, bytecode_offset);
  __ Lbu(bytecode, MemOperand(scratch2));
  __ AddWord(bytecode_size_table, bytecode_size_table,
             Operand(2 * kByteSize * interpreter::Bytecodes::kBytecodeCount));

  __ bind(&process_bytecode);

// Bailout to the return label if this is a return bytecode.
#define JUMP_IF_EQUAL(NAME)          \
  __ Branch(if_return, eq, bytecode, \
            Operand(static_cast<int64_t>(interpreter::Bytecode::k##NAME)));
  RETURN_BYTECODE_LIST(JUMP_IF_EQUAL)
#undef JUMP_IF_EQUAL

  // If this is a JumpLoop, re-execute it to perform the jump to the beginning
  // of the loop.
  Label end, not_jump_loop;
  __ Branch(&not_jump_loop, ne, bytecode,
            Operand(static_cast<int64_t>(interpreter::Bytecode::kJumpLoop)),
            Label::Distance::kNear);
  // We need to restore the original bytecode_offset since we might have
  // increased it to skip the wide / extra-wide prefix bytecode.
  __ Move(bytecode_offset, original_bytecode_offset);
  __ BranchShort(&end);

  __ bind(&not_jump_loop);
  // Otherwise, load the size of the current bytecode and advance the offset.
  __ AddWord(scratch2, bytecode_size_table, bytecode);
  __ Lb(scratch2, MemOperand(scratch2));
  __ AddWord(bytecode_offset, bytecode_offset, scratch2);

  __ bind(&end);
}

namespace {
void ResetBytecodeAge(MacroAssembler* masm, Register bytecode_array) {
  __ Sh(zero_reg,
        FieldMemOperand(bytecode_array, BytecodeArray::kBytecodeAgeOffset));
}

void ResetFeedbackVectorOsrUrgency(MacroAssembler* masm,
                                   Register feedback_vector, Register scratch) {
  DCHECK(!AreAliased(feedback_vector, scratch));
  __ Lbu(scratch,
         FieldMemOperand(feedback_vector, FeedbackVector::kOsrStateOffset));
  __ And(scratch, scratch,
         Operand(FeedbackVector::MaybeHasOptimizedOsrCodeBit::kMask));
  __ Sb(scratch,
        FieldMemOperand(feedback_vector, FeedbackVector::kOsrStateOffset));
}

}  // namespace

// static
void Builtins::Generate_BaselineOutOfLinePrologueDeopt(MacroAssembler* masm) {
  // We're here because we got deopted during BaselineOutOfLinePrologue's stack
  // check. Undo all its frame creation and call into the interpreter instead.

  // Drop bytecode offset (was the feedback vector but got replaced during
  // deopt) and bytecode array.
  __ AddWord(sp, sp, Operand(2 * kPointerSize));

  // Context, closure, argc.
  __ Pop(kContextRegister, kJavaScriptCallTargetRegister,
         kJavaScriptCallArgCountRegister);

  // Drop frame pointer
  __ LeaveFrame(StackFrame::BASELINE);

  // Enter the interpreter.
  __ TailCallBuiltin(Builtin::kInterpreterEntryTrampoline);
}

void Builtins::Generate_BaselineOutOfLinePrologue(MacroAssembler* masm) {
  UseScratchRegisterScope temps(masm);
  temps.Include({kScratchReg, kScratchReg2});
  auto descriptor =
      Builtins::CallInterfaceDescriptorFor(Builtin::kBaselineOutOfLinePrologue);
  Register closure = descriptor.GetRegisterParameter(
      BaselineOutOfLinePrologueDescriptor::kClosure);
  // Load the feedback vector from the closure.
  Register feedback_vector = temps.Acquire();
  __ LoadWord(feedback_vector,
              FieldMemOperand(closure, JSFunction::kFeedbackCellOffset));
  __ LoadWord(feedback_vector,
              FieldMemOperand(feedback_vector, Cell::kValueOffset));
  {
    UseScratchRegisterScope temp(masm);
    Register type = temps.Acquire();
    __ AssertFeedbackVector(feedback_vector, type);
  }

  // Check for an tiering state.
  Label flags_need_processing;
  Register flags = temps.Acquire();
  __ LoadFeedbackVectorFlagsAndJumpIfNeedsProcessing(
      flags, feedback_vector, CodeKind::BASELINE, &flags_need_processing);
  {
    UseScratchRegisterScope temps(masm);
    ResetFeedbackVectorOsrUrgency(masm, feedback_vector, temps.Acquire());
  }
  // Increment invocation count for the function.
  {
    UseScratchRegisterScope temps(masm);
    Register invocation_count = temps.Acquire();
    __ Lw(invocation_count,
          FieldMemOperand(feedback_vector,
                          FeedbackVector::kInvocationCountOffset));
    __ Add32(invocation_count, invocation_count, Operand(1));
    __ Sw(invocation_count,
          FieldMemOperand(feedback_vector,
                          FeedbackVector::kInvocationCountOffset));
  }

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  {
    ASM_CODE_COMMENT_STRING(masm, "Frame Setup");
    // Normally the first thing we'd do here is Push(lr, fp), but we already
    // entered the frame in BaselineCompiler::Prologue, as we had to use the
    // value lr before the call to this BaselineOutOfLinePrologue builtin.

    Register callee_context = descriptor.GetRegisterParameter(
        BaselineOutOfLinePrologueDescriptor::kCalleeContext);
    Register callee_js_function = descriptor.GetRegisterParameter(
        BaselineOutOfLinePrologueDescriptor::kClosure);
    __ Push(callee_context, callee_js_function);
    DCHECK_EQ(callee_js_function, kJavaScriptCallTargetRegister);
    DCHECK_EQ(callee_js_function, kJSFunctionRegister);

    Register argc = descriptor.GetRegisterParameter(
        BaselineOutOfLinePrologueDescriptor::kJavaScriptCallArgCount);
    // We'll use the bytecode for both code age/OSR resetting, and pushing onto
    // the frame, so load it into a register.
    Register bytecode_array = descriptor.GetRegisterParameter(
        BaselineOutOfLinePrologueDescriptor::kInterpreterBytecodeArray);
    ResetBytecodeAge(masm, bytecode_array);
    __ Push(argc, bytecode_array);
    // Baseline code frames store the feedback vector where interpreter would
    // store the bytecode offset.
    {
      UseScratchRegisterScope temp(masm);
      Register type = temps.Acquire();
      __ AssertFeedbackVector(feedback_vector, type);
    }
    // Our stack is currently aligned. We have have to push something along with
    // the feedback vector to keep it that way -- we may as well start
    // initialising the register frame.
    // TODO(v8:11429,leszeks): Consider guaranteeing that this call leaves
    // `undefined` in the accumulator register, to skip the load in the baseline
    // code.
    __ Push(feedback_vector);
  }

  Label call_stack_guard;
  Register frame_size = descriptor.GetRegisterParameter(
      BaselineOutOfLinePrologueDescriptor::kStackFrameSize);
  {
    ASM_CODE_COMMENT_STRING(masm, "Stack/interrupt check");
    // Stack check. This folds the checks for both the interrupt stack limit
    // check and the real stack limit into one by just checking for the
    // interrupt limit. The interrupt limit is either equal to the real stack
    // limit or tighter. By ensuring we have space until that limit after
    // building the frame we can quickly precheck both at once.
    UseScratchRegisterScope temps(masm);
    Register sp_minus_frame_size = temps.Acquire();
    __ SubWord(sp_minus_frame_size, sp, frame_size);
    Register interrupt_limit = temps.Acquire();
    __ LoadStackLimit(interrupt_limit,
                      MacroAssembler::StackLimitKind::kInterruptStackLimit);
    __ Branch(&call_stack_guard, Uless, sp_minus_frame_size,
              Operand(interrupt_limit));
  }

  // Do "fast" return to the caller pc in lr.
  // TODO(v8:11429): Document this frame setup better.
  __ Ret();

  __ bind(&flags_need_processing);
  {
    ASM_CODE_COMMENT_STRING(masm, "Optimized marker check");
    // Drop the frame created by the baseline call.
    __ Pop(ra, fp);
    __ OptimizeCodeOrTailCallOptimizedCodeSlot(flags, feedback_vector);
    __ Trap();
  }

  __ bind(&call_stack_guard);
  {
    ASM_CODE_COMMENT_STRING(masm, "Stack/interrupt call");
    FrameScope frame_scope(masm, StackFrame::INTERNAL);
    // Save incoming new target or generator
    __ Push(kJavaScriptCallNewTargetRegister);
    __ SmiTag(frame_size);
    __ Push(frame_size);
    __ CallRuntime(Runtime::kStackGuardWithGap);
    __ Pop(kJavaScriptCallNewTargetRegister);
  }
  __ Ret();
  temps.Exclude({kScratchReg, kScratchReg2});
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.
//
// The live registers are:
//   o a0 : actual argument count
//   o a1: the JS function object being called.
//   o a3: the incoming new target or generator object
//   o cp: our context
//   o fp: the caller's frame pointer
//   o sp: stack pointer
//   o ra: return address
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames-constants.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(
    MacroAssembler* masm, InterpreterEntryTrampolineMode mode) {
  Register closure = a1;
  // Get the bytecode array from the function object and load it into
  // kInterpreterBytecodeArrayRegister.
  __ LoadTaggedField(
      kScratchReg,
      FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  __ LoadTaggedField(
      kInterpreterBytecodeArrayRegister,
      FieldMemOperand(kScratchReg, SharedFunctionInfo::kFunctionDataOffset));
  Label is_baseline;
  GetSharedFunctionInfoBytecodeOrBaseline(
      masm, kInterpreterBytecodeArrayRegister, kScratchReg, &is_baseline);

  // The bytecode array could have been flushed from the shared function info,
  // if so, call into CompileLazy.
  Label compile_lazy;
  __ GetObjectType(kInterpreterBytecodeArrayRegister, kScratchReg, kScratchReg);
  __ Branch(&compile_lazy, ne, kScratchReg, Operand(BYTECODE_ARRAY_TYPE));

#ifndef V8_JITLESS
  Register feedback_vector = a2;
  // Load the feedback vector from the closure.
  __ LoadTaggedField(feedback_vector,
                     FieldMemOperand(closure, JSFunction::kFeedbackCellOffset));
  __ LoadTaggedField(feedback_vector,
                     FieldMemOperand(feedback_vector, Cell::kValueOffset));

  Label push_stack_frame;
  // Check if feedback vector is valid. If valid, check for optimized code
  // and update invocation count. Otherwise, setup the stack frame.
  __ LoadTaggedField(a4,
                     FieldMemOperand(feedback_vector, HeapObject::kMapOffset));
  __ Lhu(a4, FieldMemOperand(a4, Map::kInstanceTypeOffset));
  __ Branch(&push_stack_frame, ne, a4, Operand(FEEDBACK_VECTOR_TYPE),
            Label::Distance::kNear);

  // Check the tiering state.
  Label flags_need_processing;
  Register flags = a4;
  __ LoadFeedbackVectorFlagsAndJumpIfNeedsProcessing(
      flags, feedback_vector, CodeKind::INTERPRETED_FUNCTION,
      &flags_need_processing);
  {
    UseScratchRegisterScope temps(masm);
    ResetFeedbackVectorOsrUrgency(masm, feedback_vector, temps.Acquire());
  }
  Label not_optimized;
  __ bind(&not_optimized);

  // Increment invocation count for the function.
  __ Lw(a4, FieldMemOperand(feedback_vector,
                            FeedbackVector::kInvocationCountOffset));
  __ Add32(a4, a4, Operand(1));
  __ Sw(a4, FieldMemOperand(feedback_vector,
                            FeedbackVector::kInvocationCountOffset));

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  __ bind(&push_stack_frame);
#else
  // Note: By omitting the above code in jitless mode we also disable:
  // - kFlagsLogNextExecution: only used for logging/profiling; and
  // - kInvocationCountOffset: only used for tiering heuristics and code
  //   coverage.
#endif  // !V8_JITLESS
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ PushStandardFrame(closure);

  ResetBytecodeAge(masm, kInterpreterBytecodeArrayRegister);

  // Load initial bytecode offset.
  __ li(kInterpreterBytecodeOffsetRegister,
        Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Push bytecode array and Smi tagged bytecode array offset.
  __ SmiTag(a4, kInterpreterBytecodeOffsetRegister);
  __ Push(kInterpreterBytecodeArrayRegister, a4);

  // Allocate the local and temporary register file on the stack.
  Label stack_overflow;
  {
    // Load frame size (word) from the BytecodeArray object.
    __ Lw(a4, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                              BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    __ SubWord(a5, sp, Operand(a4));
    __ LoadStackLimit(a2, MacroAssembler::StackLimitKind::kRealStackLimit);
    __ Branch(&stack_overflow, Uless, a5, Operand(a2));

    // If ok, push undefined as the initial value for all register file entries.
    Label loop_header;
    Label loop_check;
    __ LoadRoot(a5, RootIndex::kUndefinedValue);
    __ BranchShort(&loop_check);
    __ bind(&loop_header);
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    __ push(a5);
    // Continue loop if not done.
    __ bind(&loop_check);
    __ SubWord(a4, a4, Operand(kSystemPointerSize));
    __ Branch(&loop_header, ge, a4, Operand(zero_reg));
  }

  // If the bytecode array has a valid incoming new target or generator object
  // register, initialize it with incoming value which was passed in a3.
  Label no_incoming_new_target_or_generator_register;
  __ Lw(a5, FieldMemOperand(
                kInterpreterBytecodeArrayRegister,
                BytecodeArray::kIncomingNewTargetOrGeneratorRegisterOffset));
  __ Branch(&no_incoming_new_target_or_generator_register, eq, a5,
            Operand(zero_reg), Label::Distance::kNear);
  __ CalcScaledAddress(a5, fp, a5, kSystemPointerSizeLog2);
  __ StoreWord(a3, MemOperand(a5));
  __ bind(&no_incoming_new_target_or_generator_register);

  // Perform interrupt stack check.
  // TODO(solanes): Merge with the real stack limit check above.
  Label stack_check_interrupt, after_stack_check_interrupt;
  __ LoadStackLimit(a5, MacroAssembler::StackLimitKind::kInterruptStackLimit);
  __ Branch(&stack_check_interrupt, Uless, sp, Operand(a5),
            Label::Distance::kNear);
  __ bind(&after_stack_check_interrupt);

  // Load accumulator as undefined.
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);

  // Load the dispatch table into a register and dispatch to the bytecode
  // handler at the current bytecode offset.
  Label do_dispatch;
  __ bind(&do_dispatch);
  __ li(kInterpreterDispatchTableRegister,
        ExternalReference::interpreter_dispatch_table_address(masm->isolate()));
  __ AddWord(a1, kInterpreterBytecodeArrayRegister,
             kInterpreterBytecodeOffsetRegister);
  __ Lbu(a7, MemOperand(a1));
  __ CalcScaledAddress(kScratchReg, kInterpreterDispatchTableRegister, a7,
                       kSystemPointerSizeLog2);
  __ LoadWord(kJavaScriptCallCodeStartRegister, MemOperand(kScratchReg));
  __ Call(kJavaScriptCallCodeStartRegister);

  __ RecordComment("--- InterpreterEntryReturnPC point ---");
  if (mode == InterpreterEntryTrampolineMode::kDefault) {
    masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(
        masm->pc_offset());
  } else {
    DCHECK_EQ(mode, InterpreterEntryTrampolineMode::kForProfiling);
    // Both versions must be the same up to this point otherwise the builtins
    // will not be interchangable.
    CHECK_EQ(
        masm->isolate()->heap()->interpreter_entry_return_pc_offset().value(),
        masm->pc_offset());
  }

  // Any returns to the entry trampoline are either due to the return bytecode
  // or the interpreter tail calling a builtin and then a dispatch.

  // Get bytecode array and bytecode offset from the stack frame.
  __ LoadWord(kInterpreterBytecodeArrayRegister,
              MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ LoadWord(kInterpreterBytecodeOffsetRegister,
              MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Either return, or advance to the next bytecode and dispatch.
  Label do_return;
  __ AddWord(a1, kInterpreterBytecodeArrayRegister,
             kInterpreterBytecodeOffsetRegister);
  __ Lbu(a1, MemOperand(a1));
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, a1, a2, a3,
                                a4, &do_return);
  __ Branch(&do_dispatch);

  __ bind(&do_return);
  // The return value is in a0.
  LeaveInterpreterFrame(masm, t0, t1);
  __ Jump(ra);

  __ bind(&stack_check_interrupt);
  // Modify the bytecode offset in the stack to be kFunctionEntryBytecodeOffset
  // for the call to the StackGuard.
  __ li(kInterpreterBytecodeOffsetRegister,
        Operand(Smi::FromInt(BytecodeArray::kHeaderSize - kHeapObjectTag +
                             kFunctionEntryBytecodeOffset)));
  __ StoreWord(
      kInterpreterBytecodeOffsetRegister,
      MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ CallRuntime(Runtime::kStackGuard);

  // After the call, restore the bytecode array, bytecode offset and accumulator
  // registers again. Also, restore the bytecode offset in the stack to its
  // previous value.
  __ LoadWord(kInterpreterBytecodeArrayRegister,
              MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ li(kInterpreterBytecodeOffsetRegister,
        Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);

  __ SmiTag(a5, kInterpreterBytecodeOffsetRegister);
  __ StoreWord(
      a5, MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  __ Branch(&after_stack_check_interrupt);

#ifndef V8_JITLESS
  __ bind(&flags_need_processing);
  __ OptimizeCodeOrTailCallOptimizedCodeSlot(flags, feedback_vector);
  __ bind(&is_baseline);
  {
    // Load the feedback vector from the closure.
    __ LoadTaggedField(
        feedback_vector,
        FieldMemOperand(closure, JSFunction::kFeedbackCellOffset));
    __ LoadTaggedField(feedback_vector,
                       FieldMemOperand(feedback_vector, Cell::kValueOffset));

    Label install_baseline_code;
    // Check if feedback vector is valid. If not, call prepare for baseline to
    // allocate it.
    __ LoadTaggedField(
        t0, FieldMemOperand(feedback_vector, HeapObject::kMapOffset));
    __ Lhu(t0, FieldMemOperand(t0, Map::kInstanceTypeOffset));
    __ Branch(&install_baseline_code, ne, t0, Operand(FEEDBACK_VECTOR_TYPE));

    // Check for an tiering state.
    __ LoadFeedbackVectorFlagsAndJumpIfNeedsProcessing(
        flags, feedback_vector, CodeKind::BASELINE, &flags_need_processing);

    // Load the baseline code into the closure.
    __ Move(a2, kInterpreterBytecodeArrayRegister);
    static_assert(kJavaScriptCallCodeStartRegister == a2, "ABI mismatch");
    __ ReplaceClosureCodeWithOptimizedCode(a2, closure);
    __ JumpCodeObject(a2);

    __ bind(&install_baseline_code);
    __ GenerateTailCallToReturnedCode(Runtime::kInstallBaselineCode);
  }
#endif  // !V8_JITLESS

  __ bind(&compile_lazy);
  __ GenerateTailCallToReturnedCode(Runtime::kCompileLazy);
  // Unreachable code.
  __ break_(0xCC);

  __ bind(&stack_overflow);
  __ CallRuntime(Runtime::kThrowStackOverflow);
  // Unreachable code.
  __ break_(0xCC);
}

static void GenerateInterpreterPushArgs(MacroAssembler* masm, Register num_args,
                                        Register start_address,
                                        Register scratch) {
  ASM_CODE_COMMENT(masm);
  // Find the address of the last argument.
  __ SubWord(scratch, num_args, Operand(1));
  __ SllWord(scratch, scratch, kSystemPointerSizeLog2);
  __ SubWord(start_address, start_address, scratch);

  // Push the arguments.
  __ PushArray(start_address, num_args,
               MacroAssembler::PushArrayOrder::kReverse);
}

// static
void Builtins::Generate_InterpreterPushArgsThenCallImpl(
    MacroAssembler* masm, ConvertReceiverMode receiver_mode,
    InterpreterPushArgsMode mode) {
  DCHECK(mode != InterpreterPushArgsMode::kArrayFunction);
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments
  //  -- a2 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- a1 : the target to call (can be any Object).
  // -----------------------------------
  Label stack_overflow;
  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // The spread argument should not be pushed.
    __ SubWord(a0, a0, Operand(1));
  }

  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ SubWord(a3, a0, Operand(kJSArgcReceiverSlots));
  } else {
    __ Move(a3, a0);
  }
  __ StackOverflowCheck(a3, a4, t0, &stack_overflow);

  // This function modifies a2 and a4.
  GenerateInterpreterPushArgs(masm, a3, a2, a4);
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ PushRoot(RootIndex::kUndefinedValue);
  }

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Pass the spread in the register a2.
    // a2 already points to the penultime argument, the spread
    // is below that.
    __ LoadWord(a2, MemOperand(a2, -kSystemPointerSize));
  }

  // Call the target.
  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Jump(BUILTIN_CODE(masm->isolate(), CallWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    __ Jump(masm->isolate()->builtins()->Call(ConvertReceiverMode::kAny),
            RelocInfo::CODE_TARGET);
  }

  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable code.
    __ break_(0xCC);
  }
}

// static
void Builtins::Generate_InterpreterPushArgsThenConstructImpl(
    MacroAssembler* masm, InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  // -- a0 : argument count
  // -- a3 : new target
  // -- a1 : constructor to call
  // -- a2 : allocation site feedback if available, undefined otherwise.
  // -- a4 : address of the first argument
  // -----------------------------------
  Label stack_overflow;
  __ StackOverflowCheck(a0, a5, t0, &stack_overflow);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // The spread argument should not be pushed.
    __ SubWord(a0, a0, Operand(1));
  }
  Register argc_without_receiver = a6;
  __ SubWord(argc_without_receiver, a0, Operand(kJSArgcReceiverSlots));
  // Push the arguments, This function modifies a4 and a5.
  GenerateInterpreterPushArgs(masm, argc_without_receiver, a4, a5);

  // Push a slot for the receiver.
  __ push(zero_reg);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Pass the spread in the register a2.
    // a4 already points to the penultimate argument, the spread
    // lies in the next interpreter register.
    __ LoadWord(a2, MemOperand(a4, -kSystemPointerSize));
  } else {
    __ AssertUndefinedOrAllocationSite(a2, t0);
  }

  if (mode == InterpreterPushArgsMode::kArrayFunction) {
    __ AssertFunction(a1);

    // Tail call to the function-specific construct stub (still in the caller
    // context at this point).
    __ Jump(BUILTIN_CODE(masm->isolate(), ArrayConstructorImpl),
            RelocInfo::CODE_TARGET);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Call the constructor with a0, a1, and a3 unmodified.
    __ Jump(BUILTIN_CODE(masm->isolate(), ConstructWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(InterpreterPushArgsMode::kOther, mode);
    // Call the constructor with a0, a1, and a3 unmodified.
    __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
  }

  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable code.
    __ break_(0xCC);
  }
}

static void Generate_InterpreterEnterBytecode(MacroAssembler* masm) {
  // Set the return address to the correct point in the interpreter entry
  // trampoline.
  Label builtin_trampoline, trampoline_loaded;
  Smi interpreter_entry_return_pc_offset(
      masm->isolate()->heap()->interpreter_entry_return_pc_offset());
  DCHECK_NE(interpreter_entry_return_pc_offset, Smi::zero());

  // If the SFI function_data is an InterpreterData, the function will have a
  // custom copy of the interpreter entry trampoline for profiling. If so,
  // get the custom trampoline, otherwise grab the entry address of the global
  // trampoline.
  __ LoadWord(t0, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
  __ LoadTaggedField(
      t0, FieldMemOperand(t0, JSFunction::kSharedFunctionInfoOffset));
  __ LoadTaggedField(
      t0, FieldMemOperand(t0, SharedFunctionInfo::kFunctionDataOffset));
  __ GetObjectType(t0, kInterpreterDispatchTableRegister,
                   kInterpreterDispatchTableRegister);
  __ Branch(&builtin_trampoline, ne, kInterpreterDispatchTableRegister,
            Operand(INTERPRETER_DATA_TYPE), Label::Distance::kNear);

  __ LoadTaggedField(
      t0, FieldMemOperand(t0, InterpreterData::kInterpreterTrampolineOffset));
  __ LoadCodeInstructionStart(t0, t0);
  __ BranchShort(&trampoline_loaded);

  __ bind(&builtin_trampoline);
  __ li(t0, ExternalReference::
                address_of_interpreter_entry_trampoline_instruction_start(
                    masm->isolate()));
  __ LoadWord(t0, MemOperand(t0));

  __ bind(&trampoline_loaded);
  __ AddWord(ra, t0, Operand(interpreter_entry_return_pc_offset.value()));

  // Initialize the dispatch table register.
  __ li(kInterpreterDispatchTableRegister,
        ExternalReference::interpreter_dispatch_table_address(masm->isolate()));

  // Get the bytecode array pointer from the frame.
  __ LoadWord(kInterpreterBytecodeArrayRegister,
              MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (v8_flags.debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ SmiTst(kInterpreterBytecodeArrayRegister, kScratchReg);
    __ Assert(ne,
              AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry,
              kScratchReg, Operand(zero_reg));
    __ GetObjectType(kInterpreterBytecodeArrayRegister, a1, a1);
    __ Assert(eq,
              AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry,
              a1, Operand(BYTECODE_ARRAY_TYPE));
  }

  // Get the target bytecode offset from the frame.
  __ SmiUntag(kInterpreterBytecodeOffsetRegister,
              MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  if (v8_flags.debug_code) {
    Label okay;
    __ Branch(&okay, ge, kInterpreterBytecodeOffsetRegister,
              Operand(BytecodeArray::kHeaderSize - kHeapObjectTag),
              Label::Distance::kNear);
    // Unreachable code.
    __ break_(0xCC);
    __ bind(&okay);
  }

  // Dispatch to the target bytecode.
  __ AddWord(a1, kInterpreterBytecodeArrayRegister,
             kInterpreterBytecodeOffsetRegister);
  __ Lbu(a7, MemOperand(a1));
  __ CalcScaledAddress(a1, kInterpreterDispatchTableRegister, a7,
                       kSystemPointerSizeLog2);
  __ LoadWord(kJavaScriptCallCodeStartRegister, MemOperand(a1));
  __ Jump(kJavaScriptCallCodeStartRegister);
}

void Builtins::Generate_InterpreterEnterAtNextBytecode(MacroAssembler* masm) {
  // Advance the current bytecode offset stored within the given interpreter
  // stack frame. This simulates what all bytecode handlers do upon completion
  // of the underlying operation.
  __ LoadWord(kInterpreterBytecodeArrayRegister,
              MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ LoadWord(kInterpreterBytecodeOffsetRegister,
              MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  Label enter_bytecode, function_entry_bytecode;
  __ Branch(&function_entry_bytecode, eq, kInterpreterBytecodeOffsetRegister,
            Operand(BytecodeArray::kHeaderSize - kHeapObjectTag +
                    kFunctionEntryBytecodeOffset));

  // Load the current bytecode.
  __ AddWord(a1, kInterpreterBytecodeArrayRegister,
             kInterpreterBytecodeOffsetRegister);
  __ Lbu(a1, MemOperand(a1));

  // Advance to the next bytecode.
  Label if_return;
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, a1, a2, a3,
                                a4, &if_return);

  __ bind(&enter_bytecode);
  // Convert new bytecode offset to a Smi and save in the stackframe.
  __ SmiTag(a2, kInterpreterBytecodeOffsetRegister);
  __ StoreWord(
      a2, MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  Generate_InterpreterEnterBytecode(masm);

  __ bind(&function_entry_bytecode);
  // If the code deoptimizes during the implicit function entry stack interrupt
  // check, it will have a bailout ID of kFunctionEntryBytecodeOffset, which is
  // not a valid bytecode offset. Detect this case and advance to the first
  // actual bytecode.
  __ li(kInterpreterBytecodeOffsetRegister,
        Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));
  __ Branch(&enter_bytecode);

  // We should never take the if_return path.
  __ bind(&if_return);
  __ Abort(AbortReason::kInvalidBytecodeAdvance);
}

void Builtins::Generate_InterpreterEnterAtBytecode(MacroAssembler* masm) {
  Generate_InterpreterEnterBytecode(masm);
}

namespace {
void Generate_ContinueToBuiltinHelper(MacroAssembler* masm,
                                      bool java_script_builtin,
                                      bool with_result) {
  const RegisterConfiguration* config(RegisterConfiguration::Default());
  int allocatable_register_count = config->num_allocatable_general_registers();
  UseScratchRegisterScope temp(masm);
  Register scratch = temp.Acquire();
  if (with_result) {
    if (java_script_builtin) {
      __ Move(scratch, a0);
    } else {
      // Overwrite the hole inserted by the deoptimizer with the return value
      // from the LAZY deopt point.
      __ StoreWord(
          a0, MemOperand(
                  sp, config->num_allocatable_general_registers() *
                              kSystemPointerSize +
                          BuiltinContinuationFrameConstants::kFixedFrameSize));
    }
  }
  for (int i = allocatable_register_count - 1; i >= 0; --i) {
    int code = config->GetAllocatableGeneralCode(i);
    __ Pop(Register::from_code(code));
    if (java_script_builtin && code == kJavaScriptCallArgCountRegister.code()) {
      __ SmiUntag(Register::from_code(code));
    }
  }

  if (with_result && java_script_builtin) {
    // Overwrite the hole inserted by the deoptimizer with the return value from
    // the LAZY deopt point. t0 contains the arguments count, the return value
    // from LAZY is always the last argument.
    constexpr int return_value_offset =
        BuiltinContinuationFrameConstants::kFixedSlotCount -
        kJSArgcReceiverSlots;
    __ AddWord(a0, a0, Operand(return_value_offset));
    __ CalcScaledAddress(t0, sp, a0, kSystemPointerSizeLog2);
    __ StoreWord(scratch, MemOperand(t0));
    // Recover arguments count.
    __ SubWord(a0, a0, Operand(return_value_offset));
  }

  __ LoadWord(
      fp,
      MemOperand(sp, BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  // Load builtin index (stored as a Smi) and use it to get the builtin start
  // address from the builtins table.
  __ Pop(t6);
  __ AddWord(sp, sp,
             Operand(BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  __ Pop(ra);
  __ LoadEntryFromBuiltinIndex(t6);
  __ Jump(t6);
}
}  // namespace

void Builtins::Generate_ContinueToCodeStubBuiltin(MacroAssembler* masm) {
  Generate_ContinueToBuiltinHelper(masm, false, false);
}

void Builtins::Generate_ContinueToCodeStubBuiltinWithResult(
    MacroAssembler* masm) {
  Generate_ContinueToBuiltinHelper(masm, false, true);
}

void Builtins::Generate_ContinueToJavaScriptBuiltin(MacroAssembler* masm) {
  Generate_ContinueToBuiltinHelper(masm, true, false);
}

void Builtins::Generate_ContinueToJavaScriptBuiltinWithResult(
    MacroAssembler* masm) {
  Generate_ContinueToBuiltinHelper(masm, true, true);
}

void Builtins::Generate_NotifyDeoptimized(MacroAssembler* masm) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kNotifyDeoptimized);
  }

  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), a0.code());
  __ LoadWord(a0, MemOperand(sp, 0 * kSystemPointerSize));
  __ AddWord(sp, sp, Operand(1 * kSystemPointerSize));  // Remove state.
  __ Ret();
}

namespace {

void Generate_OSREntry(MacroAssembler* masm, Register entry_address,
                       Operand offset = Operand(0)) {
  __ AddWord(ra, entry_address, offset);
  // And "return" to the OSR entry point of the function.
  __ Ret();
}

enum class OsrSourceTier {
  kInterpreter,
  kBaseline,
};

void OnStackReplacement(MacroAssembler* masm, OsrSourceTier source,
                        Register maybe_target_code) {
  Label jump_to_optimized_code;
  {
    // If maybe_target_code is not null, no need to call into runtime. A
    // precondition here is: if maybe_target_code is a InstructionStream object,
    // it must NOT be marked_for_deoptimization (callers must ensure this).
    __ Branch(&jump_to_optimized_code, ne, maybe_target_code,
              Operand(Smi::zero()));
  }
  ASM_CODE_COMMENT(masm);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kCompileOptimizedOSR);
  }

  // If the code object is null, just return to the caller.
  __ Ret(eq, a0, Operand(Smi::zero()));
  __ bind(&jump_to_optimized_code);

  // OSR entry tracing.
  {
    Label next;
    __ li(a1, ExternalReference::address_of_log_or_trace_osr());
    __ Lbu(a1, MemOperand(a1));
    __ Branch(&next, eq, a1, Operand(zero_reg));

    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(a0);  // Preserve the code object.
      __ CallRuntime(Runtime::kLogOrTraceOptimizedOSREntry, 0);
      __ Pop(a0);
    }

    __ bind(&next);
  }

  if (source == OsrSourceTier::kInterpreter) {
    // Drop the handler frame that is be sitting on top of the actual
    // JavaScript frame. This is the case then OSR is triggered from bytecode.
    __ LeaveFrame(StackFrame::STUB);
  }

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ LoadTaggedField(
      a1, MemOperand(a0, Code::kDeoptimizationDataOrInterpreterDataOffset -
                             kHeapObjectTag));

  // Load the OSR entrypoint offset from the deoptimization data.
  // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
  __ SmiUntag(a1, MemOperand(a1, FixedArray::OffsetOfElementAt(
                                     DeoptimizationData::kOsrPcOffsetIndex) -
                                     kHeapObjectTag));

  __ LoadCodeInstructionStart(a0, a0);

  // Compute the target address = code_entry + osr_offset
  // <entry_addr> = <code_entry> + <osr_offset>
  Generate_OSREntry(masm, a0, Operand(a1));
}
}  // namespace

void Builtins::Generate_InterpreterOnStackReplacement(MacroAssembler* masm) {
  using D = OnStackReplacementDescriptor;
  static_assert(D::kParameterCount == 1);
  OnStackReplacement(masm, OsrSourceTier::kInterpreter,
                     D::MaybeTargetCodeRegister());
}

void Builtins::Generate_BaselineOnStackReplacement(MacroAssembler* masm) {
  using D = OnStackReplacementDescriptor;
  static_assert(D::kParameterCount == 1);

  __ LoadWord(kContextRegister,
              MemOperand(fp, BaselineFrameConstants::kContextOffset));
  OnStackReplacement(masm, OsrSourceTier::kBaseline,
                     D::MaybeTargetCodeRegister());
}

// static
void Builtins::Generate_FunctionPrototypeApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0    : argc
  //  -- sp[0] : receiver
  //  -- sp[4] : thisArg
  //  -- sp[8] : argArray
  // -----------------------------------

  Register argc = a0;
  Register arg_array = a2;
  Register receiver = a1;
  Register this_arg = a5;
  Register undefined_value = a3;

  __ LoadRoot(undefined_value, RootIndex::kUndefinedValue);

  // 1. Load receiver into a1, argArray into a2 (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    // Claim (2 - argc) dummy arguments form the stack, to put the stack in a
    // consistent state for a simple pop operation.

    __ LoadWord(this_arg, MemOperand(sp, kSystemPointerSize));
    __ LoadWord(arg_array, MemOperand(sp, 2 * kSystemPointerSize));

    Label done0, done1;
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ SubWord(scratch, argc, JSParameterCount(0));
    __ Branch(&done0, ne, scratch, Operand(zero_reg), Label::Distance::kNear);
    __ Move(arg_array, undefined_value);  // if argc == 0
    __ Move(this_arg, undefined_value);   // if argc == 0
    __ bind(&done0);                      // else (i.e., argc > 0)

    __ Branch(&done1, ne, scratch, Operand(1), Label::Distance::kNear);
    __ Move(arg_array, undefined_value);  // if argc == 1
    __ bind(&done1);                      // else (i.e., argc > 1)

    __ LoadWord(receiver, MemOperand(sp));
    __ DropArgumentsAndPushNewReceiver(argc, this_arg,
                                       MacroAssembler::kCountIsInteger,
                                       MacroAssembler::kCountIncludesReceiver);
  }

  // ----------- S t a t e -------------
  //  -- a2    : argArray
  //  -- a1    : receiver
  //  -- a3    : undefined root value
  //  -- sp[0] : thisArg
  // -----------------------------------

  // 2. We don't need to check explicitly for callable receiver here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(arg_array, RootIndex::kNullValue, &no_arguments);
  __ Branch(&no_arguments, eq, arg_array, Operand(undefined_value),
            Label::Distance::kNear);

  // 4a. Apply the receiver to the given argArray.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ bind(&no_arguments);
  {
    __ li(a0, JSParameterCount(0));
    DCHECK(receiver == a1);
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
  // 1. Get the callable to call (passed as receiver) from the stack.
  { __ Pop(a1); }

  // 2. Make sure we have at least one argument.
  // a0: actual number of arguments
  {
    Label done;
    __ Branch(&done, ne, a0, Operand(JSParameterCount(0)),
              Label::Distance::kNear);
    __ PushRoot(RootIndex::kUndefinedValue);
    __ AddWord(a0, a0, Operand(1));
    __ bind(&done);
  }

  // 3. Adjust the actual number of arguments.
  __ AddWord(a0, a0, -1);

  // 4. Call the callable.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : argc
  //  -- sp[0]  : receiver
  //  -- sp[8]  : target         (if argc >= 1)
  //  -- sp[16] : thisArgument   (if argc >= 2)
  //  -- sp[24] : argumentsList  (if argc == 3)
  // -----------------------------------

  Register argc = a0;
  Register arguments_list = a2;
  Register target = a1;
  Register this_argument = a5;
  Register undefined_value = a3;

  __ LoadRoot(undefined_value, RootIndex::kUndefinedValue);

  // 1. Load target into a1 (if present), argumentsList into a2 (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    // Claim (3 - argc) dummy arguments form the stack, to put the stack in a
    // consistent state for a simple pop operation.

    __ LoadWord(target, MemOperand(sp, kSystemPointerSize));
    __ LoadWord(this_argument, MemOperand(sp, 2 * kSystemPointerSize));
    __ LoadWord(arguments_list, MemOperand(sp, 3 * kSystemPointerSize));

    Label done0, done1, done2;
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ SubWord(scratch, argc, Operand(JSParameterCount(0)));
    __ Branch(&done0, ne, scratch, Operand(zero_reg), Label::Distance::kNear);
    __ Move(arguments_list, undefined_value);  // if argc == 0
    __ Move(this_argument, undefined_value);   // if argc == 0
    __ Move(target, undefined_value);          // if argc == 0
    __ bind(&done0);                           // argc != 0

    __ Branch(&done1, ne, scratch, Operand(1), Label::Distance::kNear);
    __ Move(arguments_list, undefined_value);  // if argc == 1
    __ Move(this_argument, undefined_value);   // if argc == 1
    __ bind(&done1);                           // argc > 1

    __ Branch(&done2, ne, scratch, Operand(2), Label::Distance::kNear);
    __ Move(arguments_list, undefined_value);  // if argc == 2
    __ bind(&done2);                           // argc > 2

    __ DropArgumentsAndPushNewReceiver(argc, this_argument,
                                       MacroAssembler::kCountIsInteger,
                                       MacroAssembler::kCountIncludesReceiver);
  }

  // ----------- S t a t e -------------
  //  -- a2    : argumentsList
  //  -- a1    : target
  //  -- a3    : undefined root value
  //  -- sp[0] : thisArgument
  // -----------------------------------

  // 2. We don't need to check explicitly for callable target here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3. Apply the target to the given argumentsList.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectConstruct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : argc
  //  -- sp[0]   : receiver
  //  -- sp[8]   : target
  //  -- sp[16]  : argumentsList
  //  -- sp[24]  : new.target (optional)
  // -----------------------------------
  Register argc = a0;
  Register arguments_list = a2;
  Register target = a1;
  Register new_target = a3;
  Register undefined_value = a4;

  __ LoadRoot(undefined_value, RootIndex::kUndefinedValue);

  // 1. Load target into a1 (if present), argumentsList into a2 (if present),
  // new.target into a3 (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    // Claim (3 - argc) dummy arguments form the stack, to put the stack in a
    // consistent state for a simple pop operation.
    __ LoadWord(target, MemOperand(sp, kSystemPointerSize));
    __ LoadWord(arguments_list, MemOperand(sp, 2 * kSystemPointerSize));
    __ LoadWord(new_target, MemOperand(sp, 3 * kSystemPointerSize));

    Label done0, done1, done2;
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ SubWord(scratch, argc, Operand(JSParameterCount(0)));
    __ Branch(&done0, ne, scratch, Operand(zero_reg), Label::Distance::kNear);
    __ Move(arguments_list, undefined_value);  // if argc == 0
    __ Move(new_target, undefined_value);      // if argc == 0
    __ Move(target, undefined_value);          // if argc == 0
    __ bind(&done0);

    __ Branch(&done1, ne, scratch, Operand(1), Label::Distance::kNear);
    __ Move(arguments_list, undefined_value);  // if argc == 1
    __ Move(new_target, target);               // if argc == 1
    __ bind(&done1);

    __ Branch(&done2, ne, scratch, Operand(2), Label::Distance::kNear);
    __ Move(new_target, target);  // if argc == 2
    __ bind(&done2);

    __ DropArgumentsAndPushNewReceiver(argc, undefined_value,
                                       MacroAssembler::kCountIsInteger,
                                       MacroAssembler::kCountIncludesReceiver);
  }

  // ----------- S t a t e -------------
  //  -- a2    : argumentsList
  //  -- a1    : target
  //  -- a3    : new.target
  //  -- sp[0] : receiver (undefined)
  // -----------------------------------

  // 2. We don't need to check explicitly for constructor target here,
  // since that's the first thing the Construct/ConstructWithArrayLike
  // builtins will do.

  // 3. We don't need to check explicitly for constructor new.target here,
  // since that's the second thing the Construct/ConstructWithArrayLike
  // builtins will do.

  // 4. Construct the target with the given new.target and argumentsList.
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructWithArrayLike),
          RelocInfo::CODE_TARGET);
}

namespace {

// Allocate new stack space for |count| arguments and shift all existing
// arguments already on the stack. |pointer_to_new_space_out| points to the
// first free slot on the stack to copy additional arguments to and
// |argc_in_out| is updated to include |count|.
void Generate_AllocateSpaceAndShiftExistingArguments(
    MacroAssembler* masm, Register count, Register argc_in_out,
    Register pointer_to_new_space_out) {
  UseScratchRegisterScope temps(masm);
  Register scratch1 = temps.Acquire();
  Register scratch2 = temps.Acquire();
  Register scratch3 = temps.Acquire();
  DCHECK(!AreAliased(count, argc_in_out, pointer_to_new_space_out, scratch1,
                     scratch2));
  Register old_sp = scratch1;
  Register new_space = scratch2;
  __ mv(old_sp, sp);
  __ slli(new_space, count, kPointerSizeLog2);
  __ SubWord(sp, sp, Operand(new_space));

  Register end = scratch2;
  Register value = scratch3;
  Register dest = pointer_to_new_space_out;
  __ mv(dest, sp);
  __ CalcScaledAddress(end, old_sp, argc_in_out, kSystemPointerSizeLog2);
  Label loop, done;
  __ Branch(&done, ge, old_sp, Operand(end));
  __ bind(&loop);
  __ LoadWord(value, MemOperand(old_sp, 0));
  __ StoreWord(value, MemOperand(dest, 0));
  __ AddWord(old_sp, old_sp, Operand(kSystemPointerSize));
  __ AddWord(dest, dest, Operand(kSystemPointerSize));
  __ Branch(&loop, lt, old_sp, Operand(end));
  __ bind(&done);

  // Update total number of arguments.
  __ AddWord(argc_in_out, argc_in_out, count);
}

}  // namespace

// static
void Builtins::Generate_CallOrConstructVarargs(MacroAssembler* masm,
                                               Handle<Code> code) {
  UseScratchRegisterScope temps(masm);
  temps.Include(t1, t0);
  // ----------- S t a t e -------------
  //  -- a1 : target
  //  -- a0 : number of parameters on the stack
  //  -- a2 : arguments list (a FixedArray)
  //  -- a4 : len (number of elements to push from args)
  //  -- a3 : new.target (for [[Construct]])
  // -----------------------------------
  if (v8_flags.debug_code) {
    // Allow a2 to be a FixedArray, or a FixedDoubleArray if a4 == 0.
    Label ok, fail;
    __ AssertNotSmi(a2);
    __ GetObjectType(a2, kScratchReg, kScratchReg);
    __ Branch(&ok, eq, kScratchReg, Operand(FIXED_ARRAY_TYPE),
              Label::Distance::kNear);
    __ Branch(&fail, ne, kScratchReg, Operand(FIXED_DOUBLE_ARRAY_TYPE),
              Label::Distance::kNear);
    __ Branch(&ok, eq, a4, Operand(zero_reg), Label::Distance::kNear);
    // Fall through.
    __ bind(&fail);
    __ Abort(AbortReason::kOperandIsNotAFixedArray);

    __ bind(&ok);
  }

  Register args = a2;
  Register len = a4;

  // Check for stack overflow.
  Label stack_overflow;
  __ StackOverflowCheck(len, kScratchReg, a5, &stack_overflow);

  // Move the arguments already in the stack,
  // including the receiver and the return address.
  // a4: Number of arguments to make room for.
  // a0: Number of arguments already on the stack.
  // a7: Points to first free slot on the stack after arguments were shifted.
  Generate_AllocateSpaceAndShiftExistingArguments(masm, a4, a0, a7);

  // Push arguments onto the stack (thisArgument is already on the stack).
  {
    Label done, push, loop;
    Register src = a6;
    Register scratch = len;
    UseScratchRegisterScope temps(masm);
    Register hole_value = temps.Acquire();
    __ AddWord(src, args, FixedArray::kHeaderSize - kHeapObjectTag);
    __ Branch(&done, eq, len, Operand(zero_reg), Label::Distance::kNear);
    __ SllWord(scratch, len, kTaggedSizeLog2);
    __ SubWord(scratch, sp, Operand(scratch));
    __ LoadRoot(hole_value, RootIndex::kTheHoleValue);
    __ bind(&loop);
    __ LoadTaggedField(a5, MemOperand(src));
    __ AddWord(src, src, kTaggedSize);
    __ Branch(&push, ne, a5, Operand(hole_value), Label::Distance::kNear);
    __ LoadRoot(a5, RootIndex::kUndefinedValue);
    __ bind(&push);
    __ StoreWord(a5, MemOperand(a7, 0));
    __ AddWord(a7, a7, Operand(kSystemPointerSize));
    __ AddWord(scratch, scratch, Operand(kTaggedSize));
    __ Branch(&loop, ne, scratch, Operand(sp));
    __ bind(&done);
  }

  // Tail-call to the actual Call or Construct builtin.
  __ Jump(code, RelocInfo::CODE_TARGET);

  __ bind(&stack_overflow);
  __ TailCallRuntime(Runtime::kThrowStackOverflow);
}

// static
void Builtins::Generate_CallOrConstructForwardVarargs(MacroAssembler* masm,
                                                      CallOrConstructMode mode,
                                                      Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments
  //  -- a3 : the new.target (for [[Construct]] calls)
  //  -- a1 : the target to call (can be any Object)
  //  -- a2 : start index (to support rest parameters)
  // -----------------------------------
  UseScratchRegisterScope temps(masm);
  temps.Include(t0, t1);
  temps.Include(t2);
  // Check if new.target has a [[Construct]] internal method.
  if (mode == CallOrConstructMode::kConstruct) {
    Label new_target_constructor, new_target_not_constructor;
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ JumpIfSmi(a3, &new_target_not_constructor);
    __ LoadTaggedField(scratch, FieldMemOperand(a3, HeapObject::kMapOffset));
    __ Lbu(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ And(scratch, scratch, Operand(Map::Bits1::IsConstructorBit::kMask));
    __ Branch(&new_target_constructor, ne, scratch, Operand(zero_reg),
              Label::Distance::kNear);
    __ bind(&new_target_not_constructor);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ Push(a3);
      __ CallRuntime(Runtime::kThrowNotConstructor);
    }
    __ bind(&new_target_constructor);
  }

  __ Move(a6, fp);
  __ LoadWord(a7, MemOperand(fp, StandardFrameConstants::kArgCOffset));

  Label stack_done, stack_overflow;
  __ SubWord(a7, a7, Operand(kJSArgcReceiverSlots));
  __ SubWord(a7, a7, a2);
  __ Branch(&stack_done, le, a7, Operand(zero_reg));
  {
    // Check for stack overflow.
    __ StackOverflowCheck(a7, a4, a5, &stack_overflow);

    // Forward the arguments from the caller frame.

    // Point to the first argument to copy (skipping the receiver).
    __ AddWord(a6, a6,
               Operand(CommonFrameConstants::kFixedFrameSizeAboveFp +
                       kSystemPointerSize));
    __ CalcScaledAddress(a6, a6, a2, kSystemPointerSizeLog2);

    // Move the arguments already in the stack,
    // including the receiver and the return address.
    // a7: Number of arguments to make room for.
    // a0: Number of arguments already on the stack.
    // a2: Points to first free slot on the stack after arguments were shifted.
    Generate_AllocateSpaceAndShiftExistingArguments(masm, a7, a0, a2);

    // Copy arguments from the caller frame.
    // TODO(victorgomes): Consider using forward order as potentially more cache
    // friendly.
    {
      Label loop;
      __ bind(&loop);
      {
        UseScratchRegisterScope temps(masm);
        Register scratch = temps.Acquire(), addr = temps.Acquire();
        __ Sub32(a7, a7, Operand(1));
        __ CalcScaledAddress(addr, a6, a7, kSystemPointerSizeLog2);
        __ LoadWord(scratch, MemOperand(addr));
        __ CalcScaledAddress(addr, a2, a7, kSystemPointerSizeLog2);
        __ StoreWord(scratch, MemOperand(addr));
        __ Branch(&loop, ne, a7, Operand(zero_reg));
      }
    }
  }
  __ BranchShort(&stack_done);
  __ bind(&stack_overflow);
  __ TailCallRuntime(Runtime::kThrowStackOverflow);
  __ bind(&stack_done);

  // Tail-call to the {code} handler.
  __ Jump(code, RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_CallFunction(MacroAssembler* masm,
                                     ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments
  //  -- a1 : the function to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertCallableFunction(a1);

  Label class_constructor;
  __ LoadTaggedField(
      a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ Load32U(a3, FieldMemOperand(a2, SharedFunctionInfo::kFlagsOffset));
  __ And(kScratchReg, a3,
         Operand(SharedFunctionInfo::IsClassConstructorBit::kMask));
  __ Branch(&class_constructor, ne, kScratchReg, Operand(zero_reg));

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  __ LoadTaggedField(cp, FieldMemOperand(a1, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ Load32U(a3, FieldMemOperand(a2, SharedFunctionInfo::kFlagsOffset));
  __ And(kScratchReg, a3,
         Operand(SharedFunctionInfo::IsNativeBit::kMask |
                 SharedFunctionInfo::IsStrictBit::kMask));
  __ Branch(&done_convert, ne, kScratchReg, Operand(zero_reg));
  {
    // ----------- S t a t e -------------
    //  -- a0 : the number of arguments
    //  -- a1 : the function to call (checked to be a JSFunction)
    //  -- a2 : the shared function info.
    //  -- cp : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(a3);
    } else {
      Label convert_to_object, convert_receiver;
      __ LoadReceiver(a3, a0);
      __ JumpIfSmi(a3, &convert_to_object);
      static_assert(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ GetObjectType(a3, a4, a4);
      __ Branch(&done_convert, Ugreater_equal, a4,
                Operand(FIRST_JS_RECEIVER_TYPE));
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(a3, RootIndex::kUndefinedValue, &convert_global_proxy);
        __ JumpIfNotRoot(a3, RootIndex::kNullValue, &convert_to_object);
        __ bind(&convert_global_proxy);
        {
          // Patch receiver to global proxy.
          __ LoadGlobalProxy(a3);
        }
        __ Branch(&convert_receiver);
      }
      __ bind(&convert_to_object);
      {
        // Convert receiver using ToObject.
        // TODO(bmeurer): Inline the allocation here to avoid building the frame
        // in the fast case? (fall back to AllocateInNewSpace?)
        FrameScope scope(masm, StackFrame::INTERNAL);
        __ SmiTag(a0);
        __ Push(a0, a1);
        __ Move(a0, a3);
        __ Push(cp);
        __ Call(BUILTIN_CODE(masm->isolate(), ToObject),
                RelocInfo::CODE_TARGET);
        __ Pop(cp);
        __ Move(a3, a0);
        __ Pop(a0, a1);
        __ SmiUntag(a0);
      }
      __ LoadTaggedField(
          a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ StoreReceiver(a3, a0, kScratchReg);
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments
  //  -- a1 : the function to call (checked to be a JSFunction)
  //  -- a2 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  __ Lhu(a2,
         FieldMemOperand(a2, SharedFunctionInfo::kFormalParameterCountOffset));
  __ InvokeFunctionCode(a1, no_reg, a2, a0, InvokeType::kJump);

  // The function is a "classConstructor", need to raise an exception.
  __ bind(&class_constructor);
  {
    FrameScope frame(masm, StackFrame::INTERNAL);
    __ Push(a1);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
  }
}

namespace {

void Generate_PushBoundArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments
  //  -- a1 : target (checked to be a JSBoundFunction)
  //  -- a3 : new.target (only in case of [[Construct]])
  // -----------------------------------
  UseScratchRegisterScope temps(masm);
  temps.Include(t0, t1);
  Register bound_argc = a4;
  Register bound_argv = a2;
  // Load [[BoundArguments]] into a2 and length of that into a4.
  Label no_bound_arguments;
  __ LoadTaggedField(
      bound_argv, FieldMemOperand(a1, JSBoundFunction::kBoundArgumentsOffset));
  __ SmiUntagField(bound_argc,
                   FieldMemOperand(bound_argv, FixedArray::kLengthOffset));
  __ Branch(&no_bound_arguments, eq, bound_argc, Operand(zero_reg));
  {
    // ----------- S t a t e -------------
    //  -- a0 : the number of arguments
    //  -- a1 : target (checked to be a JSBoundFunction)
    //  -- a2 : the [[BoundArguments]] (implemented as FixedArray)
    //  -- a3 : new.target (only in case of [[Construct]])
    //  -- a4: the number of [[BoundArguments]]
    // -----------------------------------
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    Label done;
    // Reserve stack space for the [[BoundArguments]].
    {
      // Check the stack for overflow. We are not trying to catch interruptions
      // (i.e. debug break and preemption) here, so check the "real stack
      // limit".
      __ StackOverflowCheck(a4, temps.Acquire(), temps.Acquire(), nullptr,
                            &done);
      {
        FrameScope scope(masm, StackFrame::MANUAL);
        __ EnterFrame(StackFrame::INTERNAL);
        __ CallRuntime(Runtime::kThrowStackOverflow);
      }
      __ bind(&done);
    }

    // Pop receiver.
    __ Pop(scratch);

    // Push [[BoundArguments]].
    {
      Label loop, done_loop;
      __ SmiUntag(a4, FieldMemOperand(a2, FixedArray::kLengthOffset));
      __ AddWord(a0, a0, Operand(a4));
      __ AddWord(a2, a2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
      __ bind(&loop);
      __ SubWord(a4, a4, Operand(1));
      __ Branch(&done_loop, lt, a4, Operand(zero_reg), Label::Distance::kNear);
      __ CalcScaledAddress(a5, a2, a4, kTaggedSizeLog2);
      __ LoadTaggedField(kScratchReg, MemOperand(a5));
      __ Push(kScratchReg);
      __ Branch(&loop);
      __ bind(&done_loop);
    }

    // Push receiver.
    __ Push(scratch);
  }
  __ bind(&no_bound_arguments);
}

}  // namespace

// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(a1);

  // Patch the receiver to [[BoundThis]].
  {
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ LoadTaggedField(scratch,
                       FieldMemOperand(a1, JSBoundFunction::kBoundThisOffset));
    __ StoreReceiver(scratch, a0, kScratchReg);
  }

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ LoadTaggedField(
      a1, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Call_ReceiverIsAny),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments
  //  -- a1 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, class_constructor;
  UseScratchRegisterScope temps(masm);
  temps.Include(t1, t2);
  temps.Include(t4);
  Register map = temps.Acquire(), type = temps.Acquire(),
           range = temps.Acquire();
  __ JumpIfSmi(a1, &non_callable);
  __ LoadMap(map, a1);
  __ GetInstanceTypeRange(map, type, FIRST_CALLABLE_JS_FUNCTION_TYPE, range);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode),
          RelocInfo::CODE_TARGET, Uless_equal, range,
          Operand(LAST_CALLABLE_JS_FUNCTION_TYPE -
                  FIRST_CALLABLE_JS_FUNCTION_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), CallBoundFunction),
          RelocInfo::CODE_TARGET, eq, type, Operand(JS_BOUND_FUNCTION_TYPE));
  Register scratch = map;
  // Check if target has a [[Call]] internal method.
  __ Lbu(scratch, FieldMemOperand(map, Map::kBitFieldOffset));
  __ And(scratch, scratch, Operand(Map::Bits1::IsCallableBit::kMask));
  __ Branch(&non_callable, eq, scratch, Operand(zero_reg),
            Label::Distance::kNear);

  __ Jump(BUILTIN_CODE(masm->isolate(), CallProxy), RelocInfo::CODE_TARGET, eq,
          type, Operand(JS_PROXY_TYPE));

  // Check if target is a wrapped function and call CallWrappedFunction external
  // builtin
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWrappedFunction),
          RelocInfo::CODE_TARGET, eq, type, Operand(JS_WRAPPED_FUNCTION_TYPE));

  // ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that the function is not a "classConstructor".
  __ Branch(&class_constructor, eq, type, Operand(JS_CLASS_CONSTRUCTOR_TYPE));

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  // Overwrite the original receiver with the (original) target.
  __ StoreReceiver(a1, a0, kScratchReg);
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(a1, Context::CALL_AS_FUNCTION_DELEGATE_INDEX);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
  // 4. The function is a "classConstructor", need to raise an exception.
  __ bind(&class_constructor);
  {
    FrameScope frame(masm, StackFrame::INTERNAL);
    __ Push(a1);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
    __ ebreak();
  }
}

void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments
  //  -- a1 : the constructor to call (checked to be a JSFunction)
  //  -- a3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertConstructor(a1);
  __ AssertFunction(a1);

  // Calling convention for function specific ConstructStubs require
  // a2 to contain either an AllocationSite or undefined.
  __ LoadRoot(a2, RootIndex::kUndefinedValue);

  Label call_generic_stub;

  // Jump to JSBuiltinsConstructStub or JSConstructStubGeneric.
  __ LoadTaggedField(
      a4, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ Load32U(a4, FieldMemOperand(a4, SharedFunctionInfo::kFlagsOffset));
  __ And(a4, a4, Operand(SharedFunctionInfo::ConstructAsBuiltinBit::kMask));
  __ Branch(&call_generic_stub, eq, a4, Operand(zero_reg),
            Label::Distance::kNear);

  __ Jump(BUILTIN_CODE(masm->isolate(), JSBuiltinsConstructStub),
          RelocInfo::CODE_TARGET);

  __ bind(&call_generic_stub);
  __ Jump(BUILTIN_CODE(masm->isolate(), JSConstructStubGeneric),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  //  -- a3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertBoundFunction(a1);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  Label skip;
  {
#ifdef V8_COMPRESS_POINTERS
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ CmpTagged(scratch, a1, a3);
    __ Branch(&skip, ne, scratch, Operand(zero_reg), Label::Distance::kNear);
#else
    __ Branch(&skip, ne, a1, Operand(a3), Label::Distance::kNear);
#endif
  }
  __ LoadTaggedField(
      a3, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ bind(&skip);

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ LoadTaggedField(
      a1, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments
  //  -- a1 : the constructor to call (can be any Object)
  //  -- a3 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Check if target is a Smi.
  Label non_constructor, non_proxy;
  __ JumpIfSmi(a1, &non_constructor);

  // Check if target has a [[Construct]] internal method.
  UseScratchRegisterScope temps(masm);
  temps.Include(t0, t1);
  Register map = temps.Acquire();
  Register scratch = temps.Acquire();
  __ LoadTaggedField(map, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ Lbu(scratch, FieldMemOperand(map, Map::kBitFieldOffset));
  __ And(scratch, scratch, Operand(Map::Bits1::IsConstructorBit::kMask));
  __ Branch(&non_constructor, eq, scratch, Operand(zero_reg));
  Register range = temps.Acquire();
  // Dispatch based on instance type.
  __ GetInstanceTypeRange(map, scratch, FIRST_JS_FUNCTION_TYPE, range);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructFunction),
          RelocInfo::CODE_TARGET, Uless_equal, range,
          Operand(LAST_JS_FUNCTION_TYPE - FIRST_JS_FUNCTION_TYPE));

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructBoundFunction),
          RelocInfo::CODE_TARGET, eq, scratch, Operand(JS_BOUND_FUNCTION_TYPE));

  // Only dispatch to proxies after checking whether they are constructors.
  __ Branch(&non_proxy, ne, scratch, Operand(JS_PROXY_TYPE),
            Label::Distance::kNear);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructProxy),
          RelocInfo::CODE_TARGET);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  __ bind(&non_proxy);
  {
    // Overwrite the original receiver with the (original) target.
    __ StoreReceiver(a1, a0, kScratchReg);
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadNativeContextSlot(a1, Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX);
    __ Jump(masm->isolate()->builtins()->CallFunction(),
            RelocInfo::CODE_TARGET);
  }

  // Called Construct on an Object that doesn't have a [[Construct]] internal
  // method.
  __ bind(&non_constructor);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructedNonConstructable),
          RelocInfo::CODE_TARGET);
}

#if V8_ENABLE_WEBASSEMBLY
// Compute register lists for parameters to be saved. We save all parameter
// registers (see wasm-linkage.h). They might be overwritten in the runtime
// call below. We don't have any callee-saved registers in wasm, so no need to
// store anything else.
constexpr RegList kSavedGpRegs = ([]() constexpr {
  RegList saved_gp_regs;
  for (Register gp_param_reg : wasm::kGpParamRegisters) {
    saved_gp_regs.set(gp_param_reg);
  }

  // The instance has already been stored in the fixed part of the frame.
  saved_gp_regs.clear(kWasmInstanceRegister);
  // All set registers were unique.
  CHECK_EQ(saved_gp_regs.Count(), arraysize(wasm::kGpParamRegisters) - 1);
  CHECK_EQ(WasmLiftoffSetupFrameConstants::kNumberOfSavedGpParamRegs,
           saved_gp_regs.Count());
  return saved_gp_regs;
})();

constexpr DoubleRegList kSavedFpRegs = ([]() constexpr {
  DoubleRegList saved_fp_regs;
  for (DoubleRegister fp_param_reg : wasm::kFpParamRegisters) {
    saved_fp_regs.set(fp_param_reg);
  }

  CHECK_EQ(saved_fp_regs.Count(), arraysize(wasm::kFpParamRegisters));
  CHECK_EQ(WasmLiftoffSetupFrameConstants::kNumberOfSavedFpParamRegs,
           saved_fp_regs.Count());
  return saved_fp_regs;
})();

// When entering this builtin, we have just created a Wasm stack frame:
//
// [   Wasm instance   ]  <-- sp
// [ WASM frame marker ]
// [     saved fp      ]  <-- fp
//
// Add the feedback vector to the stack.
//
// [  feedback vector  ]  <-- sp
// [   Wasm instance   ]
// [ WASM frame marker ]
// [     saved fp      ]  <-- fp
void Builtins::Generate_WasmLiftoffFrameSetup(MacroAssembler* masm) {
  Register func_index = wasm::kLiftoffFrameSetupFunctionReg;
  Register vector = t1;
  Register scratch = t2;
  Label allocate_vector, done;

  __ LoadWord(vector,
              FieldMemOperand(kWasmInstanceRegister,
                              WasmInstanceObject::kFeedbackVectorsOffset));
  __ CalcScaledAddress(vector, vector, func_index, kTaggedSizeLog2);
  __ LoadWord(vector, FieldMemOperand(vector, FixedArray::kHeaderSize));
  __ JumpIfSmi(vector, &allocate_vector);
  __ bind(&done);
  __ Push(vector);
  __ Ret();

  __ bind(&allocate_vector);
  // Feedback vector doesn't exist yet. Call the runtime to allocate it.
  // We temporarily change the frame type for this, because we need special
  // handling by the stack walker in case of GC.
  __ li(scratch, StackFrame::TypeToMarker(StackFrame::WASM_LIFTOFF_SETUP));
  __ StoreWord(scratch, MemOperand(fp, TypedFrameConstants::kFrameTypeOffset));

  // Save registers.
  __ MultiPush(kSavedGpRegs);
  __ MultiPushFPU(kSavedFpRegs);
  __ Push(ra);

  // Arguments to the runtime function: instance, func_index, and an
  // additional stack slot for the NativeModule.
  __ SmiTag(func_index);
  __ Push(kWasmInstanceRegister, func_index, zero_reg);
  __ Move(cp, Smi::zero());
  __ CallRuntime(Runtime::kWasmAllocateFeedbackVector, 3);
  __ mv(vector, kReturnRegister0);

  // Restore registers and frame type.
  __ Pop(ra);
  __ MultiPopFPU(kSavedFpRegs);
  __ MultiPop(kSavedGpRegs);
  __ LoadWord(kWasmInstanceRegister,
              MemOperand(fp, WasmFrameConstants::kWasmInstanceOffset));
  __ li(scratch, StackFrame::TypeToMarker(StackFrame::WASM));
  __ StoreWord(scratch, MemOperand(fp, TypedFrameConstants::kFrameTypeOffset));
  __ Branch(&done);
}

void Builtins::Generate_WasmCompileLazy(MacroAssembler* masm) {
  // The function index was put in t0 by the jump table trampoline.
  // Convert to Smi for the runtime call
  __ SmiTag(kWasmCompileLazyFuncIndexRegister);

  {
    HardAbortScope hard_abort(masm);  // Avoid calls to Abort.
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Save registers that we need to keep alive across the runtime call.
    __ Push(kWasmInstanceRegister);
    __ MultiPush(kSavedGpRegs);
    __ MultiPushFPU(kSavedFpRegs);

    __ Push(kWasmInstanceRegister, kWasmCompileLazyFuncIndexRegister);
    // Initialize the JavaScript context with 0. CEntry will use it to
    // set the current context on the isolate.
    __ Move(kContextRegister, Smi::zero());
    __ CallRuntime(Runtime::kWasmCompileLazy, 2);

    __ SmiUntag(s1, a0);  // move return value to s1 since a0 will be restored
                          // to the value before the call
    CHECK(!kSavedGpRegs.has(s1));

    // Restore registers.
    __ MultiPopFPU(kSavedFpRegs);
    __ MultiPop(kSavedGpRegs);
    __ Pop(kWasmInstanceRegister);
  }

  // The runtime function returned the jump table slot offset as a Smi (now in
  // x17). Use that to compute the jump target.
  __ LoadWord(kScratchReg,
              FieldMemOperand(kWasmInstanceRegister,
                              WasmInstanceObject::kJumpTableStartOffset));
  __ AddWord(s1, s1, Operand(kScratchReg));
  // Finally, jump to the entrypoint.
  __ Jump(s1);
}

void Builtins::Generate_WasmDebugBreak(MacroAssembler* masm) {
  HardAbortScope hard_abort(masm);  // Avoid calls to Abort.
  {
    FrameScope scope(masm, StackFrame::WASM_DEBUG_BREAK);

    // Save all parameter registers. They might hold live values, we restore
    // them after the runtime call.
    __ MultiPush(WasmDebugBreakFrameConstants::kPushedGpRegs);
    __ MultiPushFPU(WasmDebugBreakFrameConstants::kPushedFpRegs);

    // Initialize the JavaScript context with 0. CEntry will use it to
    // set the current context on the isolate.
    __ Move(cp, Smi::zero());
    __ CallRuntime(Runtime::kWasmDebugBreak, 0);

    // Restore registers.
    __ MultiPopFPU(WasmDebugBreakFrameConstants::kPushedFpRegs);
    __ MultiPop(WasmDebugBreakFrameConstants::kPushedGpRegs);
  }
  __ Ret();
}
#endif  // V8_ENABLE_WEBASSEMBLY

void Builtins::Generate_CEntry(MacroAssembler* masm, int result_size,
                               ArgvMode argv_mode, bool builtin_exit_frame) {
  // Called from JavaScript; parameters are on stack as if calling JS function
  // a0: number of arguments including receiver
  // a1: pointer to builtin function
  // fp: frame pointer    (restored after C call)
  // sp: stack pointer    (restored as callee's sp after C call)
  // cp: current context  (C callee-saved)
  //
  // If argv_mode == ArgvMode::kRegister:
  // a2: pointer to the first argument

  if (argv_mode == ArgvMode::kRegister) {
    // Move argv into the correct register.
    __ Move(s1, a2);
  } else {
    // Compute the argv pointer in a callee-saved register.
    __ CalcScaledAddress(s1, sp, a0, kSystemPointerSizeLog2);
    __ SubWord(s1, s1, kSystemPointerSize);
  }

  // Enter the exit frame that transitions from JavaScript to C++.
  FrameScope scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(
      0, builtin_exit_frame ? StackFrame::BUILTIN_EXIT : StackFrame::EXIT);

  // s3: number of arguments  including receiver (C callee-saved)
  // s1: pointer to first argument (C callee-saved)
  // s2: pointer to builtin function (C callee-saved)

  // Prepare arguments for C routine.
  // a0 = argc
  __ Move(s3, a0);
  __ Move(s2, a1);

  // We are calling compiled C/C++ code. a0 and a1 hold our two arguments. We
  // also need to reserve the 4 argument slots on the stack.

  __ AssertStackIsAligned();

  // a0 = argc, a1 = argv, a2 = isolate
  __ li(a2, ExternalReference::isolate_address(masm->isolate()));
  __ Move(a1, s1);

  __ StoreReturnAddressAndCall(s2);

  // Result returned in a0 or a1:a0 - do not destroy these registers!

  // Check result for exception sentinel.
  Label exception_returned;
  __ LoadRoot(a4, RootIndex::kException);
  __ Branch(&exception_returned, eq, a4, Operand(a0));

  // Check that there is no pending exception, otherwise we
  // should have returned the exception sentinel.
  if (v8_flags.debug_code) {
    Label okay;
    ExternalReference pending_exception_address = ExternalReference::Create(
        IsolateAddressId::kPendingExceptionAddress, masm->isolate());
    __ li(a2, pending_exception_address);
    __ LoadWord(a2, MemOperand(a2));
    __ LoadRoot(a4, RootIndex::kTheHoleValue);
    // Cannot use check here as it attempts to generate call into runtime.
    __ Branch(&okay, eq, a4, Operand(a2), Label::Distance::kNear);
    __ stop();
    __ bind(&okay);
  }

  // Exit C frame and return.
  // a0:a1: result
  // sp: stack pointer
  // fp: frame pointer
  Register argc = argv_mode == ArgvMode::kRegister
                      // We don't want to pop arguments so set argc to no_reg.
                      ? no_reg
                      // s3: still holds argc (callee-saved).
                      : s3;
  __ LeaveExitFrame(argc, EMIT_RETURN);

  // Handling of exception.
  __ bind(&exception_returned);

  ExternalReference pending_handler_context_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerContextAddress, masm->isolate());
  ExternalReference pending_handler_entrypoint_address =
      ExternalReference::Create(
          IsolateAddressId::kPendingHandlerEntrypointAddress, masm->isolate());
  ExternalReference pending_handler_fp_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerFPAddress, masm->isolate());
  ExternalReference pending_handler_sp_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerSPAddress, masm->isolate());

  // Ask the runtime for help to determine the handler. This will set a0 to
  // contain the current pending exception, don't clobber it.
  ExternalReference find_handler =
      ExternalReference::Create(Runtime::kUnwindAndFindExceptionHandler);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(3, 0, a0);
    __ Move(a0, zero_reg);
    __ Move(a1, zero_reg);
    __ li(a2, ExternalReference::isolate_address(masm->isolate()));
    __ CallCFunction(find_handler, 3);
  }

  // Retrieve the handler context, SP and FP.
  __ li(cp, pending_handler_context_address);
  __ LoadWord(cp, MemOperand(cp));
  __ li(sp, pending_handler_sp_address);
  __ LoadWord(sp, MemOperand(sp));
  __ li(fp, pending_handler_fp_address);
  __ LoadWord(fp, MemOperand(fp));

  // If the handler is a JS frame, restore the context to the frame. Note that
  // the context will be set to (cp == 0) for non-JS frames.
  Label zero;
  __ Branch(&zero, eq, cp, Operand(zero_reg), Label::Distance::kNear);
  __ StoreWord(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ bind(&zero);

  // Compute the handler entry address and jump to it.
  UseScratchRegisterScope temp(masm);
  Register scratch = temp.Acquire();
  __ li(scratch, pending_handler_entrypoint_address);
  __ LoadWord(scratch, MemOperand(scratch));
  __ Jump(scratch);
}

void Builtins::Generate_DoubleToI(MacroAssembler* masm) {
  Label done;
  Register result_reg = t0;

  Register scratch = GetRegisterThatIsNotOneOf(result_reg);
  Register scratch2 = GetRegisterThatIsNotOneOf(result_reg, scratch);
  Register scratch3 = GetRegisterThatIsNotOneOf(result_reg, scratch, scratch2);
  DoubleRegister double_scratch = kScratchDoubleReg;

  // Account for saved regs.
  const int kArgumentOffset = 4 * kSystemPointerSize;

  __ Push(result_reg);
  __ Push(scratch, scratch2, scratch3);

  // Load double input.
  __ LoadDouble(double_scratch, MemOperand(sp, kArgumentOffset));

  // Try a conversion to a signed integer, if exception occurs, scratch is
  // set to 0
  __ Trunc_w_d(scratch3, double_scratch, scratch);

  // If we had no exceptions then set result_reg and we are done.
  Label error;
  __ Branch(&error, eq, scratch, Operand(zero_reg), Label::Distance::kNear);
  __ Move(result_reg, scratch3);
  __ Branch(&done);
  __ bind(&error);

  // Load the double value and perform a manual truncation.
  Register input_high = scratch2;
  Register input_low = scratch3;

  __ Lw(input_low, MemOperand(sp, kArgumentOffset + Register::kMantissaOffset));
  __ Lw(input_high,
        MemOperand(sp, kArgumentOffset + Register::kExponentOffset));

  Label normal_exponent;
  // Extract the biased exponent in result.
  __ ExtractBits(result_reg, input_high, HeapNumber::kExponentShift,
                 HeapNumber::kExponentBits);

  // Check for Infinity and NaNs, which should return 0.
  __ Sub32(scratch, result_reg, HeapNumber::kExponentMask);
  __ LoadZeroIfConditionZero(
      result_reg,
      scratch);  // result_reg = scratch == 0 ? 0 : result_reg
  __ Branch(&done, eq, scratch, Operand(zero_reg));

  // Express exponent as delta to (number of mantissa bits + 31).
  __ Sub32(result_reg, result_reg,
           Operand(HeapNumber::kExponentBias + HeapNumber::kMantissaBits + 31));

  // If the delta is strictly positive, all bits would be shifted away,
  // which means that we can return 0.
  __ Branch(&normal_exponent, le, result_reg, Operand(zero_reg),
            Label::Distance::kNear);
  __ Move(result_reg, zero_reg);
  __ Branch(&done);

  __ bind(&normal_exponent);
  const int kShiftBase = HeapNumber::kNonMantissaBitsInTopWord - 1;
  // Calculate shift.
  __ Add32(scratch, result_reg,
           Operand(kShiftBase + HeapNumber::kMantissaBits));

  // Save the sign.
  Register sign = result_reg;
  result_reg = no_reg;
  __ And(sign, input_high, Operand(HeapNumber::kSignMask));

  // We must specially handle shifts greater than 31.
  Label high_shift_needed, high_shift_done;
  __ Branch(&high_shift_needed, lt, scratch, Operand(32),
            Label::Distance::kNear);
  __ Move(input_high, zero_reg);
  __ BranchShort(&high_shift_done);
  __ bind(&high_shift_needed);

  // Set the implicit 1 before the mantissa part in input_high.
  __ Or(input_high, input_high,
        Operand(1 << HeapNumber::kMantissaBitsInTopWord));
  // Shift the mantissa bits to the correct position.
  // We don't need to clear non-mantissa bits as they will be shifted away.
  // If they weren't, it would mean that the answer is in the 32bit range.
  __ Sll32(input_high, input_high, scratch);

  __ bind(&high_shift_done);

  // Replace the shifted bits with bits from the lower mantissa word.
  Label pos_shift, shift_done, sign_negative;
  __ li(kScratchReg, 32);
  __ Sub32(scratch, kScratchReg, scratch);
  __ Branch(&pos_shift, ge, scratch, Operand(zero_reg), Label::Distance::kNear);

  // Negate scratch.
  __ Sub32(scratch, zero_reg, scratch);
  __ Sll32(input_low, input_low, scratch);
  __ BranchShort(&shift_done);

  __ bind(&pos_shift);
  __ Srl32(input_low, input_low, scratch);

  __ bind(&shift_done);
  __ Or(input_high, input_high, Operand(input_low));
  // Restore sign if necessary.
  __ Move(scratch, sign);
  result_reg = sign;
  sign = no_reg;
  __ Sub32(result_reg, zero_reg, input_high);
  __ Branch(&sign_negative, ne, scratch, Operand(zero_reg),
            Label::Distance::kNear);
  __ Move(result_reg, input_high);
  __ bind(&sign_negative);

  __ bind(&done);

  __ StoreWord(result_reg, MemOperand(sp, kArgumentOffset));
  __ Pop(scratch, scratch2, scratch3);
  __ Pop(result_reg);
  __ Ret();
}

void Builtins::Generate_GenericJSToWasmWrapper(MacroAssembler* masm) {
  // TODO(v8:10701): Implement for this platform.
  __ Trap();
}

void Builtins::Generate_WasmReturnPromiseOnSuspend(MacroAssembler* masm) {
  // TODO(v8:12191): Implement for this platform.
  __ Trap();
}

void Builtins::Generate_WasmSuspend(MacroAssembler* masm) {
  // TODO(v8:12191): Implement for this platform.
  __ Trap();
}

void Builtins::Generate_WasmResume(MacroAssembler* masm) {
  // TODO(v8:12191): Implement for this platform.
  __ Trap();
}

void Builtins::Generate_WasmReject(MacroAssembler* masm) {
  // TODO(v8:12191): Implement for this platform.
  __ Trap();
}

void Builtins::Generate_WasmOnStackReplace(MacroAssembler* masm) {
  // Only needed on x64.
  __ Trap();
}
namespace {

// Calls an API function. Allocates HandleScope, extracts returned value
// from handle and propagates exceptions.  Restores context.  On return removes
// *stack_space_operand * kSystemPointerSize or stack_space * kSystemPointerSize
// (GCed, includes the call JS arguments space and the additional space
// allocated for the fast call).
void CallApiFunctionAndReturn(MacroAssembler* masm, Register function_address,
                              ExternalReference thunk_ref,
                              Register thunk_last_arg, int stack_space,
                              MemOperand* stack_space_operand,
                              MemOperand return_value_operand) {
  ASM_CODE_COMMENT(masm);
  using ER = ExternalReference;

  Isolate* isolate = masm->isolate();
  MemOperand next_mem_op = __ ExternalReferenceAsOperand(
      ER::handle_scope_next_address(isolate), no_reg);
  MemOperand limit_mem_op = __ ExternalReferenceAsOperand(
      ER::handle_scope_limit_address(isolate), no_reg);
  MemOperand level_mem_op = __ ExternalReferenceAsOperand(
      ER::handle_scope_level_address(isolate), no_reg);

  Register return_value = a0;
  Register scratch = arg_reg_4;
  Register scratch2 = kScratchReg2;

  // Allocate HandleScope in callee-saved registers.
  // We will need to restore the HandleScope after the call to the API function,
  // by allocating it in callee-saved registers it'll be preserved by C code.
  Register prev_next_address_reg = s3;
  Register prev_limit_reg = s1;
  Register prev_level_reg = s2;

  // C arguments (arg_reg_1/2) are expected to be initialized outside, so this
  // function must not corrupt them (return_value overlaps with arg_reg_1 but
  // that's ok because we start using it only after the C call).
  DCHECK(!AreAliased(arg_reg_1, arg_reg_2, arg_reg_3,  // C args
                     scratch, scratch2, prev_next_address_reg, prev_limit_reg));
  // Ensure the additional argument for thunk_ref is already in the right
  // register.
  DCHECK(thunk_last_arg == arg_reg_2 || thunk_last_arg == arg_reg_3);
  DCHECK_EQ(function_address, thunk_last_arg);

  Label profiler_or_side_effects_check_enabled, done_api_call;
  {
    {
      ASM_CODE_COMMENT_STRING(masm,
                              "Allocate HandleScope in callee-save registers.");
      __ LoadWord(prev_next_address_reg, next_mem_op);
      __ LoadWord(prev_limit_reg, limit_mem_op);
      __ Lw(prev_level_reg, level_mem_op);
      __ Add32(scratch, prev_level_reg, Operand(1));
      __ Sw(scratch, level_mem_op);
    }
    __ RecordComment("Check if profiler or side effects check is enabled");
    __ Lb(scratch, __ ExternalReferenceAsOperand(
                       ER::execution_mode_address(isolate), no_reg));
    __ Branch(&profiler_or_side_effects_check_enabled, ne, scratch,
              Operand(zero_reg));
#ifdef V8_RUNTIME_CALL_STATS
    __ RecordComment("Check if RCS is enabled");
    __ li(scratch, ER::address_of_runtime_stats_flag());
    __ Lw(scratch, MemOperand(scratch, 0));
    __ Branch(&profiler_or_side_effects_check_enabled, ne, scratch,
              Operand(zero_reg));
#endif  // V8_RUNTIME_CALL_STATS

    __ RecordComment("Call the api function directly.");
    __ StoreReturnAddressAndCall(function_address);
    __ bind(&done_api_call);
  }

  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;

  __ RecordComment("Load the value from ReturnValue");
  __ LoadWord(return_value, return_value_operand);

  {
    ASM_CODE_COMMENT_STRING(
        masm,
        "No more valid handles (the result handle was the last one)."
        "Restore previous handle scope.");
    __ StoreWord(prev_next_address_reg, next_mem_op);
    if (v8_flags.debug_code) {
      __ Lw(scratch, level_mem_op);
      __ Sub32(scratch, scratch, Operand(1));
      __ Check(eq, AbortReason::kUnexpectedLevelAfterReturnFromApiCall, scratch,
               Operand(prev_level_reg));
    }
    __ Sw(prev_level_reg, level_mem_op);
    __ LoadWord(scratch, limit_mem_op);
    __ Branch(&delete_allocated_handles, ne, prev_limit_reg, Operand(scratch));
  }
  __ RecordComment("Leave the API exit frame.");
  __ bind(&leave_exit_frame);

  Register stack_space_reg = prev_limit_reg;
  if (stack_space_operand == nullptr) {
    DCHECK_NE(stack_space, 0);
    __ li(stack_space_reg, Operand(stack_space));
  } else {
    DCHECK_EQ(stack_space, 0);
    static_assert(kCArgSlotCount == 0);
    __ LoadWord(stack_space_reg, *stack_space_operand);
  }

  static constexpr bool kRegisterContainsSlotCount = false;
  __ LeaveExitFrame(stack_space_reg, NO_EMIT_RETURN,
                    kRegisterContainsSlotCount);

  {
    ASM_CODE_COMMENT_STRING(masm,
                            "Check if the function scheduled an exception.");
    __ LoadRoot(a4, RootIndex::kTheHoleValue);
    __ LoadWord(a5, __ ExternalReferenceAsOperand(
                        ER::scheduled_exception_address(isolate), no_reg));
    __ Branch(&promote_scheduled_exception, ne, a4, Operand(a5),
              Label::Distance::kNear);
  }
  {
    ASM_CODE_COMMENT_STRING(masm, "Convert return value");
    Label finish_return;
    __ JumpIfNotRoot(return_value, RootIndex::kTheHoleValue, &finish_return);
    __ LoadRoot(return_value, RootIndex::kUndefinedValue);
    __ bind(&finish_return);
  }

  __ AssertJSAny(return_value, scratch, scratch2,
                 AbortReason::kAPICallReturnedInvalidObject);

  __ Ret();

  {
    ASM_CODE_COMMENT_STRING(masm, "Call the api function via thunk wrapper.");
    __ bind(&profiler_or_side_effects_check_enabled);
    // Additional parameter is the address of the actual callback function.
    __ Move(thunk_last_arg, function_address);
    __ li(scratch, thunk_ref);
    __ StoreReturnAddressAndCall(scratch);
    __ Branch(&done_api_call);
  }

    __ RecordComment("Re-throw by promoting a scheduled exception.");
    __ bind(&promote_scheduled_exception);
    __ TailCallRuntime(Runtime::kPromoteScheduledException);

  {
    ASM_CODE_COMMENT_STRING(
        masm, "HandleScope limit has changed. Delete allocated extensions.");
    __ bind(&delete_allocated_handles);
    __ StoreWord(prev_limit_reg, limit_mem_op);
    // Save the return value in a callee-save register.
    Register saved_result = prev_limit_reg;
    __ Move(saved_result, a0);
    __ PrepareCallCFunction(1, prev_level_reg);
    __ li(arg_reg_1, ER::isolate_address(isolate));
    __ CallCFunction(ER::delete_handle_scope_extensions(), 1);
    __ Move(arg_reg_1, saved_result);
    __ Branch(&leave_exit_frame);
  }
}

MemOperand ExitFrameStackSlotOperand(int offset) {
  static constexpr int kSPOffset = 1 * kSystemPointerSize;
  return MemOperand(sp, kSPOffset + offset);
}

MemOperand ExitFrameCallerStackSlotOperand(int index) {
  return MemOperand(fp, (ExitFrameConstants::kFixedSlotCountAboveFp + index) *
                            kSystemPointerSize);
}
}  // namespace

void Builtins::Generate_CallApiCallback(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- cp                  : context
  //  -- a1                  : api function address
  //  -- a2                  : arguments count
  //  -- a3                  : call data
  //  -- a0                  : holder
  //  --
  //  -- sp[0]               : receiver
  //  -- sp[8]               : first argument
  //  -- ...
  //  -- sp[(argc) * 8]      : last argument
  // -----------------------------------
  Register function_callback_info_arg = arg_reg_1;
  Register thunk_callback_arg = arg_reg_2;

  Register api_function_address = a1;
  Register argc = a2;
  Register call_data = a3;
  Register holder = a0;
  Register scratch = kScratchReg2;

  DCHECK(!AreAliased(api_function_address, argc, call_data, holder, scratch));
  using FCI = FunctionCallbackInfo<v8::Value>;
  using FCA = FunctionCallbackArguments;

  static_assert(FCA::kArgsLength == 6);
  static_assert(FCA::kNewTargetIndex == 5);
  static_assert(FCA::kDataIndex == 4);
  static_assert(FCA::kReturnValueIndex == 3);
  static_assert(FCA::kUnusedIndex == 2);
  static_assert(FCA::kIsolateIndex == 1);
  static_assert(FCA::kHolderIndex == 0);

  // Set up FunctionCallbackInfo's implicit_args on the stack as follows:
  // Target state:
  //   sp[0 * kSystemPointerSize]: kHolder   <= FCA::implicit_args_
  //   sp[1 * kSystemPointerSize]: kIsolate
  //   sp[2 * kSystemPointerSize]: undefined (padding, unused)
  //   sp[3 * kSystemPointerSize]: undefined (kReturnValue)
  //   sp[4 * kSystemPointerSize]: kData
  //   sp[5 * kSystemPointerSize]: undefined (kNewTarget)
  // Existing state:
  //   sp[6 * kSystemPointerSize]:            <= FCA:::values_

  // Reserve space on the stack.
  static constexpr int kStackSize = FCA::kArgsLength;
  static_assert(kStackSize % 2 == 0);
  __ SubWord(sp, sp, Operand(kStackSize * kSystemPointerSize));

  // kHolder.
  __ StoreWord(holder, MemOperand(sp, FCA::kHolderIndex * kSystemPointerSize));

  // kIsolate.
  __ li(scratch, ExternalReference::isolate_address(masm->isolate()));
  __ StoreWord(scratch,
               MemOperand(sp, FCA::kIsolateIndex * kSystemPointerSize));

  // kPadding
  __ StoreWord(zero_reg,
               MemOperand(sp, FCA::kUnusedIndex * kSystemPointerSize));

  // kReturnValue
  __ LoadRoot(scratch, RootIndex::kUndefinedValue);
  __ StoreWord(scratch,
               MemOperand(sp, FCA::kReturnValueIndex * kSystemPointerSize));

  // kData.
  __ StoreWord(call_data, MemOperand(sp, FCA::kDataIndex * kSystemPointerSize));

  // kNewTarget.
  __ StoreWord(scratch,
               MemOperand(sp, FCA::kNewTargetIndex * kSystemPointerSize));

  // Keep a pointer to kHolder (= implicit_args) in a {holder} register.
  // We use it below to set up the FunctionCallbackInfo object.
  __ Move(holder, sp);

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  static constexpr int kSlotsToDropOnStackSize = 1 * kSystemPointerSize;
  static constexpr int kApiStackSpace =
      (FCI::kSize + kSlotsToDropOnStackSize) / kSystemPointerSize;
  static_assert(kApiStackSpace == 4);
  static_assert(FCI::kImplicitArgsOffset == 0);
  static_assert(FCI::kValuesOffset == 1 * kSystemPointerSize);
  static_assert(FCI::kLengthOffset == 2 * kSystemPointerSize);

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(kApiStackSpace);

  // EnterExitFrame may align the sp.
  {
    ASM_CODE_COMMENT_STRING(masm, "Initialize FunctionCallbackInfo");
    // FunctionCallbackInfo::implicit_args_ (points at kHolder as set up above).
    // Arguments are after the return address (pushed by EnterExitFrame()).
    __ StoreWord(holder, ExitFrameStackSlotOperand(FCI::kImplicitArgsOffset));

    // FunctionCallbackInfo::values_ (points at the first varargs argument
    // passed on the stack).
    __ AddWord(holder, holder,
               Operand(FCA::kArgsLengthWithReceiver * kSystemPointerSize));
    __ StoreWord(holder, ExitFrameStackSlotOperand(FCI::kValuesOffset));

    // FunctionCallbackInfo::length_.
    // Stored as int field, 32-bit integers within struct on stack always left
    // justified by n64 ABI.
    __ Sw(argc, ExitFrameStackSlotOperand(FCI::kLengthOffset));
  }
  // We also store the number of bytes to drop from the stack after returning
  // from the API function here.
  // Note: Unlike on other architectures, this stores the number of slots to
  // drop, not the number of bytes.
  MemOperand stack_space_operand =
      ExitFrameStackSlotOperand(FCI::kLengthOffset + kSlotsToDropOnStackSize);
  __ AddWord(scratch, argc, Operand(FCA::kArgsLengthWithReceiver));
  __ StoreWord(scratch, stack_space_operand);

  __ RecordComment("v8::FunctionCallback's argument.");
  // function_callback_info_arg = v8::FunctionCallbackInfo&
  __ AddWord(function_callback_info_arg, sp, Operand(1 * kSystemPointerSize));

  // It's okay if api_function_address == thunk_callback_arg, but not
  // function_callback_info_arg.
  DCHECK(!AreAliased(api_function_address, function_callback_info_arg));

  ExternalReference thunk_ref = ExternalReference::invoke_function_callback();

  MemOperand return_value_operand =
      ExitFrameCallerStackSlotOperand(FCA::kReturnValueIndex);

  static constexpr int kUseStackSpaceOperand = 0;

  AllowExternalCallThatCantCauseGC scope(masm);
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           thunk_callback_arg, kUseStackSpaceOperand,
                           &stack_space_operand, return_value_operand);
}

void Builtins::Generate_CallApiGetter(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- cp                  : context
  //  -- a1                  : receiver
  //  -- a3                  : accessor info
  //  -- a0                  : holder
  // -----------------------------------

  // Build v8::PropertyCallbackInfo::args_ array on the stack and push property
  // name below the exit frame to make GC aware of them.
  using PCA = PropertyCallbackArguments;
  static_assert(PCA::kShouldThrowOnErrorIndex == 0);
  static_assert(PCA::kHolderIndex == 1);
  static_assert(PCA::kIsolateIndex == 2);
  static_assert(PCA::kUnusedIndex == 3);
  static_assert(PCA::kReturnValueIndex == 4);
  static_assert(PCA::kDataIndex == 5);
  static_assert(PCA::kThisIndex == 6);
  static_assert(PCA::kArgsLength == 7);

  // Set up PropertyCallbackInfo's args_ on the stack as follows:
  // Target state:
  //   sp[0 * kSystemPointerSize]: name
  //   sp[1 * kSystemPointerSize]: kShouldThrowOnErrorIndex   <= PCI:args_
  //   sp[2 * kSystemPointerSize]: kHolderIndex
  //   sp[3 * kSystemPointerSize]: kIsolateIndex
  //   sp[4 * kSystemPointerSize]: kUnusedIndex
  //   sp[5 * kSystemPointerSize]: kReturnValueIndex
  //   sp[6 * kSystemPointerSize]: kDataIndex
  //   sp[7 * kSystemPointerSize]: kThisIndex / receiver

  Register name_arg = arg_reg_1;
  Register property_callback_info_arg = arg_reg_2;
  Register thunk_getter_arg = arg_reg_3;

  Register api_function_address = thunk_getter_arg;
  Register receiver = ApiGetterDescriptor::ReceiverRegister();
  Register holder = ApiGetterDescriptor::HolderRegister();
  Register callback = ApiGetterDescriptor::CallbackRegister();
  Register scratch = a4;
  DCHECK(!AreAliased(receiver, holder, callback, scratch));

  // Here and below +1 is for name() pushed after the args_ array.
  using PCA = PropertyCallbackArguments;
  __ SubWord(sp, sp, (PCA::kArgsLength + 1) * kSystemPointerSize);
  __ StoreWord(receiver,
               MemOperand(sp, (PCA::kThisIndex + 1) * kSystemPointerSize));
  __ LoadTaggedField(scratch,
                     FieldMemOperand(callback, AccessorInfo::kDataOffset));
  __ StoreWord(scratch,
               MemOperand(sp, (PCA::kDataIndex + 1) * kSystemPointerSize));
  __ LoadRoot(scratch, RootIndex::kUndefinedValue);
  __ StoreWord(scratch, MemOperand(sp, (PCA::kReturnValueIndex + 1) *
                                           kSystemPointerSize));
  __ StoreWord(scratch,
               MemOperand(sp, (PCA::kUnusedIndex + 1) * kSystemPointerSize));
  __ li(scratch, ExternalReference::isolate_address(masm->isolate()));
  __ StoreWord(scratch,
               MemOperand(sp, (PCA::kIsolateIndex + 1) * kSystemPointerSize));
  __ StoreWord(holder,
               MemOperand(sp, (PCA::kHolderIndex + 1) * kSystemPointerSize));
  // should_throw_on_error -> false
  DCHECK_EQ(0, Smi::zero().ptr());
  __ StoreWord(zero_reg, MemOperand(sp, (PCA::kShouldThrowOnErrorIndex + 1) *
                                            kSystemPointerSize));
  __ LoadTaggedField(scratch,
                     FieldMemOperand(callback, AccessorInfo::kNameOffset));
  __ StoreWord(scratch, MemOperand(sp, 0 * kSystemPointerSize));

  // v8::PropertyCallbackInfo::args_ array and name handle.
  static constexpr int kPaddingOnStackSlots = 0;
  static constexpr int kNameOnStackSlots = 1;
  // static constexpr int kNameStackIndex = kPaddingOnStackSlots;
  static constexpr int kPCAStackIndex =
      kNameOnStackSlots + kPaddingOnStackSlots;
  static constexpr int kStackUnwindSpace = PCA::kArgsLength + kPCAStackIndex;
  static_assert(kStackUnwindSpace % 2 == 0,
                "slots must be a multiple of 2 for stack pointer alignment");

  __ RecordComment(
      "Load address of v8::PropertyAccessorInfo::args_ array and name handle.");
  // name_arg = Handle<Name>(&name), name value was pushed to GC-ed stack space.
  __ Move(name_arg, sp);
  // property_callback_info_arg = v8::PCI::args_ (= &ShouldThrow)
  __ AddWord(property_callback_info_arg, name_arg,
             Operand(1 * kSystemPointerSize));

  const int kApiStackSpace = 1;
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(kApiStackSpace);

  __ RecordComment("Create v8::PropertyCallbackInfo object on the stack.");
  // Initialize its args_ field.
  __ StoreWord(property_callback_info_arg,
               MemOperand(sp, 1 * kSystemPointerSize));
  // property_callback_info_arg = v8::PropertyCallbackInfo&
  __ AddWord(property_callback_info_arg, sp, Operand(1 * kSystemPointerSize));

  __ LoadWord(
      api_function_address,
      FieldMemOperand(callback, AccessorInfo::kMaybeRedirectedGetterOffset));

  // It's okay if api_function_address == thunk_getter_arg, but not
  // property_callback_info_arg or name_arg.
  DCHECK(
      !AreAliased(api_function_address, property_callback_info_arg, name_arg));

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback();
  MemOperand return_value_operand =
      ExitFrameCallerStackSlotOperand(kPCAStackIndex + PCA::kReturnValueIndex);
  MemOperand* const kUseStackSpaceConstant = nullptr;

  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           thunk_getter_arg, kStackUnwindSpace,
                           kUseStackSpaceConstant, return_value_operand);
}

void Builtins::Generate_DirectCEntry(MacroAssembler* masm) {
  // The sole purpose of DirectCEntry is for movable callers (e.g. any general
  // purpose InstructionStream object) to be able to call into C functions that
  // may trigger GC and thus move the caller.
  //
  // DirectCEntry places the return address on the stack (updated by the GC),
  // making the call GC safe. The irregexp backend relies on this.

  // Make place for arguments to fit C calling convention. Callers use
  // EnterExitFrame/LeaveExitFrame so they handle stack restoring and we don't
  // have to do that here. Any caller must drop kCArgsSlotsSize stack space
  // after the call.
  __ AddWord(sp, sp, -kCArgsSlotsSize);

  __ StoreWord(ra,
               MemOperand(sp, kCArgsSlotsSize));  // Store the return address.
  __ Call(t6);                                    // Call the C++ function.
  __ LoadWord(t6, MemOperand(sp, kCArgsSlotsSize));  // Return to calling code.

  if (v8_flags.debug_code && v8_flags.enable_slow_asserts) {
    // In case of an error the return address may point to a memory area
    // filled with kZapValue by the GC. Dereference the address and check for
    // this.
    __ Uld(a4, MemOperand(t6));
    __ Assert(ne, AbortReason::kReceivedInvalidReturnAddress, a4,
              Operand(kZapValue));
  }

  __ Jump(t6);
}

namespace {

// This code tries to be close to ia32 code so that any changes can be
// easily ported.
void Generate_DeoptimizationEntry(MacroAssembler* masm,
                                  DeoptimizeKind deopt_kind) {
  Isolate* isolate = masm->isolate();

  // Unlike on ARM we don't save all the registers, just the useful ones.
  // For the rest, there are gaps on the stack, so the offsets remain the same.
  const int kNumberOfRegisters = Register::kNumRegisters;

  RegList restored_regs = kJSCallerSaved | kCalleeSaved;
  RegList saved_regs = restored_regs | sp | ra;

  const int kDoubleRegsSize = kDoubleSize * DoubleRegister::kNumRegisters;

  // Save all double FPU registers before messing with them.
  __ SubWord(sp, sp, Operand(kDoubleRegsSize));
  const RegisterConfiguration* config = RegisterConfiguration::Default();
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    const DoubleRegister fpu_reg = DoubleRegister::from_code(code);
    int offset = code * kDoubleSize;
    __ StoreDouble(fpu_reg, MemOperand(sp, offset));
  }

  // Push saved_regs (needed to populate FrameDescription::registers_).
  // Leave gaps for other registers.
  __ SubWord(sp, sp, kNumberOfRegisters * kSystemPointerSize);
  for (int16_t i = kNumberOfRegisters - 1; i >= 0; i--) {
    if ((saved_regs.bits() & (1 << i)) != 0) {
      __ StoreWord(ToRegister(i), MemOperand(sp, kSystemPointerSize * i));
    }
  }

  __ li(a2,
        ExternalReference::Create(IsolateAddressId::kCEntryFPAddress, isolate));
  __ StoreWord(fp, MemOperand(a2));

  const int kSavedRegistersAreaSize =
      (kNumberOfRegisters * kSystemPointerSize) + kDoubleRegsSize;

  // Get the address of the location in the code object (a2) (return
  // address for lazy deoptimization) and compute the fp-to-sp delta in
  // register a4.
  __ Move(a2, ra);
  __ AddWord(a3, sp, Operand(kSavedRegistersAreaSize));

  __ SubWord(a3, fp, a3);

  // Allocate a new deoptimizer object.
  __ PrepareCallCFunction(5, a4);
  // Pass five arguments, according to n64 ABI.
  __ Move(a0, zero_reg);
  Label context_check;
  __ LoadWord(a1,
              MemOperand(fp, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ JumpIfSmi(a1, &context_check);
  __ LoadWord(a0, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
  __ bind(&context_check);
  __ li(a1, Operand(static_cast<int64_t>(deopt_kind)));
  // a2: code object address
  // a3: fp-to-sp delta
  __ li(a4, ExternalReference::isolate_address(isolate));

  // Call Deoptimizer::New().
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ CallCFunction(ExternalReference::new_deoptimizer_function(), 5);
  }

  // Preserve "deoptimizer" object in register a0 and get the input
  // frame descriptor pointer to a1 (deoptimizer->input_);
  __ LoadWord(a1, MemOperand(a0, Deoptimizer::input_offset()));

  // Copy core registers into FrameDescription::registers_[kNumRegisters].
  DCHECK_EQ(Register::kNumRegisters, kNumberOfRegisters);
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset =
        (i * kSystemPointerSize) + FrameDescription::registers_offset();
    if ((saved_regs.bits() & (1 << i)) != 0) {
      __ LoadWord(a2, MemOperand(sp, i * kSystemPointerSize));
      __ StoreWord(a2, MemOperand(a1, offset));
    } else if (v8_flags.debug_code) {
      __ li(a2, kDebugZapValue);
      __ StoreWord(a2, MemOperand(a1, offset));
    }
  }

  int double_regs_offset = FrameDescription::double_registers_offset();
  // Copy FPU registers to
  // double_registers_[DoubleRegister::kNumAllocatableRegisters]
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    int dst_offset = code * kDoubleSize + double_regs_offset;
    int src_offset =
        code * kDoubleSize + kNumberOfRegisters * kSystemPointerSize;
    __ LoadDouble(ft0, MemOperand(sp, src_offset));
    __ StoreDouble(ft0, MemOperand(a1, dst_offset));
  }

  // Remove the saved registers from the stack.
  __ AddWord(sp, sp, Operand(kSavedRegistersAreaSize));

  // Compute a pointer to the unwinding limit in register a2; that is
  // the first stack slot not part of the input frame.
  __ LoadWord(a2, MemOperand(a1, FrameDescription::frame_size_offset()));
  __ AddWord(a2, a2, sp);

  // Unwind the stack down to - but not including - the unwinding
  // limit and copy the contents of the activation frame to the input
  // frame description.
  __ AddWord(a3, a1, Operand(FrameDescription::frame_content_offset()));
  Label pop_loop;
  Label pop_loop_header;
  __ BranchShort(&pop_loop_header);
  __ bind(&pop_loop);
  __ pop(a4);
  __ StoreWord(a4, MemOperand(a3, 0));
  __ AddWord(a3, a3, kSystemPointerSize);
  __ bind(&pop_loop_header);
  __ Branch(&pop_loop, ne, a2, Operand(sp), Label::Distance::kNear);
  // Compute the output frame in the deoptimizer.
  __ push(a0);  // Preserve deoptimizer object across call.
  // a0: deoptimizer object; a1: scratch.
  __ PrepareCallCFunction(1, a1);
  // Call Deoptimizer::ComputeOutputFrames().
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ CallCFunction(ExternalReference::compute_output_frames_function(), 1);
  }
  __ pop(a0);  // Restore deoptimizer object (class Deoptimizer).

  __ LoadWord(sp, MemOperand(a0, Deoptimizer::caller_frame_top_offset()));

  // Replace the current (input) frame with the output frames.
  Label outer_push_loop, inner_push_loop, outer_loop_header, inner_loop_header;
  // Outer loop state: a4 = current "FrameDescription** output_",
  // a1 = one past the last FrameDescription**.
  __ Lw(a1, MemOperand(a0, Deoptimizer::output_count_offset()));
  __ LoadWord(a4,
              MemOperand(a0, Deoptimizer::output_offset()));  // a4 is output_.
  __ CalcScaledAddress(a1, a4, a1, kSystemPointerSizeLog2);
  __ BranchShort(&outer_loop_header);
  __ bind(&outer_push_loop);
  // Inner loop state: a2 = current FrameDescription*, a3 = loop index.
  __ LoadWord(a2, MemOperand(a4, 0));  // output_[ix]
  __ LoadWord(a3, MemOperand(a2, FrameDescription::frame_size_offset()));
  __ BranchShort(&inner_loop_header);
  __ bind(&inner_push_loop);
  __ SubWord(a3, a3, Operand(kSystemPointerSize));
  __ AddWord(a6, a2, Operand(a3));
  __ LoadWord(a7, MemOperand(a6, FrameDescription::frame_content_offset()));
  __ push(a7);
  __ bind(&inner_loop_header);
  __ Branch(&inner_push_loop, ne, a3, Operand(zero_reg));

  __ AddWord(a4, a4, Operand(kSystemPointerSize));
  __ bind(&outer_loop_header);
  __ Branch(&outer_push_loop, lt, a4, Operand(a1));

  __ LoadWord(a1, MemOperand(a0, Deoptimizer::input_offset()));
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    const DoubleRegister fpu_reg = DoubleRegister::from_code(code);
    int src_offset = code * kDoubleSize + double_regs_offset;
    __ LoadDouble(fpu_reg, MemOperand(a1, src_offset));
  }

  // Push pc and continuation from the last output frame.
  __ LoadWord(a6, MemOperand(a2, FrameDescription::pc_offset()));
  __ push(a6);
  __ LoadWord(a6, MemOperand(a2, FrameDescription::continuation_offset()));
  __ push(a6);

  // Technically restoring 't3' should work unless zero_reg is also restored
  // but it's safer to check for this.
  DCHECK(!(restored_regs.has(t3)));
  // Restore the registers from the last output frame.
  __ Move(t3, a2);
  for (int i = kNumberOfRegisters - 1; i >= 0; i--) {
    int offset =
        (i * kSystemPointerSize) + FrameDescription::registers_offset();
    if ((restored_regs.bits() & (1 << i)) != 0) {
      __ LoadWord(ToRegister(i), MemOperand(t3, offset));
    }
  }

  __ pop(t6);  // Get continuation, leave pc on stack.
  __ pop(ra);
  __ Jump(t6);
  __ stop();
}

}  // namespace

void Builtins::Generate_DeoptimizationEntry_Eager(MacroAssembler* masm) {
  Generate_DeoptimizationEntry(masm, DeoptimizeKind::kEager);
}

void Builtins::Generate_DeoptimizationEntry_Lazy(MacroAssembler* masm) {
  Generate_DeoptimizationEntry(masm, DeoptimizeKind::kLazy);
}

namespace {

// Restarts execution either at the current or next (in execution order)
// bytecode. If there is baseline code on the shared function info, converts an
// interpreter frame into a baseline frame and continues execution in baseline
// code. Otherwise execution continues with bytecode.
void Generate_BaselineOrInterpreterEntry(MacroAssembler* masm,
                                         bool next_bytecode,
                                         bool is_osr = false) {
  Label start;
  __ bind(&start);

  // Get function from the frame.
  Register closure = a1;
  __ LoadWord(closure, MemOperand(fp, StandardFrameConstants::kFunctionOffset));

  // Get the InstructionStream object from the shared function info.
  Register code_obj = s1;
  __ LoadTaggedField(
      code_obj,
      FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  __ LoadTaggedField(
      code_obj,
      FieldMemOperand(code_obj, SharedFunctionInfo::kFunctionDataOffset));

  // Check if we have baseline code. For OSR entry it is safe to assume we
  // always have baseline code.
  if (!is_osr) {
    Label start_with_baseline;
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ GetObjectType(code_obj, scratch, scratch);
    __ Branch(&start_with_baseline, eq, scratch, Operand(CODE_TYPE));

    // Start with bytecode as there is no baseline code.
    Builtin builtin_id = next_bytecode
                             ? Builtin::kInterpreterEnterAtNextBytecode
                             : Builtin::kInterpreterEnterAtBytecode;
    __ Jump(masm->isolate()->builtins()->code_handle(builtin_id),
            RelocInfo::CODE_TARGET);

    // Start with baseline code.
    __ bind(&start_with_baseline);
  } else if (v8_flags.debug_code) {
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ GetObjectType(code_obj, scratch, scratch);
    __ Assert(eq, AbortReason::kExpectedBaselineData, scratch,
              Operand(CODE_TYPE));
  }
  if (v8_flags.debug_code) {
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    AssertCodeIsBaseline(masm, code_obj, scratch);
  }

  // Replace BytecodeOffset with the feedback vector.
  Register feedback_vector = a2;
  __ LoadTaggedField(feedback_vector,
                     FieldMemOperand(closure, JSFunction::kFeedbackCellOffset));
  __ LoadTaggedField(feedback_vector,
                     FieldMemOperand(feedback_vector, Cell::kValueOffset));
  Label install_baseline_code;
  // Check if feedback vector is valid. If not, call prepare for baseline to
  // allocate it.
  UseScratchRegisterScope temps(masm);
  Register type = temps.Acquire();
  __ GetObjectType(feedback_vector, type, type);
  __ Branch(&install_baseline_code, ne, type, Operand(FEEDBACK_VECTOR_TYPE));
  // Save BytecodeOffset from the stack frame.
  __ SmiUntag(kInterpreterBytecodeOffsetRegister,
              MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  // Replace BytecodeOffset with the feedback vector.
  __ StoreWord(
      feedback_vector,
      MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  feedback_vector = no_reg;

  // Compute baseline pc for bytecode offset.
  ExternalReference get_baseline_pc_extref;
  if (next_bytecode || is_osr) {
    get_baseline_pc_extref =
        ExternalReference::baseline_pc_for_next_executed_bytecode();
  } else {
    get_baseline_pc_extref =
        ExternalReference::baseline_pc_for_bytecode_offset();
  }

  Register get_baseline_pc = a3;
  __ li(get_baseline_pc, get_baseline_pc_extref);

  // If the code deoptimizes during the implicit function entry stack interrupt
  // check, it will have a bailout ID of kFunctionEntryBytecodeOffset, which is
  // not a valid bytecode offset.
  // TODO(pthier): Investigate if it is feasible to handle this special case
  // in TurboFan instead of here.
  Label valid_bytecode_offset, function_entry_bytecode;
  if (!is_osr) {
    __ Branch(&function_entry_bytecode, eq, kInterpreterBytecodeOffsetRegister,
              Operand(BytecodeArray::kHeaderSize - kHeapObjectTag +
                      kFunctionEntryBytecodeOffset));
  }

  __ SubWord(kInterpreterBytecodeOffsetRegister,
             kInterpreterBytecodeOffsetRegister,
             (BytecodeArray::kHeaderSize - kHeapObjectTag));

  __ bind(&valid_bytecode_offset);
  // Get bytecode array from the stack frame.
  __ LoadWord(kInterpreterBytecodeArrayRegister,
              MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ Push(kInterpreterAccumulatorRegister);
  {
    __ Move(arg_reg_1, code_obj);
    __ Move(arg_reg_2, kInterpreterBytecodeOffsetRegister);
    __ Move(arg_reg_3, kInterpreterBytecodeArrayRegister);
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallCFunction(get_baseline_pc, 3, 0);
  }
  __ LoadCodeInstructionStart(code_obj, code_obj);
  __ AddWord(code_obj, code_obj, kReturnRegister0);
  __ Pop(kInterpreterAccumulatorRegister);

  if (is_osr) {
    // Reset the OSR loop nesting depth to disarm back edges.
    // TODO(pthier): Separate baseline Sparkplug from TF arming and don't disarm
    // Sparkplug here.
    __ LoadWord(
        kInterpreterBytecodeArrayRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
    ResetBytecodeAge(masm, kInterpreterBytecodeArrayRegister);
    Generate_OSREntry(masm, code_obj);
  } else {
    __ Jump(code_obj);
  }
  __ Trap();  // Unreachable.

  if (!is_osr) {
    __ bind(&function_entry_bytecode);
    // If the bytecode offset is kFunctionEntryOffset, get the start address of
    // the first bytecode.
    __ li(kInterpreterBytecodeOffsetRegister, Operand(0));
    if (next_bytecode) {
      __ li(get_baseline_pc,
            ExternalReference::baseline_pc_for_bytecode_offset());
    }
    __ Branch(&valid_bytecode_offset);
  }

  __ bind(&install_baseline_code);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(kInterpreterAccumulatorRegister);
    __ Push(closure);
    __ CallRuntime(Runtime::kInstallBaselineCode, 1);
    __ Pop(kInterpreterAccumulatorRegister);
  }
  // Retry from the start after installing baseline code.
  __ Branch(&start);
}

}  // namespace

void Builtins::Generate_BaselineOrInterpreterEnterAtBytecode(
    MacroAssembler* masm) {
  Generate_BaselineOrInterpreterEntry(masm, false);
}

void Builtins::Generate_BaselineOrInterpreterEnterAtNextBytecode(
    MacroAssembler* masm) {
  Generate_BaselineOrInterpreterEntry(masm, true);
}

void Builtins::Generate_InterpreterOnStackReplacement_ToBaseline(
    MacroAssembler* masm) {
  Generate_BaselineOrInterpreterEntry(masm, false, true);
}

void Builtins::Generate_RestartFrameTrampoline(MacroAssembler* masm) {
  // Frame is being dropped:
  // - Look up current function on the frame.
  // - Leave the frame.
  // - Restart the frame by calling the function.

  __ LoadWord(a1, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
  __ LoadWord(a0, MemOperand(fp, StandardFrameConstants::kArgCOffset));

  // Pop return address and frame.
  __ LeaveFrame(StackFrame::INTERPRETED);

  __ li(a2, Operand(kDontAdaptArgumentsSentinel));

  __ InvokeFunction(a1, a2, a0, InvokeType::kJump);
}

#undef __

}  // namespace internal
}  // namespace v8
