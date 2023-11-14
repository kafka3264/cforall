//
// Cforall Version 1.0.0 Copyright (C) 2015 University of Waterloo
//
// The contents of this file are covered under the licence agreement in the
// file "LICENCE" distributed with Cforall.
//
// Mangler.cc --
//
// Author           : Richard C. Bilson
// Created On       : Sun May 17 21:40:29 2015
// Last Modified By : Andrew Beach
// Last Modified On : Fri Oct 21 16:18:00 2022
// Update Count     : 75
//
#include "Mangler.h"

#include <algorithm>                     // for copy, transform
#include <cassert>                       // for assert, assertf
#include <functional>                    // for const_mem_fun_t, mem_fun
#include <iterator>                      // for ostream_iterator, back_insert_ite...
#include <list>                          // for _List_iterator, list, _List_const...
#include <string>                        // for string, char_traits, operator<<

#include "AST/Pass.hpp"
#include "CodeGen/OperatorTable.h"       // for OperatorInfo, operatorLookup
#include "Common/ToString.hpp"           // for toCString
#include "Common/SemanticError.h"        // for SemanticError

namespace Mangle {
	namespace {
		/// Mangles names to a unique C identifier
		struct Mangler : public ast::WithShortCircuiting, public ast::WithVisitorRef<Mangler>, public ast::WithGuards {
			Mangler( Mangle::Mode mode );
			Mangler( const Mangler & ) = delete;

			void previsit( const ast::Node * ) { visit_children = false; }

			void postvisit( const ast::ObjectDecl * declaration );
			void postvisit( const ast::FunctionDecl * declaration );
			void postvisit( const ast::TypeDecl * declaration );

			void postvisit( const ast::VoidType * voidType );
			void postvisit( const ast::BasicType * basicType );
			void postvisit( const ast::PointerType * pointerType );
			void postvisit( const ast::ArrayType * arrayType );
			void postvisit( const ast::ReferenceType * refType );
			void postvisit( const ast::FunctionType * functionType );
			void postvisit( const ast::StructInstType * aggregateUseType );
			void postvisit( const ast::UnionInstType * aggregateUseType );
			void postvisit( const ast::EnumInstType * aggregateUseType );
			void postvisit( const ast::TypeInstType * aggregateUseType );
			void postvisit( const ast::TraitInstType * inst );
			void postvisit( const ast::TupleType * tupleType );
			void postvisit( const ast::VarArgsType * varArgsType );
			void postvisit( const ast::ZeroType * zeroType );
			void postvisit( const ast::OneType * oneType );
			void postvisit( const ast::QualifiedType * qualType );

			/// The result is the current constructed mangled name.
			std::string result() const { return mangleName; }
		  private:
			std::string mangleName;         ///< Mangled name being constructed
			typedef std::map< std::string, std::pair< int, int > > VarMapType;
			VarMapType varNums;             ///< Map of type variables to indices
			int nextVarNum;                 ///< Next type variable index
			bool isTopLevel;                ///< Is the Mangler at the top level
			bool mangleOverridable;         ///< Specially mangle overridable built-in methods
			bool typeMode;                  ///< Produce a unique mangled name for a type
			bool mangleGenericParams;       ///< Include generic parameters in name mangling if true
			bool inFunctionType = false;    ///< Include type qualifiers if false.
			bool inQualifiedType = false;   ///< Add start/end delimiters around qualified type

		  private:
			Mangler( bool mangleOverridable, bool typeMode, bool mangleGenericParams,
				int nextVarNum, const VarMapType& varNums );
			friend class ast::Pass<Mangler>;

		  private:
			void mangleDecl( const ast::DeclWithType *declaration );
			void mangleRef( const ast::BaseInstType *refType, const std::string & prefix );

			void printQualifiers( const ast::Type *type );
		}; // Mangler
	} // namespace

	std::string mangle( const ast::Node * decl, Mangle::Mode mode ) {
		return ast::Pass<Mangler>::read( decl, mode );
	}

	namespace {
		Mangler::Mangler( Mangle::Mode mode )
			: nextVarNum( 0 ), isTopLevel( true ),
			mangleOverridable  ( ! mode.no_overrideable   ),
			typeMode           (   mode.type              ),
			mangleGenericParams( ! mode.no_generic_params ) {}

		Mangler::Mangler( bool mangleOverridable, bool typeMode, bool mangleGenericParams,
			int nextVarNum, const VarMapType& varNums )
			: varNums( varNums ), nextVarNum( nextVarNum ), isTopLevel( false ),
			mangleOverridable( mangleOverridable ), typeMode( typeMode ),
			mangleGenericParams( mangleGenericParams ) {}

		void Mangler::mangleDecl( const ast::DeclWithType * decl ) {
			bool wasTopLevel = isTopLevel;
			if ( isTopLevel ) {
				varNums.clear();
				nextVarNum = 0;
				isTopLevel = false;
			} // if
			mangleName += Encoding::manglePrefix;
			const CodeGen::OperatorInfo * opInfo = CodeGen::operatorLookup( decl->name );
			if ( opInfo ) {
				mangleName += std::to_string( opInfo->outputName.size() ) + opInfo->outputName;
			} else {
				mangleName += std::to_string( decl->name.size() ) + decl->name;
			} // if
			decl->get_type()->accept( *visitor );
			if ( mangleOverridable && decl->linkage.is_overrideable ) {
				// want to be able to override autogenerated and intrinsic routines,
				// so they need a different name mangling
				if ( decl->linkage == ast::Linkage::AutoGen ) {
					mangleName += Encoding::autogen;
				} else if ( decl->linkage == ast::Linkage::Intrinsic ) {
					mangleName += Encoding::intrinsic;
				} else {
					// if we add another kind of overridable function, this has to change
					assert( false && "unknown overrideable linkage" );
				} // if
			}
			isTopLevel = wasTopLevel;
		}

		void Mangler::postvisit( const ast::ObjectDecl * decl ) {
			mangleDecl( decl );
		}

		void Mangler::postvisit( const ast::FunctionDecl * decl ) {
			mangleDecl( decl );
		}

		void Mangler::postvisit( const ast::VoidType * voidType ) {
			printQualifiers( voidType );
			mangleName += Encoding::void_t;
		}

		void Mangler::postvisit( const ast::BasicType * basicType ) {
			printQualifiers( basicType );
			assertf( basicType->kind < ast::BasicType::NUMBER_OF_BASIC_TYPES, "Unhandled basic type: %d", basicType->kind );
			mangleName += Encoding::basicTypes[ basicType->kind ];
		}

		void Mangler::postvisit( const ast::PointerType * pointerType ) {
			printQualifiers( pointerType );
			// mangle void (*f)() and void f() to the same name to prevent overloading on functions and function pointers
			if ( ! pointerType->base.as<ast::FunctionType>() ) mangleName += Encoding::pointer;
			maybe_accept( pointerType->base.get(), *visitor );
		}

		void Mangler::postvisit( const ast::ArrayType * arrayType ) {
			// TODO: encode dimension
			printQualifiers( arrayType );
			mangleName += Encoding::array + "0";
			arrayType->base->accept( *visitor );
		}

		void Mangler::postvisit( const ast::ReferenceType * refType ) {
			// don't print prefix (e.g. 'R') for reference types so that references and non-references do not overload.
			// Further, do not print the qualifiers for a reference type (but do run printQualifers because of TypeDecls, etc.),
			// by pretending every reference type is a function parameter.
			GuardValue( inFunctionType );
			inFunctionType = true;
			printQualifiers( refType );
			refType->base->accept( *visitor );
		}

		void Mangler::postvisit( const ast::FunctionType * functionType ) {
			printQualifiers( functionType );
			mangleName += Encoding::function;
			// turn on inFunctionType so that printQualifiers does not print most qualifiers for function parameters,
			// since qualifiers on outermost parameter type do not differentiate function types, e.g.,
			// void (*)(const int) and void (*)(int) are the same type, but void (*)(const int *) and void (*)(int *) are different
			GuardValue( inFunctionType );
			inFunctionType = true;
			if (functionType->returns.empty()) mangleName += Encoding::void_t;
			else accept_each( functionType->returns, *visitor );
			mangleName += "_";
			accept_each( functionType->params, *visitor );
			mangleName += "_";
		}

		void Mangler::mangleRef(
				const ast::BaseInstType * refType, const std::string & prefix ) {
			printQualifiers( refType );

			mangleName += prefix + std::to_string( refType->name.length() ) + refType->name;

			if ( mangleGenericParams && ! refType->params.empty() ) {
				mangleName += "_";
				for ( const ast::Expr * param : refType->params ) {
					auto paramType = dynamic_cast< const ast::TypeExpr * >( param );
					assertf(paramType, "Aggregate parameters should be type expressions: %s", toCString(param));
					paramType->type->accept( *visitor );
				}
				mangleName += "_";
			}
		}

		void Mangler::postvisit( const ast::StructInstType * aggregateUseType ) {
			mangleRef( aggregateUseType, Encoding::struct_t );
		}

		void Mangler::postvisit( const ast::UnionInstType * aggregateUseType ) {
			mangleRef( aggregateUseType, Encoding::union_t );
		}

		void Mangler::postvisit( const ast::EnumInstType * aggregateUseType ) {
			mangleRef( aggregateUseType, Encoding::enum_t );
		}

		void Mangler::postvisit( const ast::TypeInstType * typeInst ) {
			VarMapType::iterator varNum = varNums.find( typeInst->name );
			if ( varNum == varNums.end() ) {
				mangleRef( typeInst, Encoding::type );
			} else {
				printQualifiers( typeInst );
				// Note: Can't use name here, since type variable names do not actually disambiguate a function, e.g.
				//   forall(dtype T) void f(T);
				//   forall(dtype S) void f(S);
				// are equivalent and should mangle the same way. This is accomplished by numbering the type variables when they
				// are first found and prefixing with the appropriate encoding for the type class.
				assertf( varNum->second.second < ast::TypeDecl::NUMBER_OF_KINDS, "Unhandled type variable kind: %d", varNum->second.second );
				mangleName += Encoding::typeVariables[varNum->second.second] + std::to_string( varNum->second.first );
			} // if
		}

		void Mangler::postvisit( const ast::TraitInstType * inst ) {
			printQualifiers( inst );
			mangleName += std::to_string( inst->name.size() ) + inst->name;
		}

		void Mangler::postvisit( const ast::TupleType * tupleType ) {
			printQualifiers( tupleType );
			mangleName += Encoding::tuple + std::to_string( tupleType->types.size() );
			accept_each( tupleType->types, *visitor );
		}

		void Mangler::postvisit( const ast::VarArgsType * varArgsType ) {
			printQualifiers( varArgsType );
			static const std::string vargs = "__builtin_va_list";
			mangleName += Encoding::type + std::to_string( vargs.size() ) + vargs;
		}

		void Mangler::postvisit( const ast::ZeroType * ) {
			mangleName += Encoding::zero;
		}

		void Mangler::postvisit( const ast::OneType * ) {
			mangleName += Encoding::one;
		}

		void Mangler::postvisit( const ast::QualifiedType * qualType ) {
			bool inqual = inQualifiedType;
			if ( !inqual ) {
				// N marks the start of a qualified type
				inQualifiedType = true;
				mangleName += Encoding::qualifiedTypeStart;
			}
			qualType->parent->accept( *visitor );
			qualType->child->accept( *visitor );
			if ( !inqual ) {
				// E marks the end of a qualified type
				inQualifiedType = false;
				mangleName += Encoding::qualifiedTypeEnd;
			}
		}

		void Mangler::postvisit( const ast::TypeDecl * decl ) {
			// TODO: is there any case where mangling a TypeDecl makes sense? If so, this code needs to be
			// fixed to ensure that two TypeDecls mangle to the same name when they are the same type and vice versa.
			// Note: The current scheme may already work correctly for this case, I have not thought about this deeply
			// and the case has not yet come up in practice. Alternatively, if not then this code can be removed
			// aside from the assert false.
			assertf(false, "Mangler should not visit typedecl: %s", toCString(decl));
			assertf( decl->kind < ast::TypeDecl::Kind::NUMBER_OF_KINDS, "Unhandled type variable kind: %d", decl->kind );
			mangleName += Encoding::typeVariables[ decl->kind ] + std::to_string( decl->name.length() ) + decl->name;
		}

		// For debugging:
		__attribute__((unused)) void printVarMap( const std::map< std::string, std::pair< int, int > > &varMap, std::ostream &os ) {
			for ( std::map< std::string, std::pair< int, int > >::const_iterator i = varMap.begin(); i != varMap.end(); ++i ) {
				os << i->first << "(" << i->second.first << "/" << i->second.second << ")" << std::endl;
			} // for
		}

		void Mangler::printQualifiers( const ast::Type * type ) {
			// skip if not including qualifiers
			if ( typeMode ) return;
			auto funcType = dynamic_cast<const ast::FunctionType *>( type );
			if ( funcType && !funcType->forall.empty() ) {
				std::list< std::string > assertionNames;
				int dcount = 0, fcount = 0, vcount = 0, acount = 0;
				mangleName += Encoding::forall;
				for ( auto & decl : funcType->forall ) {
					switch ( decl->kind ) {
					case ast::TypeDecl::Dtype:
						dcount++;
						break;
					case ast::TypeDecl::Ftype:
						fcount++;
						break;
					case ast::TypeDecl::Ttype:
						vcount++;
						break;
					default:
						assertf( false, "unimplemented kind for type variable %s", SymTab::Mangler::Encoding::typeVariables[decl->kind].c_str() );
					} // switch
					varNums[ decl->name ] = std::make_pair( nextVarNum, (int)decl->kind );
				} // for
				for ( auto & assert : funcType->assertions ) {
					assertionNames.push_back( ast::Pass<Mangler>::read(
						assert->var.get(),
						mangleOverridable, typeMode, mangleGenericParams, nextVarNum, varNums ) );
					acount++;
				} // for
				mangleName += std::to_string( dcount ) + "_" + std::to_string( fcount ) + "_" + std::to_string( vcount ) + "_" + std::to_string( acount ) + "_";
				for ( const auto & a : assertionNames ) mangleName += a;
				mangleName += "_";
			} // if
			if ( ! inFunctionType ) {
				// these qualifiers do not distinguish the outermost type of a function parameter
				if ( type->is_const() ) {
					mangleName += Encoding::qualifiers.at( ast::CV::Const );
				} // if
				if ( type->is_volatile() ) {
					mangleName += Encoding::qualifiers.at( ast::CV::Volatile );
				} // if
				// Removed due to restrict not affecting function compatibility in GCC
				// if ( type->get_isRestrict() ) {
				// 	mangleName += "E";
				// } // if
				if ( type->is_atomic() ) {
					mangleName += Encoding::qualifiers.at( ast::CV::Atomic );
				} // if
			}
			if ( type->is_mutex() ) {
				mangleName += Encoding::qualifiers.at( ast::CV::Mutex );
			} // if
			if ( inFunctionType ) {
				// turn off inFunctionType so that types can be differentiated for nested qualifiers
				GuardValue( inFunctionType );
				inFunctionType = false;
			}
		}
	}	// namespace
} // namespace Mangle

// Local Variables: //
// tab-width: 4 //
// mode: c++ //
// compile-command: "make install" //
// End: //
