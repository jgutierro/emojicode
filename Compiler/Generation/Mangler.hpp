//
//  Mangler.hpp
//  EmojicodeCompiler
//
//  Created by Theo Weidmann on 05/09/2017.
//  Copyright © 2017 Theo Weidmann. All rights reserved.
//

#ifndef Mangler_hpp
#define Mangler_hpp

#include <map>
#include <string>

namespace EmojicodeCompiler {

class Function;
class Type;
class Class;
class Type;
class TypeDefinition;

std::string mangleFunction(Function *function, const std::map<size_t, Type> &genericArgs);
std::string mangleTypeName(const Type &type);
std::string mangleClassMetaName(Class *klass);
std::string mangleValueTypeMetaName(const Type &type);
std::string mangleProtocolConformance(const Type &type, const Type &protocol);

}  // namespace EmojicodeCompiler

#endif /* Mangler_hpp */
