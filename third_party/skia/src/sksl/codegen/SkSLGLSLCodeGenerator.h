/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKSL_GLSLCODEGENERATOR
#define SKSL_GLSLCODEGENERATOR

#include <unordered_map>

#include "src/sksl/SkSLOperators.h"
#include "src/sksl/SkSLStringStream.h"
#include "src/sksl/codegen/SkSLCodeGenerator.h"

namespace SkSL {

class BinaryExpression;
class Block;
class ConstructorDiagonalMatrix;
class ConstructorScalarCast;
class DoStatement;
class ExpressionStatement;
class Extension;
class FieldAccess;
class ForStatement;
class FunctionCall;
class FunctionDeclaration;
class FunctionDefinition;
class FunctionPrototype;
class IfStatement;
struct IndexExpression;
class InterfaceBlock;
class Literal;
class PostfixExpression;
class PrefixExpression;
class ReturnStatement;
class Setting;
class StructDefinition;
class SwitchStatement;
struct Swizzle;
class TernaryExpression;
class VarDeclaration;
class VariableReference;

/**
 * Converts a Program into GLSL code.
 */
class GLSLCodeGenerator : public CodeGenerator {
public:
    GLSLCodeGenerator(const Context* context, const Program* program, OutputStream* out)
    : INHERITED(context, program, out)
    , fLineEnding("\n") {}

    bool generateCode() override;

protected:
    using Precedence = Operator::Precedence;

    void write(std::string_view s);

    void writeLine(std::string_view s = std::string_view());

    void finishLine();

    virtual void writeHeader();

    virtual bool usesPrecisionModifiers() const;

    virtual std::string getTypeName(const Type& type);

    void writeStructDefinition(const StructDefinition& s);

    void writeType(const Type& type);

    void writeExtension(std::string_view name, bool require = true);

    void writeInterfaceBlock(const InterfaceBlock& intf);

    void writeFunctionStart(const FunctionDeclaration& f);

    void writeFunctionDeclaration(const FunctionDeclaration& f);

    void writeFunctionPrototype(const FunctionPrototype& f);

    virtual void writeFunction(const FunctionDefinition& f);

    void writeLayout(const Layout& layout);

    void writeModifiers(const Modifiers& modifiers, bool globalContext);

    virtual void writeInputVars();

    virtual void writeVarInitializer(const Variable& var, const Expression& value);

    const char* getTypePrecision(const Type& type);

    void writeTypePrecision(const Type& type);

    void writeVarDeclaration(const VarDeclaration& var, bool global);

    void writeFragCoord();

    virtual void writeVariableReference(const VariableReference& ref);

    void writeExpression(const Expression& expr, Precedence parentPrecedence);

    void writeIntrinsicCall(const FunctionCall& c);

    void writeMinAbsHack(Expression& absExpr, Expression& otherExpr);

    void writeDeterminantHack(const Expression& mat);

    void writeInverseHack(const Expression& mat);

    void writeTransposeHack(const Expression& mat);

    void writeInverseSqrtHack(const Expression& x);

    void writeMatrixComparisonWorkaround(const BinaryExpression& x);

    virtual void writeFunctionCall(const FunctionCall& c);

    void writeConstructorDiagonalMatrix(const ConstructorDiagonalMatrix& c,
                                        Precedence parentPrecedence);

    virtual void writeAnyConstructor(const AnyConstructor& c, Precedence parentPrecedence);

    virtual void writeCastConstructor(const AnyConstructor& c, Precedence parentPrecedence);

    virtual void writeFieldAccess(const FieldAccess& f);

    virtual void writeSwizzle(const Swizzle& swizzle);

    virtual void writeBinaryExpression(const BinaryExpression& b, Precedence parentPrecedence);

    void writeShortCircuitWorkaroundExpression(const BinaryExpression& b,
                                               Precedence parentPrecedence);

    virtual void writeTernaryExpression(const TernaryExpression& t, Precedence parentPrecedence);

    virtual void writeIndexExpression(const IndexExpression& expr);

    void writePrefixExpression(const PrefixExpression& p, Precedence parentPrecedence);

    void writePostfixExpression(const PostfixExpression& p, Precedence parentPrecedence);

    virtual void writeLiteral(const Literal& l);

    virtual void writeSetting(const Setting& s);

    void writeStatement(const Statement& s);

    void writeBlock(const Block& b);

    virtual void writeIfStatement(const IfStatement& stmt);

    void writeForStatement(const ForStatement& f);

    void writeDoStatement(const DoStatement& d);

    void writeExpressionStatement(const ExpressionStatement& s);

    virtual void writeSwitchStatement(const SwitchStatement& s);

    virtual void writeReturnStatement(const ReturnStatement& r);

    virtual void writeProgramElement(const ProgramElement& e);

    const ShaderCaps& caps() const { return fContext.fCaps; }

    const char* fLineEnding;
    StringStream fExtensions;
    StringStream fGlobals;
    StringStream fExtraFunctions;
    std::string fFunctionHeader;
    int fVarCount = 0;
    int fIndentation = 0;
    bool fAtLineStart = false;
    std::set<std::string> fWrittenIntrinsics;
    // true if we have run into usages of dFdx / dFdy
    bool fFoundDerivatives = false;
    bool fFoundExternalSamplerDecl = false;
    bool fFoundRectSamplerDecl = false;
    bool fSetupClockwise = false;
    bool fSetupFragPosition = false;
    bool fSetupFragCoordWorkaround = false;
    // if non-empty, replace all texture / texture2D / textureProj / etc. calls with this name
    std::string fTextureFunctionOverride;

    // We map function names to function class so we can quickly deal with function calls that need
    // extra processing
    enum class FunctionClass {
        kAbs,
        kAtan,
        kDeterminant,
        kDFdx,
        kDFdy,
        kFwidth,
        kFMA,
        kFract,
        kInverse,
        kInverseSqrt,
        kMin,
        kPow,
        kSaturate,
        kTexture,
        kTranspose
    };
    static std::unordered_map<std::string_view, FunctionClass>* fFunctionClasses;

    using INHERITED = CodeGenerator;
};

}  // namespace SkSL

#endif
