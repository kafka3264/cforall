//
// Cforall Version 1.0.0 Copyright (C) 2015 University of Waterloo
//
// The contents of this file are covered under the licence agreement in the
// file "LICENCE" distributed with Cforall.
//
// Decl.hpp --
//
// Author           : Aaron B. Moss
// Created On       : Thu May 9 10:00:00 2019
// Last Modified By : Andrew Beach
// Last Modified On : Wed Apr  5 10:42:00 2023
// Update Count     : 35
//

#pragma once

#include <iosfwd>
#include <string>              // for string, to_string
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "FunctionSpec.hpp"
#include "Fwd.hpp"             // for UniqueId
#include "LinkageSpec.hpp"
#include "Node.hpp"            // for ptr, readonly
#include "ParseNode.hpp"
#include "StorageClasses.hpp"
#include "Visitor.hpp"

// Must be included in *all* AST classes; should be #undef'd at the end of the file
#define MUTATE_FRIEND \
	template<typename node_t> friend node_t * mutate(const node_t * node); \
	template<typename node_t> friend node_t * shallowCopy(const node_t * node);

namespace ast {

/// Base declaration class
class Decl : public ParseNode {
public:
	std::string name;
	Storage::Classes storage;
	Linkage::Spec linkage;
	UniqueId uniqueId = 0;
	bool extension = false;

	Decl( const CodeLocation& loc, const std::string& name, Storage::Classes storage,
		Linkage::Spec linkage )
	: ParseNode( loc ), name( name ), storage( storage ), linkage( linkage ) {}

	Decl* set_extension( bool ex ) { extension = ex; return this; }

	/// Ensures this node has a unique ID
	void fixUniqueId();

	const Decl * accept( Visitor & v ) const override = 0;
private:
	Decl * clone() const override = 0;
	MUTATE_FRIEND
};

/// Typed declaration base class
class DeclWithType : public Decl {
public:
	/// Represents the type with all types and typedefs expanded.
	std::string mangleName;
	/// Stores the scope level at which the variable was declared.
	/// Used to access shadowed identifiers.
	int scopeLevel = 0;

	std::vector<ptr<Attribute>> attributes;
	Function::Specs funcSpec;
	ptr<Expr> asmName;
	bool isDeleted = false;
	bool isTypeFixed = false;

	DeclWithType( const CodeLocation& loc, const std::string& name, Storage::Classes storage,
		Linkage::Spec linkage, std::vector<ptr<Attribute>>&& attrs, Function::Specs fs )
	: Decl( loc, name, storage, linkage ), mangleName(), attributes( std::move(attrs) ),
		funcSpec(fs), asmName() {}

	std::string scopedMangleName() const { return mangleName + "_" + std::to_string(scopeLevel); }

	/// Get type of this declaration. May be generated by subclass
	virtual const Type * get_type() const = 0;
	/// Set type of this declaration. May be verified by subclass
	virtual void set_type( const Type * ) = 0;

	const DeclWithType * accept( Visitor & v ) const override = 0;
private:
	DeclWithType * clone() const override = 0;
	MUTATE_FRIEND
};

/// Object declaration `Foo foo = 42;`
class ObjectDecl final : public DeclWithType {
public:
	ptr<Type> type;
	ptr<Init> init;
	ptr<Expr> bitfieldWidth;

	ObjectDecl( const CodeLocation & loc, const std::string & name, const Type * type,
		const Init * init = nullptr, Storage::Classes storage = {},
		Linkage::Spec linkage = Linkage::Cforall, const Expr * bitWd = nullptr,
		std::vector< ptr<Attribute> > && attrs = {}, Function::Specs fs = {} )
	: DeclWithType( loc, name, storage, linkage, std::move(attrs), fs ), type( type ),
	  init( init ), bitfieldWidth( bitWd ) {}

	const Type* get_type() const override { return type; }
	void set_type( const Type * ty ) override { type = ty; }

	const DeclWithType * accept( Visitor& v ) const override { return v.visit( this ); }
private:
	ObjectDecl * clone() const override { return new ObjectDecl{ *this }; }
	MUTATE_FRIEND
};

/// Function variable arguments flag
enum ArgumentFlag { FixedArgs, VariableArgs };

/// Object declaration `int foo()`
class FunctionDecl final : public DeclWithType {
public:
	std::vector<ptr<TypeDecl>> type_params;
	std::vector<ptr<DeclWithType>> assertions;
	std::vector<ptr<DeclWithType>> params;
	std::vector<ptr<DeclWithType>> returns;
	// declared type, derived from parameter declarations
	ptr<FunctionType> type;
	/// Null for the forward declaration of a function.
	ptr<CompoundStmt> stmts;
	std::vector< ptr<Expr> > withExprs;

	/// Monomorphic Function Constructor:
	FunctionDecl( const CodeLocation & locaction, const std::string & name,
		std::vector<ptr<DeclWithType>>&& params, std::vector<ptr<DeclWithType>>&& returns,
		CompoundStmt * stmts, Storage::Classes storage = {}, Linkage::Spec linkage = Linkage::Cforall,
		std::vector<ptr<Attribute>>&& attrs = {}, Function::Specs fs = {}, ArgumentFlag isVarArgs = FixedArgs );

	/// Polymorphic Function Constructor:
	FunctionDecl( const CodeLocation & location, const std::string & name,
		std::vector<ptr<TypeDecl>>&& forall, std::vector<ptr<DeclWithType>>&& assertions,
		std::vector<ptr<DeclWithType>>&& params, std::vector<ptr<DeclWithType>>&& returns,
		CompoundStmt * stmts, Storage::Classes storage = {}, Linkage::Spec linkage = Linkage::Cforall,
		std::vector<ptr<Attribute>>&& attrs = {}, Function::Specs fs = {}, ArgumentFlag isVarArgs = FixedArgs );

	const Type * get_type() const override;
	void set_type( const Type * t ) override;

	bool has_body() const { return stmts; }

	const DeclWithType * accept( Visitor & v ) const override { return v.visit( this ); }
private:
	FunctionDecl * clone() const override { return new FunctionDecl( *this ); }
	MUTATE_FRIEND
};

/// Base class for named type aliases
class NamedTypeDecl : public Decl {
public:
	ptr<Type> base;
	std::vector<ptr<DeclWithType>> assertions;

	NamedTypeDecl(
		const CodeLocation & loc, const std::string & name, Storage::Classes storage,
		const Type * b, Linkage::Spec spec = Linkage::Cforall )
	: Decl( loc, name, storage, spec ), base( b ), assertions() {}

	/// Produces a name for the kind of alias
	virtual const char * typeString() const = 0;

private:
	NamedTypeDecl* clone() const override = 0;
	MUTATE_FRIEND
};

/// Cforall type variable: `dtype T`
class TypeDecl final : public NamedTypeDecl {
  public:
	enum Kind { Dtype, DStype, Otype, Ftype, Ttype, Dimension, NUMBER_OF_KINDS };

	Kind kind;
	bool sized;
	ptr<Type> init;

	TypeDecl(
		const CodeLocation & loc, const std::string & name, Storage::Classes storage,
		const Type * b, TypeDecl::Kind k, bool s, const Type * i = nullptr )
	: NamedTypeDecl( loc, name, storage, b ), kind( k ), sized( k == TypeDecl::Ttype || s ),
	  init( i ) {}

	const char * typeString() const override;
	/// Produces a name for generated code
	const char * genTypeString() const;

	/// convenience accessor to match Type::isComplete()
	bool isComplete() const { return sized; }

	const Decl * accept( Visitor & v ) const override { return v.visit( this ); }
  private:
	TypeDecl * clone() const override { return new TypeDecl{ *this }; }
	MUTATE_FRIEND
};

/// Data extracted from a TypeDecl.
struct TypeData {
	TypeDecl::Kind kind;
	bool isComplete;

	TypeData() : kind( TypeDecl::NUMBER_OF_KINDS ), isComplete( false ) {}
	TypeData( const TypeDecl * d ) : kind( d->kind ), isComplete( d->sized ) {}
	TypeData( TypeDecl::Kind k, bool c ) : kind( k ), isComplete( c ) {}
	TypeData( const TypeData & d1, const TypeData & d2 )
		: kind( d1.kind ), isComplete( d1.isComplete || d2.isComplete ) {}

	bool operator==( const TypeData & o ) const { return kind == o.kind && isComplete == o.isComplete; }
	bool operator!=( const TypeData & o ) const { return !(*this == o); }
};

std::ostream & operator<< ( std::ostream &, const TypeData & );

/// C-style typedef `typedef Foo Bar`
class TypedefDecl final : public NamedTypeDecl {
public:
	TypedefDecl( const CodeLocation& loc, const std::string& name, Storage::Classes storage,
		Type* b, Linkage::Spec spec = Linkage::Cforall )
	: NamedTypeDecl( loc, name, storage, b, spec ) {}

	const char * typeString() const override { return "typedef"; }

	const Decl * accept( Visitor & v ) const override { return v.visit( this ); }
private:
	TypedefDecl * clone() const override { return new TypedefDecl{ *this }; }
	MUTATE_FRIEND
};

/// Aggregate type declaration base class
class AggregateDecl : public Decl {
public:
	enum Aggregate { Struct, Union, Enum, Exception, Trait, Generator, Coroutine, Monitor, Thread, NoAggregate };
	static const char * aggrString( Aggregate aggr );

	std::vector<ptr<Decl>> members;
	std::vector<ptr<TypeDecl>> params;
	std::vector<ptr<Attribute>> attributes;
	bool body = false;
	readonly<AggregateDecl> parent = {};

	AggregateDecl( const CodeLocation& loc, const std::string& name,
		std::vector<ptr<Attribute>>&& attrs = {}, Linkage::Spec linkage = Linkage::Cforall )
	: Decl( loc, name, Storage::Classes{}, linkage ), members(), params(),
	  attributes( std::move(attrs) ) {}

	AggregateDecl* set_body( bool b ) { body = b; return this; }

	/// Produces a name for the kind of aggregate
	virtual const char * typeString() const = 0;

private:
	AggregateDecl * clone() const override = 0;
	MUTATE_FRIEND
};

/// struct declaration `struct Foo { ... };`
class StructDecl final : public AggregateDecl {
public:
	Aggregate kind;

	StructDecl( const CodeLocation& loc, const std::string& name,
		Aggregate kind = Struct,
		std::vector<ptr<Attribute>>&& attrs = {}, Linkage::Spec linkage = Linkage::Cforall )
	: AggregateDecl( loc, name, std::move(attrs), linkage ), kind( kind ) {}

	bool is_coroutine() const { return kind == Coroutine; }
	bool is_generator() const { return kind == Generator; }
	bool is_monitor  () const { return kind == Monitor  ; }
	bool is_thread   () const { return kind == Thread   ; }

	const Decl * accept( Visitor & v ) const override { return v.visit( this ); }

	const char * typeString() const override { return aggrString( kind ); }

private:
	StructDecl * clone() const override { return new StructDecl{ *this }; }
	MUTATE_FRIEND
};

/// union declaration `union Foo { ... };`
class UnionDecl final : public AggregateDecl {
public:
	UnionDecl( const CodeLocation& loc, const std::string& name,
		std::vector<ptr<Attribute>>&& attrs = {}, Linkage::Spec linkage = Linkage::Cforall )
	: AggregateDecl( loc, name, std::move(attrs), linkage ) {}

	const Decl * accept( Visitor& v ) const override { return v.visit( this ); }

	const char * typeString() const override { return aggrString( Union ); }

private:
	UnionDecl * clone() const override { return new UnionDecl{ *this }; }
	MUTATE_FRIEND
};

enum class EnumAttribute{ Value, Posn, Label };
/// enum declaration `enum Foo { ... };`
class EnumDecl final : public AggregateDecl {
public:
	// isTyped indicated if the enum has a declaration like:
	// enum (type_optional) Name {...}
	bool isTyped;
	// if isTyped == true && base.get() == nullptr, it is a "void" type enum
	ptr<Type> base;
	enum class EnumHiding { Visible, Hide } hide;
	EnumDecl( const CodeLocation& loc, const std::string& name, bool isTyped = false,
		std::vector<ptr<Attribute>>&& attrs = {}, Linkage::Spec linkage = Linkage::Cforall,
		Type const * base = nullptr, EnumHiding hide = EnumHiding::Hide,
		std::unordered_map< std::string, long long > enumValues = std::unordered_map< std::string, long long >() )
	: AggregateDecl( loc, name, std::move(attrs), linkage ), isTyped(isTyped), base(base), hide(hide), enumValues(enumValues) {}

	/// gets the integer value for this enumerator, returning true iff value found
	// Maybe it is not used in producing the enum value
	bool valueOf( const Decl * enumerator, long long& value ) const;

	const Decl * accept( Visitor & v ) const override { return v.visit( this ); }

	const char * typeString() const override { return aggrString( Enum ); }

	const std::string getUnmangeldArrayName( const EnumAttribute attr ) const;
private:
	EnumDecl * clone() const override { return new EnumDecl{ *this }; }
	MUTATE_FRIEND

	/// Map from names to enumerator values; kept private for lazy initialization
	mutable std::unordered_map< std::string, long long > enumValues;
};

/// trait declaration `trait Foo( ... ) { ... };`
class TraitDecl final : public AggregateDecl {
public:
	TraitDecl( const CodeLocation& loc, const std::string& name,
		std::vector<ptr<Attribute>>&& attrs = {}, Linkage::Spec linkage = Linkage::Cforall )
	: AggregateDecl( loc, name, std::move(attrs), linkage ) {}

	const Decl * accept( Visitor & v ) const override { return v.visit( this ); }

	const char * typeString() const override { return "trait"; }

private:
	TraitDecl * clone() const override { return new TraitDecl{ *this }; }
	MUTATE_FRIEND
};

/// With statement `with (...) ...`
/// This is a statement lexically, but a Decl is needed for the SymbolTable.
class WithStmt final : public Decl {
public:
	std::vector<ptr<Expr>> exprs;
	ptr<Stmt> stmt;

	WithStmt( const CodeLocation & loc, std::vector<ptr<Expr>> && exprs, const Stmt * stmt )
	: Decl(loc, "", Storage::Auto, Linkage::Cforall), exprs(std::move(exprs)), stmt(stmt) {}

	const Decl * accept( Visitor & v ) const override { return v.visit( this ); }
private:
	WithStmt * clone() const override { return new WithStmt{ *this }; }
	MUTATE_FRIEND
};

/// Assembly declaration: `asm ... ( "..." : ... )`
class AsmDecl final : public Decl {
public:
	ptr<AsmStmt> stmt;

	AsmDecl( const CodeLocation & loc, AsmStmt * stmt )
	: Decl( loc, "", {}, Linkage::C ), stmt(stmt) {}

	const AsmDecl * accept( Visitor & v ) const override { return v.visit( this ); }
private:
	AsmDecl * clone() const override { return new AsmDecl( *this ); }
	MUTATE_FRIEND
};

/// C-preprocessor directive `#...`
class DirectiveDecl final : public Decl {
public:
	ptr<DirectiveStmt> stmt;

	DirectiveDecl( const CodeLocation & loc, DirectiveStmt * stmt )
	: Decl( loc, "", {}, Linkage::C ), stmt(stmt) {}

	const DirectiveDecl * accept( Visitor & v ) const override { return v.visit( this ); }
private:
	DirectiveDecl * clone() const override { return new DirectiveDecl( *this ); }
	MUTATE_FRIEND
};

/// Static Assertion `_Static_assert( ... , ... );`
class StaticAssertDecl final : public Decl {
public:
	ptr<Expr> cond;
	ptr<ConstantExpr> msg;   // string literal

	StaticAssertDecl( const CodeLocation & loc, const Expr * condition, const ConstantExpr * msg )
	: Decl( loc, "", {}, Linkage::C ), cond( condition ), msg( msg ) {}

	const StaticAssertDecl * accept( Visitor & v ) const override { return v.visit( this ); }
private:
	StaticAssertDecl * clone() const override { return new StaticAssertDecl( *this ); }
	MUTATE_FRIEND
};

/// Inline Member Declaration `inline TypeName;`
class InlineMemberDecl final : public DeclWithType {
public:
	ptr<Type> type;

	InlineMemberDecl( const CodeLocation & loc, const std::string & name, const Type * type,
		Storage::Classes storage = {}, Linkage::Spec linkage = Linkage::Cforall,
		std::vector< ptr<Attribute> > && attrs = {}, Function::Specs fs = {} )
	: DeclWithType( loc, name, storage, linkage, std::move(attrs), fs ), type( type ) {}

	const Type * get_type() const override { return type; }
	void set_type( const Type * ty ) override { type = ty; }

	const DeclWithType * accept( Visitor& v ) const override { return v.visit( this ); }
private:
	InlineMemberDecl * clone() const override { return new InlineMemberDecl{ *this }; }
	MUTATE_FRIEND
};

}

#undef MUTATE_FRIEND

// Local Variables: //
// tab-width: 4 //
// mode: c++ //
// compile-command: "make install" //
// End: //
