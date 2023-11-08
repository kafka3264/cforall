//
// Cforall Version 1.0.0 Copyright (C) 2015 University of Waterloo
//
// The contents of this file are covered under the licence agreement in the
// file "LICENCE" distributed with Cforall.
//
// GenType.cc --
//
// Author           : Richard C. Bilson
// Created On       : Mon May 18 07:44:20 2015
// Last Modified By : Andrew Beach
// Last Modified On : Fri May 20 11:18:00 2022
// Update Count     : 24
//
#include "GenType.h"

#include <cassert>                // for assert, assertf
#include <list>                   // for _List_iterator, _List_const_iterator
#include <sstream>                // for operator<<, ostringstream, basic_os...

#include "AST/Print.hpp"          // for print
#include "AST/Vector.hpp"         // for vector
#include "CodeGenerator.h"        // for CodeGenerator
#include "CodeGeneratorNew.hpp"   // for CodeGenerator_new
#include "Common/UniqueName.h"    // for UniqueName
#include "SynTree/Declaration.h"  // for DeclarationWithType
#include "SynTree/Expression.h"   // for Expression
#include "SynTree/Type.h"         // for PointerType, Type, FunctionType
#include "SynTree/Visitor.h"      // for Visitor

namespace CodeGen {
	struct GenType : public WithVisitorRef<GenType>, public WithShortCircuiting {
		std::string typeString;
		GenType( const std::string &typeString, const Options &options );

		void previsit( BaseSyntaxNode * );
		void postvisit( BaseSyntaxNode * );

		void postvisit( FunctionType * funcType );
		void postvisit( VoidType * voidType );
		void postvisit( BasicType * basicType );
		void postvisit( PointerType * pointerType );
		void postvisit( ArrayType * arrayType );
		void postvisit( ReferenceType * refType );
		void postvisit( StructInstType * structInst );
		void postvisit( UnionInstType * unionInst );
		void postvisit( EnumInstType * enumInst );
		void postvisit( TypeInstType * typeInst );
		void postvisit( TupleType  * tupleType );
		void postvisit( VarArgsType * varArgsType );
		void postvisit( ZeroType * zeroType );
		void postvisit( OneType * oneType );
		void postvisit( GlobalScopeType * globalType );
		void postvisit( TraitInstType * inst );
		void postvisit( TypeofType * typeof );
		void postvisit( VTableType * vtable );
		void postvisit( QualifiedType * qualType );

	  private:
		void handleQualifiers( Type *type );
		std::string handleGeneric( ReferenceToType * refType );
		void genArray( const Type::Qualifiers &qualifiers, Type *base, Expression *dimension, bool isVarLen, bool isStatic );

		Options options;
	};

	std::string genType( Type *type, const std::string &baseString, const Options &options ) {
		PassVisitor<GenType> gt( baseString, options );
		std::ostringstream os;

		if ( ! type->get_attributes().empty() ) {
			PassVisitor<CodeGenerator> cg( os, options );
			cg.pass.genAttributes( type->get_attributes() );
		} // if

		type->accept( gt );
		return os.str() + gt.pass.typeString;
	}

	std::string genType( Type *type, const std::string &baseString, bool pretty, bool genC , bool lineMarks ) {
		return genType( type, baseString, Options(pretty, genC, lineMarks, false ) );
	}

	std::string genPrettyType( Type * type, const std::string & baseString ) {
		return genType( type, baseString, true, false );
	}

	GenType::GenType( const std::string &typeString, const Options &options ) : typeString( typeString ), options( options ) {}

	// *** BaseSyntaxNode
	void GenType::previsit( BaseSyntaxNode * ) {
		// turn off automatic recursion for all nodes, to allow each visitor to
		// precisely control the order in which its children are visited.
		visit_children = false;
	}

	void GenType::postvisit( BaseSyntaxNode * node ) {
		std::stringstream ss;
		node->print( ss );
		assertf( false, "Unhandled node reached in GenType: %s", ss.str().c_str() );
	}

	void GenType::postvisit( VoidType * voidType ) {
		typeString = "void " + typeString;
		handleQualifiers( voidType );
	}

	void GenType::postvisit( BasicType * basicType ) {
		BasicType::Kind kind = basicType->kind;
		assert( 0 <= kind && kind < BasicType::NUMBER_OF_BASIC_TYPES );
		typeString = std::string( BasicType::typeNames[kind] ) + " " + typeString;
		handleQualifiers( basicType );
	}

	void GenType::genArray( const Type::Qualifiers & qualifiers, Type * base, Expression *dimension, bool isVarLen, bool isStatic ) {
		std::ostringstream os;
		if ( typeString != "" ) {
			if ( typeString[ 0 ] == '*' ) {
				os << "(" << typeString << ")";
			} else {
				os << typeString;
			} // if
		} // if
		os << "[";

		if ( isStatic ) {
			os << "static ";
		} // if
		if ( qualifiers.is_const ) {
			os << "const ";
		} // if
		if ( qualifiers.is_volatile ) {
			os << "volatile ";
		} // if
		if ( qualifiers.is_restrict ) {
			os << "__restrict ";
		} // if
		if ( qualifiers.is_atomic ) {
			os << "_Atomic ";
		} // if
		if ( dimension != 0 ) {
			PassVisitor<CodeGenerator> cg( os, options );
			dimension->accept( cg );
		} else if ( isVarLen ) {
			// no dimension expression on a VLA means it came in with the * token
			os << "*";
		} // if
		os << "]";

		typeString = os.str();

		base->accept( *visitor );
	}

	void GenType::postvisit( PointerType * pointerType ) {
		assert( pointerType->base != 0);
		if ( pointerType->get_isStatic() || pointerType->get_isVarLen() || pointerType->dimension ) {
			genArray( pointerType->get_qualifiers(), pointerType->base, pointerType->dimension, pointerType->get_isVarLen(), pointerType->get_isStatic() );
		} else {
			handleQualifiers( pointerType );
			if ( typeString[ 0 ] == '?' ) {
				typeString = "* " + typeString;
			} else {
				typeString = "*" + typeString;
			} // if
			pointerType->base->accept( *visitor );
		} // if
	}

	void GenType::postvisit( ArrayType * arrayType ) {
		genArray( arrayType->get_qualifiers(), arrayType->base, arrayType->dimension, arrayType->get_isVarLen(), arrayType->get_isStatic() );
	}

	void GenType::postvisit( ReferenceType * refType ) {
		assert( 0 != refType->base );
		assertf( ! options.genC, "Reference types should not reach code generation." );
		handleQualifiers( refType );
		typeString = "&" + typeString;
		refType->base->accept( *visitor );
	}

	void GenType::postvisit( FunctionType * funcType ) {
		std::ostringstream os;

		if ( typeString != "" ) {
			if ( typeString[ 0 ] == '*' ) {
				os << "(" << typeString << ")";
			} else {
				os << typeString;
			} // if
		} // if

		/************* parameters ***************/

		const std::list<DeclarationWithType *> &pars = funcType->parameters;

		if ( pars.empty() ) {
			if ( funcType->get_isVarArgs() ) {
				os << "()";
			} else {
				os << "(void)";
			} // if
		} else {
			PassVisitor<CodeGenerator> cg( os, options );
			os << "(" ;

			cg.pass.genCommaList( pars.begin(), pars.end() );

			if ( funcType->get_isVarArgs() ) {
				os << ", ...";
			} // if
			os << ")";
		} // if

		typeString = os.str();

		if ( funcType->returnVals.size() == 0 ) {
			typeString = "void " + typeString;
		} else {
			funcType->returnVals.front()->get_type()->accept( *visitor );
		} // if

		// add forall
		if( ! funcType->forall.empty() && ! options.genC ) {
			// assertf( ! genC, "Aggregate type parameters should not reach code generation." );
			std::ostringstream os;
			PassVisitor<CodeGenerator> cg( os, options );
			os << "forall(";
			cg.pass.genCommaList( funcType->forall.begin(), funcType->forall.end() );
			os << ")" << std::endl;
			typeString = os.str() + typeString;
		}
	}

	std::string GenType::handleGeneric( ReferenceToType * refType ) {
		if ( ! refType->parameters.empty() ) {
			std::ostringstream os;
			PassVisitor<CodeGenerator> cg( os, options );
			os << "(";
			cg.pass.genCommaList( refType->parameters.begin(), refType->parameters.end() );
			os << ") ";
			return os.str();
		}
		return "";
	}

	void GenType::postvisit( StructInstType * structInst )  {
		typeString = structInst->name + handleGeneric( structInst ) + " " + typeString;
		if ( options.genC ) typeString = "struct " + typeString;
		handleQualifiers( structInst );
	}

	void GenType::postvisit( UnionInstType * unionInst ) {
		typeString = unionInst->name + handleGeneric( unionInst ) + " " + typeString;
		if ( options.genC ) typeString = "union " + typeString;
		handleQualifiers( unionInst );
	}

	void GenType::postvisit( EnumInstType * enumInst ) {
		if ( enumInst->baseEnum && enumInst->baseEnum->base ) {
			typeString = genType(enumInst->baseEnum->base, typeString, options);
		} else {
			typeString = enumInst->name + " " + typeString;
			if ( options.genC ) {
				typeString = "enum " + typeString;
			}
		}
		handleQualifiers( enumInst );
	}

	void GenType::postvisit( TypeInstType * typeInst ) {
		assertf( ! options.genC, "Type instance types should not reach code generation." );
		typeString = typeInst->name + " " + typeString;
		handleQualifiers( typeInst );
	}

	void GenType::postvisit( TupleType * tupleType ) {
		assertf( ! options.genC, "Tuple types should not reach code generation." );
		unsigned int i = 0;
		std::ostringstream os;
		os << "[";
		for ( Type * t : *tupleType ) {
			i++;
			os << genType( t, "", options ) << (i == tupleType->size() ? "" : ", ");
		}
		os << "] ";
		typeString = os.str() + typeString;
	}

	void GenType::postvisit( VarArgsType * varArgsType ) {
		typeString = "__builtin_va_list " + typeString;
		handleQualifiers( varArgsType );
	}

	void GenType::postvisit( ZeroType * zeroType ) {
		// ideally these wouldn't hit codegen at all, but should be safe to make them ints
		typeString = (options.pretty ? "zero_t " : "long int ") + typeString;
		handleQualifiers( zeroType );
	}

	void GenType::postvisit( OneType * oneType ) {
		// ideally these wouldn't hit codegen at all, but should be safe to make them ints
		typeString = (options.pretty ? "one_t " : "long int ") + typeString;
		handleQualifiers( oneType );
	}

	void GenType::postvisit( GlobalScopeType * globalType ) {
		assertf( ! options.genC, "Global scope type should not reach code generation." );
		handleQualifiers( globalType );
	}

	void GenType::postvisit( TraitInstType * inst ) {
		assertf( ! options.genC, "Trait types should not reach code generation." );
		typeString = inst->name + " " + typeString;
		handleQualifiers( inst );
	}

	void GenType::postvisit( TypeofType * typeof ) {
		std::ostringstream os;
		PassVisitor<CodeGenerator> cg( os, options );
		os << "typeof(";
		typeof->expr->accept( cg );
		os << ") " << typeString;
		typeString = os.str();
		handleQualifiers( typeof );
	}

	void GenType::postvisit( VTableType * vtable ) {
		assertf( ! options.genC, "Virtual table types should not reach code generation." );
		std::ostringstream os;
		os << "vtable(" << genType( vtable->base, "", options ) << ") " << typeString;
		typeString = os.str();
		handleQualifiers( vtable );
	}

	void GenType::postvisit( QualifiedType * qualType ) {
		assertf( ! options.genC, "Qualified types should not reach code generation." );
		std::ostringstream os;
		os << genType( qualType->parent, "", options ) << "." << genType( qualType->child, "", options ) << typeString;
		typeString = os.str();
		handleQualifiers( qualType );
	}

	void GenType::handleQualifiers( Type * type ) {
		if ( type->get_const() ) {
			typeString = "const " + typeString;
		} // if
		if ( type->get_volatile() ) {
			typeString = "volatile " + typeString;
		} // if
		if ( type->get_restrict() ) {
			typeString = "__restrict " + typeString;
		} // if
		if ( type->get_atomic() ) {
			typeString = "_Atomic " + typeString;
		} // if
	}

namespace {

#warning Remove the _new when old version is removed.
struct GenType_new :
		public ast::WithShortCircuiting,
		public ast::WithVisitorRef<GenType_new> {
	std::string result;
	GenType_new( const std::string &typeString, const Options &options );

	void previsit( ast::Node const * );
	void postvisit( ast::Node const * );

	void postvisit( ast::FunctionType const * type );
	void postvisit( ast::VoidType const * type );
	void postvisit( ast::BasicType const * type );
	void postvisit( ast::PointerType const * type );
	void postvisit( ast::ArrayType const * type );
	void postvisit( ast::ReferenceType const * type );
	void postvisit( ast::StructInstType const * type );
	void postvisit( ast::UnionInstType const * type );
	void postvisit( ast::EnumInstType const * type );
	void postvisit( ast::TypeInstType const * type );
	void postvisit( ast::TupleType const * type );
	void postvisit( ast::VarArgsType const * type );
	void postvisit( ast::ZeroType const * type );
	void postvisit( ast::OneType const * type );
	void postvisit( ast::GlobalScopeType const * type );
	void postvisit( ast::TraitInstType const * type );
	void postvisit( ast::TypeofType const * type );
	void postvisit( ast::VTableType const * type );
	void postvisit( ast::QualifiedType const * type );

private:
	void handleQualifiers( ast::Type const *type );
	std::string handleGeneric( ast::BaseInstType const * type );
	void genArray( const ast::CV::Qualifiers &qualifiers, ast::Type const *base, ast::Expr const *dimension, bool isVarLen, bool isStatic );
	std::string genParamList( const ast::vector<ast::Type> & );

	Options options;
};

GenType_new::GenType_new( const std::string &typeString, const Options &options ) : result( typeString ), options( options ) {}

void GenType_new::previsit( ast::Node const * ) {
	// Turn off automatic recursion for all nodes, to allow each visitor to
	// precisely control the order in which its children are visited.
	visit_children = false;
}

void GenType_new::postvisit( ast::Node const * node ) {
	std::stringstream ss;
	ast::print( ss, node );
	assertf( false, "Unhandled node reached in GenType: %s", ss.str().c_str() );
}

void GenType_new::postvisit( ast::VoidType const * type ) {
	result = "void " + result;
	handleQualifiers( type );
}

void GenType_new::postvisit( ast::BasicType const * type ) {
	ast::BasicType::Kind kind = type->kind;
	assert( 0 <= kind && kind < ast::BasicType::NUMBER_OF_BASIC_TYPES );
	result = std::string( ast::BasicType::typeNames[kind] ) + " " + result;
	handleQualifiers( type );
}

void GenType_new::genArray( const ast::CV::Qualifiers & qualifiers, ast::Type const * base, ast::Expr const *dimension, bool isVarLen, bool isStatic ) {
	std::ostringstream os;
	if ( result != "" ) {
		if ( result[ 0 ] == '*' ) {
			os << "(" << result << ")";
		} else {
			os << result;
		}
	}
	os << "[";
	if ( isStatic ) {
		os << "static ";
	}
	if ( qualifiers.is_const ) {
		os << "const ";
	}
	if ( qualifiers.is_volatile ) {
		os << "volatile ";
	}
	if ( qualifiers.is_restrict ) {
		os << "__restrict ";
	}
	if ( qualifiers.is_atomic ) {
		os << "_Atomic ";
	}
	if ( dimension != 0 ) {
		ast::Pass<CodeGenerator_new>::read( dimension, os, options );
	} else if ( isVarLen ) {
		// no dimension expression on a VLA means it came in with the * token
		os << "*";
	}
	os << "]";

	result = os.str();

	base->accept( *visitor );
}

void GenType_new::postvisit( ast::PointerType const * type ) {
	if ( type->isStatic || type->isVarLen || type->dimension ) {
		genArray( type->qualifiers, type->base, type->dimension, type->isVarLen, type->isStatic );
	} else {
		handleQualifiers( type );
		if ( result[ 0 ] == '?' ) {
			result = "* " + result;
		} else {
			result = "*" + result;
		}
		type->base->accept( *visitor );
	}
}

void GenType_new::postvisit( ast::ArrayType const * type ) {
	genArray( type->qualifiers, type->base, type->dimension, type->isVarLen, type->isStatic );
}

void GenType_new::postvisit( ast::ReferenceType const * type ) {
	assertf( !options.genC, "Reference types should not reach code generation." );
	handleQualifiers( type );
	result = "&" + result;
	type->base->accept( *visitor );
}

void GenType_new::postvisit( ast::FunctionType const * type ) {
	std::ostringstream os;

	if ( result != "" ) {
		if ( result[ 0 ] == '*' ) {
			os << "(" << result << ")";
		} else {
			os << result;
		}
	}

	if ( type->params.empty() ) {
		if ( type->isVarArgs ) {
			os << "()";
		} else {
			os << "(void)";
		}
	} else {
		os << "(" ;

		os << genParamList( type->params );

		if ( type->isVarArgs ) {
			os << ", ...";
		}
		os << ")";
	}

	result = os.str();

	if ( type->returns.size() == 0 ) {
		result = "void " + result;
	} else {
		type->returns.front()->accept( *visitor );
	}

	// Add forall clause.
	if( !type->forall.empty() && !options.genC ) {
		//assertf( !options.genC, "FunctionDecl type parameters should not reach code generation." );
		std::ostringstream os;
		ast::Pass<CodeGenerator_new> cg( os, options );
		os << "forall(";
		cg.core.genCommaList( type->forall );
		os << ")" << std::endl;
		result = os.str() + result;
	}
}

std::string GenType_new::handleGeneric( ast::BaseInstType const * type ) {
	if ( !type->params.empty() ) {
		std::ostringstream os;
		ast::Pass<CodeGenerator_new> cg( os, options );
		os << "(";
		cg.core.genCommaList( type->params );
		os << ") ";
		return os.str();
	}
	return "";
}

void GenType_new::postvisit( ast::StructInstType const * type )  {
	result = type->name + handleGeneric( type ) + " " + result;
	if ( options.genC ) result = "struct " + result;
	handleQualifiers( type );
}

void GenType_new::postvisit( ast::UnionInstType const * type ) {
	result = type->name + handleGeneric( type ) + " " + result;
	if ( options.genC ) result = "union " + result;
	handleQualifiers( type );
}

void GenType_new::postvisit( ast::EnumInstType const * type ) {
	if ( type->base && type->base->base ) {
		result = genType( type->base->base, result, options );
	} else {
		result = type->name + " " + result;
		if ( options.genC ) {
			result = "enum " + result;
		}
	}
	handleQualifiers( type );
}

void GenType_new::postvisit( ast::TypeInstType const * type ) {
	assertf( !options.genC, "TypeInstType should not reach code generation." );
	result = type->name + " " + result;
	handleQualifiers( type );
}

void GenType_new::postvisit( ast::TupleType const * type ) {
	assertf( !options.genC, "TupleType should not reach code generation." );
	unsigned int i = 0;
	std::ostringstream os;
	os << "[";
	for ( ast::ptr<ast::Type> const & t : type->types ) {
		i++;
		os << genType( t, "", options ) << (i == type->size() ? "" : ", ");
	}
	os << "] ";
	result = os.str() + result;
}

void GenType_new::postvisit( ast::VarArgsType const * type ) {
	result = "__builtin_va_list " + result;
	handleQualifiers( type );
}

void GenType_new::postvisit( ast::ZeroType const * type ) {
	// Ideally these wouldn't hit codegen at all, but should be safe to make them ints.
	result = (options.pretty ? "zero_t " : "long int ") + result;
	handleQualifiers( type );
}

void GenType_new::postvisit( ast::OneType const * type ) {
	// Ideally these wouldn't hit codegen at all, but should be safe to make them ints.
	result = (options.pretty ? "one_t " : "long int ") + result;
	handleQualifiers( type );
}

void GenType_new::postvisit( ast::GlobalScopeType const * type ) {
	assertf( !options.genC, "GlobalScopeType should not reach code generation." );
	handleQualifiers( type );
}

void GenType_new::postvisit( ast::TraitInstType const * type ) {
	assertf( !options.genC, "TraitInstType should not reach code generation." );
	result = type->name + " " + result;
	handleQualifiers( type );
}

void GenType_new::postvisit( ast::TypeofType const * type ) {
	std::ostringstream os;
	os << "typeof(";
	ast::Pass<CodeGenerator_new>::read( type, os, options );
	os << ") " << result;
	result = os.str();
	handleQualifiers( type );
}

void GenType_new::postvisit( ast::VTableType const * type ) {
	assertf( !options.genC, "Virtual table types should not reach code generation." );
	std::ostringstream os;
	os << "vtable(" << genType( type->base, "", options ) << ") " << result;
	result = os.str();
	handleQualifiers( type );
}

void GenType_new::postvisit( ast::QualifiedType const * type ) {
	assertf( !options.genC, "QualifiedType should not reach code generation." );
	std::ostringstream os;
	os << genType( type->parent, "", options ) << "." << genType( type->child, "", options ) << result;
	result = os.str();
	handleQualifiers( type );
}

void GenType_new::handleQualifiers( ast::Type const * type ) {
	if ( type->is_const() ) {
		result = "const " + result;
	}
	if ( type->is_volatile() ) {
		result = "volatile " + result;
	}
	if ( type->is_restrict() ) {
		result = "__restrict " + result;
	}
	if ( type->is_atomic() ) {
		result = "_Atomic " + result;
	}
}

std::string GenType_new::genParamList( const ast::vector<ast::Type> & range ) {
	auto cur = range.begin();
	auto end = range.end();
	if ( cur == end ) return "";
	std::ostringstream oss;
	UniqueName param( "__param_" );
	while ( true ) {
		oss << genType( *cur++, options.genC ? param.newName() : "", options );
		if ( cur == end ) break;
		oss << ", ";
	}
	return oss.str();
}

} // namespace

std::string genType( ast::Type const * type, const std::string & base, const Options & options ) {
	std::ostringstream os;
	if ( !type->attributes.empty() ) {
		ast::Pass<CodeGenerator_new> cg( os, options );
		cg.core.genAttributes( type->attributes );
	}

	return os.str() + ast::Pass<GenType_new>::read( type, base, options );
}

std::string genTypeNoAttr( ast::Type const * type, const std::string & base, const Options & options ) {
	return ast::Pass<GenType_new>::read( type, base, options );
}

} // namespace CodeGen

// Local Variables: //
// tab-width: 4 //
// mode: c++ //
// compile-command: "make install" //
// End: //
