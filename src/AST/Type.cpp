//
// Cforall Version 1.0.0 Copyright (C) 2015 University of Waterloo
//
// The contents of this file are covered under the licence agreement in the
// file "LICENCE" distributed with Cforall.
//
// Type.cpp --
//
// Author           : Aaron B. Moss
// Created On       : Mon May 13 15:00:00 2019
// Last Modified By : Andrew Beach
// Last Modified On : Thu Jul 23 14:16:00 2020
// Update Count     : 5
//

#include "Type.hpp"

#include <cassert>
#include <utility>               // for move
#include <vector>

#include "Decl.hpp"
#include "Init.hpp"
#include "Common/utility.h"      // for copy, move
#include "InitTweak/InitTweak.h" // for getPointerBase
#include "Tuples/Tuples.h"       // for isTtype

namespace ast {

const Type * Type::getComponent( unsigned i ) const {
	assertf( size() == 1 && i == 0, "Type::getComponent was called with size %d and index %d\n", size(), i );
	return this;
}

const Type * Type::stripDeclarator() const {
	const Type * t;
	const Type * a;
	for ( t = this; (a = InitTweak::getPointerBase( t )); t = a );
	return t;
}

const Type * Type::stripReferences() const {
	const Type * t;
	const ReferenceType * r;
	for ( t = this; (r = dynamic_cast<const ReferenceType *>(t) ); t = r->base );
	return t;
}

// --- BasicType

// GENERATED START, DO NOT EDIT
// GENERATED BY BasicTypes-gen.cc
const char * BasicType::typeNames[] = {
	"_Bool",
	"char",
	"signed char",
	"unsigned char",
	"signed short int",
	"unsigned short int",
	"signed int",
	"unsigned int",
	"signed long int",
	"unsigned long int",
	"signed long long int",
	"unsigned long long int",
	"__int128",
	"unsigned __int128",
	"_Float16",
	"_Float16 _Complex",
	"_Float32",
	"_Float32 _Complex",
	"float",
	"float _Complex",
	"_Float32x",
	"_Float32x _Complex",
	"_Float64",
	"_Float64 _Complex",
	"double",
	"double _Complex",
	"_Float64x",
	"_Float64x _Complex",
	"__float80",
	"_Float128",
	"_Float128 _Complex",
	"__float128",
	"long double",
	"long double _Complex",
	"_Float128x",
	"_Float128x _Complex",
};
// GENERATED END

// --- FunctionType
namespace {
	bool containsTtype( const std::vector<ptr<Type>> & l ) {
		if ( ! l.empty() ) {
			return Tuples::isTtype( l.back() );
		}
		return false;
	}
}

bool FunctionType::isTtype() const {
	return containsTtype( returns ) || containsTtype( params );
}

// --- BaseInstType

std::vector<readonly<Decl>> BaseInstType::lookup( const std::string& name ) const {
	assertf( aggr(), "Must have aggregate to perform lookup" );

	std::vector<readonly<Decl>> found;
	for ( const Decl * decl : aggr()->members ) {
		if ( decl->name == name ) { found.emplace_back( decl ); }
	}
	return found;
}

// --- SueInstType (StructInstType, UnionInstType, EnumInstType)

template<typename decl_t>
SueInstType<decl_t>::SueInstType(
	const base_type * b, CV::Qualifiers q, std::vector<ptr<Attribute>>&& as )
: BaseInstType( b->name, q, std::move(as) ), base( b ) {}

template<typename decl_t>
SueInstType<decl_t>::SueInstType(
	const base_type * b, std::vector<ptr<Expr>> && params,
	CV::Qualifiers q, std::vector<ptr<Attribute>> && as )
: BaseInstType( b->name, std::move(params), q, std::move(as) ), base( b ) {}

template<typename decl_t>
bool SueInstType<decl_t>::isComplete() const {
	return base ? base->body : false;
}

template class SueInstType<StructDecl>;
template class SueInstType<UnionDecl>;
template class SueInstType<EnumDecl>;

// --- TraitInstType

TraitInstType::TraitInstType(
	const TraitDecl * b, CV::Qualifiers q, std::vector<ptr<Attribute>>&& as )
: BaseInstType( b->name, q, move(as) ), base( b ) {}

// --- TypeInstType

TypeInstType::TypeInstType( const TypeDecl * b,
	CV::Qualifiers q, std::vector<ptr<Attribute>> && as )
: BaseInstType( b->name, q, move(as) ), base( b ), kind( b->kind ) {}

void TypeInstType::set_base( const TypeDecl * b ) {
	base = b;
	kind = b->kind;
}

bool TypeInstType::isComplete() const { return base->sized; }

// --- TupleType

TupleType::TupleType( std::vector<ptr<Type>> && ts, CV::Qualifiers q )
: Type( q ), types( move(ts) ), members() {
	// This constructor is awkward. `TupleType` needs to contain objects so that members can be
	// named, but members without initializer nodes end up getting constructors, which breaks
	// things. This happens because the object decls have to be visited so that their types are
	// kept in sync with the types listed here. Ultimately, the types listed here should perhaps
	// be eliminated and replaced with a list-view over members. The temporary solution is to
	// make a `ListInit` with `maybeConstructed = false`, so when the object is visited it is not
	// constructed. Potential better solutions include:
	//   a) Separate `TupleType` from its declarations, into `TupleDecl` and `Tuple{Inst?}Type`,
	//      similar to the aggregate types.
	//   b) Separate initializer nodes better, e.g. add a `MaybeConstructed` node that is replaced
	//      by `genInit`, rather than the current boolean flag.
	members.reserve( types.size() );
	for ( const Type * ty : types ) {
		members.emplace_back( new ObjectDecl{
			CodeLocation{}, "", ty, new ListInit( CodeLocation{}, {}, {}, NoConstruct ),
			Storage::Classes{}, Linkage::Cforall } );
	}
}

bool isUnboundType(const Type * type) {
	if (auto typeInst = dynamic_cast<const TypeInstType *>(type)) {
		// xxx - look for a type name produced by renameTyVars.

		// TODO: once TypeInstType representation is updated, it should properly check
		// if the context id is filled. this is a temporary hack for now
		return typeInst->formal_usage > 0;
	}
	return false;
}

}

// Local Variables: //
// tab-width: 4 //
// mode: c++ //
// compile-command: "make install" //
// End: //
