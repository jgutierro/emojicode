//
//  AbstractParser.cpp
//  Emojicode
//
//  Created by Theo Weidmann on 27/04/16.
//  Copyright © 2016 Theo Weidmann. All rights reserved.
//

#include "AbstractParser.hpp"
#include "CompatibilityInfoProvider.hpp"
#include "Emojis.h"
#include "Functions/Function.hpp"
#include "Functions/Initializer.hpp"
#include "Package/Package.hpp"
#include "Parsing/CompatibleFunctionParser.hpp"
#include "Types/Protocol.hpp"
#include "Compiler.hpp"
#include <map>
#include <vector>

namespace EmojicodeCompiler {

TypeIdentifier AbstractParser::parseTypeIdentifier() {
    if (stream_.nextTokenIs(TokenType::Variable)) {
        throw CompilerError(stream_.consumeToken().position(), "Generic variables not allowed here.");
    }

    if (stream_.nextTokenIs(E_CANDY)) {
        throw CompilerError(stream_.consumeToken().position(), "Unexpected 🍬.");
    }

    std::u32string enamespace;

    if (stream_.consumeTokenIf(E_ORANGE_TRIANGLE)) {
        enamespace = stream_.consumeToken(TokenType::Identifier).value();
    }
    else {
        enamespace = kDefaultNamespace;
    }

    auto typeName = stream_.consumeToken();
    return TypeIdentifier(typeName.value(), enamespace, typeName.position());
}

std::unique_ptr<FunctionParser> AbstractParser::factorFunctionParser(Package *pkg, TokenStream &stream,
                                                                     TypeContext context, Function *function) {
    if (package_->compatibilityMode()) {
        package_->compatibilityInfoProvider()->selectFunction(function);
        return std::make_unique<CompatibleFunctionParser>(pkg, stream, context);
    }
    return std::make_unique<FunctionParser>(pkg, stream, context);
}

Type AbstractParser::parseType(const TypeContext &typeContext) {
    if (stream_.nextTokenIs(E_MEDIUM_BLACK_CIRCLE)) {
        throw CompilerError(stream_.consumeToken().position(), "⚫️ not allowed here.");
    }
    if (stream_.nextTokenIs(TokenType::Class) || stream_.nextTokenIs(TokenType::Enumeration) ||
            stream_.nextTokenIs(TokenType::ValueType) || stream_.nextTokenIs(TokenType::Protocol)) {
        return parseTypeAsValueType(typeContext);
    }

    bool optional = stream_.consumeTokenIf(E_CANDY);

    if (stream_.nextTokenIs(TokenType::Variable)) {
        return parseGenericVariable(optional, typeContext);
    }
    if (stream_.nextTokenIs(E_BENTO_BOX)) {
        return parseMultiProtocol(optional, typeContext);
    }
    if (stream_.nextTokenIs(TokenType::Error)) {
        return parseErrorType(optional, typeContext);
    }
    return parseTypeMain(optional, typeContext);
}

Type AbstractParser::parseTypeAsValueType(const TypeContext &typeContext) {
    auto token = stream_.consumeToken();
    auto type = parseType(typeContext);
    validateTypeAsValueType(token, type, typeContext);
    return Type(MakeTypeAsValue, type);
}

void AbstractParser::validateTypeAsValueType(const Token &token, const Type &type, const TypeContext &typeContext) {
    if (token.type() == TokenType::Class && type.type() != TypeType::Class) {
        throw CompilerError(token.position(), "Expected a class type but got ", type.toString(typeContext), ".");
    }
    if (token.type() == TokenType::Protocol && type.type() != TypeType::Protocol) {
        throw CompilerError(token.position(), "Expected a protocol type but got ", type.toString(typeContext), ".");
    }
    if (token.type() == TokenType::ValueType && type.type() != TypeType::ValueType) {
        throw CompilerError(token.position(), "Expected a value type but got ", type.toString(typeContext), ".");
    }
    if (token.type() == TokenType::Enumeration && type.type() != TypeType::Enum) {
        throw CompilerError(token.position(), "Expected a class type but got ", type.toString(typeContext), ".");
    }
}

Type AbstractParser::parseTypeMain(bool optional, const TypeContext &typeContext) {
    if (stream_.consumeTokenIf(TokenType::BlockBegin)) {
        return parseCallableType(optional, typeContext);
    }

    auto parsedTypeIdentifier = parseTypeIdentifier();
    auto type = package_->getRawType(parsedTypeIdentifier, optional);
    if (type.canHaveGenericArguments()) {
        parseGenericArgumentsForType(&type, typeContext, parsedTypeIdentifier.position);
    }
    return type;
}

Type AbstractParser::parseMultiProtocol(bool optional, const TypeContext &typeContext) {
    auto bentoToken = stream_.consumeToken(TokenType::Identifier);

    std::vector<Type> protocols;
    while (stream_.nextTokenIsEverythingBut(E_BENTO_BOX)) {
        auto protocolType = parseType(typeContext);
        if (protocolType.type() != TypeType::Protocol) {
            package_->compiler()->error(CompilerError(bentoToken.position(),
                                                      "🍱 may only consist of non-optional protocol types."));
            continue;
        }
        protocols.push_back(protocolType);
    }
    stream_.consumeToken(TokenType::Identifier);

    if (protocols.empty()) {
        throw CompilerError(bentoToken.position(), "An empty 🍱 is invalid.");
    }

    auto type = Type(std::move(protocols));
    return optional ? Type(MakeOptional, type) : type;
}

Type AbstractParser::parseCallableType(bool optional, const TypeContext &typeContext) {
    std::vector<Type> params;
    while (stream_.nextTokenIsEverythingBut(TokenType::BlockEnd) &&
            stream_.nextTokenIsEverythingBut(TokenType::RightProductionOperator)) {
        params.emplace_back(parseType(typeContext));
    }
    auto returnType = stream_.consumeTokenIf(TokenType::RightProductionOperator) ? parseType(typeContext)
                                                                                 : Type::noReturn();
    stream_.consumeToken(TokenType::BlockEnd);

    auto type = Type(returnType, params);
    return optional ? Type(MakeOptional, type) : type;
}

Type AbstractParser::parseGenericVariable(bool optional, const TypeContext &typeContext) {
    auto varToken = stream_.consumeToken(TokenType::Variable);

    Type type = Type::noReturn();
    if (typeContext.function() != nullptr && typeContext.function()->fetchVariable(varToken.value(), optional, &type)) {
        return type;
    }
    if (typeContext.calleeType().canHaveGenericArguments() &&
        typeContext.calleeType().typeDefinition()->fetchVariable(varToken.value(), optional, &type)) {
        return type;
    }

    throw CompilerError(varToken.position(), "No such generic type variable \"", utf8(varToken.value()), "\".");
}

Type AbstractParser::parseErrorEnumType(const TypeContext &typeContext, const SourcePosition &p) {
    auto errorType = parseType(typeContext);
    if (errorType.type() != TypeType::Enum) {
        throw CompilerError(p, "Error type must be a non-optional 🦃.");
    }
    return errorType;
}

Type AbstractParser::parseErrorType(bool optional, const TypeContext &typeContext) {
    auto token = stream_.consumeToken(TokenType::Error);
    Type errorEnum = parseErrorEnumType(typeContext, token.position());
    if (optional) {
        throw CompilerError(token.position(), "The error type itself cannot be an optional. "
                            "Maybe you meant to make the contained type an optional?");
    }
    Type type = Type(MakeError, errorEnum, parseType(typeContext));
    return type;
}

void AbstractParser::parseGenericArgumentsForType(Type *type, const TypeContext &typeContext, const SourcePosition &p) {
    auto typeDef = type->typeDefinition();
    auto offset = typeDef->superGenericArguments().size();
    auto args = typeDef->superGenericArguments();

    size_t count = 0;
    for (; stream_.nextTokenIs(E_SPIRAL_SHELL) && count < typeDef->genericParameters().size(); count++) {
        auto token = stream_.consumeToken(TokenType::Identifier);

        Type argument = parseType(typeContext);
        if (!argument.compatibleTo(typeDef->constraintForIndex(offset + count), typeContext)) {
            throw CompilerError(token.position(), "Generic argument for ", argument.toString(typeContext),
                                " is not compatible to constraint ",
                                typeDef->constraintForIndex(offset + count).toString(typeContext), ".");
        }
        args.emplace_back(argument);
    }

    if (count != typeDef->genericParameters().size()) {
        throw CompilerError(p, "Type ", type->toString(TypeContext()), " requires ",
                            typeDef->genericParameters().size(), " generic arguments, but ", count, " were given.");
    }

    type->setGenericArguments(std::move(args));
}

void AbstractParser::parseParameters(Function *function, const TypeContext &typeContext, bool initializer) {
    bool argumentToVariable;
    std::vector<Parameter> params;

    while ((argumentToVariable = stream_.nextTokenIs(E_BABY_BOTTLE)) || stream_.nextTokenIs(TokenType::Variable)) {
        if (argumentToVariable) {
            auto token = stream_.consumeToken(TokenType::Identifier);
            if (!initializer) {
                throw CompilerError(token.position(), "🍼 can only be used with initializers.");
            }
        }

        auto variableToken = stream_.consumeToken(TokenType::Variable);
        auto type = parseType(typeContext);

        params.emplace_back(variableToken.value(), type);

        if (argumentToVariable) {
            dynamic_cast<Initializer *>(function)->addArgumentToVariable(variableToken.value(), variableToken.position());
        }
    }
    function->setParameters(std::move(params));
}

void AbstractParser::parseReturnType(Function *function, const TypeContext &typeContext) {
    if (stream_.consumeTokenIf(TokenType::RightProductionOperator)) {
        function->setReturnType(parseType(typeContext));
    }
}

}  // namespace EmojicodeCompiler
