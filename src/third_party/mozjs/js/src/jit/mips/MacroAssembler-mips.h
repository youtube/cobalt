/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_mips_MacroAssembler_mips_h
#define jit_mips_MacroAssembler_mips_h

#include "jsopcode.h"

#include "jit/IonCaches.h"
#include "jit/IonFrames.h"
#include "jit/mips/Assembler-mips.h"
#include "jit/MoveResolver.h"

namespace js {
namespace jit {

enum LoadStoreSize
{
    SizeByte = 8,
    SizeHalfWord = 16,
    SizeWord = 32,
    SizeDouble = 64
};

enum LoadStoreExtension
{
    ZeroExtend = 0,
    SignExtend = 1
};

enum JumpKind
{
    LongJump = 0,
    ShortJump = 1
};

struct ImmTag : public Imm32
{
    ImmTag(JSValueTag mask)
      : Imm32(int32_t(mask))
    { }
};

struct ImmType : public ImmTag
{
    ImmType(JSValueType type)
      : ImmTag(JSVAL_TYPE_TO_TAG(type))
    { }
};

static const ValueOperand JSReturnOperand = ValueOperand(JSReturnReg_Type, JSReturnReg_Data);
static const ValueOperand softfpReturnOperand = ValueOperand(v1, v0);

static Register CallReg = t9;
static const int defaultShift = 3;
static_assert(1 << defaultShift == sizeof(jsval), "The defaultShift is wrong");

class MacroAssemblerMIPS : public Assembler
{
  public:
    void convertBoolToInt32(Register source, Register dest);
    void convertInt32ToDouble(const Register &src, const FloatRegister &dest);
    void convertInt32ToDouble(const Address &src, FloatRegister dest);
    void convertUInt32ToDouble(const Register &src, const FloatRegister &dest);
    void convertDoubleToFloat(const FloatRegister &src, const FloatRegister &dest) {
        as_cvtsd(dest, src);
    }

    void branchTruncateDouble(const FloatRegister &src, const Register &dest, Label *fail);
    void convertDoubleToInt32(const FloatRegister &src, const Register &dest, Label *fail,
                              bool negativeZeroCheck = true);

    void addDouble(FloatRegister src, FloatRegister dest);
    void subDouble(FloatRegister src, FloatRegister dest);
    void mulDouble(FloatRegister src, FloatRegister dest);
    void divDouble(FloatRegister src, FloatRegister dest);

    void negateDouble(FloatRegister reg);
    void inc64(AbsoluteAddress dest);

  public:

    void ma_move(Register rd, Register rs);

    void ma_li(Register dest, const ImmGCPtr &ptr);

    void ma_li(const Register &dest, AbsoluteLabel *label);

    void ma_li(Register dest, Imm32 imm);
    void ma_liPatchable(Register dest, Imm32 imm);
    void ma_liPatchable(Register dest, ImmWord imm);

    // Shift operations
    void ma_sll(Register rd, Register rt, Imm32 shift);
    void ma_srl(Register rd, Register rt, Imm32 shift);
    void ma_sra(Register rd, Register rt, Imm32 shift);
    void ma_ror(Register rd, Register rt, Imm32 shift);
    void ma_rol(Register rd, Register rt, Imm32 shift);

    void ma_sll(Register rd, Register rt, Register shift);
    void ma_srl(Register rd, Register rt, Register shift);
    void ma_sra(Register rd, Register rt, Register shift);
    void ma_ror(Register rd, Register rt, Register shift);
    void ma_rol(Register rd, Register rt, Register shift);

    // Negate
    void ma_negu(Register rd, Register rs);

    void ma_not(Register rd, Register rs);

    // and
    void ma_and(Register rd, Register rs);
    void ma_and(Register rd, Register rs, Register rt);
    void ma_and(Register rd, Imm32 imm);
    void ma_and(Register rd, Register rs, Imm32 imm);

    // or
    void ma_or(Register rd, Register rs);
    void ma_or(Register rd, Register rs, Register rt);
    void ma_or(Register rd, Imm32 imm);
    void ma_or(Register rd, Register rs, Imm32 imm);

    // xor
    void ma_xor(Register rd, Register rs);
    void ma_xor(Register rd, Register rs, Register rt);
    void ma_xor(Register rd, Imm32 imm);
    void ma_xor(Register rd, Register rs, Imm32 imm);

    // load
    void ma_load(const Register &dest, Address address, LoadStoreSize size = SizeWord,
                 LoadStoreExtension extension = SignExtend);
    void ma_load(const Register &dest, const BaseIndex &src, LoadStoreSize size = SizeWord,
                 LoadStoreExtension extension = SignExtend);

    // store
    void ma_store(const Register &data, Address address, LoadStoreSize size = SizeWord,
                  LoadStoreExtension extension = SignExtend);
    void ma_store(const Register &data, const BaseIndex &dest, LoadStoreSize size = SizeWord,
                  LoadStoreExtension extension = SignExtend);
    void ma_store(const Imm32 &imm, const BaseIndex &dest, LoadStoreSize size = SizeWord,
                  LoadStoreExtension extension = SignExtend);

    void computeScaledAddress(const BaseIndex &address, Register dest);

    void computeEffectiveAddress(const Address &address, Register dest) {
        ma_addu(dest, address.base, Imm32(address.offset));
    }

    void computeEffectiveAddress(const BaseIndex &address, Register dest) {
        computeScaledAddress(address, dest);
        if (address.offset) {
            ma_addu(dest, dest, Imm32(address.offset));
        }
    }

    // arithmetic based ops
    // add
    void ma_addu(Register rd, Register rs, Imm32 imm);
    void ma_addu(Register rd, Register rs);
    void ma_addu(Register rd, Imm32 imm);
    void ma_addTestOverflow(Register rd, Register rs, Register rt, Label *overflow);
    void ma_addTestOverflow(Register rd, Register rs, Imm32 imm, Label *overflow);

    // subtract
    void ma_subu(Register rd, Register rs, Register rt);
    void ma_subu(Register rd, Register rs, Imm32 imm);
    void ma_subu(Register rd, Imm32 imm);
    void ma_subTestOverflow(Register rd, Register rs, Register rt, Label *overflow);
    void ma_subTestOverflow(Register rd, Register rs, Imm32 imm, Label *overflow);

    // multiplies.  For now, there are only few that we care about.
    void ma_mult(Register rs, Imm32 imm);
    void ma_mul_branch_overflow(Register rd, Register rs, Register rt, Label *overflow);
    void ma_mul_branch_overflow(Register rd, Register rs, Imm32 imm, Label *overflow);

    // divisions
    void ma_div_branch_overflow(Register rd, Register rs, Register rt, Label *overflow);
    void ma_div_branch_overflow(Register rd, Register rs, Imm32 imm, Label *overflow);

    // fast mod, uses scratch registers, and thus needs to be in the assembler
    // implicitly assumes that we can overwrite dest at the beginning of the sequence
    void ma_mod_mask(Register src, Register dest, Register hold, int32_t shift,
                     Label *negZero = NULL);

    // memory
    // shortcut for when we know we're transferring 32 bits of data
    void ma_lw(Register data, Address address);

    void ma_sw(Register data, Address address);
    void ma_sw(Imm32 imm, Address address);

    void ma_pop(Register r);
    void ma_push(Register r);

    // branches when done from within mips-specific code
    void ma_b(Register lhs, Register rhs, Label *l, Condition c, JumpKind jumpKind = LongJump);
    void ma_b(Register lhs, Imm32 imm, Label *l, Condition c, JumpKind jumpKind = LongJump);
    void ma_b(Register lhs, Address addr, Label *l, Condition c, JumpKind jumpKind = LongJump);
    void ma_b(Address addr, Imm32 imm, Label *l, Condition c, JumpKind jumpKind = LongJump);
    void ma_b(Label *l, JumpKind jumpKind = LongJump);
    void ma_bal(Label *l, JumpKind jumpKind = LongJump);

    void ma_b(Register lhs, ImmWord imm, Label* l, Condition c, JumpKind jumpKind = LongJump) {
        ma_b(lhs, Imm32(uint32_t(imm.value)), l, c, jumpKind);
    }
    void ma_b(Address addr, Register rhs, Label* l, Condition c, JumpKind jumpKind = LongJump) {
        MOZ_ASSERT(rhs != ScratchRegister);
        ma_lw(ScratchRegister, addr);
        ma_b(ScratchRegister, rhs, l, c, jumpKind);
    }
    void ma_b(Register lhs, ImmGCPtr imm, Label* l, Condition c, JumpKind jumpKind = LongJump) {
        MOZ_ASSERT(lhs != ScratchRegister);
        ma_li(ScratchRegister, imm);
        ma_b(lhs, ScratchRegister, l, c, jumpKind);
    }
    void ma_b(Address addr, ImmGCPtr imm, Label* l, Condition c, JumpKind jumpKind = LongJump) {
        ma_lw(SecondScratchReg, addr);
        ma_b(SecondScratchReg, imm, l, c, jumpKind);
    }

    // fp instructions
    void ma_lis(FloatRegister dest, float value);
    void ma_lid(FloatRegister dest, double value);
    void ma_liNegZero(FloatRegister dest);

    void ma_mv(FloatRegister src, ValueOperand dest);
    void ma_mv(ValueOperand src, FloatRegister dest);

    void ma_ls(FloatRegister fd, Address address);
    void ma_ld(FloatRegister fd, Address address);
    void ma_sd(FloatRegister fd, Address address);
    void ma_sd(FloatRegister fd, BaseIndex address);
    void ma_ss(FloatRegister fd, Address address);
    void ma_ss(FloatRegister fd, BaseIndex address);

    void ma_pop(FloatRegister fs);
    void ma_push(FloatRegister fs);

    //FP branches
    void ma_bc1s(FloatRegister lhs, FloatRegister rhs, Label *label, DoubleCondition c,
                 JumpKind jumpKind = LongJump, FPConditionBit fcc = FCC0);
    void ma_bc1d(FloatRegister lhs, FloatRegister rhs, Label *label, DoubleCondition c,
                 JumpKind jumpKind = LongJump, FPConditionBit fcc = FCC0);


    // These fuctions abstract the access to high part of the double precision
    // float register. It is intended to work on both 32 bit and 64 bit
    // floating point coprocessor.
    // :TODO: (Bug 985881) Modify this for N32 ABI to use mthc1 and mfhc1
    void moveToDoubleHi(Register src, FloatRegister dest) {
        as_mtc1(src, getOddPair(dest));
    }
    void moveFromDoubleHi(FloatRegister src, Register dest) {
        as_mfc1(dest, getOddPair(src));
    }

    void moveToDoubleLo(Register src, FloatRegister dest) {
        as_mtc1(src, dest);
    }
    void moveFromDoubleLo(FloatRegister src, Register dest) {
        as_mfc1(dest, src);
    }

    void moveToFloat32(Register src, FloatRegister dest) {
        as_mtc1(src, dest);
    }
    void moveFromFloat32(FloatRegister src, Register dest) {
        as_mfc1(dest, src);
    }

  protected:
    void branchWithCode(InstImm code, Label *label, JumpKind jumpKind);
    Condition ma_cmp(Register rd, Register lhs, Register rhs, Condition c);

    void compareFloatingPoint(FloatFormat fmt, FloatRegister lhs, FloatRegister rhs,
                              DoubleCondition c, FloatTestKind *testKind,
                              FPConditionBit fcc = FCC0);

public:
    // calls an Ion function, assumes that the stack is untouched (8 byte alinged)
    void ma_callIon(const Register reg);
    // callso an Ion function, assuming that sp has already been decremented
    void ma_callIonNoPush(const Register reg);
    // calls an ion function, assuming that the stack is currently not 8 byte aligned
    void ma_callIonHalfPush(const Register reg);

    void ma_call(void* dest);

    void ma_jump(ImmWord dest);

    void ma_cmp_set(Register dst, Register lhs, Register rhs, Condition c);
    void ma_cmp_set(Register dst, Register lhs, Imm32 imm, Condition c);
    void ma_cmp_set(Register rd, Register rs, Address addr, Condition c);
    void ma_cmp_set(Register dst, Address lhs, Register imm, Condition c);
    void ma_cmp_set_double(Register dst, FloatRegister lhs, FloatRegister rhs, DoubleCondition c);
    void ma_cmp_set_float32(Register dst, FloatRegister lhs, FloatRegister rhs, DoubleCondition c);

    void ma_cmp_set(Register dst, Register lhs, ImmWord imm, Condition c) {
        ma_cmp_set(dst, lhs, Imm32(uint32_t(imm.value)), c);
    }
    void ma_cmp_set(Register dst, Address lhs, ImmWord imm, Condition c) {
        ma_lw(ScratchRegister, lhs);
        ma_li(SecondScratchReg, Imm32(uint32_t(imm.value)));
        ma_cmp_set(dst, ScratchRegister, SecondScratchReg, c);
    }
};

class MacroAssemblerMIPSCompat : public MacroAssemblerMIPS
{
public:
    typedef MoveResolver::MoveOperand MoveOperand;
    typedef MoveResolver::Move Move;

    enum Result {
        GENERAL,
        DOUBLE
    };

    // Number of bytes the stack is adjusted inside a call to C. Calls to C may
    // not be nested.
    bool inCall_;
    uint32_t args_;
    // The actual number of arguments that were passed, used to assert that
    // the initial number of arguments declared was correct.
    uint32_t passedArgs_;

    uint32_t usedArgSlots_;
    Result firstArgType;

    bool dynamicAlignment_;

    bool enoughMemory_;
    // Compute space needed for the function call and set the properties of the
    // callee.  It returns the space which has to be allocated for calling the
    // function.
    //
    // arg            Number of arguments of the function.
    void setupABICall(uint32_t arg);

  protected:
    MoveResolver moveResolver_;

    // Extra bytes currently pushed onto the frame beyond frameDepth_. This is
    // needed to compute offsets to stack slots while temporary space has been
    // reserved for unexpected spills or C++ function calls. It is maintained
    // by functions which track stack alignment, which for clear distinction
    // use StudlyCaps (for example, Push, Pop).
    uint32_t framePushed_;
    void adjustFrame(int value) {
        setFramePushed(framePushed_ + value);
    }
  public:
    MacroAssemblerMIPSCompat()
      : inCall_(false),
        enoughMemory_(true),
        framePushed_(0)
    { }
    bool oom() const {
        return Assembler::oom();
    }

  public:
    using MacroAssemblerMIPS::call;

    void j(Label *dest) {
        ma_b(dest);
    }

    void mov(Register src, Register dest) {
        as_or(dest, src, zero);
    }
    void mov(ImmWord imm, Register dest) {
        ma_li(dest, Imm32(imm.value));
    }
    void mov(Imm32 imm, Register dest) {
        mov(ImmWord(imm.value), dest);
    }
    void mov(Register src, Address dest) {
        JS_NOT_REACHED("NYI-IC");
    }
    void mov(Address src, Register dest) {
        JS_NOT_REACHED("NYI-IC");
    }

    void call(const Register reg);
    void call(Label *label);
    void call(Imm32 imm);
    void call(ImmWord imm);
    void call(IonCode *c);
    void branch(IonCode *c) {
        BufferOffset bo = m_buffer.nextOffset();
        addPendingJump(bo, c->raw(), Relocation::IONCODE);
        ma_liPatchable(ScratchRegister, Imm32((uint32_t)c->raw()));
        as_jr(ScratchRegister);
        as_nop();
    }
    void branch(const Register reg) {
        as_jr(reg);
        as_nop();
    }
    void nop() {
        as_nop();
    }
    void ret() {
        ma_pop(ra);
        as_jr(ra);
        as_nop();
    }
    void retn(Imm32 n) {
        // pc <- [sp]; sp += n
        ma_lw(ra, Address(StackPointer, 0));
        ma_addu(StackPointer, StackPointer, n);
        as_jr(ra);
        as_nop();
    }
    void push(Imm32 imm) {
        ma_li(ScratchRegister, imm);
        ma_push(ScratchRegister);
    }
    void push(ImmWord imm) {
        ma_li(ScratchRegister, Imm32(imm.value));
        ma_push(ScratchRegister);
    }
    void push(ImmGCPtr imm) {
        ma_li(ScratchRegister, imm);
        ma_push(ScratchRegister);
    }
    void push(const Register &reg) {
        ma_push(reg);
    }
    void pop(const Register &reg) {
        ma_pop(reg);
    }

    // Emit a branch that can be toggled to a non-operation. On MIPS we use
    // "andi" instruction to toggle the branch.
    // See ToggleToJmp(), ToggleToCmp().
    CodeOffsetLabel toggledJump(Label *label);

    // Emit a "jalr" or "nop" instruction. ToggleCall can be used to patch
    // this instruction.
    CodeOffsetLabel toggledCall(IonCode *target, bool enabled);

    static size_t ToggledCallSize() {
        // Four instructions used in: MacroAssemblerMIPSCompat::toggledCall
        return 4 * sizeof(uint32_t);
    }

    CodeOffsetLabel pushWithPatch(ImmWord imm) {
        CodeOffsetLabel label = movWithPatch(imm, ScratchRegister);
        ma_push(ScratchRegister);
        return label;
    }

    CodeOffsetLabel movWithPatch(ImmWord imm, Register dest) {
        CodeOffsetLabel label = currentOffset();
        ma_liPatchable(dest, Imm32(imm.value));
        return label;
    }

    void jump(Label *label) {
        ma_b(label);
    }
    void jump(Register reg) {
        as_jr(reg);
        as_nop();
    }

    void neg32(Register reg) {
        ma_negu(reg, reg);
    }
    void negl(Register reg) {
        ma_negu(reg, reg);
    }

    // Returns the register containing the type tag.
    Register splitTagForTest(const ValueOperand &value) {
        return value.typeReg();
    }

    void branchTestGCThing(Condition cond, const Address &address, Label *label);
    void branchTestGCThing(Condition cond, const BaseIndex &src, Label *label);

    void branchTestPrimitive(Condition cond, const ValueOperand &value, Label *label);
    void branchTestPrimitive(Condition cond, const Register &tag, Label *label);

    void branchTestValue(Condition cond, const ValueOperand &value, const Value &v, Label *label);
    void branchTestValue(Condition cond, const Address &valaddr, const ValueOperand &value,
                         Label *label);

    // unboxing code
    void unboxInt32(const ValueOperand &operand, const Register &dest);
    void unboxInt32(const Address &src, const Register &dest);
    void unboxBoolean(const ValueOperand &operand, const Register &dest);
    void unboxBoolean(const Address &src, const Register &dest);
    void unboxDouble(const ValueOperand &operand, const FloatRegister &dest);
    void unboxDouble(const Address &src, const FloatRegister &dest);
    void unboxValue(const ValueOperand &src, AnyRegister dest);
    void unboxPrivate(const ValueOperand &src, Register dest);

    void notBoolean(const ValueOperand &val) {
        as_xori(val.payloadReg(), val.payloadReg(), 1);
    }

    // boxing code
    void boxDouble(const FloatRegister &src, const ValueOperand &dest);
    void boxNonDouble(JSValueType type, const Register &src, const ValueOperand &dest);

    // Extended unboxing API. If the payload is already in a register, returns
    // that register. Otherwise, provides a move to the given scratch register,
    // and returns that.
    Register extractObject(const Address &address, Register scratch);
    Register extractObject(const ValueOperand &value, Register scratch) {
        return value.payloadReg();
    }
    Register extractInt32(const ValueOperand &value, Register scratch) {
        return value.payloadReg();
    }
    Register extractBoolean(const ValueOperand &value, Register scratch) {
        return value.payloadReg();
    }
    Register extractTag(const Address &address, Register scratch);
    Register extractTag(const BaseIndex &address, Register scratch);
    Register extractTag(const ValueOperand &value, Register scratch) {
        return value.typeReg();
    }

    void boolValueToDouble(const ValueOperand &operand, const FloatRegister &dest);
    void int32ValueToDouble(const ValueOperand &operand, const FloatRegister &dest);
    void loadInt32OrDouble(const Address &address, const FloatRegister &dest);
    void loadInt32OrDouble(Register base, Register index,
                           const FloatRegister &dest, int32_t shift = defaultShift);
    void loadStaticDouble(const double *dp, const FloatRegister &dest) {
        loadConstantDouble(*dp, dest);
    }
    void loadConstantDouble(double dp, const FloatRegister &dest);

    void branchTestInt32(Condition cond, const ValueOperand &value, Label *label);
    void branchTestInt32(Condition cond, const Register &tag, Label *label);
    void branchTestInt32(Condition cond, const Address &address, Label *label);
    void branchTestInt32(Condition cond, const BaseIndex &src, Label *label);

    void branchTestInt32Truthy(bool b, const ValueOperand& value, Label* label) {
        ma_and(ScratchRegister, value.payloadReg(), value.payloadReg());
        ma_b(ScratchRegister, ScratchRegister, label, b ? NonZero : Zero);
    }

    void branchTestStringTruthy(bool b, const ValueOperand& value, Label* label) {
        Register string = value.payloadReg();
        size_t mask = (0xFFFFFFFF << JSString::LENGTH_SHIFT);
        ma_lw(SecondScratchReg, Address(string, JSString::offsetOfLengthAndFlags()));

        // Use SecondScratchReg because ma_and will clobber ScratchRegister
        ma_and(ScratchRegister, SecondScratchReg, Imm32(mask));
        ma_b(ScratchRegister, ScratchRegister, label, b ? NonZero : Zero);
    }

    void branchTestDoubleTruthy(bool b, FloatRegister value, Label* label) {
        ma_lid(ScratchFloatReg, 0.0);
        DoubleCondition cond = b ? DoubleNotEqual : DoubleEqualOrUnordered;
        ma_bc1d(value, ScratchFloatReg, label, cond);
    }

    void branchTestBooleanTruthy(bool b, const ValueOperand& operand, Label* label) {
        ma_b(operand.payloadReg(), operand.payloadReg(), label, b ? NonZero : Zero);
    }

    void branchTestBoolean(Condition cond, const ValueOperand &value, Label *label);
    void branchTestBoolean(Condition cond, const Register &tag, Label *label);
    void branchTestBoolean(Condition cond, const BaseIndex &src, Label *label);

    void branch32(Condition cond, Register lhs, Register rhs, Label *label) {
        ma_b(lhs, rhs, label, cond);
    }
    void branch32(Condition cond, Register lhs, Imm32 imm, Label *label) {
        ma_b(lhs, imm, label, cond);
    }
    void branch32(Condition cond, const Operand &lhs, Register rhs, Label *label) {
        if (lhs.getTag() == Operand::REG) {
            ma_b(lhs.toReg(), rhs, label, cond);
        } else {
            branch32(cond, lhs.toAddress(), rhs, label);
        }
    }
    void branch32(Condition cond, const Operand &lhs, Imm32 rhs, Label *label) {
        if (lhs.getTag() == Operand::REG) {
            ma_b(lhs.toReg(), rhs, label, cond);
        } else {
            branch32(cond, lhs.toAddress(), rhs, label);
        }
    }
    void branch32(Condition cond, const Address &lhs, Register rhs, Label *label) {
        ma_lw(ScratchRegister, lhs);
        ma_b(ScratchRegister, rhs, label, cond);
    }
    void branch32(Condition cond, const Address &lhs, Imm32 rhs, Label *label) {
        ma_lw(SecondScratchReg, lhs);
        ma_b(SecondScratchReg, rhs, label, cond);
    }
    void branchPtr(Condition cond, const Address &lhs, Register rhs, Label *label) {
        branch32(cond, lhs, rhs, label);
    }

    void branchPrivatePtr(Condition cond, const Address &lhs, ImmWord ptr, Label *label) {
        branchPtr(cond, lhs, ptr, label);
    }

    void branchPrivatePtr(Condition cond, const Address &lhs, Register ptr, Label *label) {
        branchPtr(cond, lhs, ptr, label);
    }

    void branchPrivatePtr(Condition cond, Register lhs, ImmWord ptr, Label *label) {
        branchPtr(cond, lhs, ptr, label);
    }

    void branchTestDouble(Condition cond, const ValueOperand &value, Label *label);
    void branchTestDouble(Condition cond, const Register &tag, Label *label);
    void branchTestDouble(Condition cond, const Address &address, Label *label);
    void branchTestDouble(Condition cond, const BaseIndex &src, Label *label);

    void branchTestNull(Condition cond, const ValueOperand &value, Label *label);
    void branchTestNull(Condition cond, const Register &tag, Label *label);
    void branchTestNull(Condition cond, const BaseIndex &src, Label *label);

    void branchTestObject(Condition cond, const ValueOperand &value, Label *label);
    void branchTestObject(Condition cond, const Register &tag, Label *label);
    void branchTestObject(Condition cond, const BaseIndex &src, Label *label);

    void branchTestString(Condition cond, const ValueOperand &value, Label *label);
    void branchTestString(Condition cond, const Register &tag, Label *label);
    void branchTestString(Condition cond, const BaseIndex &src, Label *label);

    void branchTestUndefined(Condition cond, const ValueOperand &value, Label *label);
    void branchTestUndefined(Condition cond, const Register &tag, Label *label);
    void branchTestUndefined(Condition cond, const BaseIndex &src, Label *label);
    void branchTestUndefined(Condition cond, const Address &address, Label *label);

    void branchTestNumber(Condition cond, const ValueOperand &value, Label *label);
    void branchTestNumber(Condition cond, const Register &tag, Label *label);

    void branchTestMagic(Condition cond, const ValueOperand &value, Label *label);
    void branchTestMagic(Condition cond, const Register &tag, Label *label);
    void branchTestMagic(Condition cond, const Address &address, Label *label);
    void branchTestMagic(Condition cond, const BaseIndex &src, Label *label);

    void branchTestMagicValue(Condition cond, const ValueOperand &val, JSWhyMagic why,
                              Label *label) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        // Test for magic
        Label notmagic;
        branchTestMagic(cond, val, &notmagic);
        // Test magic value
        branch32(cond, val.payloadReg(), Imm32(static_cast<int32_t>(why)), label);
        bind(&notmagic);
    }

    void branchTest32(Condition cond, const Register &lhs, const Register &rhs, Label *label) {
        JS_ASSERT(cond == Zero || cond == NonZero || cond == Signed || cond == NotSigned);
        if (lhs == rhs) {
            ma_b(lhs, rhs, label, cond);
        } else {
            as_and(ScratchRegister, lhs, rhs);
            ma_b(ScratchRegister, ScratchRegister, label, cond);
        }
    }
    void branchTest32(Condition cond, const Register &lhs, Imm32 imm, Label *label) {
        ma_li(ScratchRegister, imm);
        branchTest32(cond, lhs, ScratchRegister, label);
    }
    void branchTest32(Condition cond, const Address &address, Imm32 imm, Label *label) {
        ma_lw(SecondScratchReg, address);
        branchTest32(cond, SecondScratchReg, imm, label);
    }
    void branchTestPtr(Condition cond, const Register &lhs, const Register &rhs, Label *label) {
        branchTest32(cond, lhs, rhs, label);
    }
    void branchTestPtr(Condition cond, const Register &lhs, const Imm32 rhs, Label *label) {
        branchTest32(cond, lhs, rhs, label);
    }
    void branchTestPtr(Condition cond, const Address &lhs, Imm32 imm, Label *label) {
        branchTest32(cond, lhs, imm, label);
    }
    void branchPtr(Condition cond, Register lhs, Register rhs, Label *label) {
        ma_b(lhs, rhs, label, cond);
    }
    void branchPtr(Condition cond, Register lhs, ImmGCPtr ptr, Label *label) {
        ma_li(ScratchRegister, ptr);
        ma_b(lhs, ScratchRegister, label, cond);
    }
    void branchPtr(Condition cond, Register lhs, ImmWord imm, Label *label) {
        ma_b(lhs, Imm32(imm.value), label, cond);
    }

    void branchPtr(Condition cond, Register lhs, Imm32 imm, Label *label) {
        ma_b(lhs, imm, label, cond);
    }
    void decBranchPtr(Condition cond, const Register &lhs, Imm32 imm, Label *label) {
        subPtr(imm, lhs);
        branch32(cond, lhs, Imm32(0), label);
    }

protected:
    uint32_t getType(const Value &val);
    void moveData(const Value &val, Register data);
public:
    void moveValue(const Value &val, Register type, Register data);

    CodeOffsetJump jumpWithPatch(RepatchLabel *label);

    template <typename T>
    CodeOffsetJump branchPtrWithPatch(Condition cond, Register reg, T ptr, RepatchLabel *label) {
        movePtr(ptr, ScratchRegister);
        Label skipJump;
        ma_b(reg, ScratchRegister, &skipJump, InvertCondition(cond), ShortJump);
        CodeOffsetJump off = jumpWithPatch(label);
        bind(&skipJump);
        return off;
    }

    template <typename T>
    CodeOffsetJump branchPtrWithPatch(Condition cond, Address addr, T ptr, RepatchLabel *label) {
        loadPtr(addr, SecondScratchReg);
        movePtr(ptr, ScratchRegister);
        Label skipJump;
        ma_b(SecondScratchReg, ScratchRegister, &skipJump, InvertCondition(cond), ShortJump);
        CodeOffsetJump off = jumpWithPatch(label);
        bind(&skipJump);
        return off;
    }
    void branchPtr(Condition cond, Address addr, ImmGCPtr ptr, Label *label) {
        ma_lw(SecondScratchReg, addr);
        ma_li(ScratchRegister, ptr);
        ma_b(SecondScratchReg, ScratchRegister, label, cond);
    }
    void branchPtr(Condition cond, Address addr, ImmWord ptr, Label *label) {
        ma_lw(SecondScratchReg, addr);
        ma_b(SecondScratchReg, Imm32(ptr.value), label, cond);
    }

    void branchPtr(Condition cond, Address addr, Imm32 ptr, Label *label) {
        branchPtr(cond, addr, ImmWord(uintptr_t(ptr.value)), label);
    }

    void branchPtr(Condition cond, const AbsoluteAddress &addr, const Register &ptr, Label *label) {
        loadPtr(addr, ScratchRegister);
        ma_b(ScratchRegister, ptr, label, cond);
    }

    void branch32(Condition cond, const AbsoluteAddress &lhs, Imm32 rhs, Label *label) {
        loadPtr(lhs, SecondScratchReg); // ma_b might use scratch
        ma_b(SecondScratchReg, rhs, label, cond);
    }
    void branch32(Condition cond, const AbsoluteAddress &lhs, const Register &rhs, Label *label) {
        loadPtr(lhs, ScratchRegister);
        ma_b(ScratchRegister, rhs, label, cond);
    }

    void loadUnboxedValue(Address address, MIRType type, AnyRegister dest) {
        if (dest.isFloat())
            loadInt32OrDouble(address, dest.fpu());
        else
            ma_lw(dest.gpr(), address);
    }

    void loadUnboxedValue(BaseIndex address, MIRType type, AnyRegister dest) {
        if (dest.isFloat())
            loadInt32OrDouble(address.base, address.index, dest.fpu(), address.scale);
        else
            load32(address, dest.gpr());
    }

    void moveValue(const Value &val, const ValueOperand &dest);

    void moveValue(const ValueOperand &src, const ValueOperand &dest) {
        JS_ASSERT(src.typeReg() != dest.payloadReg());
        JS_ASSERT(src.payloadReg() != dest.typeReg());
        if (src.typeReg() != dest.typeReg())
            ma_move(dest.typeReg(), src.typeReg());
        if (src.payloadReg() != dest.payloadReg())
            ma_move(dest.payloadReg(), src.payloadReg());
    }

    void storeValue(ValueOperand val, Operand dst);
    void storeValue(ValueOperand val, const BaseIndex &dest);
    void storeValue(JSValueType type, Register reg, BaseIndex dest);
    void storeValue(ValueOperand val, const Address &dest);
    void storeValue(JSValueType type, Register reg, Address dest);
    void storeValue(const Value &val, Address dest);
    void storeValue(const Value &val, BaseIndex dest);

    void loadValue(Address src, ValueOperand val);
    void loadValue(Operand dest, ValueOperand val) {
        loadValue(dest.toAddress(), val);
    }
    void loadValue(const BaseIndex &addr, ValueOperand val);
    void tagValue(JSValueType type, Register payload, ValueOperand dest);

    void pushValue(ValueOperand val);
    void popValue(ValueOperand val);
    void pushValue(const Value &val) {
        jsval_layout jv = JSVAL_TO_IMPL(val);
        push(Imm32(jv.s.tag));
        if (val.isMarkable())
            push(ImmGCPtr(reinterpret_cast<gc::Cell *>(val.toGCThing())));
        else
            push(Imm32(jv.s.payload.i32));
    }
    void pushValue(JSValueType type, Register reg) {
        push(ImmTag(JSVAL_TYPE_TO_TAG(type)));
        ma_push(reg);
    }
    void pushValue(const Address &addr);
    void storePayload(const Value &val, Address dest);
    void storePayload(Register src, Address dest);
    void storePayload(const Value &val, Register base, Register index, int32_t shift = defaultShift);
    void storePayload(Register src, Register base, Register index, int32_t shift = defaultShift);
    void storeTypeTag(ImmTag tag, Address dest);
    void storeTypeTag(ImmTag tag, Register base, Register index, int32_t shift = defaultShift);

    void makeFrameDescriptor(Register frameSizeReg, FrameType type) {
        ma_sll(frameSizeReg, frameSizeReg, Imm32(FRAMESIZE_SHIFT));
        ma_or(frameSizeReg, frameSizeReg, Imm32(type));
    }

    void linkExitFrame();
    void linkParallelExitFrame(const Register &pt);
    void handleFailureWithHandler(void *handler);
    void handleFailureWithHandlerTail();

    /////////////////////////////////////////////////////////////////
    // Common interface.
    /////////////////////////////////////////////////////////////////
  public:
    // The following functions are exposed for use in platform-shared code.
    void Push(const Register &reg) {
        ma_push(reg);
        adjustFrame(STACK_SLOT_SIZE);
    }
    void Push(const Imm32 imm) {
        ma_li(ScratchRegister, imm);
        ma_push(ScratchRegister);
        adjustFrame(STACK_SLOT_SIZE);
    }
    void Push(const ImmWord imm) {
        ma_li(ScratchRegister, Imm32(imm.value));
        ma_push(ScratchRegister);
        adjustFrame(STACK_SLOT_SIZE);
    }
    void Push(const ImmGCPtr ptr) {
        ma_li(ScratchRegister, ptr);
        ma_push(ScratchRegister);
        adjustFrame(STACK_SLOT_SIZE);
    }
    void Push(const FloatRegister &f) {
        ma_push(f);
        adjustFrame(sizeof(double));
    }

    CodeOffsetLabel PushWithPatch(const ImmWord &word) {
        framePushed_ += sizeof(word.value);
        return pushWithPatch(word);
    }

    CodeOffsetLabel PushWithPatch(const Imm32 &imm)
    {
        return PushWithPatch(ImmWord(uintptr_t(imm.value)));
    }

    void Pop(const Register &reg) {
        ma_pop(reg);
        adjustFrame(-STACK_SLOT_SIZE);
    }
    void implicitPop(uint32_t args) {
        JS_ASSERT(args % STACK_SLOT_SIZE == 0);
        adjustFrame(-args);
    }
    uint32_t framePushed() const {
        return framePushed_;
    }
    void setFramePushed(uint32_t framePushed) {
        framePushed_ = framePushed;
    }

    // Builds an exit frame on the stack, with a return address to an internal
    // non-function. Returns offset to be passed to markSafepointAt().
    bool buildFakeExitFrame(const Register &scratch, uint32_t *offset);

    void callWithExitFrame(IonCode *target);
    void callWithExitFrame(IonCode *target, Register dynStack);

    // Makes an Ion call using the only two methods that it is sane for
    // indep code to make a call
    void callIon(const Register &callee);

    void reserveStack(uint32_t amount);
    void freeStack(uint32_t amount);
    void freeStack(Register amount);

    void add32(Register src, Register dest);
    void add32(Imm32 imm, Register dest);
    void add32(Imm32 imm, const Address &dest);
    void sub32(Imm32 imm, Register dest);
    void sub32(Register src, Register dest);

    void and32(Imm32 imm, Register dest);
    void and32(Imm32 imm, const Address &dest);
    void or32(Imm32 imm, const Address &dest);
    void xor32(Imm32 imm, Register dest);
    void xorPtr(Imm32 imm, Register dest);
    void xorPtr(Register src, Register dest);
    void orPtr(Imm32 imm, Register dest);
    void orPtr(Register src, Register dest);
    void andPtr(Imm32 imm, Register dest);
    void andPtr(Register src, Register dest);
    void addPtr(Register src, Register dest);
    void subPtr(Register src, Register dest);
    void addPtr(const Address &src, Register dest);
    void not32(Register reg);

    void move32(const Imm32 &imm, const Register &dest);
    void move32(const Register &src, const Register &dest);

    void movePtr(const Register &src, const Register &dest);
    void movePtr(const ImmWord &imm, const Register &dest);
    void movePtr(const ImmGCPtr &imm, const Register &dest);

    void load8SignExtend(const Address &address, const Register &dest);
    void load8SignExtend(const BaseIndex &src, const Register &dest);

    void load8ZeroExtend(const Address &address, const Register &dest);
    void load8ZeroExtend(const BaseIndex &src, const Register &dest);

    void load16SignExtend(const Address &address, const Register &dest);
    void load16SignExtend(const BaseIndex &src, const Register &dest);

    void load16ZeroExtend(const Address &address, const Register &dest);
    void load16ZeroExtend(const BaseIndex &src, const Register &dest);

    void load32(const Address &address, const Register &dest);
    void load32(const BaseIndex &address, const Register &dest);
    void load32(const AbsoluteAddress &address, const Register &dest);
    void load32(Operand&, Register);

    void loadPtr(const Address &address, const Register &dest);
    void loadPtr(const BaseIndex &src, const Register &dest);
    void loadPtr(const AbsoluteAddress &address, const Register &dest);

    void loadPrivate(const Address &address, const Register &dest);

    void loadDouble(const Address &addr, const FloatRegister &dest);
    void loadDouble(const BaseIndex &src, const FloatRegister &dest);

    // Load a float value into a register, then expand it to a double.
    void loadFloatAsDouble(const Address &addr, const FloatRegister &dest);
    void loadFloatAsDouble(const BaseIndex &src, const FloatRegister &dest);

    void store8(const Register &src, const Address &address);
    void store8(const Imm32 &imm, const Address &address);
    void store8(const Register &src, const BaseIndex &address);
    void store8(const Imm32 &imm, const BaseIndex &address);

    void store16(const Register &src, const Address &address);
    void store16(const Imm32 &imm, const Address &address);
    void store16(const Register &src, const BaseIndex &address);
    void store16(const Imm32 &imm, const BaseIndex &address);

    void store32(const Register &src, const AbsoluteAddress &address);
    void store32(const Register &src, const Address &address);
    void store32(const Register &src, const BaseIndex &address);
    void store32(const Imm32 &src, const Address &address);
    void store32(const Imm32 &src, const BaseIndex &address) {
        move32(src, ScratchRegister);
        storePtr(ScratchRegister, address);
    }

    void storePtr(ImmWord imm, const Address &address);
    void storePtr(ImmGCPtr imm, const Address &address);
    void storePtr(Register src, const Address &address);
    void storePtr(const Register &src, const AbsoluteAddress &dest);
    void storePtr(Register src, const BaseIndex& address);

    void storeDouble(FloatRegister src, Address addr) {
        ma_sd(src, addr);
    }
    void storeDouble(FloatRegister src, BaseIndex addr) {
        JS_ASSERT(addr.offset == 0);
        ma_sd(src, addr);
    }
    void storeDouble(const FloatRegister&, Operand);
    void moveDouble(FloatRegister src, FloatRegister dest)
    {
        as_movd(dest, src);
    }

    void storeFloat(FloatRegister src, Address addr) {
        ma_ss(src, addr);
    }
    void storeFloat(FloatRegister src, BaseIndex addr) {
        JS_ASSERT(addr.offset == 0);
        ma_ss(src, addr);
    }

    void zeroDouble(FloatRegister reg) {
        moveToDoubleLo(zero, reg);
        moveToDoubleHi(zero, reg);
    }

    void clampIntToUint8(Register reg, Register dest)
    {
        // look at (reg >> 8) if it is 0, then src shouldn't be clamped
        // if it is <0, then we want to clamp to 0,
        // otherwise, we wish to clamp to 255
        Label done;
        ma_move(ScratchRegister, reg);
        as_sra(ScratchRegister, ScratchRegister, 8);
        ma_b(ScratchRegister, ScratchRegister, &done, Assembler::Zero, ShortJump);
        {
            Label negative;
            ma_b(ScratchRegister, ScratchRegister, &negative, Assembler::Signed, ShortJump);
            {
                ma_li(reg, Imm32(255));
                ma_b(&done, ShortJump);
            }
            bind(&negative);
            {
                ma_move(reg, zero);
            }
        }
        bind(&done);
    }

    void subPtr(Imm32 imm, const Register dest);
    void addPtr(Imm32 imm, const Address &dest);
    void addPtr(ImmWord imm, const Register dest) {
        addPtr(Imm32(imm.value), dest);
    }
    void addPtr(Imm32 imm, const Register dest);

    void breakpoint();

    void branchDouble(DoubleCondition cond, const FloatRegister &lhs, const FloatRegister &rhs,
                      Label *label);

    void checkStackAlignment();

    void alignPointerUp(Register src, Register dest, uint32_t alignment);

    void rshiftPtr(Imm32 imm, Register dest) {
        ma_srl(dest, dest, imm);
    }
    void lshiftPtr(Imm32 imm, Register dest) {
        ma_sll(dest, dest, imm);
    }

    // If source is a double, load it into dest. If source is int32,
    // convert it to double. Else, branch to failure.
    void ensureDouble(const ValueOperand &source, FloatRegister dest, Label *failure);

    // Setup a call to C/C++ code, given the number of general arguments it
    // takes. Note that this only supports cdecl.
    //
    // In order for alignment to work correctly, the MacroAssembler must have a
    // consistent view of the stack displacement. It is okay to call "push"
    // manually, however, if the stack alignment were to change, the macro
    // assembler should be notified before starting a call.
    void setupAlignedABICall(uint32_t args);

    // Sets up an ABI call for when the alignment is not known. This may need a
    // scratch register.
    void setupUnalignedABICall(uint32_t args, const Register &scratch);

    // Arguments must be assigned in a left-to-right order. This process may
    // temporarily use more stack, in which case sp-relative addresses will be
    // automatically adjusted. It is extremely important that sp-relative
    // addresses are computed *after* setupABICall(). Furthermore, no
    // operations should be emitted while setting arguments.

    void passABIArg(const MoveOperand& from);
    void passABIArg(const Register& reg);
    void passABIArg(const FloatRegister& reg);
    void passABIArg(const ValueOperand& regs);

    bool buildOOLFakeExitFrame(void* fakeReturnAddr);

  private:
    void callWithABIPre(uint32_t *stackAdjust);
    // void callWithABIPost(uint32_t stackAdjust, MoveOp::Type result);
    void callWithABIPost(uint32_t stackAdjust, Result result);

  public:
    // Emits a call to a C/C++ function, resolving all argument moves.
    void callWithABI(void *fun, Result result = GENERAL);
    void callWithABI(const Address &fun, Result result = GENERAL);

    CodeOffsetLabel labelForPatch() {
        return CodeOffsetLabel(nextOffset().getOffset());
    }

    void ma_storeImm(Imm32 imm, const Address &addr) {
        ma_sw(imm, addr);
    }

    void lea(Operand addr, Register dest) {
        ma_addu(dest, addr.baseReg(), Imm32(addr.disp()));
    }

    void abiret() {
        as_jr(ra);
        as_nop();
    }

    void memIntToValue(Address Source, Address Dest) {
        load32(Source, SecondScratchReg);
        storeValue(JSVAL_TYPE_INT32, SecondScratchReg, Dest);
    }

    template <typename T1, typename T2>
    void cmp32Set(Assembler::Condition cond, T1 lhs, T2 rhs, Register dest)
    {
        ma_cmp_set(dest, lhs, rhs, cond);
    }

    template <typename T1, typename T2>
    void cmpPtrSet(Assembler::Condition cond, T1 lhs, T2 rhs, Register dest)
    {
        ma_cmp_set(dest, lhs, rhs, cond);
    }

    void testNullSet(Condition cond, const ValueOperand& value, Register dest);
    void testUndefinedSet(Condition cond, const ValueOperand& value, Register dest);
    template <typename T>
    void branchAdd32(Condition cond, T src, Register dest, Label* overflow) {
        switch (cond) {
          case Overflow:
            ma_addTestOverflow(dest, dest, src, overflow);
            break;
          default:
            JS_NOT_REACHED("NYI");
        }
    }
    template <typename T>
    void branchSub32(Condition cond, T src, Register dest, Label* overflow) {
        switch (cond) {
          case Overflow:
            ma_subTestOverflow(dest, dest, src, overflow);
            break;
          case NonZero:
          case Zero:
            sub32(src, dest);
            ma_b(dest, dest, overflow, cond);
            break;
          default:
            JS_NOT_REACHED("NYI");
        }
    }

    void PopRegsInMaskIgnore(RegisterSet set, RegisterSet ignore);

    BufferOffset ma_BoundsCheck(Register bounded) {
        BufferOffset bo = m_buffer.nextOffset();
        ma_liPatchable(bounded, Imm32(0));
        return bo;
    }
};

typedef MacroAssemblerMIPSCompat MacroAssemblerSpecific;

} // namespace jit
} // namespace js

#endif /* jit_mips_MacroAssembler_mips_h */
