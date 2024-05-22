/*
 * Copyright 2021 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/sksl/ir/SkSLFunctionDeclaration.h"

#include "include/private/SkStringView.h"
#include "src/sksl/SkSLCompiler.h"
#include "src/sksl/ir/SkSLUnresolvedFunction.h"

namespace SkSL {

static IntrinsicKind identify_intrinsic(std::string_view functionName) {
    #define SKSL_INTRINSIC(name) {#name, k_##name##_IntrinsicKind},
    static const auto* kAllIntrinsics = new std::unordered_map<std::string_view, IntrinsicKind>{
        SKSL_INTRINSIC_LIST
    };
    #undef SKSL_INTRINSIC

    if (skstd::starts_with(functionName, '$')) {
        functionName.remove_prefix(1);
    }

    auto iter = kAllIntrinsics->find(functionName);
    if (iter != kAllIntrinsics->end()) {
        return iter->second;
    }

    return kNotIntrinsic;
}

static bool check_modifiers(const Context& context,
                            int line,
                            const Modifiers& modifiers) {
    const int permitted = Modifiers::kHasSideEffects_Flag |
                          Modifiers::kInline_Flag |
                          Modifiers::kNoInline_Flag |
                          (context.fConfig->fIsBuiltinCode ? Modifiers::kES3_Flag : 0);
    modifiers.checkPermitted(context, line, permitted, /*permittedLayoutFlags=*/0);
    if ((modifiers.fFlags & Modifiers::kInline_Flag) &&
        (modifiers.fFlags & Modifiers::kNoInline_Flag)) {
        context.fErrors->error(line, "functions cannot be both 'inline' and 'noinline'");
        return false;
    }
    return true;
}

static bool check_return_type(const Context& context, int line, const Type& returnType) {
    ErrorReporter& errors = *context.fErrors;
    if (returnType.isArray()) {
        errors.error(line, "functions may not return type '" + returnType.displayName() + "'");
        return false;
    }
    if (context.fConfig->strictES2Mode() && returnType.isOrContainsArray()) {
        errors.error(line, "functions may not return structs containing arrays");
        return false;
    }
    if (!context.fConfig->fIsBuiltinCode && returnType.componentType().isOpaque()) {
        errors.error(line, "functions may not return opaque type '" + returnType.displayName() +
                           "'");
        return false;
    }
    return true;
}

static bool check_parameters(const Context& context,
                             std::vector<std::unique_ptr<Variable>>& parameters,
                             bool isMain) {
    auto typeIsValidForColor = [&](const Type& type) {
        return type.matches(*context.fTypes.fHalf4) || type.matches(*context.fTypes.fFloat4);
    };

    // The first color parameter passed to main() is the input color; the second is the dest color.
    static constexpr int kBuiltinColorIDs[] = {SK_INPUT_COLOR_BUILTIN, SK_DEST_COLOR_BUILTIN};
    unsigned int builtinColorIndex = 0;

    // Check modifiers on each function parameter.
    for (auto& param : parameters) {
        param->modifiers().checkPermitted(context, param->fLine,
                Modifiers::kConst_Flag | Modifiers::kIn_Flag | Modifiers::kOut_Flag,
                /*permittedLayoutFlags=*/0);
        const Type& type = param->type();
        // Only the (builtin) declarations of 'sample' are allowed to have shader/colorFilter or FP
        // parameters. You can pass other opaque types to functions safely; this restriction is
        // specific to "child" objects.
        if (type.isEffectChild() && !context.fConfig->fIsBuiltinCode) {
            context.fErrors->error(param->fLine, "parameters of type '" + type.displayName() +
                                                 "' not allowed");
            return false;
        }

        Modifiers m = param->modifiers();
        bool modifiersChanged = false;

        // The `in` modifier on function parameters is implicit, so we can replace `in float x` with
        // `float x`. This prevents any ambiguity when matching a function by its param types.
        if (Modifiers::kIn_Flag == (m.fFlags & (Modifiers::kOut_Flag | Modifiers::kIn_Flag))) {
            m.fFlags &= ~(Modifiers::kOut_Flag | Modifiers::kIn_Flag);
            modifiersChanged = true;
        }

        if (isMain) {
            if (ProgramConfig::IsRuntimeEffect(context.fConfig->fKind) &&
                context.fConfig->fKind != ProgramKind::kCustomMeshFragment &&
                context.fConfig->fKind != ProgramKind::kCustomMeshVertex) {
                // We verify that the signature is fully correct later. For now, if this is a
                // runtime effect of any flavor, a float2 param is supposed to be the coords, and a
                // half4/float parameter is supposed to be the input or destination color:
                if (type.matches(*context.fTypes.fFloat2)) {
                    m.fLayout.fBuiltin = SK_MAIN_COORDS_BUILTIN;
                    modifiersChanged = true;
                } else if (typeIsValidForColor(type) &&
                           builtinColorIndex < SK_ARRAY_COUNT(kBuiltinColorIDs)) {
                    m.fLayout.fBuiltin = kBuiltinColorIDs[builtinColorIndex++];
                    modifiersChanged = true;
                }
            } else if (context.fConfig->fKind == ProgramKind::kFragment) {
                // For testing purposes, we have .sksl inputs that are treated as both runtime
                // effects and fragment shaders. To make that work, fragment shaders are allowed to
                // have a coords parameter.
                if (type.matches(*context.fTypes.fFloat2)) {
                    m.fLayout.fBuiltin = SK_MAIN_COORDS_BUILTIN;
                    modifiersChanged = true;
                }
            }
        }

        if (modifiersChanged) {
            param->setModifiers(context.fModifiersPool->add(m));
        }
    }
    return true;
}

static bool check_main_signature(const Context& context, int line, const Type& returnType,
                                 std::vector<std::unique_ptr<Variable>>& parameters) {
    ErrorReporter& errors = *context.fErrors;
    ProgramKind kind = context.fConfig->fKind;

    auto typeIsValidForColor = [&](const Type& type) {
        return type.matches(*context.fTypes.fHalf4) || type.matches(*context.fTypes.fFloat4);
    };

    auto typeIsValidForAttributes = [&](const Type& type) {
        return type.isStruct() && type.name() == "Attributes";
    };

    auto typeIsValidForVaryings = [&](const Type& type) {
        return type.isStruct() && type.name() == "Varyings";
    };

    auto paramIsCoords = [&](int idx) {
        const Variable& p = *parameters[idx];
        return p.type().matches(*context.fTypes.fFloat2) &&
               p.modifiers().fFlags == 0 &&
               p.modifiers().fLayout.fBuiltin == SK_MAIN_COORDS_BUILTIN;
    };

    auto paramIsBuiltinColor = [&](int idx, int builtinID) {
        const Variable& p = *parameters[idx];
        return typeIsValidForColor(p.type()) &&
               p.modifiers().fFlags == 0 &&
               p.modifiers().fLayout.fBuiltin == builtinID;
    };

    auto paramIsInAttributes = [&](int idx) {
        const Variable& p = *parameters[idx];
        return typeIsValidForAttributes(p.type()) && p.modifiers().fFlags == 0;
    };

    auto paramIsOutVaryings = [&](int idx) {
        const Variable& p = *parameters[idx];
        return typeIsValidForVaryings(p.type()) && p.modifiers().fFlags == Modifiers::kOut_Flag;
    };

    auto paramIsInVaryings = [&](int idx) {
        const Variable& p = *parameters[idx];
        return typeIsValidForVaryings(p.type()) && p.modifiers().fFlags == 0;
    };

    auto paramIsOutColor = [&](int idx) {
        const Variable& p = *parameters[idx];
        return typeIsValidForColor(p.type()) && p.modifiers().fFlags == Modifiers::kOut_Flag;
    };

    auto paramIsInputColor = [&](int n) { return paramIsBuiltinColor(n, SK_INPUT_COLOR_BUILTIN); };
    auto paramIsDestColor  = [&](int n) { return paramIsBuiltinColor(n, SK_DEST_COLOR_BUILTIN); };

    switch (kind) {
        case ProgramKind::kRuntimeColorFilter: {
            // (half4|float4) main(half4|float4)
            if (!typeIsValidForColor(returnType)) {
                errors.error(line, "'main' must return: 'vec4', 'float4', or 'half4'");
                return false;
            }
            bool validParams = (parameters.size() == 1 && paramIsInputColor(0));
            if (!validParams) {
                errors.error(line, "'main' parameter must be 'vec4', 'float4', or 'half4'");
                return false;
            }
            break;
        }
        case ProgramKind::kRuntimeShader: {
            // (half4|float4) main(float2)  -or-  (half4|float4) main(float2, half4|float4)
            if (!typeIsValidForColor(returnType)) {
                errors.error(line, "'main' must return: 'vec4', 'float4', or 'half4'");
                return false;
            }
            bool validParams =
                    (parameters.size() == 1 && paramIsCoords(0)) ||
                    (parameters.size() == 2 && paramIsCoords(0) && paramIsInputColor(1));
            if (!validParams) {
                errors.error(line, "'main' parameters must be (float2, (vec4|float4|half4)?)");
                return false;
            }
            break;
        }
        case ProgramKind::kRuntimeBlender: {
            // (half4|float4) main(half4|float4, half4|float4)
            if (!typeIsValidForColor(returnType)) {
                errors.error(line, "'main' must return: 'vec4', 'float4', or 'half4'");
                return false;
            }
            if (!(parameters.size() == 2 &&
                  paramIsInputColor(0) &&
                  paramIsDestColor(1))) {
                errors.error(line, "'main' parameters must be (vec4|float4|half4, "
                                                                "vec4|float4|half4)");
                return false;
            }
            break;
        }
        case ProgramKind::kCustomMeshVertex: {
            // float2 main(Attributes, out Varyings)
            if (!returnType.matches(*context.fTypes.fFloat2)) {
                errors.error(line, "'main' must return: 'vec2' or 'float2'");
                return false;
            }
            if (!(parameters.size() == 2 && paramIsInAttributes(0) && paramIsOutVaryings(1))) {
                errors.error(line, "'main' parameters must be (Attributes, out Varyings");
                return false;
            }
            break;
        }
        case ProgramKind::kCustomMeshFragment: {
            // float2 main(Varyings) -or- float2 main(Varyings, out half4|float4]) -or-
            // void main(Varyings) -or- void main(Varyings, out half4|float4])
            if (!returnType.matches(*context.fTypes.fFloat2) &&
                !returnType.matches(*context.fTypes.fVoid)) {
                errors.error(line, "'main' must return: 'vec2', 'float2', 'or' 'void'");
                return false;
            }
            if (!((parameters.size() == 1 && paramIsInVaryings(0)) ||
                  (parameters.size() == 2 && paramIsInVaryings(0) && paramIsOutColor(1)))) {
                errors.error(line, "'main' parameters must be (Varyings, (out (half4|float4))?)");
                return false;
            }
            break;
        }
        case ProgramKind::kGeneric:
            // No rules apply here
            break;
        case ProgramKind::kFragment: {
            bool validParams = (parameters.size() == 0) ||
                               (parameters.size() == 1 && paramIsCoords(0));
            if (!validParams) {
                errors.error(line, "shader 'main' must be main() or main(float2)");
                return false;
            }
            break;
        }
        case ProgramKind::kVertex:
            if (parameters.size()) {
                errors.error(line, "shader 'main' must have zero parameters");
                return false;
            }
            break;
    }
    return true;
}

/**
 * Checks for a previously existing declaration of this function, reporting errors if there is an
 * incompatible symbol. Returns true and sets outExistingDecl to point to the existing declaration
 * (or null if none) on success, returns false on error.
 */
static bool find_existing_declaration(const Context& context,
                                      SymbolTable& symbols,
                                      int line,
                                      std::string_view name,
                                      std::vector<std::unique_ptr<Variable>>& parameters,
                                      const Type* returnType,
                                      const FunctionDeclaration** outExistingDecl) {
    ErrorReporter& errors = *context.fErrors;
    const Symbol* entry = symbols[name];
    *outExistingDecl = nullptr;
    if (entry) {
        std::vector<const FunctionDeclaration*> functions;
        switch (entry->kind()) {
            case Symbol::Kind::kUnresolvedFunction:
                functions = entry->as<UnresolvedFunction>().functions();
                break;
            case Symbol::Kind::kFunctionDeclaration:
                functions.push_back(&entry->as<FunctionDeclaration>());
                break;
            default:
                errors.error(line, "symbol '" + std::string(name) + "' was already defined");
                return false;
        }
        for (const FunctionDeclaration* other : functions) {
            SkASSERT(name == other->name());
            if (parameters.size() != other->parameters().size()) {
                continue;
            }
            bool match = true;
            for (size_t i = 0; i < parameters.size(); i++) {
                if (!parameters[i]->type().matches(other->parameters()[i]->type())) {
                    match = false;
                    break;
                }
            }
            if (!match) {
                continue;
            }
            if (!returnType->matches(other->returnType())) {
                std::vector<const Variable*> paramPtrs;
                paramPtrs.reserve(parameters.size());
                for (std::unique_ptr<Variable>& param : parameters) {
                    paramPtrs.push_back(param.get());
                }
                FunctionDeclaration invalidDecl(line,
                                                &other->modifiers(),
                                                name,
                                                std::move(paramPtrs),
                                                returnType,
                                                context.fConfig->fIsBuiltinCode);
                errors.error(line,
                             "functions '" + invalidDecl.description() + "' and '" +
                             other->description() + "' differ only in return type");
                return false;
            }
            for (size_t i = 0; i < parameters.size(); i++) {
                if (parameters[i]->modifiers() != other->parameters()[i]->modifiers()) {
                    errors.error(line, "modifiers on parameter " + std::to_string(i + 1) +
                                       " differ between declaration and definition");
                    return false;
                }
            }
            if (other->definition() && !other->isBuiltin()) {
                errors.error(line, "duplicate definition of " + other->description());
                return false;
            }
            *outExistingDecl = other;
            break;
        }
    }
    return true;
}

FunctionDeclaration::FunctionDeclaration(int line,
                                         const Modifiers* modifiers,
                                         std::string_view name,
                                         std::vector<const Variable*> parameters,
                                         const Type* returnType,
                                         bool builtin)
        : INHERITED(line, kSymbolKind, name, /*type=*/nullptr)
        , fDefinition(nullptr)
        , fModifiers(modifiers)
        , fParameters(std::move(parameters))
        , fReturnType(returnType)
        , fBuiltin(builtin)
        , fIsMain(name == "main")
        , fIntrinsicKind(builtin ? identify_intrinsic(name) : kNotIntrinsic) {}

const FunctionDeclaration* FunctionDeclaration::Convert(
        const Context& context,
        SymbolTable& symbols,
        int line,
        const Modifiers* modifiers,
        std::string_view name,
        std::vector<std::unique_ptr<Variable>> parameters,
        const Type* returnType) {
    bool isMain = (name == "main");

    const FunctionDeclaration* decl = nullptr;
    if (!check_modifiers(context, line, *modifiers) ||
        !check_return_type(context, line, *returnType) ||
        !check_parameters(context, parameters, isMain) ||
        (isMain && !check_main_signature(context, line, *returnType, parameters)) ||
        !find_existing_declaration(context, symbols, line, name, parameters, returnType, &decl)) {
        return nullptr;
    }
    std::vector<const Variable*> finalParameters;
    finalParameters.reserve(parameters.size());
    for (std::unique_ptr<Variable>& param : parameters) {
        finalParameters.push_back(symbols.takeOwnershipOfSymbol(std::move(param)));
    }
    if (decl) {
        return decl;
    }
    auto result = std::make_unique<FunctionDeclaration>(line, modifiers, name,
                                                        std::move(finalParameters), returnType,
                                                        context.fConfig->fIsBuiltinCode);
    return symbols.add(std::move(result));
}

std::string FunctionDeclaration::mangledName() const {
    if ((this->isBuiltin() && !this->definition()) || this->isMain()) {
        // Builtins without a definition (like `sin` or `sqrt`) must use their real names.
        return std::string(this->name());
    }
    // Built-in functions can have a $ prefix, which will fail to compile in GLSL/Metal. Remove the
    // $ and add a unique mangling specifier, so user code can't conflict with the name.
    std::string_view name = this->name();
    const char* builtinMarker = "";
    if (skstd::starts_with(name, '$')) {
        name.remove_prefix(1);
        builtinMarker = "Q";  // a unique, otherwise-unused mangle character
    }
    // GLSL forbids two underscores in a row; add an extra character if necessary to avoid this.
    const char* splitter = skstd::ends_with(name, '_') ? "x_" : "_";
    // Rename function to `funcname_returntypeparamtypes`.
    std::string result = std::string(name) + splitter + builtinMarker +
                         this->returnType().abbreviatedName();
    for (const Variable* p : this->parameters()) {
        result += p->type().abbreviatedName();
    }
    return result;
}

std::string FunctionDeclaration::description() const {
    std::string result = this->returnType().displayName() + " " + std::string(this->name()) + "(";
    std::string separator;
    for (const Variable* p : this->parameters()) {
        result += separator;
        separator = ", ";
        result += p->type().displayName();
        result += " ";
        result += p->name();
    }
    result += ")";
    return result;
}

bool FunctionDeclaration::matches(const FunctionDeclaration& f) const {
    if (this->name() != f.name()) {
        return false;
    }
    const std::vector<const Variable*>& parameters = this->parameters();
    const std::vector<const Variable*>& otherParameters = f.parameters();
    if (parameters.size() != otherParameters.size()) {
        return false;
    }
    for (size_t i = 0; i < parameters.size(); i++) {
        if (!parameters[i]->type().matches(otherParameters[i]->type())) {
            return false;
        }
    }
    return true;
}

bool FunctionDeclaration::determineFinalTypes(const ExpressionArray& arguments,
                                              ParamTypes* outParameterTypes,
                                              const Type** outReturnType) const {
    const std::vector<const Variable*>& parameters = this->parameters();
    SkASSERT(arguments.size() == parameters.size());

    outParameterTypes->reserve_back(arguments.size());
    int genericIndex = -1;
    for (size_t i = 0; i < arguments.size(); i++) {
        // Non-generic parameters are final as-is.
        const Type& parameterType = parameters[i]->type();
        if (parameterType.typeKind() != Type::TypeKind::kGeneric) {
            outParameterTypes->push_back(&parameterType);
            continue;
        }
        // We use the first generic parameter we find to lock in the generic index;
        // e.g. if we find `float3` here, all `$genType`s will be assumed to be `float3`.
        const std::vector<const Type*>& types = parameterType.coercibleTypes();
        if (genericIndex == -1) {
            for (size_t j = 0; j < types.size(); j++) {
                if (arguments[i]->type().canCoerceTo(*types[j], /*allowNarrowing=*/true)) {
                    genericIndex = j;
                    break;
                }
            }
            if (genericIndex == -1) {
                // The passed-in type wasn't a match for ANY of the generic possibilities.
                // This function isn't a match at all.
                return false;
            }
        }
        outParameterTypes->push_back(types[genericIndex]);
    }
    // Apply the generic index to our return type.
    const Type& returnType = this->returnType();
    if (returnType.typeKind() == Type::TypeKind::kGeneric) {
        if (genericIndex == -1) {
            // We don't support functions with a generic return type and no other generics.
            return false;
        }
        *outReturnType = returnType.coercibleTypes()[genericIndex];
    } else {
        *outReturnType = &returnType;
    }
    return true;
}

}  // namespace SkSL
