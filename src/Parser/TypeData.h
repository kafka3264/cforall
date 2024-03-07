//
// Cforall Version 1.0.0 Copyright (C) 2015 University of Waterloo
//
// The contents of this file are covered under the licence agreement in the
// file "LICENCE" distributed with Cforall.
//
// TypeData.h --
//
// Author           : Peter A. Buhr
// Created On       : Sat May 16 15:18:36 2015
// Last Modified By : Peter A. Buhr
// Last Modified On : Thu Feb 22 16:30:31 2024
// Update Count     : 210
//

#pragma once

#include <iosfwd>                                   // for ostream
#include <list>                                     // for list
#include <string>                                   // for string

#include "AST/Type.hpp"                             // for Type
#include "DeclarationNode.h"                        // for DeclarationNode

struct TypeData {
	enum Kind { Basic, Pointer, Reference, Array, Function, Aggregate, AggregateInst, Enum, EnumConstant, Symbolic,
				SymbolicInst, Tuple, Basetypeof, Typeof, Vtable, Builtin, GlobalScope, Qualified, Unknown };

	struct Aggregate_t {
		ast::AggregateDecl::Aggregate kind;
		const std::string * name = nullptr;
		const std::string * parent = nullptr;
		DeclarationNode * params = nullptr;
		ExpressionNode * actuals = nullptr;				// holds actual parameters later applied to AggInst
		DeclarationNode * fields = nullptr;
		std::vector<ast::ptr<ast::Attribute>> attributes;
		bool body;
		bool anon;
		bool tagged;
	};

	struct AggInst_t {
		TypeData * aggregate = nullptr;
		ExpressionNode * params = nullptr;
		bool hoistType;
	};

	struct Array_t {
		ExpressionNode * dimension = nullptr;
		bool isVarLen;
		bool isStatic;
	};

	struct Enumeration_t {
		const std::string * name = nullptr;
		DeclarationNode * constants = nullptr;
		bool body;
		bool anon;
		bool typed;
		EnumHiding hiding;
	};

	struct Function_t {
		mutable DeclarationNode * params = nullptr;		// mutables modified in buildKRFunction
		mutable DeclarationNode * idList = nullptr;		// old-style
		mutable DeclarationNode * oldDeclList = nullptr;
		StatementNode * body = nullptr;
		ExpressionNode * withExprs = nullptr;			// expressions from function's with_clause
	};

	struct Symbolic_t {
		const std::string * name = nullptr;
		bool isTypedef;									// false => TYPEGENname, true => TYPEDEFname
		DeclarationNode * params = nullptr;
		ExpressionNode * actuals = nullptr;
		DeclarationNode * assertions = nullptr;
	};

	struct Qualified_t {								// qualified type S.T
		TypeData * parent = nullptr;
		TypeData * child = nullptr;
	};

	CodeLocation location;

	Kind kind;
	TypeData * base;
	DeclarationNode::BasicType basictype = DeclarationNode::NoBasicType;
	DeclarationNode::ComplexType complextype = DeclarationNode::NoComplexType;
	DeclarationNode::Signedness signedness = DeclarationNode::NoSignedness;
	DeclarationNode::Length length = DeclarationNode::NoLength;
	DeclarationNode::BuiltinType builtintype = DeclarationNode::NoBuiltinType;

	ast::CV::Qualifiers qualifiers;
	DeclarationNode * forall = nullptr;

	Aggregate_t aggregate;
	AggInst_t aggInst;
	Array_t array;
	Enumeration_t enumeration;
	Function_t function;
	Symbolic_t symbolic;
	Qualified_t qualified;
	DeclarationNode * tuple = nullptr;
	ExpressionNode * typeexpr = nullptr;

	TypeData( Kind k = Unknown );
	~TypeData();
	void print( std::ostream &, int indent = 0 ) const;
	TypeData * clone() const;

	const std::string * leafName() const;

	TypeData * getLastBase();
	void setLastBase( TypeData * );
};


TypeData * build_type_qualifier( ast::CV::Qualifiers );
TypeData * build_basic_type( DeclarationNode::BasicType );
TypeData * build_complex_type( DeclarationNode::ComplexType );
TypeData * build_signedness( DeclarationNode::Signedness );
TypeData * build_builtin_type( DeclarationNode::BuiltinType );
TypeData * build_length( DeclarationNode::Length );
TypeData * build_forall( DeclarationNode * );
TypeData * build_global_scope();
TypeData * build_qualified_type( TypeData *, TypeData * );
TypeData * build_typedef( const std::string * name );
TypeData * build_type_gen( const std::string * name, ExpressionNode * params );
TypeData * build_vtable_type( TypeData * );

TypeData * addQualifiers( TypeData * ltype, TypeData * rtype );
TypeData * addType( TypeData * ltype, TypeData * rtype, std::vector<ast::ptr<ast::Attribute>> & );
TypeData * addType( TypeData * ltype, TypeData * rtype );
TypeData * cloneBaseType( TypeData * type, TypeData * other );
TypeData * makeNewBase( TypeData * type );


ast::Type * typebuild( const TypeData * );
TypeData * typeextractAggregate( const TypeData * td, bool toplevel = true );
ast::CV::Qualifiers buildQualifiers( const TypeData * td );
ast::Type * buildBasicType( const TypeData * );
ast::PointerType * buildPointer( const TypeData * );
ast::ArrayType * buildArray( const TypeData * );
ast::ReferenceType * buildReference( const TypeData * );
ast::AggregateDecl * buildAggregate( const TypeData *, std::vector<ast::ptr<ast::Attribute>> );
ast::BaseInstType * buildComAggInst( const TypeData *, std::vector<ast::ptr<ast::Attribute>> && attributes, ast::Linkage::Spec linkage );
ast::BaseInstType * buildAggInst( const TypeData * );
ast::TypeDecl * buildVariable( const TypeData * );
ast::EnumDecl * buildEnum( const TypeData *, std::vector<ast::ptr<ast::Attribute>> &&, ast::Linkage::Spec );
ast::TypeInstType * buildSymbolicInst( const TypeData * );
ast::TupleType * buildTuple( const TypeData * );
ast::TypeofType * buildTypeof( const TypeData * );
ast::VTableType * buildVtable( const TypeData * );
ast::Decl * buildDecl(
	const TypeData *, const std::string &, ast::Storage::Classes, ast::Expr *,
	ast::Function::Specs funcSpec, ast::Linkage::Spec, ast::Expr * asmName,
	ast::Init * init = nullptr, std::vector<ast::ptr<ast::Attribute>> && attributes = std::vector<ast::ptr<ast::Attribute>>() );
ast::FunctionType * buildFunctionType( const TypeData * );
void buildKRFunction( const TypeData::Function_t & function );

static inline ast::Type * maybeMoveBuildType( TypeData * type ) {
	ast::Type * ret = type ? typebuild( type ) : nullptr;
	delete type;
	return ret;
}

// Local Variables: //
// tab-width: 4 //
// mode: c++ //
// compile-command: "make install" //
// End: //
